using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using UnityEditor;
using UnityEngine;

namespace PhasmoMods.EmoteTools
{
    /// <summary>
    /// The emote pack tool: it finds each emote's song and clip loops from the rough guesses in
    /// the pack's emotes.json, and builds the pack's asset bundle. A pack folder (emotes.json, the
    /// loudness files, and the finished bundle) sits outside the Unity project; the asset paths in
    /// the catalogue (clip, song, clipSource, ...) are Unity project paths like
    /// Assets/Emotes/Clips/Floss.anim, and loudness paths are relative to the pack folder.
    /// Run it from the PhasmoMods menu, or in batch mode through tools/emote_pack.ps1, which
    /// passes -pack &lt;folder&gt; and reads the EMLOOP, EM and EMFAIL log lines it prints.
    /// </summary>
    public static class EmotePackTool
    {
        /// <summary>
        /// The most a cut's ClipLoop cost may be, as a share of the median cost over all the clip's frames: at or below
        /// it the clip returns to its start well enough to cut there, above it the clip is crossfaded instead. A share,
        /// not a fixed cost, because the cost grows with how far a dance's poses move. Measured 2026-11-13: clips made
        /// to loop come out near 0.07, Disco Fever's, which no frame returns from, at 0.68.
        /// </summary>
        public const double ClipCutShare = 0.25;

        // ---------------------------------------------------------------- entry points

        [MenuItem("PhasmoMods/Emotes/Find loops")]
        public static void FindLoopsFromMenu()
        {
            string pack = EditorUtility.OpenFolderPanel("Find emote loops: pick the pack folder", "", "");
            if (string.IsNullOrEmpty(pack))
            {
                return;
            }
            if (!FindLoops(pack, out string why))
            {
                EditorUtility.DisplayDialog("Emote tools", "Finding loops failed: " + why, "OK");
            }
        }

        [MenuItem("PhasmoMods/Emotes/Build pack")]
        public static void BuildFromMenu()
        {
            string pack = EditorUtility.OpenFolderPanel("Build the emote pack: pick the pack folder", "", "");
            if (string.IsNullOrEmpty(pack))
            {
                return;
            }
            if (!Build(pack, out string why))
            {
                EditorUtility.DisplayDialog("Emote tools", "Building the pack failed: " + why, "OK");
            }
        }

        /// <summary>
        /// Batch entry for -executeMethod: reads the pack folder from -pack on the command line,
        /// finds the loops, exits 0, or logs EMFAIL and exits nonzero.
        /// </summary>
        public static void FindLoopsBatch()
        {
            if (!TryPackArg(out string pack))
            {
                Debug.LogError("EMFAIL the command line has no -pack folder");
                EditorApplication.Exit(3);
                return;
            }
            if (!FindLoops(pack, out string why))
            {
                Debug.LogError("EMFAIL " + why);
                EditorApplication.Exit(2);
                return;
            }
            EditorApplication.Exit(0);
        }

        /// <summary>
        /// Batch entry for -executeMethod: reads the pack folder from -pack on the command line,
        /// builds the pack, exits 0, or logs EMFAIL and exits nonzero.
        /// </summary>
        public static void BuildBatch()
        {
            if (!TryPackArg(out string pack))
            {
                Debug.LogError("EMFAIL the command line has no -pack folder");
                EditorApplication.Exit(3);
                return;
            }
            if (!Build(pack, out string why))
            {
                Debug.LogError("EMFAIL " + why);
                EditorApplication.Exit(2);
                return;
            }
            EditorApplication.Exit(0);
        }

        private static bool TryPackArg(out string pack)
        {
            string[] args = Environment.GetCommandLineArgs();
            for (int i = 0; i < args.Length - 1; i++)
            {
                if (args[i] == "-pack")
                {
                    pack = args[i + 1];
                    return true;
                }
            }
            pack = null;
            return false;
        }

        // ---------------------------------------------------------------- finding loops

