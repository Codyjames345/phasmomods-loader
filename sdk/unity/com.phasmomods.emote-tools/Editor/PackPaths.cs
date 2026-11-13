using System;

namespace PhasmoMods.EmoteTools
{
    /// <summary>
    /// Checks on the paths an emote catalogue names, before the tool reads or writes them. A catalogue may come from
    /// another author, so nothing it names may reach outside the pack folder (its loudness files and its bundle) or the
    /// Unity project's Assets folder (its clips, songs, sources and icons). The mod checks a pack's loudness paths the
    /// same way when it loads them (mods/emotes/catalogue.h).
    /// </summary>
    public static class PackPaths
    {
        /// <summary>
        /// Whether `path` is a file under the pack folder: relative, with '/' between its parts, and no part empty, "."
        /// or "..", and no drive or colon.
        /// </summary>
        public static bool IsPackFile(string path)
        {
            if (string.IsNullOrEmpty(path) || path.IndexOf('\\') >= 0 || path.IndexOf(':') >= 0)
            {
                return false;
            }
            foreach (string part in path.Split('/'))
            {
                if (part.Length == 0 || part == "." || part == "..")
                {
                    return false;
                }
            }
            return true;
        }

        /// <summary>
        /// Whether `path` is a file under the project's Assets folder: "Assets/" then a pack file path.
        /// </summary>
        public static bool IsProjectAsset(string path)
        {
            const string prefix = "Assets/";
            return path != null && path.StartsWith(prefix, StringComparison.Ordinal) && IsPackFile(path.Substring(prefix.Length));
        }

        /// <summary>
        /// Whether a source and the output the tool writes from it are two assets: the tool would otherwise overwrite
        /// the source with its own cut. Unity's asset paths ignore case. False with no source.
        /// </summary>
        public static bool DistinctSourceAndOutput(string source, string output)
        {
            return !string.IsNullOrEmpty(source) && !string.Equals(source, output, StringComparison.OrdinalIgnoreCase);
        }

        /// <summary>
        /// Whether every path a catalogue names stays where the tool may read and write: asset paths (clips, songs,
        /// icons, sources, a synced emote's second part) under the project's Assets folder, loudness files under the
        /// pack folder, and the bundle a plain file name in it; and no source is the cut the tool writes from it. A
        /// catalogue may come from another author, and the tool writes where it says. `why` names the first that does
        /// not.
        /// </summary>
        public static bool CheckCatalogue(CatalogueFile cat, out string why)
        {
            if (!string.IsNullOrEmpty(cat.bundle) && !IsPlainFileName(cat.bundle))
            {
                why = "the bundle name " + cat.bundle + " is not a plain file name";
                return false;
            }
            foreach (EmoteEntry e in cat.emotes)
            {
                if ((!string.IsNullOrEmpty(e.clipSource) && !DistinctSourceAndOutput(e.clipSource, e.clip)) ||
                    (!string.IsNullOrEmpty(e.songSource) && !DistinctSourceAndOutput(e.songSource, e.song)))
                {
                    why = e.name + " names one asset as both a source and the cut the tool writes from it";
                    return false;
                }
                PartnerEntry p = e.partner ?? new PartnerEntry();
                foreach (string asset in new[] { e.clip, e.moveClip, e.introClip, e.song, e.introSong, e.icon, e.clipSource,
                                                 e.songSource, p.clip, p.introClip, p.song })
                {
                    if (!string.IsNullOrEmpty(asset) && !IsProjectAsset(asset))
                    {
                        why = e.name + " names " + asset + ", which is not a file under the project's Assets folder";
                        return false;
                    }
                }
                foreach (string file in new[] { e.loudness, e.introLoudness, p.loudness })
                {
                    if (!string.IsNullOrEmpty(file) && !IsPackFile(file))
                    {
                        why = e.name + " names " + file + ", which is not a file under the pack folder";
                        return false;
                    }
                }
            }
            why = null;
            return true;
        }

        /// <summary>
        /// Whether `name` is a plain file name, such as a bundle's: one pack file path part.
        /// </summary>
        public static bool IsPlainFileName(string name)
        {
            return IsPackFile(name) && name.IndexOf('/') < 0;
        }
    }
}
