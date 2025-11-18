// tests/test_io.c - VERSÃO COMPLETA
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>

#include "../include/monitor.h"

/* ==================== TESTE 1: MONITORAMENTO BÁSICO ==================== */

static int teste_monitoramento_basico(int argc, char *argv[]) {
    pid_t pid;
    int intervalo_ms;
    int amostras;
    const char *rotulo;

    if (argc == 1) {
        pid = getpid();
        intervalo_ms = 100;
        amostras = 5;
        rotulo = "auto";
    } else if (argc == 5) {
        pid = (pid_t)atoi(argv[1]);
        intervalo_ms = atoi(argv[2]);
        amostras = atoi(argv[3]);
        rotulo = argv[4];
    } else {
        fprintf(stderr, "Uso: %s <pid> <intervalo_ms> <amostras> <rotulo>\n", argv[0]);
        fprintf(stderr, "Ou rode sem argumentos para usar PID atual e parâmetros padrão.\n");
        return 1;
    }

    if (pid <= 0 || intervalo_ms <= 0 || amostras <= 0) {
        fprintf(stderr, "Parâmetros inválidos.\n");
        return 1;
    }

    if (!processo_existe(pid)) {
        fprintf(stderr, "Processo %d não existe.\n", pid);
        return 1;
    }

    printf("# Teste de I/O para PID %d (%s)\n", pid, rotulo);
    return io_monitorar_pid_csv(pid, intervalo_ms, amostras, stdout);
}

/* ==================== TESTE 2: WORKLOAD DE I/O ==================== */

static int teste_workload_io(void) {
    printf("\n=== TESTE 2: Workload de I/O ===\n");
    
    const char *test_file = "/tmp/test_io_workload.dat";
    printf("Criando workload em: %s\n", test_file);
    
    // Cria workload de I/O
    if (io_criar_workload(test_file, 10, 5) != 0) {
        fprintf(stderr, "Falha ao criar workload de I/O\n");
        return -1;
    }
    
    // Monitora o próprio processo durante o workload
    printf("Monitorando I/O durante workload...\n");
    pid_t self = getpid();
    
    io_stats_t stats_antes, stats_depois, taxas;
    
    if (io_ler_stats_processo(self, &stats_antes) != 0) {
        fprintf(stderr, "Falha ao ler stats iniciais de I/O\n");
        return -1;
    }
    
    // Executa operações de I/O
    FILE *fp = fopen(test_file, "r");
    if (fp) {
        char buffer[4096];
        size_t total_read = 0;
        while (fread(buffer, 1, sizeof(buffer), fp) > 0) {
            total_read += sizeof(buffer);
        }
        fclose(fp);
        printf("Lidos %zu bytes do arquivo de teste\n", total_read);
    }
    
    if (io_ler_stats_processo(self, &stats_depois) != 0) {
        fprintf(stderr, "Falha ao ler stats finais de I/O\n");
        return -1;
    }
    
    // Calcula taxas
    if (io_calcular_taxas(&stats_antes, &stats_depois, 1000, &taxas) == 0) {
        printf("Taxas de I/O durante teste:\n");
        printf("  Read:  %llu B/s\n", taxas.read_bytes);
        printf("  Write: %llu B/s\n", taxas.write_bytes);
    }
    
    // Limpeza
    unlink(test_file);
    printf("Teste de workload concluído.\n");
    return 0;
}

/* ==================== TESTE 3: I/O DO SISTEMA ==================== */

static int teste_io_sistema(void) {
    printf("\n=== TESTE 3: I/O do Sistema ===\n");
    
    io_stats_t stats_sistema;
    if (io_ler_stats_sistema(&stats_sistema) == 0) {
        printf("Estatísticas de I/O do sistema:\n");
        io_imprimir_stats(&stats_sistema, "do Sistema");
        return 0;
    } else {
        fprintf(stderr, "Falha ao ler I/O do sistema\n");
        return -1;
    }
}

/* ==================== TESTE 4: USO INSTANTÂNEO ==================== */

static int teste_uso_instantaneo(void) {
    printf("\n=== TESTE 4: Uso Instantâneo de I/O ===\n");
    
    pid_t self = getpid();
    io_stats_t taxas;
    
    printf("Obtendo uso instantâneo de I/O para PID %d...\n", self);
    
    if (io_obter_uso_instantaneo(self, &taxas) == 0) {
        printf("Uso instantâneo de I/O:\n");
        printf("  Read:  %llu B/s\n", taxas.read_bytes);
        printf("  Write: %llu B/s\n", taxas.write_bytes);
        printf("  Read syscalls:  %llu/s\n", taxas.read_syscalls);
        printf("  Write syscalls: %llu/s\n", taxas.write_syscalls);
        return 0;
    } else {
        fprintf(stderr, "Falha ao obter uso instantâneo de I/O\n");
        return -1;
    }
}

/* ==================== FUNÇÃO PRINCIPAL ==================== */

int main(int argc, char *argv[]) {
    printf("============================================\n");
    printf("  TESTES DO MÓDULO DE I/O - RESOURCE MONITOR\n");
    printf("============================================\n");
    
    int erro = 0;
    
    // Teste 1: Monitoramento básico (compatível com versão original)
    if (teste_monitoramento_basico(argc, argv) != 0) {
        fprintf(stderr, "ERRO no Teste 1 (Monitoramento Básico)\n");
        erro = 1;
    }
    
    // Testes adicionais apenas se não foram passados argumentos
    if (argc == 1) {
        if (teste_workload_io() != 0) {
            fprintf(stderr, "ERRO no Teste 2 (Workload I/O)\n");
            erro = 1;
        }
        
        if (teste_io_sistema() != 0) {
            fprintf(stderr, "ERRO no Teste 3 (I/O Sistema)\n");
            erro = 1;
        }
        
        if (teste_uso_instantaneo() != 0) {
            fprintf(stderr, "ERRO no Teste 4 (Uso Instantâneo)\n");
            erro = 1;
        }
    }
    
    if (!erro) {
        printf("\n✅ Todos os testes de I/O foram executados com sucesso!\n");
    } else {
        printf("\n❌ Alguns testes de I/O falharam.\n");
    }
    
    return erro ? EXIT_FAILURE : EXIT_SUCCESS;
}