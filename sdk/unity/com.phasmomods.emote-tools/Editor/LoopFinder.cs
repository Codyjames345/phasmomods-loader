using System;

namespace PhasmoMods.EmoteTools
{
    /// <summary>
    /// One answer from a LoopFinder search: At is the found sample lag (SongLoop) or frame
    /// index (ClipLoop), and Score is the quality measure at that point.
    /// </summary>
    public struct LoopResult
    {
        /// <summary>
        /// The sample lag or frame index found, or 0 when nothing was found.
        /// </summary>
        public int At;

        /// <summary>
        /// The quality measure at At: the correlation for SongLoop (1 is a perfect repeat),
        /// the cost for ClipLoop (lower is better).
        /// </summary>
        public double Score;
    }

    /// <summary>
    /// Finds emote loop points offline from a rough guess: the sample lag at which an emote
    /// song's audio repeats, and the clip frame at which an emote animation returns to its
    /// start. Pure managed code with no game or Unity types, so it runs both inside the Unity
    /// editor and in plain .NET tests. Call it from the editor or from a build step, never
    /// from a running contract.
    /// </summary>
    public static class LoopFinder
    {
        /// <summary>
        /// The song's loop length in samples: the lag within guessSeconds plus or minus
        /// searchSeconds (converted at rate samples per second) at which the audio best
        /// repeats its first compareSeconds, by normalised cross-correlation (1 is a perfect
        /// repeat). A coarse pass tries every 8th lag using 8-sample block means: the mean of
        /// mono[8j..8j+7] against the mean of mono[lag+8j..lag+8j+7] over
        /// compareSeconds*rate/8 blocks, scored with the same correlation formula, taken from
        /// prefix sums so each block mean is O(1). Means keep a narrow correlation peak from
        /// falling between the coarse grid points, because overlapping shifted blocks still
        /// share samples. A fine pass then tries every lag within plus or minus 16 of the
        /// best coarse one (clamped to the range), sample by sample; its exact correlation is
        /// the reported Score. A lag whose compared window would run past the end of the
        /// audio is skipped; if no lag fits, At is 0 and Score is double.NegativeInfinity.
        /// mono is one channel of audio samples, rate is samples per second, guessSeconds the
        /// expected loop length and searchSeconds the half width around it to try,
        /// compareSeconds how many leading seconds are used as the pattern.
        /// </summary>
        public static LoopResult SongLoop(float[] mono, int rate, double guessSeconds, double searchSeconds = 1.0, double compareSeconds = 4.0)
        {
            LoopResult none = new LoopResult { At = 0, Score = double.NegativeInfinity };
            if (mono == null || rate <= 0)
            {
                return none;
            }
            int n = (int)Math.Round(compareSeconds * rate);
            if (n < 8)
            {
                return none;
            }
            int lo = (int)Math.Round((guessSeconds - searchSeconds) * rate);
            int hi = (int)Math.Round((guessSeconds + searchSeconds) * rate);
            if (lo < 1)
            {
                lo = 1;
            }
            double[] prefix = new double[mono.Length + 1];
            for (int i = 0; i < mono.Length; i++)
            {
                prefix[i + 1] = prefix[i] + mono[i];
            }
            int blocks = n / 8;
            double best = double.NegativeInfinity;
            int bestAt = 0;
            for (int lag = lo; lag <= hi; lag += 8)
            {
                if (lag + 8 * blocks > mono.Length)
                {
                    continue;
                }
                double s = CorrelateBlockMeans(prefix, lag, blocks);
                if (s > best)
                {
                    best = s;
                    bestAt = lag;
                }
            }
            if (bestAt == 0)
            {
                return none;
            }
            int fineLo = Math.Max(lo, bestAt - 16);
            int fineHi = Math.Min(hi, bestAt + 16);
            for (int lag = fineLo; lag <= fineHi; lag++)
            {
                if (lag == bestAt || lag + n > mono.Length)
                {
                    continue;
                }
                double s = Correlate(mono, lag, n);
                if (s > best)
                {
                    best = s;
                    bestAt = lag;
                }
            }
            return new LoopResult { At = bestAt, Score = best };
        }

