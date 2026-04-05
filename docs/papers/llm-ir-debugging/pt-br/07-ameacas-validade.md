# 7. Ameaças à Validade

## 7.1 Validade Interna

**Acumulação de prompts**: O prompt do sistema cresceu de aproximadamente 2.000 para 8.000 palavras ao longo do período de estudo. Sessões mais tardias têm mais orientação comportamental, confundindo comparações entre períodos iniciais e tardios. Mitigamos isso rastreando taxas de adoção relativas às datas de intervenção em vez do tempo absoluto.

**Confusão de tarefas**: Diferentes tarefas de desenvolvimento naturalmente produzem distribuições de ferramentas distintas. Sessões de depuração de codegen (por exemplo, trabalho com SIMD) produzem mais chamadas `emit-ir`, enquanto sessões de desenvolvimento de bibliotecas produzem mais chamadas `test`. A abordagem de estudo de caso (Seção 5.4) controla parcialmente isso ao analisar padrões dentro de uma mesma tarefa.

**Independência entre sessões**: Sessões dentro de uma mesma thread de conversa compartilham contexto. Um padrão de depuração bem-sucedido em uma sessão pode se transferir para sessões subsequentes, inflando métricas de adoção. Mitigamos isso calculando métricas no nível de sessão e reportando médias móveis.

## 7.2 Validade Externa

**Desenvolvedor único**: Todas as sessões envolvem o fluxo de trabalho de um único desenvolvedor. Outros desenvolvedores podem usar estratégias de prompting, decomposições de tarefas ou preferências de ferramentas diferentes.

**Linguagem única**: O sistema de tipos do TML, o pipeline de compilação e o conjunto de ferramentas MCP são específicos deste projeto. Os resultados podem não se generalizar para linguagens com modelos de erro diferentes (por exemplo, linguagens dinamicamente tipadas) ou outros ecossistemas de ferramentas.

**Família de LLM única**: Os modelos Claude podem ter preferências inerentes por certos padrões de ferramentas (por exemplo, preferência por JSON estruturado) que não se transferem para outras famílias de modelos.

## 7.3 Validade de Construto

**Interpretação da taxa de erro**: Uma "taxa de erro" alta no `check` (30,4%) reflete o uso da ferramenta conforme projetado (encontrar erros de tipo), não falha da ferramenta. Nossas métricas não distinguem entre erros esperados (erros de tipo encontrados pelo check) e erros inesperados (travamentos da ferramenta).

**Taxa de adoção como proxy de eficácia**: Medimos adoção (com que frequência uma ferramenta é usada), mas não eficácia (se o uso da ferramenta leva a uma resolução de bugs mais rápida). Maior adoção não significa necessariamente melhores resultados de depuração.
