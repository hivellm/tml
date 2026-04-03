// Node.js SQLite Benchmark — better-sqlite3
// Install: npm install better-sqlite3
const Database = require('better-sqlite3');

const db = new Database(':memory:');
db.pragma('journal_mode = WAL');
db.pragma('synchronous = OFF');
db.exec('CREATE TABLE World (id INTEGER PRIMARY KEY, randomNumber INTEGER NOT NULL)');

// Seed 100 rows
const seedStmt = db.prepare('INSERT INTO World (id, randomNumber) VALUES (?, ?)');
for (let i = 1; i <= 100; i++) {
    seedStmt.run(i, i * 7 + 13);
}

const ITERS = 10000;

// INSERT
const insertStmt = db.prepare('INSERT INTO World (id, randomNumber) VALUES (?, ?)');
const t1 = process.hrtime.bigint();
for (let i = 0; i < ITERS; i++) {
    insertStmt.run(1000 + i, i * 3);
}
const insert_ns = Number(process.hrtime.bigint() - t1);

// SELECT
const selectStmt = db.prepare('SELECT id, randomNumber FROM World WHERE id = ?');
const t2 = process.hrtime.bigint();
for (let i = 0; i < ITERS; i++) {
    selectStmt.get(i + 1);
}
const select_ns = Number(process.hrtime.bigint() - t2);

// UPDATE
const updateStmt = db.prepare('UPDATE World SET randomNumber = ? WHERE id = ?');
const t3 = process.hrtime.bigint();
for (let i = 0; i < ITERS; i++) {
    updateStmt.run(i * 11, i + 1);
}
const update_ns = Number(process.hrtime.bigint() - t3);

// DELETE
const deleteStmt = db.prepare('DELETE FROM World WHERE id = ?');
const t4 = process.hrtime.bigint();
for (let i = 0; i < ITERS; i++) {
    deleteStmt.run(1000 + i);
}
const delete_ns = Number(process.hrtime.bigint() - t4);

db.close();

console.log(`node,insert,${ITERS},${insert_ns}`);
console.log(`node,select,${ITERS},${select_ns}`);
console.log(`node,update,${ITERS},${update_ns}`);
console.log(`node,delete,${ITERS},${delete_ns}`);