        /// <summary>
        /// For every emote in the pack's emotes.json: cut its song to the loop the finder measures
        /// (or take the song loop from a song that already loops), make its clip loop by cutting or
        /// crossfading, and write the measured songLoop values back into the catalogue text once,
        /// with its comments and layout kept. False and why when something is missing or fails;
        /// the catalogue is then left untouched.
        /// </summary>
        public static bool FindLoops(string pack, out string why)
        {
            string packPath = Path.GetFullPath(pack);
            string cataloguePath = Path.Combine(packPath, "emotes.json");
            if (!File.Exists(cataloguePath))
            {
                why = "no emotes.json in " + packPath;
                return false;
            }
            string text = File.ReadAllText(cataloguePath);
            CatalogueFile cat = Catalogue.Parse(text);
            if (cat == null || cat.emotes == null || cat.emotes.Length == 0)
            {
                why = "emotes.json holds no emotes";
                return false;
            }
            if (!CheckPaths(cat, out why))
            {
                return false;
            }
            foreach (EmoteEntry emote in cat.emotes)
            {
                if (string.IsNullOrEmpty(emote.name))
                {
                    why = "an emote in emotes.json has no name";
                    return false;
                }
                double songLoop = 0.0;
                if (!string.IsNullOrEmpty(emote.songSource) && emote.songLoopGuess > 0f)
                {
                    if (!CutSong(emote, ref text, out songLoop, out why))
                    {
                        return false;
                    }
                }
                else if (!string.IsNullOrEmpty(emote.song) && emote.songLoopGuess <= 0f)
                {
                    if (!MeasureExistingSong(emote, ref text, out songLoop, out why))
                    {
                        return false;
                    }
                }
                if (!string.IsNullOrEmpty(emote.clipSource) && emote.clipLoopGuess > 0f)
                {
                    // A clip that no frame near its guess brings back to its start is crossfaded to the guess: its
                    // tail past the guess blends into its start (Disco Fever's guess is its song's loop, so the two keep
                    // in step; a clip guessed at its own length less a fade loops on its own).
                    if (!MakeClip(emote, emote.clipLoopGuess, out why))
                    {
                        return false;
                    }
                }
            }
            File.WriteAllText(cataloguePath, text, new UTF8Encoding(false));
            why = null;
            return true;
        }

        // Step 1: cut the song source at its measured loop, write the loop wav at the emote's song
        // path, and record the length in the catalogue text.
        private static bool CutSong(EmoteEntry emote, ref string text, out double loopSeconds, out string why)
        {
            loopSeconds = 0.0;
            AudioImporter importer = AssetImporter.GetAtPath(emote.songSource) as AudioImporter;
            if (importer == null)
            {
                why = "no audio source at " + emote.songSource;
                return false;
            }
            AudioImporterSampleSettings read = importer.defaultSampleSettings;
            read.loadType = AudioClipLoadType.DecompressOnLoad;
            read.compressionFormat = AudioCompressionFormat.PCM;
            importer.defaultSampleSettings = read;
            importer.SaveAndReimport();
            AudioClip source = AssetDatabase.LoadAssetAtPath<AudioClip>(emote.songSource);
            if (source == null)
            {
                why = "Unity could not load the audio source at " + emote.songSource;
                return false;
            }
            float[] data = new float[source.samples * source.channels];
            source.GetData(data, 0);
            LoopResult r = LoopFinder.SongLoop(LoopFinder.Mono(data, source.channels), source.frequency, emote.songLoopGuess);
            if (r.At < 1)
            {
                why = "no song loop found in " + emote.songSource + " near " +
                      emote.songLoopGuess.ToString(CultureInfo.InvariantCulture) + " s";
                return false;
            }
            // The loop is cut from its second time round, not its first: Unity's MP3 decoder keeps the encoder's padding
            // at the start (2257 samples on the bundled songs, where ffmpeg drops it), and a loop starting on padding
            // that never comes round again drops out for that long each time it loops. One loop in, the song runs on
            // unbroken, so the cut's end runs into its start as the song ran into its second loop.
            int channels = source.channels;
            if (data.Length < 2 * r.At * channels)
            {
                why = emote.songSource + " is shorter than two of its loops, so the loop cannot be cut clear of its start";
                return false;
            }
            float[] loop = new float[r.At * channels];
            Array.Copy(data, r.At * channels, loop, 0, loop.Length);
            string songPath = ProjectFile(emote.song);
            Directory.CreateDirectory(Path.GetDirectoryName(songPath));
            WavWriter.Write(songPath, loop, channels, source.frequency, r.At);
            AssetDatabase.ImportAsset(emote.song);
            loopSeconds = r.At / (double)source.frequency;
            text = CatalogueText.SetSongLoop(text, emote.name, 0, loopSeconds);
            string line = string.Format("EMLOOP {0} song {1} ({2:F4})", emote.name, r.At, r.Score);
            if (r.Score < 0.99)
            {
                line += " POOR";
            }
            Debug.Log(line);
            why = null;
            return true;
        }

