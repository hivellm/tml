# 3. Design do Sistema

## 3.1 Arquitetura de Instrumentação

Instrumentamos o servidor MCP do TML para registrar cada invocação de ferramenta em um arquivo NDJSON de escrita sequencial (`mcp-call-log.jsonl`). Cada entrada de registro contém:

```json
{
  "event": "tool_call",
  "session": "1774678866829",
  "seq": 4,
  "ts": "2026-04-01T14:30:05Z",
  "tool": "test",
  "params": { "suite": "core/str", "structured": true },
  "duration_ms": 37200,
  "is_error": false
}
```

O registro é transparente para o LLM -- ele não o vê nem modifica seu comportamento com base no log. Os identificadores de sessão são gerados no início da conversa e persistem em todas as chamadas de ferramentas dentro de uma conversa.

## 3.2 Pipeline de Dados

O pipeline de análise processa os logs NDJSON brutos em três estágios:

1. **Coleta bruta** (`mcp-call-log.jsonl`): Apenas escrita sequencial, imutável, com cada chamada de ferramenta registrada com parâmetros e duração.
2. **Armazenamento estruturado** (SQLite): Tabela de sessões (metadados, métricas agregadas), tabela de chamadas de ferramentas (classificadas por categoria), tabela de transições (arestas ferramenta-a-ferramenta para análise sequencial).
3. **Métricas derivadas**: Calculadas a partir dos dados estruturados -- taxas de adoção, taxas de erro, probabilidades de transição, tendências longitudinais.

## 3.3 Taxonomia de Ferramentas

Classificamos as 17 ferramentas MCP em cinco categorias com base em seu papel no fluxo de trabalho de depuração:

| Categoria | Ferramentas | Propósito |
|-----------|-------------|-----------|
| Execução | `test`, `run`, `build`, `compile` | Verificar hipóteses executando código |
| Diagnóstico | `check`, `emit-ir`, `emit-mir`, `explain` | Inspecionar internos do compilador em camadas específicas |
| Documentação | `docs/search`, `docs/get`, `docs/list`, `docs/resolve` | Pesquisar APIs e recursos da linguagem |
| Manutenção | `format`, `lint`, `cache/invalidate` | Qualidade de código e gerenciamento de cache |
| Projeto | `project/coverage`, `project/structure`, `debug`, `profile`, `inspect` | Operações em nível de projeto e depuração em tempo de execução |

Essa taxonomia permite análise no nível de categoria (por exemplo, "Qual fração das chamadas é diagnóstica?") enquanto preserva a granularidade no nível de ferramenta.

## 3.4 Definições de Métricas

Definimos as seguintes métricas para analisar o comportamento de depuração dos LLMs:

- **Taxa de adoção de check**: `check_calls / (check_calls + test_calls)`. Mede a fração do esforço de validação gasto em verificação de tipos rápida versus execução completa de testes.
- **Razão de diagnóstico**: `diagnosis_calls / total_calls`. Mede o engajamento diagnóstico geral.
- **Preferência por IR**: `(emit_ir + emit_mir) / total_calls`. Mede a frequência de inspeção direta de IR.
- **Taxa de erro**: `error_calls / total_calls`. Mede com que frequência as invocações de ferramentas produzem erros (falhas de compilação, falhas de testes, parâmetros inválidos).
- **Granularidade de testes**: Distribuição das chamadas de teste entre invocações de arquivo único, suíte e suíte completa.
- **Adoção de debug_layers**: Fração das chamadas de teste que incluem o parâmetro `debug_layers=true`.
