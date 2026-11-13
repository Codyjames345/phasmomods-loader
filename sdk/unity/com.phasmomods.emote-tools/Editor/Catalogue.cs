using System;
#if UNITY_5_3_OR_NEWER
using UnityEngine;
#endif

namespace PhasmoMods.EmoteTools
{
    // The shape of an emote catalogue (mods/emotes/assets/emotes.json) as JsonUtility reads it
    // from the text with its comments stripped. An empty string means the field is absent, and a
    // guess of 0 means absent. The types build outside Unity too, for the tool's offline tests.
    [Serializable]
    public class CatalogueFile
    {
        public int format;
        public string bundle;
        public EmoteEntry[] emotes;
    }

    [Serializable]
    public class EmoteEntry
    {
        public string name, clip, moveClip, introClip, song, introSong, icon, label, loudness,
                     introLoudness, clipSource, songSource;
        public float clipLoopGuess, songLoopGuess;
        // Seconds into clipSource, a seamless loop, where the clip the tool writes starts (0: its own start).
        public float clipStart;
        public SongLoopEntry songLoop;
        public PartnerEntry partner;
    }

    // A synced emote's second part (mods/emotes/catalogue.h): JsonUtility makes one even when the field is absent,
    // so an emote is synced exactly when its partner names a clip.
    [Serializable]
    public class PartnerEntry
    {
        public string clip, introClip, song, loudness;
        public PartnerOffset offset;

        public bool Named
        {
            get { return !string.IsNullOrEmpty(clip); }
        }
    }

    [Serializable]
    public class PartnerOffset
    {
        public float right, forward;
    }

    [Serializable]
    public class SongLoopEntry
    {
        public float start;
        public float seconds;
    }

#if UNITY_5_3_OR_NEWER
    // Reads a catalogue's text into those types.
    public static class Catalogue
    {
        public static CatalogueFile Parse(string text)
        {
            return JsonUtility.FromJson<CatalogueFile>(CatalogueText.StripComments(text));
        }
    }
#endif
}
