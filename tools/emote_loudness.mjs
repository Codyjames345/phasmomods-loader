// Measures each Emotes song's loudness for the ghost's hearing (sdk/api/talk_loudness.h): for every folder under
// mods/emotes/assets with an MP3, it decodes the song to mono with ffmpeg and writes loudness/<name>.txt, one RMS
// amplitude (0 to 1) per 0.1 s window, which the mod reads with pm::api::talk::parse_rms and envelope_of_rms. Run it
// after the author changes a song, and commit the loudness files: the songs themselves stay out of git.
//
// usage: node tools/emote_loudness.mjs [assets folder]   (ffmpeg on PATH, or PM_FFMPEG naming it)
import { spawnSync } from 'node:child_process';
import { existsSync, mkdirSync, readdirSync, statSync, writeFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const assets = path.resolve(process.argv[2] ?? path.join(root, 'mods', 'emotes', 'assets'));
const ffmpeg = process.env.PM_FFMPEG ?? 'ffmpeg';
const rate = 8000;          // samples a second the song is measured at
const windowSeconds = 0.1;  // talk_loudness.h's kWindowSeconds
const window = Math.round(rate * windowSeconds);

if (!existsSync(assets)) {
  console.error(`no assets folder at ${assets}`);
  process.exit(2);
}
let made = 0;
for (const name of readdirSync(assets).sort()) {
  const folder = path.join(assets, name);
  if (!statSync(folder).isDirectory()) continue;
  const song = readdirSync(folder).find((f) => f.toLowerCase().endsWith('.mp3'));
  if (!song) continue;
  const run = spawnSync(ffmpeg, ['-v', 'error', '-i', path.join(folder, song), '-ac', '1', '-ar', String(rate),
    '-f', 'f32le', '-'], { maxBuffer: 1 << 30 });
  if (run.status !== 0) {
    console.error(`ffmpeg could not decode ${song}: ${run.stderr}`);
    process.exit(3);
  }
  const samples = new Float32Array(run.stdout.buffer, run.stdout.byteOffset, Math.floor(run.stdout.length / 4));
  const lines = [`# ${song}: RMS amplitude per ${windowSeconds} s window, ${samples.length / rate} s`];
  for (let at = 0; at < samples.length; at += window) {
    const end = Math.min(samples.length, at + window);
    let sum = 0;
    for (let i = at; i < end; ++i) sum += samples[i] * samples[i];
    lines.push(Math.sqrt(sum / (end - at)).toFixed(4));
  }
  // The catalogue names each emote's loudness file loudness/<name>.txt, its folder's name with hyphens (emotes.json).
  mkdirSync(path.join(assets, 'loudness'), { recursive: true });
  writeFileSync(path.join(assets, 'loudness', `${name.replaceAll('_', '-')}.txt`), lines.join('\n') + '\n');
  console.log(`${name}: ${lines.length - 1} windows`);
  ++made;
}
if (made === 0) {
  console.error(`no songs under ${assets}`);
  process.exit(4);
}
