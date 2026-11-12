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
        /// Whether `name` is a plain file name, such as a bundle's: one pack file path part.
        /// </summary>
        public static bool IsPlainFileName(string name)
        {
            return IsPackFile(name) && name.IndexOf('/') < 0;
        }
    }
}
