// tests/test_memory.c - VERSÃO COMPLETA
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>

#include "../include/monitor.h"

/* ==================== TESTE 1: LEITURA BÁSICA ==================== */

static int teste_leitura_basica(int argc, char *argv[]) {
    pid_t pid;

    if (argc == 1) {
        pid = getpid();
    } else if (argc == 2) {
        pid = (pid_t)atoi(argv[1]);
    } else {
        fprintf(stderr, "Uso: %s <pid>\n", argv[0]);
        fprintf(stderr, "Ou rode sem argumentos para usar o PID atual.\n");
        return 1;
    }

    if (pid <= 0) {
        fprintf(stderr, "PID inválido.\n");
        return 1;
    }

    if (!processo_existe(pid)) {
        fprintf(stderr, "Processo %d não existe.\n", pid);
        return 1;
    }

    mem_proc_stats_t proc;
    mem_sys_stats_t sys;

    if (mem_ler_processo(pid, &proc) != 0) {
        fprintf(stderr, "Falha ao ler estatísticas de memória do processo.\n");
        return 1;
    }

    if (mem_ler_sistema(&sys) != 0) {
        fprintf(stderr, "Falha ao ler estatísticas de memória do sistema.\n");
        return 1;
    }

    double uso = mem_calcular_percentual_uso(&proc, &sys);

    printf("PID %d:\n", pid);
    printf("  RSS: %llu kB\n", proc.rss_kb);
    printf("  VSZ: %llu kB\n", proc.vsz_kb);
    printf("  Memória em uso: %.2f%%\n", uso);

    printf("\nRelatório detalhado:\n");
    if (mem_gerar_relatorio(pid, stdout) != 0) {
        fprintf(stderr, "Falha ao gerar relatório de memória.\n");
        return 1;
    }

    return 0;
}

/* ==================== TESTE 2: MONITORAMENTO CONTÍNUO ==================== */

static int teste_monitoramento_continuo(void) {
    printf("\n=== TESTE 2: Monitoramento Contínuo de Memória ===\n");
    
    pid_t self = getpid();
    const char *arquivo_csv = "memory_monitor_test.csv";
    int intervalo_ms = 500;
    int amostras = 6;
    
    printf("Monitorando PID %d por %d amostras (%d ms intervalo)\n", 
           self, amostras, intervalo_ms);
    printf("Salvando em: %s\n", arquivo_csv);
    
    FILE *fp = fopen(arquivo_csv, "w");
    if (!fp) {
        fprintf(stderr, "Erro ao criar arquivo CSV: %s\n", arquivo_csv);
        return -1;
    }
    
    int rc = mem_monitorar_pid_csv(self, intervalo_ms, amostras, fp);
    fclose(fp);
    
    if (rc == 0) {
        printf("Monitoramento concluído. Verifique %s para análise.\n", arquivo_csv);
    } else {
        fprintf(stderr, "Falha no monitoramento contínuo\n");
    }
    
    return rc;
}

/* ==================== TESTE 3: STRESS DE MEMÓRIA ==================== */

static int teste_stress_memoria(void) {
    printf("\n=== TESTE 3: Stress de Memória ===\n");
    
    printf("Alocando e liberando memória para testar métricas...\n");
    
    // Leitura inicial
    mem_proc_stats_t proc_antes, proc_depois;
    pid_t self = getpid();
    
    if (mem_ler_processo(self, &proc_antes) != 0) {
        fprintf(stderr, "Falha na leitura inicial\n");
        return -1;
    }
    
    printf("Estado inicial - RSS: %llu kB, VSZ: %llu kB\n", 
           proc_antes.rss_kb, proc_antes.vsz_kb);
    
    // Aloca memória
    size_t block_size = 10 * 1024 * 1024; // 10 MB
    char *memory_block = malloc(block_size);
    if (!memory_block) {
        fprintf(stderr, "Falha ao alocar memória\n");
        return -1;
    }
    
    // Preenche a memória para forçar commit
    memset(memory_block, 0xAA, block_size);
    
    sleep(1); // Dá tempo para as métricas atualizarem
    
    // Leitura após alocação
    if (mem_ler_processo(self, &proc_depois) != 0) {
        fprintf(stderr, "Falha na leitura após alocação\n");
        free(memory_block);
        return -1;
    }
    
    printf("Estado após alocação - RSS: %llu kB, VSZ: %llu kB\n",
           proc_depois.rss_kb, proc_depois.vsz_kb);
    
    long long delta_rss = proc_depois.rss_kb - proc_antes.rss_kb;
    long long delta_vsz = proc_depois.vsz_kb - proc_antes.vsz_kb;
    
    printf("Variação - ΔRSS: %lld kB, ΔVSZ: %lld kB\n", delta_rss, delta_vsz);
    
    // Verifica se as métricas refletem a alocação
    if (delta_rss > 0 || delta_vsz > 0) {
        printf("✅ Métricas de memória respondendo a alocações\n");
    } else {
        printf("⚠️  Métricas não mostraram variação significativa\n");
    }
    
    // Libera memória
    free(memory_block);
    printf("Memória liberada.\n");
    
    return 0;
}

