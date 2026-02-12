const fs = require('fs');
const path = require('path');

function findTestFiles(dir) {
  let results = [];
  if (!fs.existsSync(dir)) return results;
  const entries = fs.readdirSync(dir, { withFileTypes: true });
  for (const e of entries) {
    const full = path.join(dir, e.name);
    if (e.isDirectory()) {
      results = results.concat(findTestFiles(full));
    } else if (e.name.endsWith('.test.tml')) {
      results.push(full);
    }
  }
  return results;
}

const libDir = path.join(__dirname, 'lib');
const files = findTestFiles(libDir);
const groups = {};

for (const f of files) {
  const content = fs.readFileSync(f, 'utf8');
  const matches = content.match(/@test/g);
  const count = matches ? matches.length : 0;
  const relPath = path.relative(__dirname, f).split(path.sep).join('/');
  const dir = path.dirname(relPath).split(path.sep).join('/');
  if (!groups[dir]) groups[dir] = [];
  groups[dir].push({ file: path.basename(f), tests: count });
}

const dirs = Object.keys(groups).sort();
let totalFiles = 0;
let totalTests = 0;

for (const dir of dirs) {
  const items = groups[dir].sort((a, b) => a.file.localeCompare(b.file));
  const dirTotal = items.reduce((s, i) => s + i.tests, 0);
  
  console.log('');
  console.log('='.repeat(95));
  console.log('Directory: ' + dir);
  console.log('-'.repeat(95));
  console.log('File'.padEnd(65) + '@test count'.padStart(12));
  console.log('-'.repeat(95));
  
  for (const item of items) {
    console.log(item.file.padEnd(65) + String(item.tests).padStart(12));
    totalFiles++;
    totalTests += item.tests;
  }
  
  console.log('-'.repeat(95));
  console.log(('TOTAL (' + items.length + ' files)').padEnd(65) + String(dirTotal).padStart(12));
}

console.log('');
console.log('='.repeat(95));
console.log('GRAND TOTAL: ' + totalFiles + ' files, ' + totalTests + ' @test annotations');
console.log('='.repeat(95));
