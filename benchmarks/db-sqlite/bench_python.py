"""Python SQLite Benchmark — stdlib sqlite3"""
import sqlite3
import time

conn = sqlite3.connect(':memory:')
conn.execute('PRAGMA journal_mode=WAL')
conn.execute('PRAGMA synchronous=OFF')
conn.execute('CREATE TABLE World (id INTEGER PRIMARY KEY, randomNumber INTEGER NOT NULL)')

# Seed 100 rows
for i in range(1, 101):
    conn.execute('INSERT INTO World (id, randomNumber) VALUES (?, ?)', (i, i * 7 + 13))
conn.commit()

ITERS = 10000

# INSERT
t1 = time.perf_counter_ns()
for i in range(ITERS):
    conn.execute('INSERT INTO World (id, randomNumber) VALUES (?, ?)', (1000 + i, i * 3))
conn.commit()
insert_ns = time.perf_counter_ns() - t1

# SELECT
t2 = time.perf_counter_ns()
for i in range(ITERS):
    conn.execute('SELECT id, randomNumber FROM World WHERE id = ?', (i + 1,)).fetchone()
select_ns = time.perf_counter_ns() - t2

# UPDATE
t3 = time.perf_counter_ns()
for i in range(ITERS):
    conn.execute('UPDATE World SET randomNumber = ? WHERE id = ?', (i * 11, i + 1))
conn.commit()
update_ns = time.perf_counter_ns() - t3

# DELETE
t4 = time.perf_counter_ns()
for i in range(ITERS):
    conn.execute('DELETE FROM World WHERE id = ?', (1000 + i,))
conn.commit()
delete_ns = time.perf_counter_ns() - t4

conn.close()

print(f'python,insert,{ITERS},{insert_ns}')
print(f'python,select,{ITERS},{select_ns}')
print(f'python,update,{ITERS},{update_ns}')
print(f'python,delete,{ITERS},{delete_ns}')
