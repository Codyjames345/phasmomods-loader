using System;
using System.Globalization;
using System.Text;

namespace PhasmoMods.EmoteTools
{
    /// <summary>
    /// Edits an emote catalogue's text in place, so its comments and layout survive: the catalogue
    /// is JSON with "//" line comments (mods/emotes/assets/emotes.json), which a JSON library would
    /// throw away. Pure managed code with no Unity types, so it runs both inside the editor and in
    /// plain .NET tests. Every scan here ignores braces, colons and quotes inside comments and
    /// inside JSON strings.
    /// </summary>
    public static class CatalogueText
    {
        /// <summary>
        /// Removes every "//" comment up to the end of its line, keeping newlines and leaving
        /// anything after "//" inside a JSON string (a "\" // note\"" keeps its slash pair).
        /// Null in, null out.
        /// </summary>
        public static string StripComments(string text)
        {
            if (text == null)
            {
                return null;
            }
            StringBuilder sb = new StringBuilder(text.Length);
            bool inString = false;
            bool escaped = false;
            for (int i = 0; i < text.Length; i++)
            {
                char c = text[i];
                if (inString)
                {
                    sb.Append(c);
                    if (escaped)
                    {
                        escaped = false;
                    }
                    else if (c == '\\')
                    {
                        escaped = true;
                    }
                    else if (c == '"')
                    {
                        inString = false;
                    }
                }
                else if (c == '"')
                {
                    inString = true;
                    sb.Append(c);
                }
                else if (c == '/' && i + 1 < text.Length && text[i + 1] == '/')
                {
                    i = EndOfLine(text, i);
                    if (i < text.Length)
                    {
                        sb.Append('\n');
                    }
                }
                else
                {
                    sb.Append(c);
                }
            }
            return sb.ToString();
        }

        /// <summary>
        /// Finds the object in the top-level "emotes" array whose "name" value equals name: open is
        /// the index of its "{" and close the index of its matching "}". False when absent, with
        /// open and close at -1.
        /// </summary>
        public static bool FindEmote(string text, string name, out int open, out int close)
        {
            open = -1;
            close = -1;
            if (text == null || name == null)
            {
                return false;
            }
            int arrOpen = EmotesArrayOpen(text);
            if (arrOpen < 0)
            {
                return false;
            }
            int i = arrOpen + 1;
            while (true)
            {
                i = SkipSpaceAndComments(text, i);
                if (i < text.Length && text[i] == ',')
                {
                    i = SkipSpaceAndComments(text, i + 1);
                }
                if (i >= text.Length || text[i] == ']')
                {
                    return false;
                }
                if (text[i] != '{')
                {
                    return false;
                }
                int end = MatchBrace(text, i);
                if (end < 0)
                {
                    return false;
                }
                int afterColon;
                if (FindKey(text, i, end, "name", out afterColon))
                {
                    int valueOpen = SkipSpaceAndComments(text, afterColon);
                    int valueClose;
                    string value;
                    if (valueOpen < end && text[valueOpen] == '"' && ReadString(text, valueOpen, out valueClose, out value) && value == name)
                    {
                        open = i;
                        close = end;
                        return true;
                    }
                }
                i = end + 1;
            }
        }

        /// <summary>
        /// Replaces the value of that emote's "songLoop" object with {"start": start, "seconds":
        /// seconds}, or, when the emote has no songLoop, inserts ", "songLoop": {...}" just before
        /// the emote's closing brace, after its last non-space character. Numbers are written
        /// invariant with format "0.######". Throws ArgumentException when the emote is absent.
        /// Every other byte, comments included, is unchanged.
        /// </summary>
        public static string SetSongLoop(string text, string name, double start, double seconds)
        {
            int open;
            int close;
            if (!FindEmote(text, name, out open, out close))
            {
                throw new ArgumentException("the catalogue has no emote named " + name, "name");
            }
            string replacement = "{\"start\": " + Num(start) + ", \"seconds\": " + Num(seconds) + "}";
            int afterColon;
            if (FindKey(text, open, close, "songLoop", out afterColon))
            {
                int valueOpen = SkipSpaceAndComments(text, afterColon);
                if (valueOpen < close && text[valueOpen] == '{')
                {
                    int valueClose = MatchBrace(text, valueOpen);
                    if (valueClose < 0)
                    {
                        throw new ArgumentException("emote " + name + " has a songLoop that is not a closed object", "text");
                    }
                    return text.Substring(0, valueOpen) + replacement + text.Substring(valueClose + 1);
                }
            }
            int at = close;
            while (at > open + 1 && char.IsWhiteSpace(text[at - 1]))
            {
                at--;
            }
            return text.Substring(0, at) + ", \"songLoop\": " + replacement + text.Substring(at);
        }

        /// <summary>
        /// The emote's "field": true|false value at the top level of its object, or fallback when
        /// the emote, the field or its word is absent.
        /// </summary>
        public static bool BoolField(string text, string name, string field, bool fallback)
        {
            int open;
            int close;
            if (!FindEmote(text, name, out open, out close))
            {
                return fallback;
            }
            int afterColon;
            if (!FindKey(text, open, close, field, out afterColon))
            {
                return fallback;
            }
            int at = SkipSpaceAndComments(text, afterColon);
            int wordEnd = at;
            while (wordEnd < close && char.IsLetter(text[wordEnd]))
            {
                wordEnd++;
            }
            string word = text.Substring(at, wordEnd - at);
            if (word == "true")
            {
                return true;
            }
            if (word == "false")
            {
                return false;
            }
            return fallback;
        }

