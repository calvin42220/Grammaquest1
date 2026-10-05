// Extrait des constantes JS (tableaux/objets) d'un fichier HTML et les renvoie en JSON
const fs = require('fs'), vm = require('vm');
function extract(src, name) {
  const re = new RegExp('(?:const|let|var)\\s+' + name + '\\s*=\\s*');
  const m = re.exec(src);
  if (!m) return undefined;
  let i = m.index + m[0].length;
  const start = i;
  let depth = 0, started = false;
  for (; i < src.length; i++) {
    const c = src[i], d = src[i + 1];
    if (c === '/' && d === '/') { while (i < src.length && src[i] !== '\n') i++; continue; }
    if (c === '/' && d === '*') { i = src.indexOf('*/', i + 2) + 1; continue; }
    if (c === '"' || c === "'" || c === '`') {
      const q = c; i++;
      while (i < src.length && src[i] !== q) { if (src[i] === '\\') i++; i++; }
      continue;
    }
    if (c === '[' || c === '{' || c === '(') { depth++; started = true; }
    if (c === ']' || c === '}' || c === ')') { depth--; if (started && depth === 0) { i++; break; } }
  }
  return vm.runInNewContext('(' + src.slice(start, i) + ')');
}
module.exports = { extract };
if (require.main === module) {
  const [file, ...names] = process.argv.slice(2);
  const src = fs.readFileSync(file, 'utf8');
  const out = {};
  for (const n of names) out[n] = extract(src, n);
  console.log(JSON.stringify(out));
}
