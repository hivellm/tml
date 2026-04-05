# 2. Contexto

## 2.1 O Model Context Protocol

O Model Context Protocol (MCP) [4] é um padrão aberto para conectar LLMs a ferramentas e fontes de dados externas. O MCP define uma arquitetura cliente-servidor em que o LLM (cliente) invoca ferramentas em um servidor via JSON-RPC. Cada ferramenta possui um esquema tipado e o servidor retorna resultados estruturados. O MCP permite que LLMs interajam com compiladores, executores de testes, sistemas de documentação e outras infraestruturas de desenvolvimento por meio de uma interface uniforme.

Ao contrário de integrações ad-hoc de ferramentas (por exemplo, execução de comandos shell), o MCP fornece um limite estruturado onde cada invocação pode ser registrada, classificada e analisada. Essa propriedade torna o MCP um ponto de instrumentação ideal para estudar o comportamento dos LLMs.

## 2.2 O Compilador TML

TML (To Machine Language) é uma linguagem de programação de sistemas projetada para geração e análise de código por LLMs [5]. Seu compilador é implementado em C++ com um backend LLVM embutido e segue um pipeline de demanda orientado por consultas:

```
Source -> Lexer -> Parser -> Type Checker -> Borrow Checker
      -> HIR -> THIR -> MIR -> LLVM IR -> Object Code -> Executable
```

O compilador expõe 17 ferramentas MCP que abrangem compilação, testes, diagnósticos, documentação e gerenciamento de projetos. Essas ferramentas fornecem ao LLM acesso a cada camada de compilação, desde a verificação de tipos (`check`) passando pelas representações intermediárias (`emit-ir`, `emit-mir`) até a execução de testes (`test`).

A biblioteca padrão do TML contém 500+ tipos e 5.000+ funções, com desenvolvimento ativo em estruturas de dados núcleo, intrínsecos SIMD, redes e ligações com banco de dados. Essa amplitude garante que as sessões de depuração cubram domínios diversos e categorias variadas de bugs.

## 2.3 Saída de Depuração Multicamada

Uma inovação central do sistema neste estudo é a flag `--debug-layers`, que faz o compilador emitir informações diagnósticas de múltiplas camadas de compilação quando um teste falha. Em vez de exibir apenas a mensagem de falha da asserção, `--debug-layers` fornece:

- **Fonte**: A linha de código exata que falhou
- **HIR**: A expressão com açúcar sintático removido e tipos resolvidos
- **MIR**: Os blocos básicos em forma SSA com fluxo de controle explícito
- **LLVM IR**: O IR final antes da geração do código de máquina
- **Dicas de diagnóstico**: Sugestões geradas pelo compilador sobre em qual camada o bug provavelmente se encontra

Essa saída multicamada foi projetada para reduzir o número de chamadas de ferramentas necessárias para diagnosticar um bug, fornecendo contexto diagnóstico completo em uma única resposta. A hipótese é que os LLMs conseguem identificar padrões entre as camadas de IR de forma mais eficiente do que executando chamadas de ferramentas sequenciais para reunir as mesmas informações.

## 2.4 Trabalhos Anteriores

As pesquisas sobre uso de ferramentas por LLMs têm se concentrado principalmente em benchmarks que medem se os LLMs conseguem invocar ferramentas corretamente [6, 7], em vez de observar como eles usam ferramentas na prática. Estudos de geração de código por LLMs [1, 8, 9] examinaram a qualidade da saída, mas não o processo iterativo de depuração. O trabalho em reparo automático de programas [10, 11] estudou estratégias de correção, mas tipicamente com conjuntos de ferramentas restritos (editar + testar).

O trabalho mais relacionado são estudos sobre o comportamento humano de depuração [12, 13], que estabeleceram que programadores experientes usam estratégias diagnósticas sistemáticas, enquanto iniciantes recorrem a tentativa e erro. Nosso estudo questiona se os LLMs exibem padrões semelhantes e se suas estratégias podem ser moldadas por engenharia de prompts.

Trabalhos recentes sobre agentes LLM [2, 3, 14] demonstraram uso de ferramentas em múltiplas etapas em tarefas de engenharia de software, mas esses estudos tipicamente utilizam benchmarks curados (SWE-bench [15], HumanEval [16]) em vez de observar o desenvolvimento orgânico. Nosso estudo preenche essa lacuna com dados longitudinais, in situ, de desenvolvimento de compilador em produção.
