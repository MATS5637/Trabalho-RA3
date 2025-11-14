// memory_monitor.c - VERSÃO CORRIGIDA
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <time.h>

#include "../include/monitor.h"

/* ==================== ESTADO INTERNO ==================== */

typedef struct {
    pid_t            pid;
    mem_proc_stats_t proc_antes;
    mem_sys_stats_t  sys_antes;
    int              iniciado;
} mem_monitor_state_t;

static mem_monitor_state_t mem_state = { .pid = -1, .iniciado = 0 };

/* ==================== FUNÇÕES AUXILIARES ==================== */

static void obter_timestamp_mem(char *buffer, size_t size) {
    time_t now = time(NULL);
    if (now == (time_t)-1) {
        snprintf(buffer, size, "ERRO_TIMESTAMP");
        return;
    }
    struct tm tm_info;
    localtime_r(&now, &tm_info);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", &tm_info);
}

static void dormir_ms_mem(int ms) {
    struct timespec req = {
        .tv_sec  = ms / 1000,
        .tv_nsec = (ms % 1000) * 1000000L
    };
    struct timespec rem;
    while (nanosleep(&req, &rem) == -1 && errno == EINTR) {
        req = rem;
    }
}

// Função auxiliar para ler valores do /proc/*/status
static unsigned long long ler_valor_status(const char *linha) {
    char *ptr = strchr(linha, ':');
    if (!ptr) return 0;
    
    ptr++; // Pula o ':'
    
    // Pula espaços
    while (*ptr == ' ' || *ptr == '\t') ptr++;
    
    unsigned long long valor;
    if (sscanf(ptr, "%llu", &valor) == 1) {
        return valor;
    }
    return 0;
}

/* ==================== LEITURA – PROCESSO ==================== */

int mem_ler_processo(pid_t pid, mem_proc_stats_t *out) {
    if (!out || pid <= 0) {
        fprintf(stderr, "mem_ler_processo: parâmetros inválidos\n");
        return -1;
    }

    memset(out, 0, sizeof(*out));

    /* -------- /proc/<pid>/status -------- */
    char caminho[64];
    snprintf(caminho, sizeof(caminho), "/proc/%d/status", pid);

    FILE *fp = fopen(caminho, "r");
    if (!fp) {
        return -1;
    }

    char linha[256];
    while (fgets(linha, sizeof(linha), fp)) {
        if (strncmp(linha, "VmRSS:", 6) == 0) {
            out->rss_kb = ler_valor_status(linha);
        } else if (strncmp(linha, "VmSize:", 7) == 0) {
            out->vsz_kb = ler_valor_status(linha);
        } else if (strncmp(linha, "RssFile:", 8) == 0) {
            out->shared_kb += ler_valor_status(linha);
        } else if (strncmp(linha, "RssShmem:", 9) == 0) {
            out->shared_kb += ler_valor_status(linha);
        } else if (strncmp(linha, "VmSwap:", 7) == 0) {
            out->swap_kb = ler_valor_status(linha);
        }
    }
    fclose(fp);

    /* -------- /proc/<pid>/stat: page faults -------- */
    snprintf(caminho, sizeof(caminho), "/proc/%d/stat", pid);
    fp = fopen(caminho, "r");
    if (!fp) {
        return 0; // Page faults são opcionais
    }

    char buf[1024];
    if (!fgets(buf, sizeof(buf), fp)) {
        fclose(fp);
        return 0;
    }
    fclose(fp);

    // Encontra o final do nome do processo (entre parênteses)
    char *p = strrchr(buf, ')');
    if (!p) {
        return 0;
    }
    p += 2; // Pula ") "

    // Parsing robusto: procura pelos campos minflt e majflt
    // Campos: state ppid pgrp sid tty_nr tpgid flags minflt cminflt majflt cmajflt...
    int campo = 0;
    char *token = strtok(p, " ");
    
    while (token != NULL) {
        campo++;
        
        if (campo == 10) { // minflt
            out->minor_faults += strtoull(token, NULL, 10);
        } else if (campo == 11) { // cminflt
            out->minor_faults += strtoull(token, NULL, 10);
        } else if (campo == 12) { // majflt
            out->major_faults += strtoull(token, NULL, 10);
        } else if (campo == 13) { // cmajflt
            out->major_faults += strtoull(token, NULL, 10);
            break; // Já temos o que precisamos
        }
        
        token = strtok(NULL, " ");
    }

    return 0;
}

