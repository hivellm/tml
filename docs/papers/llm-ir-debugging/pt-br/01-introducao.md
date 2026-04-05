# 1. Introdução

A aplicação de Modelos de Linguagem de Grande Escala à engenharia de software expandiu-se rapidamente, passando da completude de código [1] para fluxos de trabalho autônomos de desenvolvimento em múltiplas etapas [2, 3]. Embora a qualidade da geração de código tenha recebido atenção considerável, o *comportamento de depuração* dos LLMs -- como eles diagnosticam e corrigem erros em sistemas complexos -- permanece pouco compreendido. Essa lacuna é especialmente crítica em tarefas de programação de sistemas como o desenvolvimento de compiladores, onde os bugs abrangem múltiplas camadas de abstração (lexer, parser, verificador de tipos, representações intermediárias, geração de código de máquina) e exigem raciocínio diagnóstico estruturado.

O desenvolvimento de compiladores apresenta um domínio de depuração singularmente desafiador. Um único bug pode se manifestar como uma falha em tempo de execução, mas sua causa raiz pode estar na inferência de tipos, na construção da representação intermediária (IR), nas passagens de otimização ou na geração de código. Uma depuração eficaz requer navegar por essas camadas de forma sistemática -- uma tarefa que testa se os LLMs conseguem executar fluxos de trabalho diagnósticos estruturados de múltiplas etapas em vez de recorrer a iterações de tentativa e erro.

O Model Context Protocol (MCP) [4] fornece uma interface padronizada entre LLMs e ferramentas de desenvolvimento, permitindo instrumentação detalhada do uso de ferramentas. Ao registrar cada invocação de ferramenta MCP durante o desenvolvimento orgânico do compilador, podemos observar o comportamento de depuração do LLM in situ -- sem as restrições artificiais de benchmarks ou os vieses de tarefas contrivadas.

Este artigo realiza as seguintes contribuições:

1. **O primeiro conjunto de dados empíricos** do uso de ferramentas de depuração por LLMs no desenvolvimento de um compilador em produção: 3.251 chamadas em 300 sessões, 17 ferramentas distintas, abrangendo 30 dias de desenvolvimento orgânico no compilador TML.

2. **Análise quantitativa dos padrões de uso de ferramentas**, revelando que os LLMs são predominantemente orientados a testes (52,7%), subutilizam diagnósticos de IR (7,5%), e preferem fortemente testes de granularidade fina em detrimento de testes abrangentes (74,9% testes de arquivo único).

3. **Evidências de que intervenções baseadas em prompts alteram mensuravelmente o comportamento de depuração dos LLMs**, com a adoção de verificação de tipos acelerando de 8,8% para 25,3% em 10 dias após regras explícitas com justificativa quantitativa.

4. **Recomendações de design para ecossistemas de ferramentas MCP**, incluindo diagnósticos ativos por padrão, sugestão automática de etapas de pré-validação e redução de latência como principal alavanca para a adoção de ferramentas.
