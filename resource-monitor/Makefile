Compilador = gcc
FLAGS = -std=c2x
Otimizacao = -O2
DEBUG = -g
Avisos = -Wall -Wextra -Wpedantic


Fontes = src
Header = include
Objeto = build
Executavel = bin
Testes = tests
Scripts = scripts
Documentos = docs

CFLAGS = $(FLAGS) $(Otimizacao) $(DEBUG) $(Avisos) -I$(Header)
LINK_FLAGS = -lm

Programa_fin = $(Executavel)/resource-monitor

Codigos_fonte = \
	$(Fontes)/main.c \
	$(Fontes)/cpu_monitor.c \
	$(Fontes)/io_monitor.c \
	$(Fontes)/cgroup_manager.c \
	$(Fontes)/memory_monitor.c \
	$(Fontes)/namespace_analyzer.c 

Codigos_teste = \
	$(Testes)/test_cpu.c \
	$(Testes)/test_io.c \
	$(Testes)/test_memory.c 

objetos = $(Codigos_fonte:$(Fontes)/%.c=$(Objeto)/%.o)

Testes_comp = \
	$(Executavel)/test_cpu \
	$(Executavel)/test_io \
	$(Executavel)/test_memory

all: construir

construir: prep_diretorios $(Programa_fin)

prep_diretorios:
	@mkdir -p $(Objeto) $(Executavel)
	@echo "Diretórios preparados."

$(Programa_fin): $(objetos)
	$(Compilador) $(objetos) -o $@ $(LINK_FLAGS)
	@echo "Compilação concluída: $(Programa_fin)"

$(Objeto)/%.o: $(Fontes)/%.c
	$(Compilador) $(CFLAGS) -c $< -o $@
$(Objeto)/test_%.o: $(Testes)/test_%.c
	$(Compilador) $(CFLAGS) -c $< -o $@

testar: prep_diretorios $(Testes_comp)
	@echo "Todos os testes compilados."

exe_testes: testar
	@echo "Testando"
	@for teste in $(Testes_comp); do \
		echo "Executando $$teste..."; \
		$$teste || echo "teste falhou: $$teste"; \
	done
	@echo "Todos os testes executados."

$(Executavel)/test_cpu: $(Objeto)/test_cpu.o $(Objeto)/cpu_monitor.o
	@echo "Teste de CPU..."
	$(Compilador) $^ -o $@ $(LINK_FLAGS)
$(Executavel)/test_io: $(Objeto)/test_io.o $(Objeto)/io_monitor.o
	@echo "Teste de I/O..."
	$(Compilador) $^ -o $@ $(LINK_FLAGS)
$(Executavel)/test_memory: $(Objeto)/test_memory.o $(Objeto)/memory_monitor.o
	@echo "Teste de Memória..."
	$(Compilador) $^ -o $@ $(LINK_FLAGS)

rodar: construir
	@echo "Iniciando o monitoramento..."
	@$(Programa_fin)

limpar:
	@echo "limpando os arquivos..."
	rm -rf $(Objeto) $(Executavel)
	@echo "Limpeza concluída."

ajuda:
	@echo "Makefile para Resource Monitor"
	@echo ""
	@echo "Comandos disponíveis:"
	@echo "  make construir    - Compila o programa principal."
	@echo "  make testar       - Compila todos os testes."
	@echo "  make exe_testes   - Executa todos os testes compilados."
	@echo "  make rodar        - Executa o monitor de recursos."
	@echo "  make limpar      - Remove arquivos compilados."
.PHONY: all construir prep_diretorios testar exe_testes rodar limpar ajuda	