# Compilador e flags
CC          = gcc
STD         = -std=c99
OPT         = -O2
DBG         = -g
WARN        = -Wall -Wextra -Wpedantic
INCLUDE_DIR = include
CFLAGS      = $(STD) $(OPT) $(DBG) $(WARN) -I$(INCLUDE_DIR) -MMD -MP
LDFLAGS     =
LDLIBS      = -lm

# Compilação paralela para builds mais rápidos
MAKEFLAGS += --no-print-directory -j$(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Pastas
SRC_DIR   = src
OBJ_DIR   = build
BIN_DIR   = bin
TEST_DIR  = tests
DOCS_DIR  = docs
SCRIPTS   = scripts

# Alvos
PROG      = $(BIN_DIR)/resource-monitor

SRC_FILES = \
	$(SRC_DIR)/main.c \
	$(SRC_DIR)/cpu_monitor.c \
	$(SRC_DIR)/io_monitor.c \
	$(SRC_DIR)/cgroup_manager.c \
	$(SRC_DIR)/memory_monitor.c \
	$(SRC_DIR)/namespace_analyzer.c

TEST_SRC  = \
	$(TEST_DIR)/test_cpu.c \
	$(TEST_DIR)/test_io.c  \
	$(TEST_DIR)/test_memory.c

OBJ       = $(SRC_FILES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
DEP       = $(OBJ:.o=.d)

TEST_OBJS = $(TEST_SRC:$(TEST_DIR)/%.c=$(OBJ_DIR)/%.o)
TEST_DEP  = $(TEST_OBJS:.o=.d)
TEST_BINS = $(BIN_DIR)/test_cpu $(BIN_DIR)/test_io $(BIN_DIR)/test_memory

# Regras principais
.PHONY: all construir testar exe_testes rodar limpar ajuda valgrind_test

all: construir

construir: prep_diretorios $(PROG)

prep_diretorios:
	@mkdir -p $(OBJ_DIR) $(BIN_DIR)
	@echo "Diretórios preparados."

$(PROG): $(OBJ) | $(BIN_DIR)
	$(CC) $(LDFLAGS) $(OBJ) -o $@ $(LDLIBS)
	@echo "Compilação concluída: $@"

# Objetos (gera .d)
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(TEST_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Testes
testar: prep_diretorios $(TEST_BINS)
	@echo "Todos os testes compilados."

exe_testes: testar
	@echo "Executando testes..."
	@for teste in $(TEST_BINS); do \
		echo "Executando $$teste..."; \
		"$$teste" || echo "Teste falhou: $$teste"; \
	done
	@echo "Todos os testes executados."

# Regra pattern para testes (mais concisa)
$(BIN_DIR)/test_%: $(OBJ_DIR)/test_%.o $(OBJ_DIR)/%_monitor.o | $(BIN_DIR)
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

# Regras explícitas mantidas para clareza (podem ser removidas se usar apenas pattern rule)
$(BIN_DIR)/test_cpu: $(OBJ_DIR)/test_cpu.o $(OBJ_DIR)/cpu_monitor.o | $(BIN_DIR)
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

$(BIN_DIR)/test_io: $(OBJ_DIR)/test_io.o $(OBJ_DIR)/io_monitor.o | $(BIN_DIR)
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

$(BIN_DIR)/test_memory: $(OBJ_DIR)/test_memory.o $(OBJ_DIR)/memory_monitor.o | $(BIN_DIR)
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

# Execução
rodar: construir
	@echo "Iniciando o monitoramento..."
	@$(PROG)

# Limpeza
limpar:
	@echo "Limpando arquivos..."
	@rm -rf $(OBJ_DIR) $(BIN_DIR)
	@echo "Limpeza concluída."

# Valgrind para detecção de memory leaks
valgrind_test: testar
	@echo "Executando testes com Valgrind..."
	@for teste in $(TEST_BINS); do \
		echo "Valgrind: $$teste"; \
		valgrind --leak-check=full --track-origins=yes --error-exitcode=1 "$$teste" && \
		echo "$$teste: OK" || echo "$$teste: MEMORY LEAK DETECTED"; \
	done
	@echo "Análise com Valgrind concluída."

# Ajuda
ajuda:
	@echo "Makefile para Resource Monitor"
	@echo ""
	@echo "Comandos disponíveis:"
	@echo "  make construir    - Compila o programa principal"
	@echo "  make testar       - Compila todos os testes"
	@echo "  make exe_testes   - Executa todos os testes compilados"
	@echo "  make rodar        - Executa o monitor de recursos"
	@echo "  make valgrind_test - Executa testes com valgrind (detecta memory leaks)"
	@echo "  make limpar       - Remove arquivos compilados"
	@echo "  make ajuda        - Mostra esta ajuda"

# Garante existência das pastas em dependências order-only
$(OBJ_DIR) $(BIN_DIR):
	@mkdir -p $@

# Inclui dependências geradas (-MMD) - AGORA INCLUI TESTES TAMBÉM
-include $(DEP) $(TEST_DEP)