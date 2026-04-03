// Rust SQLite Benchmark — rusqlite
// Add to Cargo.toml: rusqlite = { version = "0.31", features = ["bundled"] }
use rusqlite::Connection;
use std::time::Instant;

fn main() {
    let conn = Connection::open_in_memory().unwrap();
    conn.execute_batch("PRAGMA journal_mode=WAL; PRAGMA synchronous=OFF;").unwrap();
    conn.execute("CREATE TABLE World (id INTEGER PRIMARY KEY, randomNumber INTEGER NOT NULL)", []).unwrap();

    // Seed 100 rows
    for i in 1..=100i64 {
        conn.execute("INSERT INTO World (id, randomNumber) VALUES (?1, ?2)", [i, i * 7 + 13]).unwrap();
    }

    let iters: i64 = 10000;

    // INSERT
    let t1 = Instant::now();
    {
        let mut stmt = conn.prepare("INSERT INTO World (id, randomNumber) VALUES (?1, ?2)").unwrap();
        for i in 0..iters {
            stmt.execute([1000 + i, i * 3]).unwrap();
        }
    }
    let insert_ns = t1.elapsed().as_nanos();

    // SELECT
    let t2 = Instant::now();
    {
        let mut stmt = conn.prepare("SELECT id, randomNumber FROM World WHERE id = ?1").unwrap();
        for i in 0..iters {
            let _row: (i64, i64) = stmt.query_row([i + 1], |row| {
                Ok((row.get(0)?, row.get(1)?))
            }).unwrap();
        }
    }
    let select_ns = t2.elapsed().as_nanos();

    // UPDATE
    let t3 = Instant::now();
    {
        let mut stmt = conn.prepare("UPDATE World SET randomNumber = ?1 WHERE id = ?2").unwrap();
        for i in 0..iters {
            stmt.execute([i * 11, i + 1]).unwrap();
        }
    }
    let update_ns = t3.elapsed().as_nanos();

    // DELETE
    let t4 = Instant::now();
    {
        let mut stmt = conn.prepare("DELETE FROM World WHERE id = ?1").unwrap();
        for i in 0..iters {
            stmt.execute([1000 + i]).unwrap();
        }
    }
    let delete_ns = t4.elapsed().as_nanos();

    println!("rust,insert,{},{}", iters, insert_ns);
    println!("rust,select,{},{}", iters, select_ns);
    println!("rust,update,{},{}", iters, update_ns);
    println!("rust,delete,{},{}", iters, delete_ns);
}
