// Build a slim PE of GameAssembly.dll for REA (Ghidra on Windows only admits PE targets, and full
// auto-analysis of the 113 MB original does not finish inside REA's startup deadline).
// The slim PE keeps the real image base and section addresses, so every address REA reports is a
// real GameAssembly address. Only the selected RVA ranges carry code; the rest of the image is
// uninitialised. Every method in names.tsv becomes an export, so pseudocode names its callees.
// usage: node slimpe.mjs <GameAssembly.dll> <out.dll> <rvaFrom>-<rvaTo> [<rvaFrom>-<rvaTo> ...]
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const [src, out, ...rangeArgs] = process.argv.slice(2);
if (!src || !out || rangeArgs.length === 0) {
  console.error('usage: node slimpe.mjs <GameAssembly.dll> <out.dll> <rvaFrom>-<rvaTo> ...');
  process.exit(2);
}
const ALIGN = 0x1000;
const alignUp = (v, a = ALIGN) => Math.ceil(v / a) * a;
const alignDown = (v, a = ALIGN) => Math.floor(v / a) * a;

const game = fs.readFileSync(src);
const peOff = game.readUInt32LE(0x3c);
const optSize = game.readUInt16LE(peOff + 20);
const imageBase = game.readBigUInt64LE(peOff + 24 + 24);
const sections = [];
for (let i = 0, s = peOff + 24 + optSize; i < game.readUInt16LE(peOff + 6); ++i, s += 40) {
  sections.push({ name: game.toString('latin1', s, s + 8).replace(/\0+$/, ''), va: game.readUInt32LE(s + 12),
    vsize: game.readUInt32LE(s + 8), raw: game.readUInt32LE(s + 20), rawSize: game.readUInt32LE(s + 16) });
}
const fileOffset = (rva) => {
  const s = sections.find((x) => rva >= x.va && rva < x.va + x.rawSize);
  if (!s) throw new Error(`RVA ${rva.toString(16)} is not file-backed`);
  return rva - s.va + s.raw;
};
const codeEnd = Math.max(...sections.filter((s) => s.name === '.text' || s.name === 'il2cpp').map((s) => alignUp(s.va + s.vsize)));

// Code ranges, page aligned and merged.
const ranges = rangeArgs.map((r) => r.split('-').map((h) => parseInt(h, 16)))
  .map(([a, b]) => [alignDown(a), alignUp(b)]).sort((x, y) => x[0] - y[0])
  .reduce((acc, r) => {
    const last = acc[acc.length - 1];
    if (last && r[0] <= last[1]) last[1] = Math.max(last[1], r[1]);
    else acc.push(r);
    return acc;
  }, []);
if (ranges[0][0] < ALIGN || ranges[ranges.length - 1][1] > codeEnd) throw new Error('range outside the code sections');

// Exports name methods as Class.Method (ASCII only: each obfuscated character U+0D00..U+0D7F
// becomes two hex digits, so names stay unique and stable). The export ordinal table is 16-bit,
// so at most 65535 names fit: methods inside the code ranges first, then methods those ranges
// call directly, then the remaining methods of the assemblies game logic calls most.
const MAX_EXPORTS = 0xffff;
const FILL_ASSEMBLIES = ['UnityEngine.CoreModule', 'PhotonUnityNetworking', 'PhotonRealtime', 'UnityEngine.AIModule',
  'UnityEngine.PhysicsModule', 'Unity.InputSystem', 'UnityEngine.UI', 'Unity.TextMeshPro', 'Micosmo.SensorToolkit',
  'Assembly-CSharp', 'mscorlib'];
