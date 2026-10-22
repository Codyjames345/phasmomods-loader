// List the direct call sites (call rel32 / jmp rel32) of GameAssembly functions, with the method
// that contains each site, from names.tsv. A fast stand-in for whole-program xrefs, which REA's
// Ghidra cannot provide for the full 113 MB binary inside its startup deadline; decompile the
// callers it finds with REA on a slim PE (slimpe.mjs).
// usage: node callers.mjs <GameAssembly.dll> <rva> [<rva> ...]
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const [src, ...targets] = process.argv.slice(2);
if (!src || targets.length === 0) {
  console.error('usage: node callers.mjs <GameAssembly.dll> <rva> [<rva> ...]');
  process.exit(2);
}
const game = fs.readFileSync(src);
const peOff = game.readUInt32LE(0x3c);
const optSize = game.readUInt16LE(peOff + 20);
const code = [];
for (let i = 0, s = peOff + 24 + optSize; i < game.readUInt16LE(peOff + 6); ++i, s += 40) {
  const name = game.toString('latin1', s, s + 8).replace(/\0+$/, '');
  if (name === '.text' || name === 'il2cpp') {
    code.push({ va: game.readUInt32LE(s + 12), raw: game.readUInt32LE(s + 20), size: game.readUInt32LE(s + 16) });
  }
}

const starts = [];
const names = new Map();
for (const line of fs.readFileSync(path.join(here, 'names.tsv'), 'utf8').split('\n')) {
  const tab = line.indexOf('\t');
  if (tab <= 0) continue;
  const rva = parseInt(line.slice(0, tab), 16);
  if (!names.has(rva)) {
    names.set(rva, line.slice(tab + 1));
    starts.push(rva);
  }
}
starts.sort((a, b) => a - b);
const containing = (rva) => {
  let lo = 0, hi = starts.length - 1, best = -1;
  while (lo <= hi) {
    const mid = (lo + hi) >> 1;
    if (starts[mid] <= rva) { best = mid; lo = mid + 1; } else hi = mid - 1;
  }
  return best < 0 ? null : starts[best];
};

const wanted = new Set(targets.map((t) => parseInt(t, 16)));
const hits = new Map([...wanted].map((t) => [t, []]));
for (const sec of code) {
  for (let i = 0; i + 5 <= sec.size; ++i) {
    const op = game[sec.raw + i];
    if (op !== 0xe8 && op !== 0xe9) continue;
    const at = sec.va + i;
    const target = at + 5 + game.readInt32LE(sec.raw + i + 1);
    if (!wanted.has(target)) continue;
    const fn = containing(at);
    hits.get(target).push({ at, fn, kind: op === 0xe8 ? 'call' : 'jmp' });
  }
}
const short = (s) => (s || '?').replace(/[ഀ-ൿ]/g, (c) => (c.charCodeAt(0) - 0xd00).toString(16).padStart(2, '0'));
for (const [target, list] of hits) {
  console.log(`== ${target.toString(16).toUpperCase()} ${short(names.get(target)).slice(0, 140)}: ${list.length} site(s)`);
  for (const h of list) {
    console.log(`  ${h.kind} at ${h.at.toString(16).toUpperCase()} in ${h.fn?.toString(16).toUpperCase()} ${short(names.get(h.fn)).slice(0, 150)}`);
  }
}
