# 8. Design da Biblioteca Padrão

## 8.1 Visão Geral

A biblioteca padrão do TML é organizada em três camadas distintas: uma camada de fundação (core::), uma biblioteca padrão completa (std::) e um framework de testes (test::). Em abril de 2026, a biblioteca compreende 2.251 arquivos-fonte no total, com 1.682 arquivos de teste atingindo 93,2% de cobertura de funções em 1.659 testes passando.

Essa arquitetura reflete uma filosofia de baterias inclusas. Ao contrário do Rust, que delega a maior parte das funcionalidades ao crates.io, o TML empacota servidor HTTP, drivers de banco de dados, primitivas criptográficas, busca de texto completo e busca vetorial na distribuição padrão.

---

## 8.2 Arquitetura em Três Camadas

### 8.2.1 A Camada Core (core::)

A camada core:: contém 199 arquivos-fonte fornecendo a fundação da linguagem sem dependências externas. Todo programa TML faz link com core:: por padrão.

**Tipos e Opcionais.** `Maybe[T]` e `Outcome[T, E]` são abstrações de custo zero para tipos opcionais e de resultado. Os construtores são `Just(T)`/`Nothing` e `Ok(T)`/`Err(E)`. A nomenclatura prioriza clareza: Maybe comunica opcionalidade de forma explícita; Outcome comunica computações bidirecionais.

**Processamento de Strings e Caracteres.** `core::str` fornece 58 funções para slices de strings UTF-8 imutáveis: `len`, `char_at`, `substring`, `contains`, `find`, `split`, `trim`, `replace`, `join` e funções `parse_*`. Suporte ao Unicode 15.1.0 em `core::unicode`.

**Coleções e Iteração.** `core::iter` define o behavior `Iterator` com mais de 30 adaptadores: `map`, `filter`, `fold`, `take`, `skip`, `zip`, `chain`, `flatten`, `enumerate`. `core::slice` fornece views de cópia-zero `Slice[T]` e `MutSlice[T]`.

**Behaviors (Traits).** `core::clone` define `Duplicate` e `Copy`. `core::cmp` define `PartialEq`, `Eq`, `PartialOrd`, `Ord`. `core::fmt` define `Display` e `Debug`. `core::ops` cobre operadores aritméticos, lógicos, de indexação e objetos chamáveis.

**Memória e Alocação.** `core::alloc` fornece `Heap[T]` (ownership exclusivo), `Shared[T]` (contagem de referências), `Sync[T]` (contagem de referências atômica). Alocadores especializados: `Arena` (bump-pointer), `Pool` (lock-free), `SmallVec`/`SmallString` (SSO), `CacheAligned`/`Padded` (favoráveis ao cache).

**Codificação.** `core::encoding` fornece base64, base32, base58, hex e 9 outros formatos binário-para-texto.

**Primitivas Async.** `core::task` fornece `Poll[T]`, `Context`, `Waker`. `core::future` define `Future`. `core::async_iter` define `AsyncIterator`.

**SIMD.** `core::simd` fornece tipos de vetor `I32x4`, `F32x4`, `I64x2`, `F64x2`, `U8x16` com intrínsecos: `ptr_read`, `ptr_write`, `mem_alloc`, `mem_free`, `copy_nonoverlapping`.

### 8.2.2 A Biblioteca Padrão (std::)

A camada std:: contém 336 arquivos-fonte fornecendo a superfície completa para desenvolvimento de aplicações.

**Texto.** `std::text` fornece `Text` — string mutável com crescimento dinâmico, com operações push e Small String Optimization.

**Coleções.** `List[T]` (array dinâmico), `HashMap[K,V]`, `BTreeMap`, `BTreeSet`, `Deque`, `BinaryHeap`, `MinHeap`, `Buffer` (operações em bytes).

**Concorrência.** `Mutex[T]`, `RwLock[T]`, `Arc[T]`, atômicos, canais (`Sender`/`Receiver`), `Barrier`, `CondVar`, `Once`, filas/pilhas lock-free, `Semaphore`, `WaitGroup`.