        /// <summary>
        /// The clip frame that best returns to frame 0: for each frame index f within
        /// guessSeconds plus or minus searchSeconds (converted at fps frames per second, with
        /// 1 &lt;= f and f &lt; frames.Length), cost = sum over curves of
        /// (frames[f][c] - frames[0][c]) squared, plus motionWeight times the sum over curves
        /// of ((frames[f][c] - frames[f-1][c]) - (frames[1][c] - frames[0][c])) squared.
        /// Lower is better; At is the best f and Score its cost. If no f is in range, At is 0
        /// and Score is double.PositiveInfinity. frames[i] is every curve's value at frame i,
        /// fps is the clip's frame rate, guessSeconds the expected loop length, searchSeconds
        /// the half width around it to try and motionWeight the price of a direction change at
        /// the seam.
        /// </summary>
        public static LoopResult ClipLoop(float[][] frames, float fps, double guessSeconds, double searchSeconds = 1.0, float motionWeight = 4f)
        {
            if (frames == null || frames.Length < 2 || fps <= 0f)
            {
                return new LoopResult { At = 0, Score = double.PositiveInfinity };
            }
            int lo = (int)Math.Round((guessSeconds - searchSeconds) * fps);
            int hi = (int)Math.Round((guessSeconds + searchSeconds) * fps);
            if (lo < 1)
            {
                lo = 1;
            }
            if (hi > frames.Length - 1)
            {
                hi = frames.Length - 1;
            }
            int bestAt = 0;
            double bestCost = double.PositiveInfinity;
            for (int f = lo; f <= hi; f++)
            {
                double cost = LoopCost(frames, f, motionWeight);
                if (cost < bestCost)
                {
                    bestCost = cost;
                    bestAt = f;
                }
            }
            return new LoopResult { At = bestAt, Score = bestCost };
        }

        /// <summary>
        /// Root mean square of each window of windowSeconds, the last partial window counted
        /// over its own samples. rate is samples per second. The result has one entry per
        /// window, and is empty for empty audio.
        /// </summary>
        public static float[] RmsWindows(float[] mono, int rate, double windowSeconds = 0.1)
        {
            if (mono == null || rate <= 0)
            {
                return new float[0];
            }
            int w = (int)Math.Round(windowSeconds * rate);
            if (w < 1)
            {
                w = 1;
            }
            int count = (mono.Length + w - 1) / w;
            float[] rms = new float[count];
            for (int k = 0; k < count; k++)
            {
                int start = k * w;
                int end = Math.Min(start + w, mono.Length);
                double sum = 0.0;
                for (int i = start; i < end; i++)
                {
                    sum += mono[i] * mono[i];
                }
                rms[k] = (float)Math.Sqrt(sum / (end - start));
            }
            return rms;
        }

        /// <summary>
        /// Interleaved samples averaged to one channel: channel samples c, c+1, ... for each
        /// channels go together, and the output has one value per group.
        /// </summary>
        public static float[] Mono(float[] interleaved, int channels)
        {
            if (interleaved == null)
            {
                return new float[0];
            }
            int ch = channels < 1 ? 1 : channels;
            float[] one = new float[interleaved.Length / ch];
            for (int i = 0; i < one.Length; i++)
            {
                double sum = 0.0;
                for (int c = 0; c < ch; c++)
                {
                    sum += interleaved[i * ch + c];
                }
                one[i] = (float)(sum / ch);
            }
            return one;
        }

        // Normalised cross-correlation of the 8-sample block means of mono[0..] with those of
        // mono[at..], over the first `blocks` whole blocks of each, scored with the same
        // formula as Correlate. Block means come from prefix, the running totals of mono.
        private static double CorrelateBlockMeans(double[] prefix, int at, int blocks)
        {
            double sumAB = 0.0;
            double sumAA = 0.0;
            double sumBB = 0.0;
            for (int j = 0; j < blocks; j++)
            {
                int p = 8 * j;
                double a = (prefix[p + 8] - prefix[p]) / 8.0;
                double b = (prefix[at + p + 8] - prefix[at + p]) / 8.0;
                sumAB += a * b;
                sumAA += a * a;
                sumBB += b * b;
            }
            double denominator = Math.Sqrt(sumAA * sumBB);
            if (denominator == 0.0)
            {
                return 0.0;
            }
            return sumAB / denominator;
        }

        // Normalised cross-correlation of mono[0..n-1] with mono[at..at+n-1]: the sum of the
        // products over the square root of the product of the two sums of squares, 0 when
        // either window is silent.
        private static double Correlate(float[] mono, int at, int n)
        {
            double sumAB = 0.0;
            double sumAA = 0.0;
            double sumBB = 0.0;
            for (int i = 0; i < n; i++)
            {
                double a = mono[i];
                double b = mono[at + i];
                sumAB += a * b;
                sumAA += a * a;
                sumBB += b * b;
            }
            double denominator = Math.Sqrt(sumAA * sumBB);
            if (denominator == 0.0)
            {
                return 0.0;
            }
            return sumAB / denominator;
        }

        // How far frame f sits from frame 0 in value, plus the weighted price of the step into
        // f differing from the step into frame 1.
        private static double LoopCost(float[][] frames, int f, float motionWeight)
        {
            double pose = 0.0;
            double motion = 0.0;
            float[] cur = frames[f];
            float[] prev = frames[f - 1];
            float[] first = frames[0];
            float[] second = frames[1];
            for (int c = 0; c < cur.Length; c++)
            {
                double p = cur[c] - first[c];
                pose += p * p;
                double m = (cur[c] - prev[c]) - (second[c] - first[c]);
                motion += m * m;
            }
            return pose + motionWeight * motion;
        }
    }
}
