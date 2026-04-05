# 8. Trabalhos Relacionados

## 8.1 Geração de Código e Depuração por LLMs

Chen et al. [1] introduziram o Codex e o HumanEval, estabelecendo benchmarks para geração de código por LLMs. Trabalhos subsequentes melhoraram as taxas de aprovação por meio de prompting com cadeia de pensamento [8], auto-reparo [9] e refinamento iterativo [14]. Nosso trabalho difere ao estudar o *processo* de depuração em vez da qualidade do resultado final.

Jimenez et al. [15] introduziram o SWE-bench, um benchmark de issues reais do GitHub que requerem edições em múltiplos arquivos. Embora os estudos do SWE-bench incluam depuração, o foco está na resolução de ponta a ponta em vez dos padrões de uso de ferramentas. Nossa instrumentação MCP fornece dados comportamentais mais detalhados.

## 8.2 Uso de Ferramentas por LLMs

Schick et al. [6] demonstraram que LLMs podem aprender a usar ferramentas por meio de prompting com poucos exemplos (Toolformer). Qin et al. [7] propuseram o ToolLLM com um benchmark para uso complexo de ferramentas. Esses trabalhos focam na *capacidade* (o LLM consegue usar a ferramenta corretamente?) em vez do *comportamento* (como o LLM escolhe entre ferramentas?). Nosso estudo aborda a segunda questão.

Patil et al. [20] estudaram a seleção de ferramentas em agentes LLM, constatando que os modelos tendem a depender excessivamente de ferramentas familiares. Nossa constatação de que `test` domina apesar de alternativas mais baratas está alinhada com essa observação.

## 8.3 Reparo Automático de Programas

Le Goues et al. [10] e Monperrus [11] fizeram levantamentos sobre reparo automático de programas, estabelecendo o paradigma gerar-e-validar. Nossa observação de que os LLMs seguem um padrão semelhante (editar-testar-repetir) sugere que a depuração por LLMs compartilha semelhanças estruturais com o APR clássico, embora com a adição do uso de ferramentas diagnósticas.

## 8.4 Comportamento Humano de Depuração

Katz e Anderson [12] estudaram estratégias de depuração de especialistas versus iniciantes, constatando que especialistas usam testes sistemáticos de hipóteses enquanto iniciantes recorrem a tentativa e erro. Nossa constatação de que o comportamento do LLM muda de dominado por testes para orientado a diagnóstico ao longo do tempo é paralela a essa trajetória de iniciante a especialista.

Ko e Myers [13] identificaram que a dificuldade de depuração se correlaciona com a distância entre sintoma e causa. A saída de depuração multicamada (`--debug-layers`) foi projetada para reduzir essa distância ao mostrar todas as camadas de compilação simultaneamente.
