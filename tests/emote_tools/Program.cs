using System;
using PhasmoMods.EmoteTools;

// Runs the emote tools checks and reports each failure.
static class Program
{
    static int failures;

    static int Main()
    {
        SongLoopFoundToTheSample();
        SongLoopWithNoiseStillFound();
        SongGuessOutsideSearchReportsLowScore();
        ClipLoopFoundOnPeriodicCurves();
        ClipMotionTermBreaksAPoseTie();
        RmsOfFullScaleSquareIsOne();
        RmsCountsAPartialLastWindow();
        MonoAveragesChannels();
        if (failures == 0)
        {
            Console.WriteLine("emote tools tests passed");
            return 0;
        }
        Console.WriteLine(failures + " check(s) failed");
        return 1;
    }

    static void Fail(string name, string what)
    {
        failures++;
        Console.WriteLine("FAIL " + name + ": " + what);
    }

    // A seeded random period tiled to length samples, values in -0.5..0.5, plus optional
    // seeded noise of plus or minus one percent on every sample.
    static float[] PeriodicSong(int period, int length, int seed, bool noise)
    {
        Random rng = new Random(seed);
        float[] onePeriod = new float[period];
        for (int i = 0; i < period; i++)
        {
            onePeriod[i] = (float)(rng.NextDouble() - 0.5);
        }
        float[] song = new float[length];
        for (int i = 0; i < length; i++)
        {
            float v = onePeriod[i % period];
            if (noise)
            {
                v += (float)(rng.NextDouble() * 0.02 - 0.01);
            }
            song[i] = v;
        }
        return song;
    }

    // frames[i] is every curve's value at frame i, each curve sin(2*pi*i/period + phaseStep*c).
    static float[][] SinFrames(int frameCount, int curveCount, int period, double phaseStep)
    {
        float[][] frames = new float[frameCount][];
        for (int i = 0; i < frameCount; i++)
        {
            frames[i] = new float[curveCount];
            for (int c = 0; c < curveCount; c++)
            {
                frames[i][c] = (float)Math.Sin(2.0 * Math.PI * i / period + phaseStep * c);
            }
        }
        return frames;
    }

    static void SongLoopFoundToTheSample()
    {
        const int rate = 48000;
        float[] song = PeriodicSong(59259, 30 * rate, 1, false);
        LoopResult r = LoopFinder.SongLoop(song, rate, 1.2);
        if (r.At != 59259)
        {
            Fail("song_loop_found_to_the_sample", "At was " + r.At + ", expected 59259");
        }
        if (r.Score < 0.99)
        {
            Fail("song_loop_found_to_the_sample", "Score was " + r.Score + ", expected at least 0.99");
        }
    }

    static void SongLoopWithNoiseStillFound()
    {
        const int rate = 48000;
        float[] song = PeriodicSong(59259, 30 * rate, 2, true);
        LoopResult r = LoopFinder.SongLoop(song, rate, 1.2);
        if (Math.Abs(r.At - 59259) > 2)
        {
            Fail("song_loop_with_noise_still_found", "At was " + r.At + ", expected within 2 samples of 59259");
        }
    }

    static void SongGuessOutsideSearchReportsLowScore()
    {
        const int rate = 48000;
        float[] song = PeriodicSong(59259, 30 * rate, 1, false);
        LoopResult r = LoopFinder.SongLoop(song, rate, 3.0, 0.5);
        if (r.Score >= 0.5)
        {
            Fail("song_guess_outside_search_reports_low_score", "Score was " + r.Score + ", expected below 0.5");
        }
    }

    static void ClipLoopFoundOnPeriodicCurves()
    {
        float[][] frames = SinFrames(200, 6, 37, 2.0 * Math.PI / 6.0);
        LoopResult r = LoopFinder.ClipLoop(frames, 30f, 1.2);
        if (r.At != 37)
        {
            Fail("clip_loop_found_on_periodic_curves", "At was " + r.At + ", expected 37");
        }
    }

    static void ClipMotionTermBreaksAPoseTie()
    {
        // Frame 20 has the same value as frame 0 but is moving the other way, so the
        // motion term must pick frame 40. Search 0.6 s reaches frame 40 (1.333 s),
        // which 0.5 s (1.3 s, rounding to frame 39) would not, and still holds 20.
        float[][] frames = SinFrames(120, 1, 40, 0.0);
        LoopResult r = LoopFinder.ClipLoop(frames, 30f, 0.8, 0.6);
        if (r.At != 40)
        {
            Fail("clip_motion_term_breaks_a_pose_tie", "At was " + r.At + ", expected 40");
        }
    }

    static void RmsOfFullScaleSquareIsOne()
    {
        const int rate = 8000;
        float[] song = new float[rate];
        for (int i = 0; i < rate; i++)
        {
            song[i] = (i % 2 == 0) ? 1f : -1f;
        }
        float[] windows = LoopFinder.RmsWindows(song, rate);
        if (windows.Length != 10)
        {
            Fail("rms_of_a_full_scale_square_is_one", "window count was " + windows.Length + ", expected 10");
        }
        for (int w = 0; w < windows.Length; w++)
        {
            if (Math.Abs(windows[w] - 1.0) > 1e-6)
            {
                Fail("rms_of_a_full_scale_square_is_one", "window " + w + " rms was " + windows[w] + ", expected within 1e-6 of 1");
            }
        }
    }

    static void RmsCountsAPartialLastWindow()
    {
        const int rate = 8000;
        float[] song = new float[8400];
        for (int i = 0; i < song.Length; i++)
        {
            song[i] = 0.5f;
        }
        float[] windows = LoopFinder.RmsWindows(song, rate);
        if (windows.Length != 11)
        {
            Fail("rms_counts_a_partial_last_window", "window count was " + windows.Length + ", expected 11");
        }
        else if (Math.Abs(windows[10] - 0.5) > 1e-6)
        {
            Fail("rms_counts_a_partial_last_window", "last window rms was " + windows[10] + ", expected within 1e-6 of 0.5");
        }
    }

    static void MonoAveragesChannels()
    {
        float[] got = LoopFinder.Mono(new float[] { 1f, 3f, -1f, 1f }, 2);
        if (got.Length != 2 || got[0] != 2f || got[1] != 0f)
        {
            string joined = "";
            for (int i = 0; i < got.Length; i++)
            {
                joined += (i > 0 ? ", " : "") + got[i];
            }
            Fail("mono_averages_channels", "got {" + joined + "}, expected {2, 0}");
        }
    }
}