        // Step 2: the song is a loop already, so its whole length is the song loop.
        private static bool MeasureExistingSong(EmoteEntry emote, ref string text, out double loopSeconds, out string why)
        {
            loopSeconds = 0.0;
            AudioClip song = AssetDatabase.LoadAssetAtPath<AudioClip>(emote.song);
            if (song == null)
            {
                why = "no song at " + emote.song;
                return false;
            }
            loopSeconds = song.samples / (double)song.frequency;
            text = CatalogueText.SetSongLoop(text, emote.name, 0, loopSeconds);
            Debug.Log("EMLOOP " + emote.name + " song as is " + song.samples);
            why = null;
            return true;
        }

        // Step 3: make the clip loop, either by cutting it at the frame that returns to the start
        // or by crossfading its end into its start over the song loop length.
        private static bool MakeClip(EmoteEntry emote, float crossfadeTo, out string why)
        {
            AnimationClip source = AssetDatabase.LoadAssetAtPath<AnimationClip>(emote.clipSource);
            if (source == null)
            {
                why = "no clip source at " + emote.clipSource;
                return false;
            }
            if (string.IsNullOrEmpty(emote.clip))
            {
                why = emote.name + " has a clipSource but no clip path to write";
                return false;
            }
            float fps = source.frameRate;
            EditorCurveBinding[] all = AnimationUtility.GetCurveBindings(source);
            List<EditorCurveBinding> measured = new List<EditorCurveBinding>();
            foreach (EditorCurveBinding b in all)
            {
                if (!b.propertyName.StartsWith("Root") && !b.propertyName.StartsWith("Motion"))
                {
                    measured.Add(b);
                }
            }
            AnimationCurve[] curves = measured.Select(b => AnimationUtility.GetEditorCurve(source, b)).ToArray();
            int last = Mathf.RoundToInt(source.length * fps);
            float[][] frames = new float[last + 1][];
            for (int i = 0; i <= last; i++)
            {
                frames[i] = new float[curves.Length];
                float t = i / fps;
                for (int c = 0; c < curves.Length; c++)
                {
                    frames[i][c] = curves[c].Evaluate(t);
                }
            }
            LoopResult r = LoopFinder.ClipLoop(frames, fps, emote.clipLoopGuess);
            // How far each frame is from returning to the start, for judging the cut: the cost at the clip's own end
            // and the median over every frame, beside the best.
            List<double> costs = new List<double>();
            int anywhere = 0;
            double anywhereCost = double.PositiveInfinity;
            for (int f = 1; f <= last; f++)
            {
                double c = LoopFinder.ClipLoop(frames, fps, f / (double)fps, 0.4 / fps).Score;
                costs.Add(c);
                if (f >= fps / 2 && c < anywhereCost)
                {
                    anywhereCost = c;
                    anywhere = f;
                }
            }
            Debug.Log(string.Format(CultureInfo.InvariantCulture, "EMLOOP {0} clip anywhere past half a second: frame {1} ({2:F4})",
                                    emote.name, anywhere, anywhereCost));
            costs.Sort();
            double median = costs.Count > 0 ? costs[costs.Count / 2] : 0.0;
            double share = median > 0.0 ? r.Score / median : 0.0;
            Debug.Log(string.Format(CultureInfo.InvariantCulture,
                "EMLOOP {0} clip costs: best frame {1} of {2} ({3:F4}, {4:F2} of the median {5:F4}), at its end {6:F4}",
                emote.name, r.At, last, r.Score, share, median,
                LoopFinder.ClipLoop(frames, fps, last / (double)fps, 0.4 / fps).Score));
            AnimationClip made;
            if (share <= ClipCutShare)
            {
                made = CutClip(source, all, r.At, fps);
                Debug.Log(string.Format("EMLOOP {0} clip {1} ({2:F4})", emote.name, r.At, r.Score));
            }
            else
            {
                if (!MakeLoop(source, emote.name, crossfadeTo, out made, out why))
                {
                    return false;
                }
                Debug.Log(string.Format("EMLOOP {0} clip crossfaded ({1:F4})", emote.name, r.Score));
            }
            AnimationClipSettings settings = AnimationUtility.GetAnimationClipSettings(source);
            settings.loopTime = true;
            settings.stopTime = made.length;
            AnimationUtility.SetAnimationClipSettings(made, settings);
            AssetDatabase.DeleteAsset(emote.clip);
            AssetDatabase.CreateAsset(made, emote.clip);
            AssetDatabase.SaveAssets();
            why = null;
            return true;
        }

