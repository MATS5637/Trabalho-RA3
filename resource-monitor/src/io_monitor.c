// io_monitor.c - versão completa e corrigida
#define _POSIX_C_SOURCE 200809L  // para nanosleep/localtime_r

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>
#include "../include/monitor.h"  // precisa declarar io_stats_t e os protótipos aqui

/* ==================== ESTADO INTERNO ==================== */

typedef struct {
    pid_t      pid;
    io_stats_t stats_antes;
    int        iniciado;
} io_monitor_state_t;

static io_monitor_state_t io_state = { .pid = -1, .iniciado = 0 };

/* ==================== FUNÇÕES AUXILIARES ==================== */

static void obter_timestamp_io(char *buffer, size_t size) {
    time_t now = time(NULL);
    if (now == (time_t)-1) {
        snprintf(buffer, size, "ERRO_TIMESTAMP");
        return;
    }
    struct tm tm_info;
    localtime_r(&now, &tm_info);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", &tm_info);
}

// sleep em ms com nanosleep (mais robusto que usleep)
static void dormir_ms_io(int ms) {
    struct timespec req = {
        .tv_sec  = ms / 1000,
        .tv_nsec = (long)(ms % 1000) * 1000000L
    };
    struct timespec rem;

    while (nanosleep(&req, &rem) == -1 && errno == EINTR) {
        req = rem;
    }
}

/* ==================== LEITURA DE I/O DO PROCESSO ==================== */

int io_ler_stats_processo(pid_t pid, io_stats_t *stats) {
    if (!stats || pid <= 0) {
        fprintf(stderr, "Erro: parâmetros inválidos em io_ler_stats_processo\n");
        return -1;
    }

    memset(stats, 0, sizeof(io_stats_t));

    // /proc/<pid>/io → estatísticas de I/O do processo
    char caminho[64];
    snprintf(caminho, sizeof(caminho), "/proc/%d/io", pid);

    FILE *fp = fopen(caminho, "r");
    if (!fp) {
        // pode ser falta de permissão ou processo já terminou
        return -1;
    }

    char linha[256];
    while (fgets(linha, sizeof(linha), fp)) {
        if (strncmp(linha, "rchar:", 6) == 0) {
            sscanf(linha + 6, "%llu", &stats->read_bytes);
        } else if (strncmp(linha, "wchar:", 6) == 0) {
            sscanf(linha + 6, "%llu", &stats->write_bytes);
        } else if (strncmp(linha, "syscr:", 6) == 0) {
            sscanf(linha + 6, "%llu", &stats->read_syscalls);
        } else if (strncmp(linha, "syscw:", 6) == 0) {
            sscanf(linha + 6, "%llu", &stats->write_syscalls);
        }
    }
    fclose(fp);

    // Tenta estimar "operações de disco" usando major faults em /proc/<pid>/stat
    snprintf(caminho, sizeof(caminho), "/proc/%d/stat", pid);
    fp = fopen(caminho, "r");
    if (fp) {
        char buffer[1024];
        if (fgets(buffer, sizeof(buffer), fp)) {
            // nome do processo vai até o último ')'
            char *p = strrchr(buffer, ')');
            if (p) {
                p += 2; // pula ") "

                // Depois do nome temos:
                // state(1) ppid(2) pgid(3) sid(4) tty_nr(5) tty_pgrp(6)
                // flags(7) minflt(8) cminflt(9) majflt(10) cmajflt(11) ...
                // Queremos o majflt → pular 9 espaços e ler o próximo número
                for (int i = 0; i < 9; i++) {
                    p = strchr(p, ' ');
                    if (!p) break;
                    p++;
                }
                if (p) {
                    unsigned long majflt = 0;
                    if (sscanf(p, "%lu", &majflt) == 1) {
                        stats->disk_operations = (unsigned long long)majflt;
                    }
                }
            }
        }
        fclose(fp);
    }

    return 0;
}

/* ==================== CÁLCULO DE TAXAS ==================== */

