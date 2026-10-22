// Build re/names.tsv: GameAssembly RVA -> Class::Method(signature), from Cpp2IL DiffableCs output.
// usage: node names.mjs <DiffableCs dir>
import fs from 'node:fs';
import path from 'node:path';

const root = process.argv[2];
const out = [];
const walk = (dir) => {
  for (const e of fs.readdirSync(dir, { withFileTypes: true })) {
    const p = path.join(dir, e.name);
    if (e.isDirectory()) walk(p);
    else if (e.name.endsWith('.cs')) scan(p);
  }
};
const scan = (file) => {
  const rel = path.relative(root, file).replace(/\\/g, '/').replace(/\.cs$/, '');
  const asm = rel.split('/')[0];
  const cls = rel.split('/').slice(1).join('.');
  const nested = [];
  let rva = null;
  for (const line of fs.readFileSync(file, 'utf8').split(/\r?\n/)) {
    const depth = line.match(/^\t*/)[0].length;
    const tm = line.match(/^\t*(?:[a-z]+ )*(?:class|struct|enum|interface) (\S+)/);
    if (tm) { nested.length = depth; nested[depth] = tm[1]; continue; }
    const am = line.match(/\[Address\(RVA = "0x([0-9A-F]+)"/);
    if (am) { rva = am[1]; continue; }
    if (rva && line.includes('(') && !/^\t*\[/.test(line)) {
      const sig = line.trim().replace(/\s*\{\s*\}.*$/, '').replace(/^(?:\[[^\]]*\]\s*)*/, '');
      const owner = [cls, ...nested.slice(1, depth).filter(Boolean)].join('/');
      out.push(`${rva}\t${asm}:${owner}::${sig}`);
      rva = null;
    }
  }
};
walk(root);
fs.writeFileSync(path.join(path.dirname(new URL(import.meta.url).pathname.slice(1)), 'names.tsv'), out.join('\n'));
console.log(out.length, 'entries');
