// What tools/gamecheck.ps1 leaves for the RE scripts: re/names.tsv (RVA -> method) and
// re/map-addresses.tsv (map key -> kind, value, length). Both scripts stop with the command to run
// when either is missing.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
export const namesFile = path.join(here, 'names.tsv');
const addressesFile = path.join(here, 'map-addresses.tsv');

export function requireTables() {
  if (!fs.existsSync(namesFile) || !fs.existsSync(addressesFile)) {
    console.error('Run tools/gamecheck.ps1 first.');
    process.exit(2);
  }
}

// A map key's code: { start, length } for a method or code entry that resolved in the checked build.
export function codeOf(key) {
  for (const line of fs.readFileSync(addressesFile, 'utf8').split('\n')) {
    const [k, kind, value, length] = line.replace(/\r$/, '').split('\t');
    if (k !== key) continue;
    if (kind !== 'method' && kind !== 'code') {
      console.error(`${key} is a ${kind} entry, not code.`);
      process.exit(2);
    }
    return { start: parseInt(value, 16), length: parseInt(length, 16) };
  }
  console.error(`${key} is not in re/map-addresses.tsv: no such map key, or it did not resolve in this build.`);
  process.exit(2);
}
