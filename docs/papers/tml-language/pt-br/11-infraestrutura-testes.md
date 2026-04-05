# 11. Infraestrutura de Testes

## 11.1 Visão Geral

O TML implementa uma arquitetura de testes baseada em subprocessos que se distancia significativamente dos modelos de testes in-process encontrados no libtest do Rust e no pacote de testes do Go. A justificativa para essa escolha, os trade-offs e as implicações para depuração assistida por LLM são examinados nesta seção.

Em abril de 2026, o sistema de testes executou 1.682 arquivos de teste contendo 1.659 testes passando, atingindo 93,2% de cobertura de funções. O sistema gera saída NDJSON estruturada para consumo por máquinas em IDEs e ferramentas de IA. O recurso distintivo — diagnósticos multi-camada de IR em falha (debug layers) — permite que LLMs entendam bugs lendo representações intermediárias do compilador em vez de apenas o código-fonte.

---

## 11.2 Arquitetura Baseada em Subprocessos

### 11.2.1 Justificativa do Design

O sistema de testes do TML descobre todas as funções de teste (marcadas com o decorador @test) e as agrupa por módulo. Cada módulo é compilado em um executável independente. O processo coordenador inicia um subprocesso por módulo, comunica via protocolo NDJSON e agrega os resultados.

Essa arquitetura contrasta com modelos convencionais in-process. O libtest do Rust compila todos os testes em um único binário; o harness de testes executa os testes in-process. Porém, uma falha de segmentação ou corrupção de memória em um teste pode corromper todo o harness, perdendo todos os resultados subsequentes dos testes.

Os testes do Go são semelhantes — todos os testes compilam em um binário e executam in-process. O runtime do Go fornece panics com segurança de memória, mas bugs de concorrência e crashes de extensões C podem encerrar toda a execução de testes.

O modelo de subprocesso do TML isola cada suite de testes em seu próprio processo. Um crash em um módulo não afeta os outros. O coordenador detecta códigos de saída, registra falhas e continua com o próximo módulo.

### 11.2.2 Ciclo de Vida do Processo

O coordenador: (1) descobre arquivos .test.tml, (2) agrupa testes por diretório ou individualmente, (3) compila em executáveis independentes em paralelo, (4) inicia processos com variáveis de ambiente, (5) comunica via protocolo NDJSON, (6) agrega resultados de subprocessos paralelos, (7) gera relatórios.

---

## 11.3 Protocolo NDJSON

Cada subprocesso de teste emite eventos NDJSON (JSON delimitado por nova linha). O formato é analisável por máquina, permitindo que ferramentas consumam resultados de forma programática.

Vantagens: legível por máquina (LLMs analisam JSON estruturado), hierárquico (informações aninhadas), streaming (resultados em tempo real), extensível (novos campos sem quebrar consumidores).

Tipos de evento padrão: `test_start`, `test_pass`, `test_fail`, `test_crash`, `test_skip`, `suite_pass`, `suite_fail`, `suite_crash`.

---

## 11.4 Sistema de Cobertura

O sistema de cobertura usa instrumentação baseada em variável de ambiente, evitando a complexidade do profiling baseado em LLVM. Cada função é instrumentada com uma marca de cobertura que escreve em `TML_COVERAGE_FILE`.

Vantagens: sem sobrecarga do LLVM, operação rápida O(1), preciso (cada função executada é registrada), portável (todas as plataformas sem conhecimento de LLVM).

Status atual: 93,2% de cobertura de funções (1.659/1.775 funções). A meta é 95%.

---

## 11.5 Debug Layers

A flag `--debug-layers` emite automaticamente representações intermediárias (HIR, MIR, LLVM IR) para funções de teste com falha.

Quando um teste falha, o compilador emite: HIR (AST desugared após verificação de tipos), MIR (blocos básicos em forma SSA) e LLVM IR (representação intermediária final).

Exemplo: Um teste falha devido a offset de campo de struct errado na memória. Sem debug-layers, a saída é a mensagem de falha de asserção. Com debug-layers, a saída inclui HIR mostrando a definição da struct, MIR mostrando operações de memória com offsets e LLVM IR mostrando o layout de tipo real e decisões de alinhamento.

Um agente de IA lendo saída multicamada pode diagnosticar imediatamente problemas como discrepâncias de padding.

Hipótese: LLMs depuram mais efetivamente com IR estruturada do que com mensagens em linguagem natural. Linguagem natural requer adivinhação e raciocínio. IR estruturada permite ler definições diretamente, comparar com offsets reais e identificar discrepâncias com certeza.

Medições da pesquisa LLM-IR-Debugging (atualizado para 298 sessões, 3.241 chamadas): preferência por IR aumentou 443% (de 6 para 26%), diagnóstico melhorou 27%, precisão de correção aumentou 34%.

---

## 11.6 Comparação com Outros Frameworks de Teste

Vantagens únicas do TML: isolamento por subprocesso com recuperação de crash, saída NDJSON legível por máquina, diagnósticos automáticos multi-camada de IR. Esses recursos tornam o TML bem adequado para depuração assistida por LLM.

| Recurso | Rust | Go | Python | TML |
|---------|------|----|----|-----|
| In-process | Sim | Sim | Sim | Não |
| Isolamento de crash | Não | Não | Não | Sim |
| Saída NDJSON | Não | Não | Não | Sim |
| Diagnósticos IR | Não | Não | Não | Sim |
| JSON estruturado | Não | Não | Não | Sim |

---

## 11.7 Desempenho

Cobertura de Testes: 1.682 arquivos, 1.659 passando, 0 com crash, 93,2% de cobertura de funções.

Desempenho: Suite completa em 37,2 segundos, sobrecarga média de 500ms mais computação, mais lento em std::http (4,2s), mais rápido em core::num (150ms).

Uso de Ferramentas (3.241 chamadas, 298 sessões): test 52,7%, check 17,5%, emit-IR 7,2%, debug-layers 9,0%.

O tempo de execução de 37 segundos é o principal gargalo. A Fase 0 inclui implementação de LLVM ORC JIT, com projeção de reduzir a execução para 2-3 segundos, melhorando o ciclo editar-testar de 42s para 7s.
