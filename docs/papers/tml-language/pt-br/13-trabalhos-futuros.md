# 13. Trabalhos Futuros

## 13.1 Auto-Hospedagem

O item mais ambicioso do roadmap do TML é a **auto-hospedagem**: reescrever o compilador TML em TML. Atualmente implementado em C++ (~100K linhas), o compilador seria progressivamente migrado para TML, começando pela biblioteca padrão (já em grande parte em TML) e se estendendo ao parser, verificador de tipos e codegen.

A auto-hospedagem serve a múltiplos propósitos:
1. **Validação**: Uma linguagem madura o suficiente para implementar seu próprio compilador demonstra capacidade para uso no mundo real.
2. **Bootstrapping**: Elimina a dependência da toolchain C++ para construir o TML.
3. **Dogfooding**: Força a linguagem a lidar com programação de sistemas complexa — análise, estruturas de dados, geração de código, I/O de arquivos.
4. **Eliminação de C++**: Alinha-se com o objetivo explícito do projeto de minimizar código C e C++.

A migração é planejada em fases:
- **Fase 1**: Biblioteca padrão totalmente em TML (atualmente 93,2% completa).
- **Fase 2**: Lexer e parser em TML (requer processamento de strings e estruturas de dados).
- **Fase 3**: Verificador de tipos em TML (requer estruturas de dados e algoritmos complexos).
- **Fase 4**: Geração de IR em TML (requer bindings FFI para LLVM).
- **Fase 5**: Auto-hospedagem completa.

---

## 13.2 Async/Await

O runtime async do TML está parcialmente implementado. A linguagem suporta declarações `async func` e a keyword `await`, com lowering de async como um passe MIR que transforma corpos de funções async em máquinas de estado. O trabalho restante inclui:

- **Executor async**: Um pool de threads com work-stealing para agendar tarefas async.
- **Integração de I/O**: I/O de arquivo e rede async pela infraestrutura existente de IOCP (Windows) e epoll (Linux).
- **Concorrência estruturada**: Grupos de tarefas, cancelamento e propagação de timeout.
- **Iteradores async**: Behavior `AsyncIterator` com laços `async for`.

O design parte do modelo async do Rust (futures de custo zero, baseado em poll) enquanto visa ergonomias mais simples (sem complexidade de `Pin`, pinning automático quando seguro).

---

## 13.3 Alvo WebAssembly

O backend LLVM do TML pode ter como alvo WebAssembly por meio dos backends wasm32/wasm64 do LLVM. No entanto, suporte prático a WASM requer:

- **Bindings WASI**: Mapeamento de I/O da biblioteca padrão do TML para chamadas de sistema WASI.
- **Gerenciamento de memória**: Adaptação do alocador para o modelo de memória linear do WASM.
- **Otimização de tamanho binário**: Eliminação agressiva de código morto para produzir binários WASM pequenos.
- **Interop com JavaScript**: Bindings FFI para chamar JavaScript a partir do TML e vice-versa.

O suporte a WebAssembly habilitaria o TML para aplicações baseadas em navegador, funções serverless (Cloudflare Workers, Fastly Compute) e sistemas de plugins.

---

## 13.4 Gerenciador de Pacotes

O TML atualmente não possui um gerenciador de pacotes. A biblioteca padrão é monolítica e vem com o compilador. Um ecossistema de pacotes requereria:

- **Formato de pacote**: Provavelmente baseado no formato `.rlib` existente com metadados.
- **Registro**: Um repositório central para pacotes publicados.
- **Resolução de dependências**: Resolução de versões com versionamento semântico.
- **Integração de build**: Integração com o sistema de build existente do TML.

O design provavelmente se inspirará no Cargo (Rust) para ergonomia e nos módulos Go para simplicidade, com atenção particular à descoberta de pacotes amigável a LLMs (busca em linguagem natural, documentação de API estruturada).

---

## 13.5 Validação Formal das Afirmações de Design LLM-First

A tese central deste artigo — que sintaxe baseada em keywords e LL(1) melhora a precisão de geração de código por LLM — é apoiada por raciocínio de design e testes informais, mas carece de validação experimental formal. Trabalhos futuros devem incluir:

