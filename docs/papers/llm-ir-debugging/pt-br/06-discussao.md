# 6. Discussão

## 6.1 Implicações para o Design de Ferramentas

Nossos resultados sugerem vários princípios para o design de ferramentas MCP voltadas a consumidores LLM:

**Diagnósticos ativos por padrão superam os opcionais.** O contraste entre a adoção de `structured=true` (95,7%, sem regra necessária) e a adoção de `debug_layers` (11,1%, regra explícita necessária) demonstra que os LLMs adotam funcionalidades que estão ativas por padrão, mas raramente ativam funcionalidades que exigem ativação explícita. Os designers de ferramentas devem tornar a saída diagnóstica o padrão em caso de falha, não um parâmetro opcional.

**A latência determina a seleção de ferramentas.** A forte preferência do LLM por `test` (52,7%) em relação ao `check` (17,4%) persiste apesar de `check` ser 10x mais rápido e explicitamente promovido. Atribuímos isso à preferência do LLM por feedback *definitivo* (o teste passa ou falha) em vez de feedback *parcial* (verifica os tipos, mas pode ainda travar em tempo de execução). Reduzir a latência dos testes via execução JIT abordaria essa preferência diretamente.

**Ferramentas de granularidade fina são preferidas.** A taxa de 74,9% de testes de arquivo único sugere que os LLMs preferem fortemente feedback direcionado em vez de abrangente. Os designers de ferramentas devem otimizar para invocação de granularidade fina (função única, arquivo único) em vez de operações em lote.

**Ferramentas de documentação são subutilizadas, mas altamente confiáveis.** Com taxas de erro de 0,0-0,7%, as ferramentas de documentação são as mais confiáveis do conjunto, mas representam apenas 13,4% das chamadas. Uma melhor integração (por exemplo, sugestão automática de documentação relevante quando um erro de tipo ocorre) poderia aumentar a adoção.

## 6.2 Implicações para Engenharia de Prompts

Nossos dados longitudinais fornecem evidências para vários princípios de engenharia de prompts:

**Justificativa quantitativa acelera a adoção.** A regra check-antes-de-test incluía "check é 10x mais rápido que test", fornecendo uma razão concreta. Essa regra produziu adoção composta (8,8% -> 25,3% em 10 dias). Regras sem justificativa quantitativa (por exemplo, debug_layers) produziram crescimento mais lento e linear.

**Regras produzem mudança comportamental composta, não transitória.** A curva de adoção do check está *acelerando* (crescimento período a período de +36%, +46%, +45%), não se estabilizando. Isso sugere que o LLM internaliza a justificativa e a aplica de forma mais consistente com a experiência, em vez de simplesmente obedecer quando a regra está em evidência.

**O atrito cognitivo determina o teto de adoção.** Funcionalidades que requerem simples mudanças de sequenciamento (check antes de test) alcançam adoção mais alta do que funcionalidades que exigem troca de modo (debug_layers em caso de falha). Os engenheiros de prompts devem minimizar as etapas cognitivas entre o reconhecimento da regra e sua execução.

**Menções de novas ferramentas produzem adoção rápida, mas superficial.** Cinco novas ferramentas adicionadas ao prompt do sistema (`build`, `debug`, `profile`, `inspect`, `docs_resolve`) apareceram nos logs em 1-2 sessões, mas com taxas muito baixas (1-16 chamadas no total). A conscientização inicial sobre as ferramentas é alta; o uso sustentado requer exemplos práticos e gatilhos claros.

## 6.3 A Mudança de Tentativa e Erro para Raciocínio Diagnóstico

O achado mais significativo é a mudança estrutural na estratégia de depuração ao longo do período de observação. O comportamento do LLM evoluiu de um padrão "executar e observar" dominado por testes (60% test, 8,8% check) para um padrão diagnóstico "analisar e depois verificar" (44% test, 25,3% check, 9,2% emit-ir nas sessões recentes).

Essa mudança espelha a trajetória de iniciante a especialista observada em estudos humanos de depuração [12, 13]: programadores iniciantes recorrem a tentativa e erro por padrão, enquanto especialistas usam diagnóstico sistemático. A trajetória do LLM sugere que a engenharia de prompts e a experiência no domínio podem impulsionar essa transição, embora ela ocorra ao longo de dias em vez dos meses ou anos típicos do desenvolvimento de habilidades humanas.

O estudo de caso SIMD (Seção 5.4) demonstra o ponto de chegada dessa trajetória: sessões em que `check` supera `test` em frequência, e o LLM segue fluxos de trabalho diagnósticos estruturados com múltiplas ferramentas. Se esse comportamento persiste em diferentes domínios de tarefas permanece uma questão em aberto.

## 6.4 Limitações

Este estudo tem várias limitações importantes:

1. **Projeto único**: Todos os dados provêm de um projeto de compilador (TML). Os padrões de uso de ferramentas podem diferir para desenvolvimento web, sistemas embarcados ou outros domínios.
2. **LLM único**: Todos os dados provêm do Claude Opus 4.6 [17]. Outros modelos (GPT-4 [18], Gemini [19], modelos de código aberto) podem exibir comportamentos de depuração diferentes.
3. **Dados orgânicos**: Sem experimentos controlados, não podemos estabelecer relações causais entre intervenções e mudanças comportamentais. As tendências observadas podem ser confundidas por variação de tarefas, efeitos de aprendizado ou acumulação de prompts.
4. **Efeito do observador**: O LLM sabe que as ferramentas existem e que o prompt do sistema promove certos padrões de uso. Essa consciência pode influenciar o comportamento independentemente da utilidade intrínseca das ferramentas.
5. **Estimativa de tokens**: Não temos contagens exatas de tokens para as respostas das ferramentas, limitando nossa capacidade de analisar a densidade de informação por chamada.