/* ==================== TESTE 4: PAGE FAULTS ==================== */

static int teste_page_faults(void) {
    printf("\n=== TESTE 4: Monitoramento de Page Faults ===\n");
    
    pid_t self = getpid();
    mem_proc_stats_t proc;
    
    if (mem_ler_processo(self, &proc) != 0) {
        fprintf(stderr, "Falha ao ler estatísticas do processo\n");
        return -1;
    }
    
    printf("Page Faults atuais:\n");
    printf("  Minor faults: %llu\n", proc.minor_faults);
    printf("  Major faults: %llu\n", proc.major_faults);
    
    // Força alguns page faults acessando memória
    printf("Forçando acesso a memória para gerar page faults...\n");
    
    size_t size = 100 * 1024 * 1024; // 100 MB
    char *buffer = malloc(size);
    if (buffer) {
        // Acessa a memória para potencialmente gerar page faults
        for (size_t i = 0; i < size; i += 4096) {
            buffer[i] = (char)(i % 256);
        }
        
        // Lê novamente após acesso
        mem_proc_stats_t proc_depois;
        if (mem_ler_processo(self, &proc_depois) == 0) {
            printf("Page Faults após acesso:\n");
            printf("  Minor faults: %llu\n", proc_depois.minor_faults);
            printf("  Major faults: %llu\n", proc_depois.major_faults);
            
            unsigned long long delta_minor = proc_depois.minor_faults - proc.minor_faults;
            unsigned long long delta_major = proc_depois.major_faults - proc.major_faults;
            
            printf("Variação - ΔMinor: %llu, ΔMajor: %llu\n", delta_minor, delta_major);
        }
        
        free(buffer);
    }
    
    return 0;
}

/* ==================== FUNÇÃO PRINCIPAL ==================== */

int main(int argc, char *argv[]) {
    printf("===============================================\n");
    printf("  TESTES DO MÓDULO DE MEMÓRIA - RESOURCE MONITOR\n");
    printf("===============================================\n");
    
    int erro = 0;
    
    // Teste 1: Leitura básica (compatível com versão original)
    if (teste_leitura_basica(argc, argv) != 0) {
        fprintf(stderr, "ERRO no Teste 1 (Leitura Básica)\n");
        erro = 1;
    }
    
    // Testes adicionais apenas se não foi passado PID específico
    if (argc == 1) {
        if (teste_monitoramento_continuo() != 0) {
            fprintf(stderr, "ERRO no Teste 2 (Monitoramento Contínuo)\n");
            erro = 1;
        }
        
        if (teste_stress_memoria() != 0) {
            fprintf(stderr, "ERRO no Teste 3 (Stress de Memória)\n");
            erro = 1;
        }
        
        if (teste_page_faults() != 0) {
            fprintf(stderr, "ERRO no Teste 4 (Page Faults)\n");
            erro = 1;
        }
    }
    
    if (!erro) {
        printf("\n✅ Todos os testes de memória foram executados com sucesso!\n");
        printf("📊 Verifique 'memory_monitor_test.csv' para análise detalhada.\n");
    } else {
        printf("\n❌ Alguns testes de memória falharam.\n");
    }
    
    return erro ? EXIT_FAILURE : EXIT_SUCCESS;
}