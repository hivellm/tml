# 9. Conclusão

## 9.1 Resumo dos Achados

Este artigo apresentou o primeiro estudo empírico do uso de ferramentas de depuração por LLMs no desenvolvimento de um compilador em produção. A partir de 3.251 chamadas de ferramentas em 300 sessões, identificamos cinco achados principais:

1. **LLMs são predominantemente orientados a testes** (52,7% das chamadas), preferindo feedback definitivo de aprovação/reprovação em vez de análise diagnóstica. Isso espelha o padrão de tentativa e erro observado em programadores humanos iniciantes.

2. **Intervenções baseadas em prompts produzem mudança comportamental mensurável e composta.** A regra check-antes-de-test elevou a adoção de 8,8% para 25,3% em 10 dias, com trajetória acelerada (não estabilizada).

3. **A adoção de funcionalidades é inversamente correlacionada ao atrito cognitivo.** Funcionalidades ativas por padrão (saída estruturada) alcançam 95,7% de adoção. Funcionalidades opcionais que exigem mudança comportamental (debug_layers) atingem apenas 11,1% apesar de regras explícitas.

4. **Uma mudança estrutural de tentativa e erro para raciocínio diagnóstico está em curso**, com a participação dos testes caindo de 60% para 44% à medida que o uso de check e emit-ir aumenta. Sessões avançadas mostram check superando test em frequência.

5. **A latência das ferramentas é o principal gargalo do desenvolvimento.** A execução de testes a 37 segundos por chamada domina o tempo de computação. A execução JIT (latência projetada de 2 segundos) transformaria a velocidade de desenvolvimento.

## 9.2 Recomendações

Para **designers de ferramentas MCP**: Torne a saída diagnóstica ativa por padrão em casos de falha. Otimize para invocação de granularidade fina. Reduza a latência como a melhoria de maior alavancagem.

Para **engenheiros de prompts**: Inclua justificativa quantitativa nas regras comportamentais. Minimize o atrito cognitivo entre o reconhecimento da regra e sua execução. Espere adoção composta ao longo de dias, não conformidade imediata.

Para **pesquisadores**: Instrumente servidores MCP para estudos longitudinais. Compare o uso de ferramentas entre famílias de LLMs, domínios de programação e níveis de experiência do desenvolvedor.

## 9.3 Trabalhos Futuros

Este estudo abre várias direções para pesquisas futuras:

1. **Experimentos controlados**: Curar um conjunto de bugs estratificado por camada de compilação e medir o tempo de resolução sob diferentes condições diagnósticas (linha de base, debug-layers, sugestão automática).
2. **Comparação entre modelos**: Replicar o estudo com GPT-4, Gemini e modelos de código aberto para identificar preferências de depuração específicas a cada modelo.
3. **Estudo multiprojeto**: Estender a instrumentação para desenvolvimento web, sistemas embarcados e fluxos de trabalho de DevOps para testar a generalização.
4. **Prompting adaptativo**: Usar dados de uso de ferramentas em tempo real para ajustar dinamicamente os prompts do sistema, reforçando padrões eficazes e corrigindo os ineficazes.
5. **Medição do impacto do JIT**: Quantificar como a redução da latência dos testes (via execução JIT) altera a distribuição de ferramentas e a estratégia de depuração.

[A ser atualizado com dados adicionais conforme o estudo avança.]