        private static string Num(double value)
        {
            return value.ToString("0.######", CultureInfo.InvariantCulture);
        }

        // The index of "[" that opens the top-level "emotes" array, or -1 when the text has no
        // root object holding an "emotes" key whose value is an array.
        private static int EmotesArrayOpen(string text)
        {
            int root = SkipSpaceAndComments(text, 0);
            if (root >= text.Length || text[root] != '{')
            {
                return -1;
            }
            int rootClose = MatchBrace(text, root);
            if (rootClose < 0)
            {
                return -1;
            }
            int depth = 0;
            int i = root + 1;
            while (i < rootClose)
            {
                char c = text[i];
                if (c == '{' || c == '[')
                {
                    depth++;
                    i++;
                }
                else if (c == '}' || c == ']')
                {
                    depth--;
                    i++;
                }
                else if (c == '/' && i + 1 < text.Length && text[i + 1] == '/')
                {
                    i = EndOfLine(text, i);
                }
                else if (c == '"')
                {
                    int strEnd;
                    string value;
                    if (!ReadString(text, i, out strEnd, out value))
                    {
                        return -1;
                    }
                    int next = SkipSpaceAndComments(text, strEnd + 1);
                    if (depth == 0 && value == "emotes" && next < rootClose && text[next] == ':')
                    {
                        next = SkipSpaceAndComments(text, next + 1);
                        return next < rootClose && text[next] == '[' ? next : -1;
                    }
                    i = strEnd + 1;
                }
                else
                {
                    i++;
                }
            }
            return -1;
        }

        // True when the object from open (its "{") to close (its "}") has a key equal to key at the
        // object's own level, not inside a nested one; afterColon is just past its ':'.
        private static bool FindKey(string text, int open, int close, string key, out int afterColon)
        {
            afterColon = -1;
            int depth = 0;
            int i = open + 1;
            while (i < close)
            {
                char c = text[i];
                if (c == '{' || c == '[')
                {
                    depth++;
                    i++;
                }
                else if (c == '}' || c == ']')
                {
                    depth--;
                    i++;
                }
                else if (c == '/' && i + 1 < text.Length && text[i + 1] == '/')
                {
                    i = EndOfLine(text, i);
                }
                else if (c == '"')
                {
                    int strEnd;
                    string value;
                    if (!ReadString(text, i, out strEnd, out value))
                    {
                        return false;
                    }
                    int next = SkipSpaceAndComments(text, strEnd + 1);
                    if (depth == 0 && value == key && next < close && text[next] == ':')
                    {
                        afterColon = next + 1;
                        return true;
                    }
                    i = strEnd + 1;
                }
                else
                {
                    i++;
                }
            }
            return false;
        }

        // The index of the "}" matching the "{" at open, or -1. Nested braces inside strings and
        // comments do not count.
        private static int MatchBrace(string text, int open)
        {
            int depth = 0;
            int i = open;
            while (i < text.Length)
            {
                char c = text[i];
                if (c == '"')
                {
                    int strEnd;
                    string value;
                    if (!ReadString(text, i, out strEnd, out value))
                    {
                        return -1;
                    }
                    i = strEnd + 1;
                }
                else if (c == '/' && i + 1 < text.Length && text[i + 1] == '/')
                {
                    i = EndOfLine(text, i);
                }
                else if (c == '{')
                {
                    depth++;
                    i++;
                }
                else if (c == '}')
                {
                    depth--;
                    i++;
                    if (depth == 0)
                    {
                        return i - 1;
                    }
                }
                else
                {
                    i++;
                }
            }
            return -1;
        }

        // From a '"' at i to its closing '"', skipping backslash escapes: end is the index of the
        // closing quote and value the text between the quotes, escapes left in.
        private static bool ReadString(string text, int i, out int end, out string value)
        {
            end = -1;
            value = null;
            if (i >= text.Length || text[i] != '"')
            {
                return false;
            }
            int j = i + 1;
            while (j < text.Length)
            {
                char c = text[j];
                if (c == '\\')
                {
                    j += 2;
                }
                else if (c == '"')
                {
                    end = j;
                    value = text.Substring(i + 1, j - i - 1);
                    return true;
                }
                else
                {
                    j++;
                }
            }
            return false;
        }

        // The first index at or after i that is not white space and not inside a "//" comment.
        private static int SkipSpaceAndComments(string text, int i)
        {
            while (i < text.Length)
            {
                char c = text[i];
                if (c == '/' && i + 1 < text.Length && text[i + 1] == '/')
                {
                    i = EndOfLine(text, i);
                }
                else if (char.IsWhiteSpace(c))
                {
                    i++;
                }
                else
                {
                    break;
                }
            }
            return i;
        }

        // From a "//" at i, the index of the '\n' that ends the comment, or text.Length.
        private static int EndOfLine(string text, int i)
        {
            while (i < text.Length && text[i] != '\n')
            {
                i++;
            }
            return i;
        }
    }
}