        // A clip cut at frame cutAt: every binding, the Root ones included, has a key at
        // t = i / fps for i = 0 .. cutAt with the source's value there, on ClampedAuto tangents.
        private static AnimationClip CutClip(AnimationClip source, EditorCurveBinding[] bindings, int cutAt, float fps)
        {
            AnimationClip clip = new AnimationClip { frameRate = fps };
            foreach (EditorCurveBinding b in bindings)
            {
                AnimationCurve sourceCurve = AnimationUtility.GetEditorCurve(source, b);
                Keyframe[] keys = new Keyframe[cutAt + 1];
                for (int i = 0; i <= cutAt; i++)
                {
                    float t = i / fps;
                    keys[i] = new Keyframe(t, sourceCurve.Evaluate(t));
                }
                AnimationCurve curve = new AnimationCurve(keys);
                for (int k = 0; k < curve.length; k++)
                {
                    AnimationUtility.SetKeyLeftTangentMode(curve, k, AnimationUtility.TangentMode.ClampedAuto);
                    AnimationUtility.SetKeyRightTangentMode(curve, k, AnimationUtility.TangentMode.ClampedAuto);
                }
                AnimationUtility.SetEditorCurve(clip, b, curve);
            }
            return clip;
        }

        // A clip `loop` seconds long from a longer source, ported from the build script this
        // tool replaces: the source's last seconds past `loop` are blended into its first, so at
        // its start it is where the source was that long before its end, easing into the source's
        // own start, and its end runs on into its start. Keys fall evenly to end exactly at `loop`.
        // Rotations (curves ending Q.x to Q.w) blend as unit quaternions on the same side. The
        // maths is the reference's unchanged; it returns the clip without settings or asset.
        private static bool MakeLoop(AnimationClip source, string name, float loop, out AnimationClip made, out string why)
        {
            made = null;
            var fps = source.frameRate;
            var length = source.length;
            var fade = length - loop;
            if (fade <= 0f)
            {
                why = name + " is no longer than its loop, so a crossfade cannot loop it";
                return false;
            }
            var frames = Mathf.RoundToInt(loop * fps);
            var step = loop / frames;
            var clip = new AnimationClip { frameRate = fps };
            var bindings = AnimationUtility.GetCurveBindings(source);
            var curves = bindings.ToDictionary(b => b.propertyName, b => AnimationUtility.GetEditorCurve(source, b));
            var keys = bindings.ToDictionary(b => b.propertyName, b => new Keyframe[frames + 1]);
            for (int i = 0; i <= frames; ++i)
            {
                var t = i * step;
                var s = t < fade ? Mathf.SmoothStep(0f, 1f, t / fade) : 1f;
                var from = length - fade + t;  // where the source's end runs on from
                foreach (var b in bindings)
                {
                    var n = b.propertyName;
                    float v;
                    if (s >= 1f) v = curves[n].Evaluate(t);
                    else if (n.EndsWith("Q.x") || n.EndsWith("Q.y") || n.EndsWith("Q.z") || n.EndsWith("Q.w"))
                    {
                        var stem = n.Substring(0, n.Length - 1);
                        if (!curves.ContainsKey(stem + "x") || !curves.ContainsKey(stem + "w")) v = Mathf.Lerp(curves[n].Evaluate(from), curves[n].Evaluate(t), s);
                        else
                        {
                            var a = new Quaternion(curves[stem + "x"].Evaluate(from), curves[stem + "y"].Evaluate(from),
                                                   curves[stem + "z"].Evaluate(from), curves[stem + "w"].Evaluate(from));
                            var c = new Quaternion(curves[stem + "x"].Evaluate(t), curves[stem + "y"].Evaluate(t),
                                                   curves[stem + "z"].Evaluate(t), curves[stem + "w"].Evaluate(t));
                            var q = Quaternion.Slerp(a, c, s);
                            var axis = n[n.Length - 1];
                            v = axis == 'x' ? q.x : axis == 'y' ? q.y : axis == 'z' ? q.z : q.w;
                        }
                    }
                    else v = Mathf.Lerp(curves[n].Evaluate(from), curves[n].Evaluate(t), s);
                    keys[n][i] = new Keyframe(t, v);
                }
            }
            foreach (var b in bindings)
            {
                var curve = new AnimationCurve(keys[b.propertyName]);
                for (int k = 0; k < curve.length; ++k)
                {
                    AnimationUtility.SetKeyLeftTangentMode(curve, k, AnimationUtility.TangentMode.ClampedAuto);
                    AnimationUtility.SetKeyRightTangentMode(curve, k, AnimationUtility.TangentMode.ClampedAuto);
                }
                AnimationUtility.SetEditorCurve(clip, b, curve);
            }
            made = clip;
            why = null;
            return true;
        }