1. **Experimentos controlados**: Apresentar tarefas de programação equivalentes a LLMs em TML e Rust, medindo taxas de erro de sintaxe, correção semântica e tempo-até-geração-correta.
2. **Medição de eficiência de tokens**: Comparação precisa de contagens de tokens BPE para programas equivalentes entre linguagens.
3. **Taxonomia de erros**: Classificação sistemática de erros de geração por LLM por linguagem, identificando quais erros são induzidos por sintaxe versus semânticos.
4. **Estudo longitudinal**: À medida que os dados de treinamento de LLMs incluem mais código TML, a precisão de geração melhora mais rápido do que para outras linguagens?

O projeto de pesquisa contínua de depuração de IR por LLM [36] fornece infraestrutura para alguns desses experimentos, com registro de uso de ferramentas e coleta de dados estruturados já implementados.

---

## 13.6 Maturidade Multiplataforma

O TML atualmente tem como alvo principal o Windows (plataforma de desenvolvimento primária), com Linux e macOS como alvos secundários. Os trabalhos futuros incluem:

- **CI/CD no Linux**: Testes automatizados no Linux.
- **Suporte a macOS**: Alvo Apple Silicon (ARM64) com formato binário Mach-O.
- **Compilação cruzada**: Construção para uma plataforma a partir de outra, aproveitando as capacidades de compilação cruzada do Zig CC.
- **Otimização ARM64**: Otimizações específicas para processadores ARM.

---

## 13.7 Integração com IDEs

O TML tem suporte preliminar a IDEs por meio do seu servidor MCP, mas a integração completa requer:

- **Language Server Protocol (LSP)**: Ir para definição, encontrar referências, renomear, ações de código.
- **Realce semântico**: Classificação de tokens para coloração de sintaxe baseada em informação de tipos.
- **Diagnósticos inline**: Verificação de tipos em tempo real enquanto o usuário digita.
- **Integração com depurador**: DAP (Debug Adapter Protocol) para depuração passo a passo com inspeção de variáveis.

A arquitetura de compilação baseada em consultas é bem adequada para implementação de LSP, uma vez que consultas individuais podem ser reavaliadas incrementalmente quando o código-fonte muda.

---

## 13.8 Recursos Avançados do Sistema de Tipos

Diversas extensões do sistema de tipos estão sendo consideradas:

- **Tipos de ordem superior (Higher-kinded types)**: Parâmetros de behavior que são construtores de tipo (`Functor[F[_]]`).
- **GADTs** (Generalized Algebraic Data Types): Variantes indexadas por tipo para DSLs incorporadas com segurança de tipos.
- **Sistema de efeitos**: Rastreamento de efeitos colaterais no sistema de tipos para análise de pureza.
- **Tipos dependentes** (limitados): Expressões em tempo de compilação em posições de tipo além de genéricos constantes.

Cada extensão seria avaliada com base no princípio de design LLM-first: ela melhora ou degrada a precisão de geração de código por LLM? Recursos do sistema de tipos que são poderosos mas sintaticamente complexos (bounds de trait de ordem superior do Rust, SFINAE do C++) seriam rejeitados em favor de alternativas mais simples que atingem 80% da expressividade com 20% da complexidade.

---

## 13.9 Conclusão

O TML representa uma exploração inicial de um novo espaço de design: linguagens de programação que tratam a geração de código por LLM como uma restrição de design de primeira classe. A linguagem demonstra que é possível combinar as garantias de segurança do modelo de ownership do Rust com uma sintaxe sistematicamente otimizada tanto para legibilidade humana quanto para geração por máquina.

O pipeline de IR em cinco camadas do compilador, os 52 passes de otimização MIR e o backend LLVM embarcado fornecem desempenho competitivo com linguagens de sistemas existentes. A biblioteca padrão abrangente (500+ tipos, 5.000+ funções) e o ferramental MCP integrado criam um ecossistema projetado do zero para desenvolvimento assistido por IA.

À medida que os LLMs se tornam cada vez mais centrais no desenvolvimento de software, antecipamos que os princípios explorados no TML — significados únicos para tokens, gramáticas LL(1), nomes auto-documentados, interfaces de ferramentas estruturadas — influenciarão o design de linguagens de programação futuras, seja como novas linguagens ou como evolução das existentes.

O campo de "design de linguagem consciente de LLMs" é incipiente. O TML é um ponto de dados no que provavelmente se tornará uma rica área de pesquisa na interseção de teoria de linguagens de programação, engenharia de compiladores e inteligência artificial.