**Rede.** `TcpStream`, `TcpListener`, tipos de endereço de socket, `NetEventLoop` para I/O não-bloqueante. Suporte a IOCP no Windows.

**HTTP.** 11 subdiretórios cobrindo: `App` (roteamento estilo Express), `Router` (árvore radix), framework (middleware, guards, pipes), protocolo (HTTP/2, WebSockets), utilitários (chunked, CORS, compressão, rate-limiting, arquivos estáticos, cookies, multipart, SSE, range requests, cache-control).

**I/O de Arquivos.** `File`, `Dir`, `Path`, `PathBuf`, `BufReader`, `BufWriter`, `LineWriter`, `Lines`.

**Banco de Dados.** Arquitetura multi-driver: SQLite com desempenho 3-4x o do Rust, suporte a PostgreSQL, ORM, construção de queries, gerenciamento de schema, migrações.

**JSON.** `Json`, `JsonObject`, `JsonArray`, `parse`/`to_string`, behaviors `ToJson`/`FromJson`, `Builder` fluente.

**Criptografia.** Hash (variantes SHA, MD5, BLAKE3), HMAC, AES-GCM, ChaCha20-Poly1305, RSA, ECDSA, Ed25519, PBKDF2, Argon2, X.509, Diffie-Hellman.

**Busca.** Busca de texto completo BM25, vizinho mais próximo aproximado HNSW, distância SIMD.

**Outros.** math, random, regex, zlib, datetime, uuid, url, mime, semver, log, cli, glob, events, profiler, console (log estruturado).

### 8.2.3 O Framework de Testes (test::)

Contém 14 arquivos fornecendo infraestrutura de testes como biblioteca. Decoradores: `@test`, `@bench`, `@should_panic`, `@should_error`, `@before_all`, `@after_all`, `@before_each`, `@after_each`, `@fixture`.

Asserções: `assert`, `assert_eq`, `assert_ne`, `assert_lt`, `assert_le`, `assert_gt`, `assert_ge` com mensagens personalizadas.

Módulos: testes baseados em propriedades, mocking, testes de rede end-to-end, rastreamento de cobertura.

---

## 8.3 Filosofia de Design: Baterias Inclusas

O Rust deliberadamente exclui quase tudo além de traits de memória e I/O. O custo disso: aplicações requerem dependências substanciais.

O Go inclui HTTP, JSON, interface de banco de dados, criptografia. O Go domina a infraestrutura cloud porque programadores podem escrever serviços completos em 50 linhas.

O Python empacota 80MB de stdlib. Programadores se beneficiam de bibliotecas integradas.

O TML segue a abordagem do Go. As inclusões refletem o caso de uso alvo: serviços adjacentes a IA, pipelines de dados, ferramentas para desenvolvedores. Uma linguagem projetada para desenvolvimento assistido por LLMs se beneficia quando os LLMs podem escrever serviços completos sem precisar entender convenções de dependências.

---

## 8.4 Migração do Runtime C

A biblioteca está em transição de implementações C para TML puro usando intrínsecos de memória. Regra de migração: TML puro (preferido), FFI para C (aceitável), novo código C (último recurso apenas para I/O de nível de SO).

Meta: compilador auto-hospedado reescrito em TML. Cada função migrada serve dois propósitos: funciona hoje e elimina dependência de C para o compilador futuro.

---

## 8.5 Estatísticas dos Módulos

| Camada | Arquivos | Subsistemas |
|--------|----------|-------------|
| core:: | 199 | Tipos, strings, coleções, behaviors, memória, codificação, async, SIMD |
| std:: | 336 | Texto, coleções, sync, rede, HTTP (11), arquivo, JSON, criptografia, BD, busca |
| test:: | 14 | Asserções, benchmarks, mocking, cobertura, e2e |
| Total | 549 | -- |

Arquivos de teste: 1.682
Total de arquivos .tml: 2.251
Cobertura: 93,2% (1.659/1.775 funções)
Testes passando: 1.659
Testes com crash: 0
Meta: 95% de cobertura