        // ---------------------------------------------------------------- building

        /// <summary>
        /// Builds the pack's asset bundle: checks and loops each named clip, measures each song's
        /// loudness file and sets its import, sets each icon's import, gives every named asset the
        /// catalogue's bundle name, builds it, and copies it into the pack folder. False and why
        /// when any asset is missing, a clip is not humanoid, or the bundle holds other than the
        /// named paths.
        /// </summary>
        public static bool Build(string pack, out string why)
        {
            string packPath = Path.GetFullPath(pack);
            string cataloguePath = Path.Combine(packPath, "emotes.json");
            if (!File.Exists(cataloguePath))
            {
                why = "no emotes.json in " + packPath;
                return false;
            }
            string text = File.ReadAllText(cataloguePath);
            CatalogueFile cat = Catalogue.Parse(text);
            if (cat == null || cat.emotes == null || cat.emotes.Length == 0)
            {
                why = "emotes.json holds no emotes";
                return false;
            }
            if (!CheckPaths(cat, out why))
            {
                return false;
            }
            if (string.IsNullOrEmpty(cat.bundle))
            {
                why = "emotes.json names no bundle";
                return false;
            }

            AssetDatabase.Refresh(ImportAssetOptions.ForceSynchronousImport);
            foreach (string path in AssetDatabase.GetAllAssetPaths())
            {
                AssetImporter imp = AssetImporter.GetAtPath(path);
                if (imp != null && !string.IsNullOrEmpty(imp.assetBundleName))
                {
                    imp.SetAssetBundleNameAndVariant("", "");
                }
            }

            List<string> named = new List<string>();
            foreach (EmoteEntry emote in cat.emotes)
            {
                if (string.IsNullOrEmpty(emote.name))
                {
                    why = "an emote in emotes.json has no name";
                    return false;
                }
                bool wantsLoop = CatalogueText.BoolField(text, emote.name, "loops", true);
                if (!SetClip(emote.clip, wantsLoop, out why))
                {
                    return false;
                }
                AddNamed(named, emote.clip);
                if (!string.IsNullOrEmpty(emote.moveClip))
                {
                    if (!SetClip(emote.moveClip, wantsLoop, out why))
                    {
                        return false;
                    }
                    AddNamed(named, emote.moveClip);
                }
                if (!string.IsNullOrEmpty(emote.introClip))
                {
                    if (!SetClip(emote.introClip, false, out why))
                    {
                        return false;
                    }
                    AddNamed(named, emote.introClip);
                }
            }

            foreach (EmoteEntry emote in cat.emotes)
            {
                if (!string.IsNullOrEmpty(emote.song))
                {
                    if (!SetSong(emote.song, emote.loudness, packPath, out why))
                    {
                        return false;
                    }
                    AddNamed(named, emote.song);
                }
                if (!string.IsNullOrEmpty(emote.introSong))
                {
                    if (!SetSong(emote.introSong, emote.introLoudness, packPath, out why))
                    {
                        return false;
                    }
                    AddNamed(named, emote.introSong);
                }
            }

            foreach (EmoteEntry emote in cat.emotes)
            {
                if (!string.IsNullOrEmpty(emote.icon))
                {
                    if (!SetIcon(emote.icon, emote.name, out why))
                    {
                        return false;
                    }
                    AddNamed(named, emote.icon);
                }
            }

            foreach (string path in named)
            {
                AssetImporter imp = AssetImporter.GetAtPath(path);
                if (imp == null)
                {
                    why = "no asset at " + path;
                    return false;
                }
                imp.SetAssetBundleNameAndVariant(cat.bundle, "");
            }
            string outFolder = ProjectFile("bundle-out");
            Directory.CreateDirectory(outFolder);
            AssetBundleManifest manifest = BuildPipeline.BuildAssetBundles(outFolder,
                BuildAssetBundleOptions.ChunkBasedCompression, BuildTarget.StandaloneWindows64);
            if (manifest == null)
            {
                why = "the build made no bundle";
                return false;
            }
            string[] inBundle = AssetDatabase.GetAssetPathsFromAssetBundle(cat.bundle);
            HashSet<string> got = new HashSet<string>(inBundle.Select(p => p.ToLowerInvariant()));
            foreach (string p in got)
            {
                Debug.Log("EM asset " + p);
            }
            HashSet<string> want = new HashSet<string>(named.Select(p => p.ToLowerInvariant()));
            List<string> extra = got.Where(p => !want.Contains(p)).OrderBy(p => p).ToList();
            List<string> missing = want.Where(p => !got.Contains(p)).OrderBy(p => p).ToList();
            if (extra.Count > 0 || missing.Count > 0)
            {
                string detail = "";
                if (extra.Count > 0)
                {
                    detail += ", the bundle also holds: " + string.Join(", ", extra);
                }
                if (missing.Count > 0)
                {
                    detail += ", missing from the bundle: " + string.Join(", ", missing);
                }
                why = "the bundle does not hold exactly the named assets" + detail;
                return false;
            }
            string built = Path.Combine(outFolder, cat.bundle);
            string target = Path.Combine(packPath, cat.bundle);
            File.Copy(built, target, true);
            Debug.Log("EM OK " + target);
            why = null;
            return true;
        }

