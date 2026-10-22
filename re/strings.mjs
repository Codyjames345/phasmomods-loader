// Dump IL2CPP string literals from global-metadata.dat (metadata v31 header layout).
// usage: node strings.mjs <global-metadata.dat> [regex]
import fs from 'node:fs';

const [file, pattern] = process.argv.slice(2);
const buf = fs.readFileSync(file);
if (buf.readUInt32LE(0) !== 0xfab11baf) throw new Error('not IL2CPP metadata');
const literalOffset = buf.readInt32LE(8);
const literalSize = buf.readInt32LE(12);
const dataOffset = buf.readInt32LE(16);
const re = pattern ? new RegExp(pattern, 'i') : null;
for (let i = 0; i < literalSize / 8; i++) {
  const length = buf.readUInt32LE(literalOffset + i * 8);
  const index = buf.readInt32LE(literalOffset + i * 8 + 4);
  const s = buf.toString('utf8', dataOffset + index, dataOffset + index + length);
  if (!re || re.test(s)) console.log(`${i}\t${JSON.stringify(s)}`);
}
