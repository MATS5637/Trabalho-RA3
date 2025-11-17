# Resource Monitor – Documentação de Arquitetura

Este documento descreve a ARQUITETURA INTERNA do sistema **Resource Monitor**, incluindo módulos, algoritmos, fluxos de dados, acesso ao `/proc`, estrutura de arquivos, integrações internas e decisões de design.  
**Este documento NÃO contém instruções de uso.**  
Essas instruções devem ficar exclusivamente no `README.md`.

---

# 1. Visão Geral da Arquitetura

O Resource Monitor é estruturado em uma arquitetura modular: cada aspecto monitorado (CPU, memória, I/O, namespaces, cgroups) tem um módulo próprio e completamente isolado.  
O `main.c` funciona como orquestrador, chamando os módulos quando necessário.

A base do projeto é a leitura do sistema de arquivos virtual `/proc`, que expõe estatísticas do kernel em tempo real.

Componentes centrais:

- **cpu_monitor.c** — leitura e cálculo de uso da CPU.
- **memory_monitor.c** — leitura de estatísticas de memória do processo e do sistema.
- **io_monitor.c** — leitura de operações de I/O do processo.
- **namespace_analyzer.c** — inspeção de namespaces.
- **cgroup_manager.c** — análise de cgroups aplicados ao processo.
- **main.c** — CLI e interface interativa.
- **tests/** — testes automatizados independentes.
- **scripts/** — scripts de análise externa (visualização e comparação).

---

# 2. Estrutura do Projeto

include/
monitor.h
cgroup.h
namespace.h

src/
main.c
cpu_monitor.c
memory_monitor.c
io_monitor.c
cgroup_manager.c
namespace_analyzer.c

tests/
test_cpu.c
test_io.c
test_memory.c

scripts/
compare_tools.sh
visualize.py

docs/
README.md
architecture.md

bin/
build/
Makefile

yaml
Copiar código

Cada módulo `.c` possui um `.h` correspondente contendo suas interfaces públicas.

---

# 3. Comunicação Entre Módulos

A arquitetura utiliza **módulos independentes**, mas padronizados pela API em `monitor.h`.  
O fluxo geral:

main.c
└─ lê argumentos ou abre menu interativo
└─ chama a função específica de monitoramento
├─ cpu_monitor()
├─ mem_monitor()
├─ io_monitor()
├─ namespaces_monitor()
└─ cgroups_monitor()
└─ cada módulo acessa arquivos em /proc

yaml
Copiar código

Todos os módulos utilizam **stdout** para dados CSV ou relatórios estruturados.

---

# 4. Arquitetura dos Módulos

Abaixo, a estrutura interna técnica de cada módulo.

---

## 4.1. CPU Monitor (`cpu_monitor.c`)

### Arquivos lidos:
- `/proc/<pid>/stat`
- `/proc/stat`

### Estrutura interna:
- Função `cpu_ler_processo()` extrai valores do processo.
- Função `cpu_ler_sistema()` extrai estatísticas globais.
- Função `cpu_calcular_percentual()` calcula uso diferencial baseado em duas amostras.
- Função `cpu_monitor_loop()` executa amostras em intervalo e gera CSV.

### Fluxo do algoritmo:
coletar estatísticas do processo
coletar estatísticas do sistema
esperar intervalo
coletar novamente
calcular diferenças (deltas)
imprimir CSV

yaml
Copiar código

---

## 4.2. Memory Monitor (`memory_monitor.c`)

### Arquivos lidos:
- `/proc/<pid>/status`
- `/proc/meminfo`

### Estrutura:
- Leitura do RSS e VSZ do processo.
- Leitura de MemTotal e MemAvailable do sistema.
- Cálculo percentual de uso do processo.
- Geração de relatório detalhado com todas as métricas do `/status`.

---

## 4.3. I/O Monitor (`io_monitor.c`)

### Arquivo lido:
- `/proc/<pid>/io`

Campos importantes:
- `read_bytes`
- `write_bytes`
- `syscr`
- `syscw`

Fluxo:
fazer leitura 1
esperar intervalo
fazer leitura 2
calcular taxas por segundo
emitir CSV

yaml
Copiar código

---

## 4.4. Namespace Analyzer (`namespace_analyzer.c`)

### Diretório lido:
- `/proc/<pid>/ns`

Cada namespace é um link simbólico como:
uts:[4026531838]

yaml
Copiar código

O módulo apenas lista os namespaces encontrados e suas identificações internas.

---

## 4.5. Cgroup Manager (`cgroup_manager.c`)

### Arquivo analisado:
- `/proc/<pid>/cgroup`

Campos:
- nível hierárquico
- subsistemas (cpu, memory, systemd…)
- caminho no cgroup

Fluxo:
abrir arquivo /proc/<pid>/cgroup
para cada linha:
quebrar nos 3 campos
exibir hierarquia, subsistema e caminho

yaml
Copiar código

---

# 5. Módulo Principal (`main.c`)

O `main` possui duas responsabilidades:

### 1) Interpretar argumentos (modo CLI)
Exemplos:
cpu <pid> <intervalo> <amostras>
mem <pid> <intervalo> <amostras>
io <pid> <intervalo> <amostras>
ns <pid>
cg <pid>

yaml
Copiar código

### 2) Fornecer modo interativo (menu)
Inclui:
- leitura e validação de entradas
- loop de execução
- integração com todos os módulos
- listagem de PIDs reais percorrendo `/proc`

---

# 6. Testes Automatizados

Localização: `tests/`

### architecture:
- Cada teste compila isoladamente com seu módulo correspondente.
- Os testes NÃO dependem do `main`.
- Each test creates its own workload (ou usa o próprio processo).

Funcional:
- `test_cpu.c`: valida coleta de CPU e cálculo de uso.
- `test_io.c`: valida taxas de I/O.
- `test_memory.c`: verifica leituras de `/status` e `/meminfo`.

---

# 7. Scripts Externos

Localização: `scripts/`

## 7.1. compare_tools.sh
- Executa o resource-monitor e o comando nativo `ps`.
- Gera arquivos CSV para comparação de precisão.
- CSVs são gravados em `docs/`.

## 7.2. visualize.py
- Lê CSVs gerados pelo resource-monitor ou pelo script acima.
- Gera gráficos para análise (CPU, memória, I/O).
- Requer matplotlib.

---

# 8. Principais Decisões de Design

- **Arquitetura modular** facilita manutenção e extensão.
- **Uso exclusivo de `/proc`** atende ao requisito do projeto.
- **CSV como formato padrão** garante integração com scripts.
- **Testes independentes** garantem confiabilidade por módulo.
- **Interativo + CLI** cobre múltiplas formas de operação.
- **Separação total entre lógica e interface** (main não processa cálculos).

---

# 9. Limitações e Considerações

- O programa funciona apenas em Linux.
- Leituras dependem da granularidade do kernel.
- I/O pode variar de acordo com buffers internos.
- Processos de curta duração podem terminar entre amostras.

---

# 10. Conclusão

Este documento descreve **apenas a arquitetura interna** do Resource Monitor.  
Todos os detalhes sobre *instalação, execução, comandos, exemplos, prints, saída CSV, dependências e testes* devem ser documentados exclusivamente no **README.md**.