/* ==================== LEITURA – SISTEMA ==================== */

int mem_ler_sistema(mem_sys_stats_t *out) {
    if (!out) return -1;

    memset(out, 0, sizeof(*out));

    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp) {
        perror("mem_ler_sistema: /proc/meminfo");
        return -1;
    }

    char linha[256];
    
    while (fgets(linha, sizeof(linha), fp)) {
        // Formato: "MemTotal:       16384000 kB"
        char *colon = strchr(linha, ':');
        if (!colon) continue;
        
        *colon = '\0'; // Separa chave do valor
        char *chave = linha;
        char *valor_str = colon + 1;
        
        // Pula espaços
        while (*valor_str == ' ' || *valor_str == '\t') valor_str++;
        
        unsigned long long valor = strtoull(valor_str, NULL, 10);
        
        if (strcmp(chave, "MemTotal") == 0) {
            out->mem_total_kb = valor;
        } else if (strcmp(chave, "MemFree") == 0) {
            out->mem_free_kb = valor;
        } else if (strcmp(chave, "MemAvailable") == 0) {
            out->mem_available_kb = valor;
        } else if (strcmp(chave, "Buffers") == 0) {
            out->buffers_kb = valor;
        } else if (strcmp(chave, "Cached") == 0) {
            out->cached_kb = valor;
        } else if (strcmp(chave, "SwapTotal") == 0) {
            out->swap_total_kb = valor;
        } else if (strcmp(chave, "SwapFree") == 0) {
            out->swap_free_kb = valor;
        }
    }

    fclose(fp);
    return 0;
}

/* ==================== CÁLCULOS ==================== */

double mem_calcular_percentual_uso(const mem_proc_stats_t *proc,
                                   const mem_sys_stats_t  *sys) {
    if (!proc || !sys || sys->mem_total_kb == 0) return 0.0;
    return (double)proc->rss_kb / (double)sys->mem_total_kb * 100.0;
}

/* ==================== MONITORAMENTO CONTÍNUO (CSV) ==================== */

