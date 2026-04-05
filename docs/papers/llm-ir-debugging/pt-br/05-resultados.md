# 5. Resultados

## 5.1 Padrões de Uso de Ferramentas

### 5.1.1 Distribuição Geral

A Tabela 1 apresenta a distribuição completa do uso de ferramentas em todas as 3.251 chamadas.

**Tabela 1: Distribuição do Uso de Ferramentas (N=3.251)**

| Ferramenta | Chamadas | Porcentagem | Taxa de Erro |
|------------|----------|-------------|--------------|
| test | 1.712 | 52,7% | 3,0% |
| check | 566 | 17,4% | 30,4% |
| docs/search | 283 | 8,7% | 0,7% |
| emit-ir | 233 | 7,2% | 2,6% |
| run | 175 | 5,4% | 52,0% |
| docs/list | 93 | 2,9% | 0,0% |
| cache/invalidate | 75 | 2,3% | 21,3% |
| docs/get | 58 | 1,8% | 0,0% |
| build | 17 | 0,5% | 64,7% |
| compile | 15 | 0,5% | 26,7% |
| emit-mir | 10 | 0,3% | 80,0% |
| explain | 4 | 0,1% | 0,0% |
| project/coverage | 3 | 0,1% | 100,0% |
| docs/resolve | 3 | 0,1% | 0,0% |
| debug | 2 | 0,1% | 0,0% |
| profile | 1 | <0,1% | 0,0% |
| inspect | 1 | <0,1% | 0,0% |
| **Total** | **3.251** | **100%** | **11,2%** |

A distribuição é bastante assimétrica: duas ferramentas (`test` e `check`) respondem por 70,1% de todas as chamadas. As cinco principais ferramentas respondem por 91,4%.

### 5.1.2 Distribuição por Categoria

A Tabela 2 mostra a distribuição por categoria funcional.

**Tabela 2: Uso de Ferramentas por Categoria (N=3.251)**

| Categoria | Chamadas | Porcentagem | Descrição |
|-----------|----------|-------------|-----------|
| Execução | 1.919 | 59,0% | test, run, build, compile |
| Diagnóstico | 813 | 25,0% | check, emit-ir, emit-mir, explain |
| Documentação | 437 | 13,4% | docs/search, docs/get, docs/list, docs/resolve |
| Manutenção | 75 | 2,3% | cache/invalidate |
| Projeto | 7 | 0,2% | project/coverage, debug, profile, inspect |

A execução domina com 59,0%, mas o diagnóstico constitui um expressivo 25,0% -- uma em cada quatro chamadas de ferramentas é diagnóstica. As ferramentas de documentação representam 13,4%, indicando que o LLM pesquisa APIs regularmente antes ou durante a implementação. Ferramentas de manutenção e de projeto são raramente utilizadas.

### 5.1.3 Granularidade dos Testes

O LLM prefere fortemente testes direcionados em vez de validação abrangente:

**Tabela 3: Granularidade dos Testes (N=1.712)**

| Granularidade | Chamadas | Porcentagem |
|---------------|----------|-------------|
| Arquivo único (parâmetro path) | ~1.282 | 74,9% |
| Suíte de módulo (parâmetro suite) | ~403 | 23,5% |
| Suíte de testes completa | ~27 | 1,6% |

Essa preferência por testes de granularidade fina está alinhada com ciclos de feedback rápidos: testes de arquivo único são concluídos mais rapidamente do que execuções de suíte, permitindo ciclos de edição-teste mais ágeis.

### 5.1.4 Módulos Mais Testados

A Tabela 4 mostra os módulos que recebem mais atenção nos testes, indicando áreas de desenvolvimento ativo e pontos de depuração persistentes.

**Tabela 4: Top 10 Alvos de Teste**

| Módulo | Chamadas de Teste | Domínio |
|--------|-------------------|---------|
| core/str | 114 | Operações com strings |
| std/db | 61 | Ligações com banco de dados |
| core/iter | 47 | Sistema de iteradores |
| core/fmt | 44 | Formatação |
| std/collections | 42 | Estruturas de dados |
| core/option | 23 | Tipo Option |
| std/json | 20 | Análise de JSON |
| core/num | 19 | Tipos numéricos |
| core/ops | 19 | Sobrecarga de operadores |
| std/http | 19 | Framework HTTP |

