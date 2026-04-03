// Go SQLite Benchmark — mattn/go-sqlite3
// Install: go get github.com/mattn/go-sqlite3
package main

import (
	"database/sql"
	"fmt"
	"time"

	_ "github.com/mattn/go-sqlite3"
)

func main() {
	db, err := sql.Open("sqlite3", ":memory:")
	if err != nil {
		panic(err)
	}
	defer db.Close()

	db.Exec("PRAGMA journal_mode=WAL")
	db.Exec("PRAGMA synchronous=OFF")
	db.Exec("CREATE TABLE World (id INTEGER PRIMARY KEY, randomNumber INTEGER NOT NULL)")

	// Seed 100 rows
	for i := 1; i <= 100; i++ {
		db.Exec("INSERT INTO World (id, randomNumber) VALUES (?, ?)", i, i*7+13)
	}

	iters := 10000

	// INSERT
	t1 := time.Now()
	insertStmt, _ := db.Prepare("INSERT INTO World (id, randomNumber) VALUES (?, ?)")
	for i := 0; i < iters; i++ {
		insertStmt.Exec(1000+i, i*3)
	}
	insertStmt.Close()
	insertNs := time.Since(t1).Nanoseconds()

	// SELECT
	t2 := time.Now()
	selectStmt, _ := db.Prepare("SELECT id, randomNumber FROM World WHERE id = ?")
	for i := 0; i < iters; i++ {
		var id, rn int
		selectStmt.QueryRow(i + 1).Scan(&id, &rn)
	}
	selectStmt.Close()
	selectNs := time.Since(t2).Nanoseconds()

	// UPDATE
	t3 := time.Now()
	updateStmt, _ := db.Prepare("UPDATE World SET randomNumber = ? WHERE id = ?")
	for i := 0; i < iters; i++ {
		updateStmt.Exec(i*11, i+1)
	}
	updateStmt.Close()
	updateNs := time.Since(t3).Nanoseconds()

	// DELETE
	t4 := time.Now()
	deleteStmt, _ := db.Prepare("DELETE FROM World WHERE id = ?")
	for i := 0; i < iters; i++ {
		deleteStmt.Exec(1000 + i)
	}
	deleteStmt.Close()
	deleteNs := time.Since(t4).Nanoseconds()

	fmt.Printf("go,insert,%d,%d\n", iters, insertNs)
	fmt.Printf("go,select,%d,%d\n", iters, selectNs)
	fmt.Printf("go,update,%d,%d\n", iters, updateNs)
	fmt.Printf("go,delete,%d,%d\n", iters, deleteNs)
}