        // Every path the catalogue names stays where the tool may read and write: asset paths under the project's
        // Assets folder, loudness files under the pack folder, and the bundle a plain file name in it. A catalogue may
        // come from another author, and the tool writes songs, clips, loudness files and the bundle where it says.
        private static bool CheckPaths(CatalogueFile cat, out string why)
        {
            if (!string.IsNullOrEmpty(cat.bundle) && !PackPaths.IsPlainFileName(cat.bundle))
            {
                why = "the bundle name " + cat.bundle + " is not a plain file name";
                return false;
            }
            foreach (EmoteEntry e in cat.emotes)
            {
                foreach (string asset in new[] { e.clip, e.moveClip, e.introClip, e.song, e.introSong, e.icon, e.clipSource, e.songSource })
                {
                    if (!string.IsNullOrEmpty(asset) && !PackPaths.IsProjectAsset(asset))
                    {
                        why = e.name + " names " + asset + ", which is not a file under the project's Assets folder";
                        return false;
                    }
                }
                foreach (string file in new[] { e.loudness, e.introLoudness })
                {
                    if (!string.IsNullOrEmpty(file) && !PackPaths.IsPackFile(file))
                    {
                        why = e.name + " names " + file + ", which is not a file under the pack folder";
                        return false;
                    }
                }
            }
            why = null;
            return true;
        }

        // A project path such as Assets/Emotes/Loops/floss.wav as a full path on disk.
        private static string ProjectFile(string projectPath)
        {
            return Path.GetFullPath(Path.Combine(Directory.GetParent(Application.dataPath).FullName, projectPath));
        }

        private static void AddNamed(List<string> named, string path)
        {
            if (!named.Contains(path))
            {
                named.Add(path);
            }
        }