int io_calcular_taxas(const io_stats_t *antes, const io_stats_t *depois,
                      int intervalo_ms, io_stats_t *taxas) {
    if (!antes || !depois || !taxas || intervalo_ms <= 0) {
        fprintf(stderr, "Erro: parâmetros inválidos em io_calcular_taxas\n");
        return -1;
    }

    double segundos = intervalo_ms / 1000.0;

    unsigned long long d_read_bytes = 0, d_write_bytes = 0;
    unsigned long long d_read_sys   = 0, d_write_sys   = 0;
    unsigned long long d_disk_ops   = 0;

    if (depois->read_bytes      >= antes->read_bytes)
        d_read_bytes  = depois->read_bytes      - antes->read_bytes;
    if (depois->write_bytes     >= antes->write_bytes)
        d_write_bytes = depois->write_bytes     - antes->write_bytes;
    if (depois->read_syscalls   >= antes->read_syscalls)
        d_read_sys    = depois->read_syscalls   - antes->read_syscalls;
    if (depois->write_syscalls  >= antes->write_syscalls)
        d_write_sys   = depois->write_syscalls  - antes->write_syscalls;
    if (depois->disk_operations >= antes->disk_operations)
        d_disk_ops    = depois->disk_operations - antes->disk_operations;

    taxas->read_bytes      = (unsigned long long)(d_read_bytes  / segundos);
    taxas->write_bytes     = (unsigned long long)(d_write_bytes / segundos);
    taxas->read_syscalls   = (unsigned long long)(d_read_sys    / segundos);
    taxas->write_syscalls  = (unsigned long long)(d_write_sys   / segundos);
    taxas->disk_operations = (unsigned long long)(d_disk_ops    / segundos);

    return 0;
}

/* ==================== MONITORAMENTO CONTÍNUO (CSV) ==================== */

int io_monitorar_pid_csv(pid_t pid, int intervalo_ms, int amostras, FILE *saida) {
    if (pid <= 0 || intervalo_ms < 1 || amostras <= 0) {
        fprintf(stderr, "Erro: parâmetros inválidos em io_monitorar_pid_csv\n");
        return -1;
    }

    if (!saida) saida = stdout;

    io_stats_t stats_antes, stats_depois, taxas;

    // leitura inicial
    if (io_ler_stats_processo(pid, &stats_antes) != 0) {
        fprintf(stderr, "Erro: não foi possível ler stats de I/O do processo %d\n", pid);
        return -1;
    }

    // cabeçalho CSV
    fprintf(saida,
            "timestamp,amostra,read_bps,write_bps,read_syscalls_ps,write_syscalls_ps,disk_ops_ps\n");
    fflush(saida);

    if (saida != stdout) {
        fprintf(stderr, "Monitorando I/O do PID %d (%d amostras, intervalo: %dms)\n",
                pid, amostras, intervalo_ms);
    }

    for (int i = 0; i < amostras; i++) {
        dormir_ms_io(intervalo_ms);

        if (io_ler_stats_processo(pid, &stats_depois) != 0) {
            fprintf(stderr, "Processo %d terminou durante monitoramento de I/O\n", pid);
            return -1;
        }

        if (io_calcular_taxas(&stats_antes, &stats_depois, intervalo_ms, &taxas) != 0) {
            fprintf(stderr, "Erro ao calcular taxas de I/O\n");
            return -1;
        }

        char ts[64];
        obter_timestamp_io(ts, sizeof(ts));
        fprintf(saida, "%s,%d,%llu,%llu,%llu,%llu,%llu\n",
                ts, i,
                taxas.read_bytes, taxas.write_bytes,
                taxas.read_syscalls, taxas.write_syscalls,
                taxas.disk_operations);
        fflush(saida);

        stats_antes = stats_depois;

        if (saida != stdout && (amostras <= 10 || (i + 1) % 10 == 0)) {
            fprintf(stderr,
                    "Amostra I/O %d/%d: read=%.2f KB/s, write=%.2f KB/s\n",
                    i + 1, amostras,
                    taxas.read_bytes  / 1024.0,
                    taxas.write_bytes / 1024.0);
        }
    }

    if (saida != stdout) {
        fprintf(stderr, "Monitoramento de I/O concluído: %d amostras coletadas\n", amostras);
    }

    return 0;
}

/* ==================== USO INSTANTÂNEO ==================== */

int io_obter_uso_instantaneo(pid_t pid, io_stats_t *taxas) {
    if (pid <= 0 || !taxas) {
        fprintf(stderr, "Erro: parâmetros inválidos em io_obter_uso_instantaneo\n");
        return -1;
    }

    io_stats_t stats_depois;

    // primeira vez ou PID mudou → inicializa base
    if (!io_state.iniciado || io_state.pid != pid) {
        if (io_ler_stats_processo(pid, &io_state.stats_antes) != 0) {
            fprintf(stderr, "Erro na leitura inicial de I/O para PID %d\n", pid);
            return -1;
        }
        io_state.pid      = pid;
        io_state.iniciado = 1;

        // pequeno intervalo para formar delta
        dormir_ms_io(100); // 100 ms
    }

    if (io_ler_stats_processo(pid, &stats_depois) != 0) {
        fprintf(stderr, "Erro ao ler stats de I/O atuais para PID %d\n", pid);
        return -1;
    }

    // aproxima: considera intervalo de 1000 ms para taxas "por segundo"
    if (io_calcular_taxas(&io_state.stats_antes, &stats_depois, 1000, taxas) != 0) {
        return -1;
    }

    io_state.stats_antes = stats_depois;
    return 0;
}