O módulo `core/str` recebeu de longe o maior volume de testes (114 chamadas), refletindo seu papel central na biblioteca padrão e a frequência de bugs de codegen relacionados a strings.

## 5.2 Eficácia das Regras

### 5.2.1 Adoção de Check-Antes-de-Test

A intervenção comportamental mais impactante foi a regra "Use `check` ANTES de `test`" (INT-001), que inclui a justificativa de que `check` é 10x mais rápido do que `test` para detectar erros de tipo. A Tabela 5 mostra a trajetória de adoção.

**Tabela 5: Adoção de Check ao Longo do Tempo**

| Ponto de Medição | Data | check % | Razão check/test | Tamanho do Conjunto |
|------------------|------|---------|-----------------|---------------------|
| Linha de Base | 2026-03-25 | 8,8% | 1:6,9 | ~238 chamadas |
| Pós INT-001 | 2026-03-30 | 12,0% | 1:5,0 | 1.321 chamadas |
| Geral (Abr 4) | 2026-04-04 | 17,5% | 1:3,0 | 3.251 chamadas |
| Últimas 50 sessões | 2026-04-04 | 25,3% | 1:1,7 | ~838 chamadas |

A razão check/test melhorou de 1:6,9 (linha de base) para 1:1,7 (sessões recentes) -- uma melhoria de 4x. Criticamente, a taxa de adoção está *acelerando*, não se estabilizando: o crescimento período a período aumentou de +36% (linha de base até 30 de março) para +46% (30 de março até 4 de abril geral) e +45% (geral até recente). Esse efeito composto sugere que o LLM está internalizando a justificativa, não apenas cumprindo o texto da regra.

### 5.2.2 Adoção de Debug Layers

O parâmetro `debug_layers`, que fornece saída de IR multicamada em caso de falha de teste, mostrou adoção mais lenta, mas constante:

**Tabela 6: Adoção de debug_layers ao Longo do Tempo**

| Ponto de Medição | Taxa de Adoção | Observações |
|------------------|----------------|-------------|
| Linha de Base | 1,4% (3/216) | Antes de qualquer regra |
| Pós INT-002 | 7,6% (101/1.321) | Após regra explícita |
| Geral (Abr 4) | 9,6% (~311/3.251) | Crescimento contínuo |
| Últimas 50 sessões | 11,1% | Dados mais recentes |

Ao contrário da adoção do `check`, o crescimento de `debug_layers` é linear em vez de exponencial. Atribuímos isso ao maior atrito cognitivo: usar `check` antes de `test` é uma simples mudança de sequenciamento, enquanto `debug_layers` exige reconhecer uma falha de teste e então mudar para o modo diagnóstico em vez de editar o código imediatamente.

### 5.2.3 Saída Estruturada (Adoção Espontânea)

Para comparação, o parâmetro `structured=true` nas chamadas de teste -- que retorna JSON interpretável por máquina em vez de texto -- atingiu 95,7% de adoção sem nenhuma regra explícita. Isso demonstra que funcionalidades que oferecem benefício óbvio de UX alcançam adoção quase universal, enquanto funcionalidades que exigem mudança comportamental requerem prompts explícitos.

**Tabela 7: Comparação de Adoção de Funcionalidades**

| Funcionalidade | Taxa de Adoção | Regra Necessária? | Atrito Cognitivo |
|----------------|----------------|-------------------|------------------|
| saída estruturada | 95,7% | Não | Baixo (formato melhor) |
| check antes de test | 25,3% (recente) | Sim (INT-001) | Médio (nova etapa) |
| debug_layers | 11,1% (recente) | Sim (INT-002) | Alto (troca de modo) |
| docs antes de impl | ~15,5% (recente) | Sim (INT-003) | Médio (etapa de pesquisa) |

### 5.2.4 Declínio da Dominância dos Testes

Uma mudança estrutural é evidente na queda da participação das chamadas `test` ao longo do tempo:

**Tabela 8: Tendência de Dominância dos Testes**

| Período | test % | check % | emit-ir % |
|---------|--------|---------|-----------|
| Linha de Base | ~60% | 8,8% | ~2% |
| Mar 30 | 60,3% | 12,0% | 3,9% |
| Abr 4 geral | 52,7% | 17,5% | 7,2% |
| Últimas 50 sessões | 44,0% | 25,3% | 9,2% |

