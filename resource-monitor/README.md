

# Resource Monitor – RA3 – Sistemas Operacionais
Monitor de recursos implementado em C para Linux, utilizando informações expostas no diretório `/proc`.  
O projeto permite monitorar **CPU**, **Memória**, **I/O**, **Namespaces** e **Cgroups** de qualquer processo do sistema.

Este repositório inclui:
- Executável `resource-monitor`
- Modo interativo (menu)
- Modo CLI (com argumentos)
- Testes automáticos (CPU, I/O e Memória)
- Scripts de comparação com ferramentas reais do Linux (`ps`)
- Ferramenta gráfica de visualização (`visualize.py`)
- Documentação completa em `docs/architecture.md`

---

# 📦 **Instalação e Compilação**

Requer Linux e gcc:

```bash
sudo apt install build-essential
----------------------------------------------
Compile o projeto:

make construir


O binário será criado em:

bin/resource-monitor
--------------------------------------------
🚀 Modos de Execução
1. Modo CLI (com argumentos)

Permite executar funções específicas diretamente:

🔍 Monitorar CPU:
./bin/resource-monitor cpu <PID> <intervalo_ms> <amostras>

🔍 Monitorar Memória:
./bin/resource-monitor mem <PID> <intervalo_ms> <amostras>

🔍 Monitorar I/O:
./bin/resource-monitor io <PID> <intervalo_ms> <amostras>

🔍 Analisar Namespaces:
./bin/resource-monitor ns <PID>

🔍 Analisar Cgroups:
./bin/resource-monitor cg <PID>


