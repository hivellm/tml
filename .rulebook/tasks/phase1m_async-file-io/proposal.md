# Proposal: phase1m_async-file-io

## Why

`std::aio` fornece event loop para sockets (poll/epoll). Mas I/O de arquivo
é 100% síncrono (C FFI bloqueante em `std::file`). Para um database ou
qualquer aplicação high-throughput, leituras/gravações de arquivo bloqueiam
o event loop, desperdiçando threads.

Problemas:
- WAL (Write-Ahead Log) não pode ser async
- Backup/restore bloqueia o server
- Cold data pages leem syncronamente
- UzDB não pode usar async file I/O para reads do disco

Source: UzDB feedback letter, P3-11.

## What Changes

1. **`std::aio::AsyncFile`** — wrapper async sobre file descriptors:
   - `AsyncFile::open(path, flags) -> Future[Outcome[AsyncFile, IoError]]`
   - `AsyncFile::read_async(buf, n) -> Future[Outcome[I64, IoError]]`
   - `AsyncFile::write_async(buf, n) -> Future[Outcome[I64, IoError]]`
   - `AsyncFile::sync_async() -> Future[Outcome[Unit, IoError]]`
   - `AsyncFile::close_async() -> Future[Outcome[Unit, IoError]]`

2. **Backend por plataforma**:
   - Windows: IOCP com `ReadFile`/`WriteFile` + OVERLAPPED
   - Linux: io_uring (preferencial) ou thread pool fallback
   - macOS: kqueue + thread pool

3. **Integração com event loop existente**:
   - Reutilizar `std::aio` runtime em vez de criar loop novo
   - `AsyncFile::read_async` registra no mesmo poll do socket I/O
   - Futures resolvem no mesmo executor

4. **Positioned I/O (pread/pwrite)**:
   - `read_at_async(offset, buf, n)` — útil para DB page reads
   - `write_at_async(offset, buf, n)` — útil para WAL

5. **Testes**:
   - Leitura async de arquivo 1MB não bloqueia outros futures
   - Múltiplas leituras concorrentes
   - Write + fsync async
   - Timeout / cancelamento de I/O em progresso

## Impact

- Affected specs: std/aio, std/file
- Affected code: `lib/std/src/aio/async_file.tml` (novo), `lib/std/runtime/aio/file_iocp.c` (Windows), `lib/std/runtime/aio/file_iouring.c` (Linux)
- Breaking change: NO (API nova, std::file síncrono permanece)
- User benefit: Habilita DB/server high-throughput. Paridade com Tokio/asyncio/libuv. P3.
