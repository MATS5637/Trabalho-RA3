📘 Resource Monitor – README 📌 Introdução

Este projeto implementa um monitor de recursos de processos Linux, conforme especificado no PDF do Trabalho RA3. Ele coleta métricas de:

🧠 Memória

🖥️ CPU

💾 I/O

🧩 Namespaces

📦 Cgroups

O projeto inclui:

Código modular em C

Testes automáticos

Scripts de comparação

Script de visualização

Modo interativo e modo linha de comando

📁 Estrutura do Projeto
resource-monitor/
├── bin/ # Executáveis finais
├── build/ # Objetos compilados
├── data/ # Arquivos CSV gerados pelos testes
├── docs/
│ ├── ARCHITECTURE.md # Documento explicando a arquitetura
├── include/ # Cabeçalhos (.h)
│ ├── cgroup.h
│ ├── monitor.h
│ └── namespace.h
├── scripts/ # Scripts utilitários
│ ├── compare_tools.sh # Script de comparação entre ferramentas
│ └── visualize.py # Visualização gráfica dos CSVs
├── src/ # Código-fonte principal
│ ├── main.c
│ ├── cpu_monitor.c
│ ├── memory_monitor.c
│ ├── io_monitor.c
│ ├── cgroup_manager.c
│ └── namespace_analyzer.c
├── tests/ # Testes automáticos
│ ├── test_cpu.c
│ ├── test_io.c
│ └── test_memory.c
└── Makefile # Automação de compilação e testes

🔧 1. Preparar o Ambiente Requisitos:

GCC

Linux com suporte a /proc

Python 3 (para gráficos)

matplotlib (instalação recomendada via virtualenv)

Criar ambiente: python3 -m venv venv source venv/bin/activate pip install matplotlib

🏗️ 2. Compilar o Projeto make construir

Isso gera o executável:

bin/resource-monitor

🧪 3. Executar os Testes Automáticos Compilar testes: make testar

Executar todos os testes: make exe_testes

Rodar testes com Valgrind: make valgrind_test

Os testes verificam:

CPU

Memória

I/O

Todos de forma isolada.

🖥️ 4. Modos de Uso

O programa possui dois modos:

4.1. Modo Interativo (recomendado) ./bin/resource-monitor

Nele você pode:

Monitorar CPU

Monitorar Memória

Monitorar I/O

Analisar Namespaces

Gerenciar Cgroups

Listar PIDs disponíveis

Escolher o que deseja sem precisar lembrar parâmetros

4.2. Modo Linha de Comando (automação) 🔹 Monitorar CPU ./bin/resource-monitor cpu <intervalo_ms>

🔹 Monitorar Memória ./bin/resource-monitor mem

🔹 Monitorar I/O ./bin/resource-monitor io <intervalo_ms>

🔹 Namespaces de um processo ./bin/resource-monitor ns

🔹 Criar Cgroup ./bin/resource-monitor cgroup-create <cpu_cores> <mem_mb>

🔹 Adicionar processo ao Cgroup ./bin/resource-monitor cgroup-add

📊 5. Visualizar Gráficos

Após gerar arquivos CSV nas pastas data/, execute:

python3 scripts/visualize.py data/arquivo.csv

Gera gráficos de:

CPU

Memória

I/O

⚖️ 6. Script de Comparação entre Ferramentas

Para comparar performance entre duas ferramentas/métricas:

chmod +x scripts/compare_tools.sh ./scripts/compare_tools.sh <intervalo_ms>

O script:

Executa medições reais

Salva CSVs em data/

Gera gráficos automaticamente

🧹 7. Limpar Arquivos de Build make limpar

📦 8. Observações Importantes

Os testes automáticos usam PIDs gerados em runtime.

O projeto segue estritamente o formato solicitado no PDF.

Códigos modularizados conforme a especificação.

Scripts externos funcionam independentemente do main.