A participação dos testes caiu 16 pontos percentuais da linha de base para as sessões recentes. A capacidade liberada foi absorvida por `check` (+16,5pp) e `emit-ir` (+7,2pp). Isso representa uma mudança do estilo "executar e observar" para "analisar e depois verificar" -- a estratégia diagnóstica associada a programadores humanos experientes [12].

## 5.3 Análise de Erros

### 5.3.1 Taxa de Erro Geral

A taxa de erro geral em todas as ferramentas foi de 11,2% (365/3.251), caindo de 13,2% no ponto de medição de 30 de março -- uma melhoria de 15%.

**Tabela 9: Taxa de Erro por Categoria de Ferramenta**

| Categoria | Taxa de Erro | Interpretação |
|-----------|--------------|---------------|
| Execução | ~8,2% | Reflete bugs reais no código em desenvolvimento |
| Diagnóstico | ~26,5% | Erros do check = erros de tipo sendo diagnosticados (esperado) |
| Documentação | ~0,5% | Ferramentas altamente confiáveis |
| Manutenção | 21,3% | Falhas na invalidação de cache |

A alta taxa de erro do `check` (30,4%) não indica falta de confiabilidade da ferramenta. Na verdade, `check` é usado *especificamente* para encontrar erros de tipo -- erros são a saída esperada, não falhas. Da mesma forma, erros do `run` (52,0%) refletem travamentos em tempo de execução no código em desenvolvimento.

A taxa de erro muito baixa das ferramentas de documentação (0,0-0,7%) confirma sua confiabilidade e sugere que deveriam ser usadas com mais frequência como etapa de pré-implementação.

### 5.3.2 Tendência da Taxa de Erro

A taxa de erro em declínio sugere melhoria comportamental genuína:

**Tabela 10: Taxa de Erro ao Longo do Tempo**

| Período | Taxa de Erro | Variação |
|---------|--------------|----------|
| Linha de Base | ~15% (est.) | -- |
| Mar 30 | 13,2% | -1,8pp |
| Abr 4 | 11,2% | -2,0pp |

Na taxa de melhoria observada (-1,8-2,0pp por período de 5 dias), a taxa de erro pode se aproximar de 9% nos próximos 10 dias. Fatores que contribuem para isso incluem: (1) a filtragem check-first captura erros antes de execuções de testes custosas; (2) a pesquisa proativa de documentação reduz erros de tipo; (3) as regras acumuladas no prompt fornecem mais orientação por sessão.

### 5.3.3 Análise de Erros por Ferramenta

Duas ferramentas exibem taxas de erro notavelmente altas que merecem discussão:

- **emit-mir** (80,0%, 8/10 erros): O printer de MIR tem cobertura limitada para certas construções de IR. Quando o LLM solicita MIR para funções que usam padrões não suportados, a ferramenta falha. A amostra pequena (N=10) também infla essa taxa.
- **build** (64,7%, 11/17 erros): Erros de build refletem falhas de compilação durante o desenvolvimento iterativo, ocorrendo frequentemente quando o LLM tenta compilar módulos parcialmente implementados.

## 5.4 Estudo de Caso: Sessão da Biblioteca SIMD

A sessão da biblioteca SIMD de Arquitetura de Instrução (IA) em 2026-04-03 oferece uma visão detalhada do comportamento maduro de depuração do LLM em um contexto com geração de código intensa. Essa sessão envolveu a implementação de intrínsecos SIMD (`F64x2`, `I32x4`) e produziu frequentes bugs de codegen que exigiram diagnóstico em nível de IR.

### 5.4.1 Características da Sessão

Duas sessões representativas deste trabalho:

**Tabela 11: Distribuição de Ferramentas na Sessão SIMD**

| Sessão | Total | check | emit-ir | test | docs | Padrão |
|--------|-------|-------|---------|------|------|--------|
| Sessão A | 59 | 17 (29%) | 16 (27%) | 11 (19%) | 12 (20%) | Diagnóstico equilibrado |
| Sessão B | 63 | 33 (52%) | 5 (8%) | 16 (25%) | 10 (16%) | Dominância de check |
| Típica | ~10,9 | ~1,9 | ~0,8 | ~5,7 | ~1,5 | Dominância de test |

Essas sessões mostram um comportamento qualitativamente diferente da distribuição geral. A Sessão B atingiu 52% de uso de check -- o maior observado -- demonstrando que o padrão check-first é alcançável em sessões complexas quando a tarefa assim exige.

