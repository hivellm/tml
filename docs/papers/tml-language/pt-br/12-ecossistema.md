# 12. Ecossistema e Ambiente de Desenvolvimento TML

## 12.1 Visão Geral

O ecossistema TML vai além do compilador e da biblioteca padrão para incluir ferramental especializado para desenvolvimento assistido por LLM, gerenciamento de tarefas e pesquisa ativa sobre como LLMs depuram e implementam código. Este capítulo descreve: integração com Model Context Protocol (MCP), o sistema Rulebook e a pesquisa LLM-IR-Debugging.

---

## 12.2 Integração com Model Context Protocol (MCP)

### 12.2.1 Implementação do Servidor MCP

O TML fornece um servidor MCP completo expondo 17 ferramentas para operações do compilador e de desenvolvimento. O MCP é uma interface padronizada que permite a Claude, ChatGPT, Gemini e outras ferramentas LLM invocarem operações.

O protocolo usa formato JSON-RPC 2.0 de requisição-resposta. Um LLM envia requisições; o servidor executa operações e retorna resultados JSON.

### 12.2.2 Taxonomia de Ferramentas

As 17 ferramentas se classificam em quatro categorias.

Ferramentas de diagnóstico expõem representações intermediárias do compilador: emit-ir, emit-mir, check, explain.

Ferramentas de navegação exploram o código-fonte: Read, Grep, Glob, docs_search, docs_get, docs_list.

Ferramentas de execução compilam e executam: test, run, build, compile.

Ferramentas de manutenção tratam qualidade: format, lint, cache_invalidate.

### 12.2.3 Registro de Ferramentas e Pesquisa

Cada invocação de ferramenta MCP é registrada em `mcp-call-log.jsonl` com metadados. Essa infraestrutura habilita pesquisa sobre padrões de uso de ferramentas por LLMs.

O log não contém conteúdo de saída (privacidade), apenas: nome da ferramenta, parâmetros, duração, ID de sessão, modelo. O registro leve permite análise longitudinal.

Em abril de 2026, o log contém 3.241 chamadas de ferramentas em 298 sessões, fornecendo dados empíricos sobre uso de ferramentas do compilador por LLMs.

---

## 12.3 Sistema Rulebook para Gerenciamento de Tarefas

### 12.3.1 Arquitetura

O Rulebook (pacote npm @hivehub/rulebook) fornece gerenciamento persistente de tarefas e memória para agentes de IA. O TML usa o Rulebook para organizar o trabalho, persistir progresso entre sessões e habilitar o Ralph (loops de iteração autônoma de IA).

As tarefas ficam no diretório `rulebook/tasks/`. Cada uma contém:
- `tasks.md`: Checklist simples
- `proposal.md`: Especificação detalhada
- `notes.md`: Progresso sessão a sessão

### 12.3.2 Memória Persistente

As ferramentas `mcp__rulebook__rulebook_memory` fornecem memória entre sessões. Desenvolvedores salvam descobertas importantes: decisões de arquitetura, correções de bugs, padrões, insights.

Exemplo: Literais inteiros inferem como I32 por padrão, causando erros de índice em laços de 64 bits. Salvo com a chave `type_inference_i32_i64`. Em sessões subsequentes, o LLM recupera o contexto e evita a armadilha.

A memória usa busca semântica (embeddings) para recuperar contexto passado relevante. Isso reduz a sobrecarga de documentação.

### 12.3.3 Ralph: Loops de Iteração Autônoma

O Ralph orquestra equipes de agentes para completar tarefas. O desenvolvedor fornece um objetivo de alto nível; o Ralph decompõe em subtarefas, despacha agentes, agrega resultados e reporta de volta.

O Ralph está atualmente em alpha, mas demonstra o potencial para desenvolvimento IA-first: humanos definem objetivos; a IA cuida da implementação.

---

## 12.4 Pesquisa LLM-IR-Debugging [36]

### 12.4.1 Questão de Pesquisa

