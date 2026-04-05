# 4. Metodologia

## 4.1 Desenho do Estudo

Este é um estudo observacional do comportamento de depuração de LLMs durante o desenvolvimento orgânico de um compilador. Ao contrário de experimentos controlados com conjuntos de bugs curados, observamos o LLM (Claude Opus 4.6 [17]) enquanto desenvolve o compilador TML e a biblioteca padrão em tempo real, encontrando e corrigindo bugs conforme surgem naturalmente.

O estudo cobre 30 dias de desenvolvimento ativo (2026-03-05 a 2026-04-04), durante os quais o LLM implementou módulos da biblioteca padrão (operações com strings, iteradores, coleções, intrínsecos SIMD, ligações com banco de dados), corrigiu bugs de codegen do compilador e manteve a cobertura de testes.

## 4.2 Condições Experimentais

O estudo abrange duas condições principais, com transições ocorrendo durante o período de observação:

**Condição A (Linha de Base)**: Apenas mensagens de erro padrão. O LLM deve invocar manualmente `check`, `emit-ir` ou `emit-mir` para inspecionar as camadas de compilação. Ativa de 2026-03-05 a 2026-03-26. Aproximadamente 6 sessões, ~238 chamadas.

**Condição B (Debug-Layers Padrão)**: A flag `--debug-layers` está ativada por padrão em todas as chamadas de teste. Em caso de falha no teste, a saída inclui automaticamente HIR, MIR e LLVM IR para a função que falhou, junto com dicas de diagnóstico. Ativa a partir de 2026-03-26. 292 sessões, ~3.013 chamadas.

Além disso, o prompt do sistema (`CLAUDE.md`) foi atualizado iterativamente com regras comportamentais ao longo do período de estudo. As principais intervenções incluem:

- **INT-001** (2026-03-26): Adicionada regra "Use `check` ANTES de `test`" com justificativa quantitativa ("check é 10x mais rápido").
- **INT-002** (2026-03-28): Adicionada regra "SEMPRE use `debug_layers=true` na PRIMEIRA falha de teste".
- **INT-003** (2026-03-29): Adicionada regra "NUNCA leia arquivos-fonte para entender APIs -- use as ferramentas MCP de documentação".

## 4.3 Coleta de Dados

Todos os dados foram coletados de desenvolvimento orgânico -- o LLM não recebeu bugs curados ou tarefas artificiais. Essa escolha de design prioriza a validade ecológica em detrimento do controle experimental. O LLM encontrou bugs reais durante o desenvolvimento real, usou ferramentas conforme desejou e foi livre para adotar ou ignorar regras baseadas em prompts.

A coleta de dados é automática e transparente. O servidor MCP registra cada chamada de ferramenta sem que o LLM esteja ciente disso. Nenhuma chamada foi filtrada ou excluída da análise. O conjunto de dados total compreende 3.251 chamadas de ferramentas em 300 sessões utilizando 17 ferramentas distintas.

## 4.4 Ameaças à Validade Interna

O design observacional introduz vários fatores de confusão:

1. **Acumulação de regras**: O prompt do sistema cresceu ao longo do tempo, de forma que sessões mais tardias têm mais orientação comportamental. Abordamos isso rastreando tendências longitudinais e comparando sessões iniciais versus tardias.
2. **Variação de tarefas**: Diferentes tarefas de desenvolvimento (trabalho em bibliotecas versus depuração de codegen) naturalmente produzem distribuições de ferramentas diferentes. Abordamos isso por meio do estudo de caso (Seção 5.4).
3. **Efeitos de aprendizado**: O LLM pode melhorar com a experiência acumulada dentro e entre sessões. A análise em nível de sessão controla parcialmente esse fator.