### 5.4.2 Padrão do Fluxo de Trabalho de Depuração

Um padrão recorrente emergiu em múltiplos bugs durante a sessão SIMD:

```
check -> emit-ir(function="...") -> [fix compiler C++]
      -> cache_invalidate -> check -> test
```

Esse padrão de seis etapas apareceu em três bugs separados dentro de uma única sessão, sugerindo internalização do padrão. O LLM começou com a verificação de tipos rápida, escalou para inspeção de IR quando necessário, aplicou a correção, invalidou o cache, revalidou com `check` e somente então executou o teste completo.

**Exemplo: Incompatibilidade de tipo extractelement em F64x2.get()**

O compilador emitia `extractelement <2 x double> %vec, i32 0` quando o tipo de índice SIMD deveria ter sido `i64`. A sequência diagnóstica foi:

1. `check` -- detectou a inconsistência de tipo
2. `emit-ir(function="test_f64x2_get")` -- revelou `i32` versus o esperado `i64`
3. Correção aplicada ao código C++ do compilador
4. `cache_invalidate` -- limpou o cache de compilação desatualizado
5. `check` -- verificou que a correção compilou
6. `test(path="ia_f64x2.test.tml")` -- confirmou o comportamento em tempo de execução

Esse fluxo de trabalho estruturado é exatamente o padrão diagnóstico que `--debug-layers` foi projetado para atalhar: as etapas 1-2 poderiam ser substituídas por uma única chamada `test` com `debug_layers=true`.

### 5.4.3 Impacto do debug_layers

Quando um bug de codegen de closure produziu comportamento incorreto em tempo de execução (não um erro de tipo), o LLM usou `test(path="ia_closure.test.tml", debug_layers=true)`. A saída multicamada mostrou que o MIR capturou corretamente a variável de closure, mas o LLVM IR gerou uma cópia na pilha desatualizada em vez da referência viva na heap. Isso identificou imediatamente o bug como um problema de codegen MIR-para-LLVM, direcionando a correção para `mir_codegen.cpp` em vez do verificador de tipos ou do construtor de MIR.

Sem `debug_layers`, o LLM teria investigado a camada de compilação errada primeiro, exigindo uma estimativa de 3-5 chamadas diagnósticas adicionais.

## 5.5 Latência e Velocidade de Desenvolvimento

A latência das ferramentas tem um impacto mensurável na seleção de ferramentas:

**Tabela 12: Faixas de Latência das Ferramentas**

| Faixa | Latência | Ferramentas | Participação no Uso |
|-------|---------|-------------|---------------------|
| Rápida | 40-1.000 ms | cache/invalidate, docs/*, check | 24,4% |
| Média | 3-5 seg | emit-ir, emit-mir, run | 12,9% |
| Lenta | ~37 seg | test | 52,7% |

A ferramenta dominante (`test`) é também a mais lenta, com aproximadamente 37 segundos por invocação. Com as chamadas de teste respondendo por 52,7% de todas as invocações, a latência dos testes é o principal gargalo do desenvolvimento. Um modo projetado de execução JIT em processo reduziria a latência dos testes de 37 segundos para aproximadamente 2 segundos (aceleração de 18,5x), reduzindo o ciclo de iteração edição-teste de 42 segundos para 7 segundos.

## 5.6 Padrões de Transição entre Ferramentas

A análise sequencial das chamadas de ferramentas revela fluxos de trabalho característicos:

**Tabela 13: Padrões de Transição entre Ferramentas**

| Transição | Contagem Observada | Interpretação |
|-----------|-------------------|---------------|
| test -> test | ~480 | Ciclo de repetição (padrão dominante) |
| check -> test | ~150 | Validação com pesquisa prévia |
| docs/* -> implementação | ~130 | Desenvolvimento com referência prévia |
| test -> emit-ir | ~8 | Exploração de IR após falha |
| test -> check | ~12 | Retorno à verificação de tipos |

O auto-laço `test -> test` domina (aproximadamente 480 ocorrências), representando o padrão "executar e observar" em que o LLM edita o código e reexecuta o teste sem diagnóstico intermediário. A transição `check -> test` (aproximadamente 150 ocorrências) representa o padrão "pesquisa-primeiro" promovido. A transição `docs -> implementação` (aproximadamente 130 ocorrências) mostra pesquisa proativa de APIs.