const ascii = (s) => s.replace(/[ഀ-ൿ]/g, (c) => (c.charCodeAt(0) - 0xd00).toString(16).padStart(2, '0'))
  .replace(/[^A-Za-z0-9_.$<>`]/g, '_');
const byRva = new Map();
for (const line of fs.readFileSync(path.join(here, 'names.tsv'), 'utf8').split('\n')) {
  const tab = line.indexOf('\t');
  if (tab <= 0) continue;
  const rva = parseInt(line.slice(0, tab), 16);
  if (byRva.has(rva) || rva < ALIGN || rva >= codeEnd) continue;
  const full = line.slice(tab + 1);
  const assembly = full.slice(0, full.indexOf(':'));
  const owner = full.slice(full.indexOf(':') + 1, full.indexOf('::')).split(/[./]/).pop();
  const sig = full.slice(full.indexOf('::') + 2);
  const method = (sig.slice(0, sig.indexOf('(')).trim().split(' ').pop() || 'anon').replace(/<.*$/, '');
  byRva.set(rva, { name: `${ascii(owner)}.${ascii(method)}`, assembly });
}
const inRanges = (rva) => ranges.some(([a, b]) => rva >= a && rva < b);
const called = new Set();
for (const [a, b] of ranges) {
  const base = fileOffset(a);
  for (let i = 0; i + 5 <= b - a; ++i) {
    if (game[base + i] !== 0xe8) continue;
    const target = a + i + 5 + game.readInt32LE(base + i + 1);
    if (byRva.has(target)) called.add(target);
  }
}
const priority = (rva, assembly) => {
  if (inRanges(rva)) return 0;
  if (called.has(rva)) return 1;
  const i = FILL_ASSEMBLIES.indexOf(assembly);
  return i < 0 ? 2 + FILL_ASSEMBLIES.length : 2 + i;
};
const chosen = [...byRva.entries()].map(([rva, v]) => ({ rva, name: v.name, rank: priority(rva, v.assembly) }))
  .sort((x, y) => x.rank - y.rank || x.rva - y.rva).slice(0, MAX_EXPORTS);
const used = new Map();
const exportsList = chosen.sort((a, b) => a.rva - b.rva).map(({ rva, name }) => {
  const n = used.has(name) ? `${name}_${rva.toString(16).toUpperCase()}` : name;
  used.set(name, true);
  return { rva, name: n };
}).sort((a, b) => (a.name < b.name ? -1 : a.name > b.name ? 1 : 0));

// Section plan: uninitialised gaps between initialised code ranges, then the export directory.
const plan = [];
let cursor = ALIGN;
ranges.forEach(([a, b], i) => {
  if (a > cursor) plan.push({ name: `.gap${i}`, va: cursor, vsize: a - cursor, data: null });
  plan.push({ name: `.code${i}`, va: a, vsize: b - a, data: game.subarray(fileOffset(a), fileOffset(a) + (b - a)) });
  cursor = b;
});
if (cursor < codeEnd) plan.push({ name: '.gapE', va: cursor, vsize: codeEnd - cursor, data: null });

const edataVa = codeEnd;
const dllName = 'GameAssembly.dll\0';
const nExp = exportsList.length;
const dirSize = 40, eat = dirSize, npt = eat + nExp * 4, ot = npt + nExp * 4, strs = ot + nExp * 2;
const nameBytes = exportsList.map((e) => Buffer.from(e.name + '\0', 'latin1'));
const edata = Buffer.alloc(strs + dllName.length + nameBytes.reduce((s, b) => s + b.length, 0));
edata.writeUInt32LE(edataVa + strs, 12);
edata.writeUInt32LE(1, 16);
edata.writeUInt32LE(nExp, 20);
edata.writeUInt32LE(nExp, 24);
edata.writeUInt32LE(edataVa + eat, 28);
edata.writeUInt32LE(edataVa + npt, 32);
edata.writeUInt32LE(edataVa + ot, 36);
edata.write(dllName, strs, 'latin1');
let sp = strs + dllName.length;
exportsList.forEach((e, i) => {
  edata.writeUInt32LE(e.rva, eat + i * 4);
  edata.writeUInt32LE(edataVa + sp, npt + i * 4);
  edata.writeUInt16LE(i, ot + i * 2);
  nameBytes[i].copy(edata, sp);
  sp += nameBytes[i].length;
});
plan.push({ name: '.edata', va: edataVa, vsize: edata.length, data: edata, edata: true });

// Headers: DOS stub, PE32+ file and optional headers, section table.
const FILE_ALIGN = 0x200;
const headerSize = alignUp(0x40 + 24 + 240 + plan.length * 40, FILE_ALIGN);
let rawCursor = headerSize;
for (const s of plan) {
  s.raw = s.data ? rawCursor : 0;
  s.rawSize = s.data ? alignUp(s.data.length, FILE_ALIGN) : 0;
  rawCursor += s.rawSize;
}
const img = Buffer.alloc(rawCursor);
img.write('MZ', 0, 'latin1');
img.writeUInt32LE(0x40, 0x3c);
const pe = 0x40;
img.write('PE\0\0', pe, 'latin1');
img.writeUInt16LE(0x8664, pe + 4);
img.writeUInt16LE(plan.length, pe + 6);
img.writeUInt16LE(240, pe + 20);
img.writeUInt16LE(0x2022, pe + 22);  // executable, large address aware, DLL
const o = pe + 24;
img.writeUInt16LE(0x20b, o);
img.writeUInt32LE(ranges[0][0], o + 16);  // entry point: first code range
img.writeBigUInt64LE(imageBase, o + 24);
img.writeUInt32LE(ALIGN, o + 32);
img.writeUInt32LE(FILE_ALIGN, o + 36);
img.writeUInt16LE(6, o + 40);
img.writeUInt16LE(6, o + 48);
const last = plan[plan.length - 1];
img.writeUInt32LE(alignUp(last.va + last.vsize), o + 56);
img.writeUInt32LE(headerSize, o + 60);
img.writeUInt16LE(2, o + 68);  // GUI subsystem
img.writeUInt32LE(16, o + 108);
img.writeUInt32LE(edataVa, o + 112);
img.writeUInt32LE(edata.length, o + 116);
plan.forEach((s, i) => {
  const h = o + 240 + i * 40;
  img.write(s.name.slice(0, 8), h, 'latin1');
  img.writeUInt32LE(s.vsize, h + 8);
  img.writeUInt32LE(s.va, h + 12);
  img.writeUInt32LE(s.rawSize, h + 16);
  img.writeUInt32LE(s.raw, h + 20);
  // code: CODE|EXECUTE|READ; uninitialised gaps: UNINITIALIZED|EXECUTE|READ; edata: INIT|READ
  img.writeUInt32LE(s.edata ? 0x40000040 : s.data ? 0x60000020 : 0x60000080, h + 36);
  if (s.data) s.data.copy(img, s.raw);
});
fs.writeFileSync(out, img);
console.log(`wrote ${out}: ${ranges.map(([a, b]) => `${a.toString(16)}-${b.toString(16)}`).join(', ')}, ${nExp} exports, ${(img.length / 1e6).toFixed(1)} MB`);
