# Comportamento de Depuração de LLMs no Desenvolvimento de Compiladores: Um Estudo Empírico de Padrões de Uso de Ferramentas via Model Context Protocol

**Andre Ferreira**

Projeto HiveLLM, 2026

---

## Sumário

1. [Resumo](pt-br/00-resumo.md)
2. [Introdução](pt-br/01-introducao.md)
3. [Fundamentação Teórica](pt-br/02-fundamentacao.md)
4. [Design do Sistema](pt-br/03-design-sistema.md)
5. [Metodologia](pt-br/04-metodologia.md)
6. [Resultados](pt-br/05-resultados.md)
7. [Discussão](pt-br/06-discussao.md)
8. [Ameaças à Validade](pt-br/07-ameacas-validade.md)
9. [Trabalhos Relacionados](pt-br/08-trabalhos-relacionados.md)
10. [Conclusão](pt-br/09-conclusao.md)
11. [Referências](pt-br/10-referencias.md)

---

## Estatísticas do Artigo

| Métrica | Valor |
|---------|-------|
| Total de seções | 11 |
| Total de linhas | ~613 |
| Contagem estimada de palavras | ~6.800 |
| Período de coleta de dados | 30 dias |
| Invocações de ferramentas analisadas | 3.251 |
| Sessões observadas | 300 |
| Ferramentas distintas estudadas | 17 |

---

## Como Ler Este Artigo

Este artigo está organizado como uma coleção de seções independentes, cada uma em seu próprio arquivo para facilitar a navegação e revisão. As seções seguem uma progressão lógica:

- **Seção 1** (Introdução): Motiva o estudo — por que o comportamento de depuração de LLMs no desenvolvimento de compiladores é pouco estudado e o que a instrumentação MCP possibilita.
- **Seção 2** (Fundamentação): Cobre o compilador TML, o conjunto de ferramentas MCP e a infraestrutura de coleta de dados.
- **Seção 3** (Design do Sistema): Descreve a arquitetura do servidor MCP, as 17 ferramentas expostas e o mecanismo de logging.
- **Seção 4** (Metodologia): Define as métricas, a janela de observação de 30 dias e o design de intervenção comportamental.
- **Seção 5** (Resultados): Os principais achados empíricos — distribuição de ferramentas, curvas de adoção, o estudo de caso SIMD, impacto da latência e padrões de transição.
- **Seção 6** (Discussão): Implicações para design de ferramentas, engenharia de prompts e a mudança de tentativa e erro para raciocínio diagnóstico.
- **Seção 7** (Ameaças à Validade): Validade interna, externa e de construto — o que limita a generalização dos achados.
- **Seção 8** (Trabalhos Relacionados): Situa o estudo em relação à geração de código por LLMs, uso de ferramentas, reparo automático de programas e pesquisa em depuração humana.
- **Seção 9** (Conclusão): Cinco achados principais, recomendações para designers de ferramentas, engenheiros de prompts e pesquisadores, e direções de trabalho futuro.
- **Seção 10** (Referências): Bibliografia completa [1]–[20].

Cada seção pode ser lida de forma independente, embora o artigo completo ofereça a compreensão mais abrangente.

---

## Principais Achados

### 1. LLMs São Predominantemente Orientados a Testes (Seção 5)
52,7% de todas as chamadas de ferramentas são invocações de `test`. LLMs preferem feedback definitivo de aprovação/reprovação em vez de análise diagnóstica incremental, espelhando padrões de tentativa e erro observados em programadores humanos iniciantes.

### 2. Intervenções por Prompts Produzem Mudança Comportamental Mensurável (Seções 5–6)
Uma única regra ("use `check` antes de `test`") elevou a adoção de verificação de tipos de 8,8% para 25,3% em 10 dias, com trajetória acelerada (não estabilizada).

### 3. A Adoção de Funcionalidades Correlaciona Inversamente com o Atrito Cognitivo (Seção 6)
Funcionalidades ativas por padrão (`structured=true`) alcançam 95,7% de adoção. Funcionalidades opcionais que exigem troca de modo (`debug_layers`) atingem apenas 11,1% apesar de regras explícitas.

### 4. Uma Mudança Estrutural em Direção ao Raciocínio Diagnóstico Está em Curso (Seção 6)
A participação dos testes caiu de 60% para 44% em 30 dias, enquanto o uso de `check` e `emit-ir` cresceu. Sessões tardias mostram `check` superando `test` em frequência.

### 5. A Latência das Ferramentas É o Principal Gargalo do Desenvolvimento (Seção 5)
Testes são executados a ~37 segundos por chamada e respondem por 52,7% de todas as chamadas. A execução JIT projetada (~2 segundos) reduziria o ciclo edição-teste de 42 para 7 segundos.

---

## Citação

```
A. Ferreira, "Comportamento de Depuração de LLMs no Desenvolvimento de Compiladores:
Um Estudo Empírico de Padrões de Uso de Ferramentas via Model Context Protocol,"
Projeto HiveLLM, 2026.
```
