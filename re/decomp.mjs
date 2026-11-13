// Decompile GameAssembly functions by RVA or map key (GhostAI.Hunting) with headless Ghidra and
// annotate known callees.
// usage: node decomp.mjs <out.c> <rva|key> [<rva|key> ...]
// Requires the one-time import (see re/README.md), Ghidra's support folder on PATH, and the tables
// tools/gamecheck.ps1 writes. The Ghidra project folder is re/ghidra, or PM_GHIDRA_PROJECT's folder
// when a worktree under a dotted path needs one outside it.
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
// The Ghidra project folder is re/ghidra, or the folder named by PM_GHIDRA_PROJECT (set only in the
// process environment, like PM_GAME_DIR). Ghidra's ProjectLocator refuses a project path with any
// folder whose name starts with '.', which every git worktree under .claude has.
const project = path.resolve(process.env.PM_GHIDRA_PROJECT || path.join(here, 'ghidra'));
const hint = "Set PM_GHIDRA_PROJECT to a Ghidra project outside the dotted folder, for example the main checkout's re\\ghidra.";
if (project.split(path.sep).some((p) => p.startsWith('.'))) {
  console.error(`Ghidra rejects a project path with a folder starting with '.': ${project}`);
  console.error(hint);
  process.exit(2);
}
if (!fs.existsSync(project)) {
  console.error(`no Ghidra project folder at ${project}.`);
  console.error(hint);
  process.exit(2);
}
if (!fs.existsSync(path.join(project, 'GA.gpr'))) {
  console.error(`${project} holds no GA.gpr: run the one-time import there first (re/README.md section 2).`);
  console.error(hint);
  process.exit(2);
}
const rvas = targets.map((t) => (/^(0x)?[0-9a-f]+$/i.test(t) ? t : codeOf(t).start.toString(16)));
// analyzeHeadless.bat lives in Ghidra's support folder, which must be on PATH. Search PATH entries
// only: where.exe checks the current directory first, so a planted analyzeHeadless.bat would run.
const cwd = path.resolve(process.cwd());
let headless;
for (const entry of (process.env.PATH || '').split(';')) {
  if (!entry || !path.isAbsolute(entry)) continue;
  const dir = path.resolve(entry);
  if (dir === cwd || dir.startsWith(cwd + path.sep)) continue;
  const bat = path.join(dir, 'analyzeHeadless.bat');
  if (fs.existsSync(bat)) { headless = bat; break; }
}
if (!headless) {
  console.error("Put Ghidra's support folder, which holds analyzeHeadless.bat, on PATH.");
  process.exit(2);
}
const raw = path.resolve(outFile + '.raw');
// .bat files need cmd.exe; quote every argument because the install path has spaces.
const q = (s) => `"${s}"`;
const args = [project, 'GA', '-process', 'GameAssembly.dll', '-noanalysis',
  '-readOnly', '-scriptPath', here, '-postScript', 'Decomp.java', raw, ...rvas];
execFileSync('cmd.exe', ['/d', '/s', '/c',
  `"${[q(headless), ...args.map(q)].join(' ')}"`],
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