        // The clip must exist and be humanoid; loopTime is set (saving when it changed) as the
        // catalogue wants it.
        private static bool SetClip(string path, bool loopTime, out string why)
        {
            AnimationClip clip = AssetDatabase.LoadAssetAtPath<AnimationClip>(path);
            if (clip == null)
            {
                why = "no AnimationClip at " + path;
                return false;
            }
            if (!clip.humanMotion)
            {
                why = path + " is not a humanoid clip";
                return false;
            }
            AnimationClipSettings settings = AnimationUtility.GetAnimationClipSettings(clip);
            if (settings.loopTime != loopTime)
            {
                settings.loopTime = loopTime;
                AnimationUtility.SetAnimationClipSettings(clip, settings);
                EditorUtility.SetDirty(clip);
                AssetDatabase.SaveAssets();
            }
            Debug.Log(string.Format("EM clip {0}: {1:F3} s, loops {2}, root motion {3}", path, clip.length,
                                    clip.isLooping, clip.hasRootCurves));
            why = null;
            return true;
        }

        // The song: read it decoded to PCM to measure its loudness into the pack's file, then set
        // the import the bundle builds with, exactly as the build script this tool replaces did.
        private static bool SetSong(string path, string loudnessField, string packPath, out string why)
        {
            AudioImporter importer = AssetImporter.GetAtPath(path) as AudioImporter;
            if (importer == null)
            {
                why = "no song at " + path;
                return false;
            }
            AudioImporterSampleSettings read = importer.defaultSampleSettings;
            read.loadType = AudioClipLoadType.DecompressOnLoad;
            read.compressionFormat = AudioCompressionFormat.PCM;
            importer.defaultSampleSettings = read;
            importer.SaveAndReimport();
            AudioClip audio = AssetDatabase.LoadAssetAtPath<AudioClip>(path);
            if (audio == null)
            {
                why = "Unity could not load the song at " + path;
                return false;
            }
            float[] data = new float[audio.samples * audio.channels];
            audio.GetData(data, 0);
            float[] mono = LoopFinder.Mono(data, audio.channels);
            if (!string.IsNullOrEmpty(loudnessField))
            {
                float[] windows = LoopFinder.RmsWindows(mono, audio.frequency);
                StringBuilder sb = new StringBuilder();
                sb.Append("# ").Append(Path.GetFileName(path))
                  .Append(": RMS amplitude per 0.1 s window, ")
                  .Append((mono.Length / (double)audio.frequency).ToString(CultureInfo.InvariantCulture))
                  .Append(" s\n");
                foreach (float v in windows)
                {
                    sb.Append(v.ToString("F4", CultureInfo.InvariantCulture)).Append('\n');
                }
                string loudPath = Path.GetFullPath(Path.Combine(packPath, loudnessField));
                Directory.CreateDirectory(Path.GetDirectoryName(loudPath));
                File.WriteAllText(loudPath, sb.ToString(), new UTF8Encoding(false));
            }
            AudioImporterSampleSettings buildSettings = importer.defaultSampleSettings;
            buildSettings.loadType = AudioClipLoadType.CompressedInMemory;
            buildSettings.compressionFormat = AudioCompressionFormat.Vorbis;
            buildSettings.quality = 1.0f;  // the songs are MP3s already: a second lossy pass at full quality (heard as low quality at 0.7)
            importer.defaultSampleSettings = buildSettings;
            importer.loadInBackground = false;
            importer.SaveAndReimport();
            Debug.Log(string.Format("EM song {0}: {1:F3} s, {2} channels, {3} Hz", path, audio.length,
                                    audio.channels, audio.frequency));
            why = null;
            return true;
        }

        // The icon's import settings, unchanged from the build script this tool replaces.
        private static bool SetIcon(string path, string name, out string why)
        {
            TextureImporter icon = AssetImporter.GetAtPath(path) as TextureImporter;
            if (icon == null)
            {
                why = "no icon at " + path;
                return false;
            }
            icon.textureType = TextureImporterType.Default;
            icon.alphaIsTransparency = true;
            icon.alphaSource = TextureImporterAlphaSource.FromInput;
            icon.mipmapEnabled = false;
            icon.wrapMode = TextureWrapMode.Clamp;
            icon.npotScale = TextureImporterNPOTScale.None;
            icon.textureCompression = TextureImporterCompression.Uncompressed;
            icon.SaveAndReimport();
            Texture2D tex = AssetDatabase.LoadAssetAtPath<Texture2D>(path);
            Debug.Log(string.Format("EM icon {0}: {1}x{2}", name, tex.width, tex.height));
            why = null;
            return true;
        }
    }
}
