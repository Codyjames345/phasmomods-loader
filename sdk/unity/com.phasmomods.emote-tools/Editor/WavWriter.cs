using System;
using System.IO;

namespace PhasmoMods.EmoteTools
{
    /// <summary>
    /// Writes decoded audio back out as a small uncompressed WAV file, used when the emote pack
    /// tool cuts an emote's song to its measured loop. Pure managed code with no Unity types, so
    /// it runs both inside the editor and in plain .NET tests.
    /// </summary>
    public static class WavWriter
    {
        /// <summary>
        /// A complete RIFF/WAVE file: the 44-byte header (format 1 PCM, 16-bit little endian) and
        /// the first frames frames of interleaved, a frame being one sample per channel, so
        /// interleaved holds frames times channels values and extra values are ignored. Each
        /// sample is clamped to -1..1, scaled by 32767 and rounded to the nearest integer.
        /// interleaved is channel samples c, c+1, ... grouped per frame; channels is at least 1
        /// whatever is passed, rate is samples per second.
        /// </summary>
        public static byte[] Pcm16(float[] interleaved, int channels, int rate, int frames)
        {
            int ch = channels < 1 ? 1 : channels;
            int available = interleaved == null ? 0 : interleaved.Length / ch;
            int n = frames < 0 ? 0 : (frames > available ? available : frames);
            int dataBytes = n * ch * 2;
            byte[] wav = new byte[44 + dataBytes];
            WriteTag(wav, 0, "RIFF");
            WriteU32(wav, 4, (uint)(36 + dataBytes));
            WriteTag(wav, 8, "WAVE");
            WriteTag(wav, 12, "fmt ");
            WriteU32(wav, 16, 16);
            WriteU16(wav, 20, 1);
            WriteU16(wav, 22, (ushort)ch);
            WriteU32(wav, 24, (uint)rate);
            WriteU32(wav, 28, (uint)(rate * ch * 2));
            WriteU16(wav, 32, (ushort)(ch * 2));
            WriteU16(wav, 34, 16);
            WriteTag(wav, 36, "data");
            WriteU32(wav, 40, (uint)dataBytes);
            for (int i = 0; i < n * ch; i++)
            {
                double v = interleaved[i];
                if (v < -1.0)
                {
                    v = -1.0;
                }
                else if (v > 1.0)
                {
                    v = 1.0;
                }
                short sample = (short)Math.Round(v * 32767.0);
                wav[44 + 2 * i] = (byte)(sample & 0xFF);
                wav[44 + 2 * i + 1] = (byte)((sample >> 8) & 0xFF);
            }
            return wav;
        }

        /// <summary>
        /// Writes the bytes of Pcm16 to path, replacing any file there. The folder must exist.
        /// </summary>
        public static void Write(string path, float[] interleaved, int channels, int rate, int frames)
        {
            File.WriteAllBytes(path, Pcm16(interleaved, channels, rate, frames));
        }

        private static void WriteTag(byte[] wav, int at, string tag)
        {
            for (int i = 0; i < tag.Length; i++)
            {
                wav[at + i] = (byte)tag[i];
            }
        }

        private static void WriteU16(byte[] wav, int at, ushort value)
        {
            wav[at] = (byte)(value & 0xFF);
            wav[at + 1] = (byte)((value >> 8) & 0xFF);
        }

        private static void WriteU32(byte[] wav, int at, uint value)
        {
            wav[at] = (byte)(value & 0xFF);
            wav[at + 1] = (byte)((value >> 8) & 0xFF);
            wav[at + 2] = (byte)((value >> 16) & 0xFF);
            wav[at + 3] = (byte)((value >> 24) & 0xFF);
        }
    }
}