int mem_monitorar_pid_csv(pid_t pid, int intervalo_ms, int amostras, FILE *saida) {
    if (pid <= 0 || intervalo_ms < 1 || amostras <= 0) {
        fprintf(stderr, "mem_monitorar_pid_csv: parâmetros inválidos\n");
        return -1;
    }

    if (!saida) saida = stdout;

    mem_proc_stats_t proc_stats;
    mem_sys_stats_t  sys_stats;

    // Leitura inicial para verificar se funciona
    if (mem_ler_processo(pid, &proc_stats) != 0) {
        fprintf(stderr, "mem_monitorar_pid_csv: não foi possível ler processo %d\n", pid);
        return -1;
    }
    if (mem_ler_sistema(&sys_stats) != 0) {
        fprintf(stderr, "mem_monitorar_pid_csv: não foi possível ler memória do sistema\n");
        return -1;
    }

    fprintf(saida,
        "timestamp,amostra,rss_kb,vsz_kb,shared_kb,swap_kb,"
        "minor_faults,major_faults,mem_total_kb,mem_free_kb,mem_available_kb,proc_mem_percent\n");
    fflush(saida);

    if (saida != stdout) {
        fprintf(stderr,
                "Monitorando memória do PID %d (%d amostras, intervalo: %dms)\n",
                pid, amostras, intervalo_ms);
    }

    for (int i = 0; i < amostras; i++) {
        dormir_ms_mem(intervalo_ms);

        if (!processo_existe(pid)) {
            fprintf(stderr, "Processo %d terminou durante monitoramento de memória\n", pid);
            return -1;
        }

        if (mem_ler_processo(pid, &proc_stats) != 0) {
            fprintf(stderr, "Falha ao ler processo %d (amostra %d)\n", pid, i);
            return -1;
        }
        
        if (mem_ler_sistema(&sys_stats) != 0) {
            fprintf(stderr, "Falha ao ler sistema (amostra %d)\n", i);
            // Continua mesmo com erro no sistema (processo é mais importante)
        }

        double pct = mem_calcular_percentual_uso(&proc_stats, &sys_stats);

        char ts[64];
        obter_timestamp_mem(ts, sizeof(ts));

        fprintf(saida,
                "%s,%d,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%.2f\n",
                ts, i,
                proc_stats.rss_kb,
                proc_stats.vsz_kb,
                proc_stats.shared_kb,
                proc_stats.swap_kb,
                proc_stats.minor_faults,
                proc_stats.major_faults,
                sys_stats.mem_total_kb,
                sys_stats.mem_free_kb,
                sys_stats.mem_available_kb,
                pct);
        fflush(saida);

        // Feedback progresso
        if (saida != stdout && (amostras <= 10 || (i + 1) % 10 == 0)) {
            fprintf(stderr, "Amostra %d/%d: RSS=%llu KB (%.2f%%)\n", 
                    i + 1, amostras, proc_stats.rss_kb, pct);
        }
    }

    if (saida != stdout) {
        fprintf(stderr, "Monitoramento de memória concluído: %d amostras.\n", amostras);
    }

    return 0;
}

/* ==================== RELATÓRIO SIMPLES ==================== */

int mem_gerar_relatorio(pid_t pid, FILE *saida) {
    if (pid <= 0) {
        fprintf(stderr, "mem_gerar_relatorio: PID inválido\n");
        return -1;
    }
    if (!saida) saida = stdout;

    mem_proc_stats_t proc;
    mem_sys_stats_t  sys;

    if (mem_ler_processo(pid, &proc) != 0) {
        fprintf(stderr, "mem_gerar_relatorio: não foi possível ler processo %d\n", pid);
        return -1;
    }
    if (mem_ler_sistema(&sys) != 0) {
        fprintf(stderr, "mem_gerar_relatorio: não foi possível ler memória do sistema\n");
        return -1;
    }

    double pct = mem_calcular_percentual_uso(&proc, &sys);

    fprintf(saida, "\n=== Relatório de Memória do PID %d ===\n", pid);
    fprintf(saida, "RSS:       %llu kB (%.2f MB)\n", proc.rss_kb, proc.rss_kb / 1024.0);
    fprintf(saida, "VSZ:       %llu kB (%.2f MB)\n", proc.vsz_kb, proc.vsz_kb / 1024.0);
    fprintf(saida, "Shared:    %llu kB\n", proc.shared_kb);
    fprintf(saida, "Swap:      %llu kB\n", proc.swap_kb);
    fprintf(saida, "Minor PF:  %llu\n", proc.minor_faults);
    fprintf(saida, "Major PF:  %llu\n", proc.major_faults);
    fprintf(saida, "Uso:       %.2f%% da memória física\n", pct);
    fprintf(saida, "--- Sistema ---\n");
    fprintf(saida, "MemTotal:  %llu kB (%.2f GB)\n", sys.mem_total_kb, sys.mem_total_kb / (1024.0 * 1024.0));
    fprintf(saida, "MemFree:   %llu kB\n", sys.mem_free_kb);
    fprintf(saida, "MemAvail:  %llu kB\n", sys.mem_available_kb);
    fprintf(saida, "=====================================\n");

    return 0;
}