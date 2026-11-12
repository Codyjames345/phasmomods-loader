using System;
using UnityEngine;

namespace PhasmoMods.EmoteTools
{
    // The shape of an emote catalogue (mods/emotes/assets/emotes.json) as JsonUtility reads it
    // from the text with its comments stripped. An empty string means the field is absent, and a
    // guess of 0 means absent.
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
        public SongLoopEntry songLoop;
    }

    [Serializable]
    public class SongLoopEntry
    {
        public float start;
        public float seconds;
    }

    // Reads a catalogue's text into those types.
    public static class Catalogue
    {
        public static CatalogueFile Parse(string text)
        {
            return JsonUtility.FromJson<CatalogueFile>(CatalogueText.StripComments(text));
        }
    }
}
