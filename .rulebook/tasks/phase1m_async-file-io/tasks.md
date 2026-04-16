## 1. API design
- [ ] 1.1 Escrever `lib/std/src/aio/async_file.tml` com struct AsyncFile
- [ ] 1.2 Definir Future[Outcome[T, IoError]] signatures
- [ ] 1.3 Decidir entre handle integer ou struct opaque

## 2. Windows IOCP backend
- [ ] 2.1 `lib/std/runtime/aio/file_iocp.c` com OVERLAPPED reads/writes
- [ ] 2.2 Registrar file handle no IOCP port existente do aio runtime
- [ ] 2.3 Completion callback resolve o Future
- [ ] 2.4 Positioned I/O via OVERLAPPED.Offset/OffsetHigh

## 3. Linux io_uring backend
- [ ] 3.1 `lib/std/runtime/aio/file_iouring.c` com SQE/CQE
- [ ] 3.2 Fallback para thread pool se kernel <5.1
- [ ] 3.3 Positioned I/O via IORING_OP_READ/WRITE com offset

## 4. macOS kqueue + thread pool
- [ ] 4.1 Async wrapper sobre pread/pwrite em thread pool
- [ ] 4.2 kqueue para sinalizar completion ao event loop

## 5. Runtime integration
- [ ] 5.1 Adicionar file handles ao poll loop de aio
- [ ] 5.2 Executor dispatch de Future de file completion
- [ ] 5.3 Cancellation: drop do Future cancela I/O pendente

## 6. Testes
- [ ] 6.1 Leitura async 1MB não bloqueia outros futures
- [ ] 6.2 100 reads concorrentes em paralelo
- [ ] 6.3 Write + sync_async, verificar durabilidade
- [ ] 6.4 Timeout / cancelamento funciona
- [ ] 6.5 Benchmark: throughput vs sync file API

## 7. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 7.1 Atualizar `docs/stdlib/aio.md` com AsyncFile
- [ ] 7.2 Testes cobrindo todos os backends
- [ ] 7.3 Rodar suíte completa, zero regressões