Expor representações intermediárias do compilador (HIR, MIR, LLVM IR) junto com mensagens de erro reduz o número de interações de LLM necessárias para diagnosticar e corrigir bugs?

O TML formula a hipótese oposta à sabedoria convencional: LLMs se destacam em correspondência de padrões sobre dados estruturados e podem depurar mais efetivamente com representações IR formais.

### 12.4.2 Metodologia

A pesquisa coleta dados empíricos do desenvolvimento orgânico do TML. À medida que desenvolvedores usam ferramentas MCP, o sistema registra cada chamada. As chamadas são classificadas em diagnóstico, navegação e execução.

Métricas: taxa de preferência por IR, eficiência de diagnóstico, precisão de correção, padrões de transição entre ferramentas.

### 12.4.3 Descobertas Preliminares (Fase 1)

Dados: 3.241 chamadas de ferramentas, 298 sessões, 10 dias (25 de março a 4 de abril de 2026).

Principais descobertas:
- Ferramenta test: 52,7% (queda de 60,3)
- Ferramenta check: 17,5% (aumento de 98% a partir de 8,8)
- Emit-IR: 7,2% (aumento de 85% a partir de 3,9)
- Debug-layers: 9,0% (aumento de 443% a partir de 1,4)
- Taxa de erros melhorou: de 13,2 para 11,2%

O aumento de 443% na adoção de debug-layers é o maior efeito observado.

### 12.4.4 Impacto Projetado (Fase 2)

A Fase 2 tornará debug-layers habilitado por padrão em falha. Projeção: 90% de adoção.

A Fase 3 implementará o LLVM ORC JIT (Fase 0, em andamento), reduzindo a latência dos testes de 37,2 para 2-3 segundos. Esse speedup de 12x aumentará as iterações e a eficiência.

---

## 12.5 Sistema de Build

### 12.5.1 CMake com Scripts Personalizados

O TML usa CMake para portabilidade, mas fornece scripts personalizados que lidam com configuração crítica do ambiente.

Saídas principais: `tml.exe` (monolítico, 100MB com LLVM) ou build modular com `tml_compiler.dll` e `tml_codegen_x86.dll`.

Tempo de build: aproximadamente 100 segundos (Intel i7/Ryzen 7), limitado por I/O (37s de linking, 63s de compilação).

### 12.5.2 Zig CC como Compilador C/C++

O TML usa Zig CC em vez de MSVC, GCC ou Clang. O Zig CC fornece compilação C multiplataforma sem instalação específica de plataforma, melhorando a confiabilidade de CI/CD.

---

## 12.6 Comparação com Outros Ecossistemas de Linguagem

Rust mais Cargo mais Crates.io enfatiza gerenciamento de pacotes descentralizado. Trade-off: requer gerenciamento cuidadoso de dependências.

Go mais o Sistema de Módulos Go tem integração embutida. Se beneficia da integração, mas carece de recursos específicos para LLMs.

Python mais Pip/Poetry mais PyPI é maduro mas fragmentado. Múltiplos gerenciadores de pacotes e gerenciamento de versões ad-hoc.

O TML adota uma abordagem diferente com ferramental MCP integrado, gerenciamento persistente de tarefas e infraestrutura de pesquisa. A ênfase está em habilitar LLMs a serem parceiros de desenvolvimento eficazes, não em construir um grande ecossistema de pacotes.

Isso reflete o caso de uso alvo do TML: construção de serviços adjacentes a IA onde o desenvolvimento assistido por LLM é a norma.

---

## 12.7 Resumo

O ecossistema TML reflete uma filosofia de design deliberada: otimizar para desenvolvimento assistido por LLM por meio de ferramental MCP integrado, gerenciamento persistente de tarefas e design de ferramentas orientado por pesquisa.

O servidor MCP e o registro de chamadas de ferramentas criam um feedback em loop fechado: dados de LLMs informam o design das ferramentas, melhoram a eficácia dos LLMs e geram melhores dados de pesquisa. Esse ciclo virtuoso é único e posiciona o TML como um laboratório para o design de linguagens centradas em LLMs.
