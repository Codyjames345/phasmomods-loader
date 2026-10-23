// Decompile GameAssembly functions by RVA or map key (GhostAI.Hunting) with headless Ghidra and
// annotate known callees.
// usage: node decomp.mjs <out.c> <rva|key> [<rva|key> ...]
// Requires the one-time import into re/ghidra (see re/README.md), GHIDRA_INSTALL_DIR, and the tables
// tools/gamecheck.ps1 writes.
import { execFileSync } from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { codeOf, namesFile, requireTables } from './map.mjs';

const here = path.dirname(fileURLToPath(import.meta.url));
const [outFile, ...targets] = process.argv.slice(2);
if (!outFile || targets.length === 0) {
  console.error('usage: node decomp.mjs <out.c> <rva|key> [<rva|key> ...]');
  process.exit(2);
}
requireTables();
const rvas = targets.map((t) => (/^(0x)?[0-9a-f]+$/i.test(t) ? t : codeOf(t).start.toString(16)));
const ghidra = process.env.GHIDRA_INSTALL_DIR;
if (!ghidra) {
  console.error('Set GHIDRA_INSTALL_DIR to the Ghidra installation folder.');
  process.exit(2);
}
const raw = path.resolve(outFile + '.raw');
// .bat files need cmd.exe; quote every argument because the install path has spaces.
const q = (s) => `"${s}"`;
const args = [path.join(here, 'ghidra'), 'GA', '-process', 'GameAssembly.dll', '-noanalysis',
  '-readOnly', '-scriptPath', here, '-postScript', 'Decomp.java', raw, ...rvas];
execFileSync('cmd.exe', ['/d', '/s', '/c',
  `"${[q(path.join(ghidra, 'support', 'analyzeHeadless.bat')), ...args.map(q)].join(' ')}"`],
  { stdio: ['ignore', 'ignore', 'inherit'], windowsVerbatimArguments: true });

const names = new Map();
for (const line of fs.readFileSync(namesFile, 'utf8').split('\n')) {
  const tab = line.indexOf('\t');
  if (tab > 0 && !names.has(line.slice(0, tab))) names.set(line.slice(0, tab), line.slice(tab + 1));
}
const text = fs.readFileSync(raw, 'utf8').replace(/\b(?:FUN|func_0x|thunk_FUN)_?18([0-9a-f]{7})\b/g, (m, rva) => {
  const n = names.get(rva.toUpperCase().replace(/^0+/, ''));
  return n ? `${m}/*${n}*/` : m;
});
fs.writeFileSync(outFile, text);
fs.unlinkSync(raw);
console.log(`wrote ${outFile}`);
