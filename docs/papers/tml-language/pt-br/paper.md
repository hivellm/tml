# TML: Uma Linguagem de Programação de Sistemas Projetada para Geração de Código por LLMs

**Uma Análise Técnica Abrangente**

---

## Sumário

1. [Resumo](00-resumo.md)
2. [Introdução e Motivação](01-motivacao.md)
3. [Design de Sintaxe: Palavras-Chave em vez de Símbolos](02-design-sintaxe.md)
4. [Sistema de Tipos](03-sistema-tipos.md)
5. [Modelo de Memória](04-modelo-memoria.md)
6. [Arquitetura do Compilador](05-arquitetura-compilador.md)
7. [Pipeline de IR em Múltiplas Camadas](06-pipeline-ir.md)
8. [Arquitetura de Otimização](07-otimizacao.md)
9. [Design da Biblioteca Padrão](08-design-stdlib.md)
10. [Comparação Entre Linguagens](09-tabela-comparacao.md)
11. [Design LLM-First](10-otimizacao-llm.md)
12. [Infraestrutura de Testes](11-infraestrutura-testes.md)
13. [Ecossistema](12-ecossistema.md)
14. [Trabalhos Futuros e Conclusão](13-trabalhos-futuros.md)
15. [Referências](14-referencias.md)

---

## Estatísticas do Artigo

| Métrica | Valor |
|---------|-------|
| Total de seções | 15 |
| Total de linhas | ~2.531 |
| Tamanho total | ~142 KB |
| Contagem estimada de palavras | ~25.000 |
| Linguagens comparadas | TML, Rust, C++, Go, Python, Zig, Swift, Kotlin |
| Dimensões de comparação | 30+ |

---

## Como Ler Este Artigo

Este artigo é organizado como uma coleção de seções autocontidas, cada uma em seu próprio arquivo para facilitar a navegação e revisão. As seções seguem uma progressão lógica:

- **Seções 1–3** (Motivação, Sintaxe, Tipos): A visão em nível de linguagem — como o TML se parece e por quê.
- **Seção 4** (Memória): O modelo de segurança — como o TML previne erros de memória.
- **Seções 5–7** (Compilador, IR, Otimização): A visão de implementação — como o TML compila código.
- **Seção 8** (Biblioteca Padrão): A visão de ecossistema — o que o TML oferece por padrão.
- **Seção 9** (Comparação): A análise competitiva — como o TML se compara em diversas dimensões.
- **Seção 10** (LLM-First): A tese central — por que linguagens de programação deveriam ser projetadas para IA.
- **Seções 11–12** (Testes, Ecossistema): A visão de ferramentas — como o TML apoia o desenvolvimento.
- **Seção 13** (Trabalhos Futuros): O roteiro — para onde o TML está indo.

Cada seção pode ser lida de forma independente, embora o artigo completo proporcione a compreensão mais abrangente.

---

## Principais Descobertas

### 1. Design de Sintaxe (Seção 2)
O TML elimina 24+ fontes de ambiguidade de símbolos encontradas em Rust/C++/Python ao usar palavras-chave em vez de símbolos. Cada token tem exatamente um significado, e a gramática LL(1) está alinhada com a geração autoregressiva dos LLMs.

### 2. Sistema de Tipos (Seção 3)
O TML alcança segurança de tipos equivalente ao Rust com sintaxe mais simples: sem anotações de tempo de vida explícitas, nomes de tipos autodocumentados (Maybe, Outcome, Heap) e behaviors baseados em palavras-chave.

### 3. Arquitetura do Compilador (Seção 5)
Compilação por consultas demanda-dirigida (similar ao rustc) com cinco camadas de IR, LLVM+LLD embutidos e compilação incremental com invalidação de cache por fingerprints. 52 passos de otimização MIR.

### 4. Design LLM-First (Seção 10)
A tese central: linguagens de programação deveriam ser projetadas considerando a geração de código por LLMs. As inovações específicas do TML — significados únicos de tokens, gramática LL(1), nomes autodocumentados, integração com ferramentas MCP, camadas de debug para diagnóstico multi-IR — demonstram que isso é viável e benéfico.

### 5. Biblioteca Padrão Abrangente (Seção 8)
500+ tipos e 5.000+ funções, incluindo HTTP, JSON, criptografia, drivers de banco de dados, SIMD e algoritmos de busca — uma abordagem batteries-included que reduz a dependência de um ecossistema externo de pacotes.

### 6. Eficiência de Tokens (Seção 9)
O TML economiza 15–40% de tokens em comparação ao Rust para padrões de código equivalentes, impactando diretamente a quantidade de código que cabe nas janelas de contexto dos LLMs.

---

## Citação

```
TML Project. (2026). TML: A Systems Programming Language Designed for LLM Code Generation.
Technical Report. https://github.com/user/tml
```