/* ==================== I/O DO SISTEMA (/proc/diskstats) ==================== */

int io_ler_stats_sistema(io_stats_t *stats) {
    if (!stats) return -1;

    memset(stats, 0, sizeof(io_stats_t));

    FILE *fp = fopen("/proc/diskstats", "r");
    if (!fp) {
        perror("Erro ao abrir /proc/diskstats");
        return -1;
    }

    char linha[512];
    while (fgets(linha, sizeof(linha), fp)) {
        unsigned int major, minor;
        char dev_name[32];
        unsigned long reads, writes, sectors_read, sectors_written;

        int lidos = sscanf(linha,
                           "%u %u %31s %lu %*u %lu %*u %lu %*u %lu",
                           &major, &minor, dev_name,
                           &reads, &sectors_read, &writes, &sectors_written);
        if (lidos < 7)
            continue;

        // filtra alguns dispositivos que não são discos "reais"
        if (strncmp(dev_name, "loop", 4) == 0 ||
            strncmp(dev_name, "dm-",  3) == 0 ||
            strncmp(dev_name, "zram", 4) == 0 ||
            strncmp(dev_name, "ram",  3) == 0) {
            continue;
        }

        stats->read_bytes      += (unsigned long long)sectors_read   * 512ULL;
        stats->write_bytes     += (unsigned long long)sectors_written * 512ULL;
        stats->read_syscalls   += (unsigned long long)reads;
        stats->write_syscalls  += (unsigned long long)writes;
        // não temos "disk_operations" globais aqui → deixamos 0 ou somar reads+writes se quiser
    }

    fclose(fp);
    return 0;
}

/* ==================== GERADOR DE CARGA DE I/O ==================== */

int io_criar_workload(const char *arquivo, size_t tamanho_mb, int operacoes) {
    if (!arquivo || tamanho_mb == 0 || operacoes <= 0) {
        fprintf(stderr, "Parâmetros inválidos em io_criar_workload\n");
        return -1;
    }

    printf("Criando workload I/O: %s (%zu MB, %d operações)\n",
           arquivo, tamanho_mb, operacoes);

    FILE *fp = fopen(arquivo, "w");
    if (!fp) {
        perror("Erro ao criar arquivo de teste");
        return -1;
    }

    size_t bloco = 1024 * 1024; // 1 MB
    char *buffer = (char *)malloc(bloco);
    if (!buffer) {
        fclose(fp);
        fprintf(stderr, "Erro ao alocar buffer de I/O\n");
        return -1;
    }
    memset(buffer, 'X', bloco);

    int max_ops = (int)tamanho_mb;
    if (operacoes < max_ops) max_ops = operacoes;

    for (int i = 0; i < max_ops; i++) {
        size_t escritos = fwrite(buffer, 1, bloco, fp);
        if (escritos != bloco) {
            fprintf(stderr, "Erro na escrita %d (escreveu %zu bytes)\n", i, escritos);
            break;
        }
        fflush(fp); // força escrita
    }

    free(buffer);
    fclose(fp);

    // Lê o arquivo de volta para gerar leitura
    fp = fopen(arquivo, "r");
    if (fp) {
        char read_buf[4096];
        while (fread(read_buf, 1, sizeof(read_buf), fp) > 0) {
            // só consome os dados
        }
        fclose(fp);
    }

    printf("Workload I/O concluído\n");
    return 0;
}

/* ==================== UTILITÁRIOS ==================== */

void io_resetar_estado(void) {
    io_state.pid      = -1;
    io_state.iniciado = 0;
    memset(&io_state.stats_antes, 0, sizeof(io_stats_t));
    fprintf(stderr, "Estado do monitor de I/O resetado\n");
}

void io_imprimir_stats(const io_stats_t *stats, const char *rotulo) {
    if (!stats) return;

    printf("\n=== Estatísticas de I/O %s===\n", rotulo ? rotulo : "");
    printf("Bytes lidos:    %llu (%.2f KB, %.2f MB)\n",
           stats->read_bytes,
           stats->read_bytes / 1024.0,
           stats->read_bytes / (1024.0 * 1024.0));
    printf("Bytes escritos: %llu (%.2f KB, %.2f MB)\n",
           stats->write_bytes,
           stats->write_bytes / 1024.0,
           stats->write_bytes / (1024.0 * 1024.0));
    printf("Syscalls leitura: %llu\n",  stats->read_syscalls);
    printf("Syscalls escrita: %llu\n",  stats->write_syscalls);
    printf("Operações de disco (aprox.): %llu\n", stats->disk_operations);
    printf("====================================\n");
}