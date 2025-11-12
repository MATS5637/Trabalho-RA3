// cpu_monitor.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <errno.h>
#include "monitor.h"

/*
  Assumindo em monitor.h:

  typedef struct {
      unsigned long long user, nice, system, idle;
      unsigned long long iowait, irq, softirq, steal;
      unsigned long long total, active;
  } cpu_times_t;

  typedef struct {
      unsigned long long utime, stime;
      unsigned long long total_time;
  } proc_cpu_t;

  Assinaturas públicas usadas aqui:
  int    cpu_ler_times_sistema(cpu_times_t *out);
  double cpu_calculo_percentual(const cpu_times_t *antes, const cpu_times_t *depois);
  int    cpu_ler_processo(pid_t pid, proc_cpu_t *out);
  double cpu_calculo_percentual_processo(const proc_cpu_t *antes, const proc_cpu_t *depois,
                                         const cpu_times_t *sys_antes, const cpu_times_t *sys_depois);
  int    cpu_monitorar_pid_csv(pid_t pid, int intervalo_ms, int amostras, FILE *saida);
  double cpu_obter_uso_instantaneo(pid_t pid);
  void   cpu_resetar_estado(void);
  int    processo_existe(pid_t pid);
*/

/* -------------------- Estado para uso instantâneo -------------------- */
typedef struct {
    pid_t pid;
    cpu_times_t sys_antes;
    proc_cpu_t  proc_antes;
    int iniciado;
} cpu_monitor_state_t;

static cpu_monitor_state_t monitor_state = { .pid = -1, .iniciado = 0 };

/* -------------------- Utilitários -------------------- */

/* Timestamp legível (thread-safe) */
static void obter_timestamp(char *buffer, size_t size) {
    time_t now = time(NULL);
    if (now == (time_t)-1) {
        snprintf(buffer, size, "ERRO_TIMESTAMP");
        return;
    }
    struct tm tm_info;
    localtime_r(&now, &tm_info);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", &tm_info);
}

/* Sleep com retry em EINTR */
static void dormir_ms(int intervalo_ms) {
    struct timespec req = {
        .tv_sec  = intervalo_ms / 1000,
        .tv_nsec = (long)(intervalo_ms % 1000) * 1000000L
    };
    struct timespec rem = req;
    while (nanosleep(&rem, &rem) == -1 && errno == EINTR) {
        /* retry até completar */
    }
}

/* -------------------- Leitura de tempos do sistema -------------------- */

int cpu_ler_times_sistema(cpu_times_t *out) {
    if (!out) {
        fprintf(stderr, "Erro: ponteiro nulo em cpu_ler_times_sistema\n");
        return -1;
    }

    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) {
        fprintf(stderr, "Erro ao abrir /proc/stat: %s\n", strerror(errno));
        return -1;
    }

    char buffer[512];
    if (!fgets(buffer, sizeof(buffer), fp)) {
        fprintf(stderr, "Erro ao ler /proc/stat\n");
        fclose(fp);
        return -1;
    }
    fclose(fp);

    /* Ler até 10 campos (guest/guest_nice ignorados no cálculo) */
    unsigned long long user=0, nice=0, system=0, idle=0;
    unsigned long long iowait=0, irq=0, softirq=0, steal=0, guest=0, guest_nice=0;

    int n = sscanf(buffer,
                   "cpu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
                   &user, &nice, &system, &idle,
                   &iowait, &irq, &softirq, &steal,
                   &guest, &guest_nice);

    if (n < 4) {
        fprintf(stderr, "Formato inesperado em /proc/stat (lidos=%d)\n", n);
        return -1;
    }

    cpu_times_t t = {0};
    t.user    = user;
    t.nice    = nice;
    t.system  = system;
    t.idle    = idle;
    t.iowait  = (n > 4) ? iowait  : 0;
    t.irq     = (n > 5) ? irq     : 0;
    t.softirq = (n > 6) ? softirq : 0;
    t.steal   = (n > 7) ? steal   : 0;

    /* total = todas as colunas principais */
    t.total  = t.user + t.nice + t.system + t.idle + t.iowait + t.irq + t.softirq + t.steal;
    /* active = total - (idle + iowait)  → iowait conta como “inativo” */
    t.active = t.total - (t.idle + t.iowait);

    *out = t;
    return 0;
}

double cpu_calculo_percentual(const cpu_times_t *antes, const cpu_times_t *depois) {
    if (!antes || !depois) {
        fprintf(stderr, "Erro: ponteiro nulo em cpu_calculo_percentual\n");
        return 0.0;
    }

    unsigned long long delta_total  = (depois->total  > antes->total)  ? (depois->total  - antes->total)  : 0ULL;
    unsigned long long delta_active = (depois->active > antes->active) ? (depois->active - antes->active) : 0ULL;

    if (delta_total == 0ULL) return 0.0;
    return (double)delta_active / (double)delta_total * 100.0;
}

/* -------------------- Leitura de tempos do processo -------------------- */

int cpu_ler_processo(pid_t pid, proc_cpu_t *out) {
    if (!out) {
        fprintf(stderr, "Erro: ponteiro nulo em cpu_ler_processo\n");
        return -1;
    }
    if (pid <= 0) {
        fprintf(stderr, "Erro: PID inválido: %d\n", pid);
        return -1;
    }

    char caminho[64];
    snprintf(caminho, sizeof(caminho), "/proc/%d/stat", pid);

    FILE *fp = fopen(caminho, "r");
    if (!fp) {
        fprintf(stderr, "Erro ao abrir %s: %s\n", caminho, strerror(errno));
        return -1;
    }

    char linha[1024];
    if (!fgets(linha, sizeof(linha), fp)) {
        fprintf(stderr, "Erro ao ler %s\n", caminho);
        fclose(fp);
        return -1;
    }
    fclose(fp);

    /* /proc/<pid>/stat: o nome do processo vai até o último ')' */
    char *p = strrchr(linha, ')');
    if (!p) {
        fprintf(stderr, "Formato inesperado em %s (não encontrou ')')\n", caminho);
        return -1;
    }
    p++; /* avança para depois do ')' */

    /* Campos 14–17: utime, stime, cutime, cstime (usar 64-bit) */
    unsigned long long utime=0, stime=0, cutime=0, cstime=0;
    int lidos = sscanf(p,
        " %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %llu %llu %llu %llu",
        &utime, &stime, &cutime, &cstime);

    if (lidos < 2) {
        fprintf(stderr, "Não foi possível ler tempos do processo %d (lidos=%d)\n", pid, lidos);
        return -1;
    }

    out->utime      = utime;
    out->stime      = stime;
    out->total_time = utime + stime + cutime + cstime; /* se não lidos, ficam 0 */
    return 0;
}

double cpu_calculo_percentual_processo(const proc_cpu_t *antes, const proc_cpu_t *depois,
                                       const cpu_times_t *sys_antes, const cpu_times_t *sys_depois) {
    if (!antes || !depois || !sys_antes || !sys_depois) {
        fprintf(stderr, "Erro: ponteiro nulo em cpu_calculo_percentual_processo\n");
        return 0.0;
    }

    unsigned long long delta_proc = (depois->total_time > antes->total_time)
                                    ? (depois->total_time - antes->total_time) : 0ULL;
    unsigned long long delta_sys  = (sys_depois->total   > sys_antes->total)
                                    ? (sys_depois->total - sys_antes->total)   : 0ULL;

    if (delta_sys == 0ULL) return 0.0;

    /* % da capacidade total da máquina (máximo ~100%). */
    double pct = ((double)delta_proc / (double)delta_sys) * 100.0;

    /* Para estilo “top” (pode ultrapassar 100% em multicore), descomente:
       long ncpu = sysconf(_SC_NPROCESSORS_ONLN);
       if (ncpu < 1) ncpu = 1;
       pct *= (double)ncpu;
    */
    return pct;
}

/* -------------------- Loop de monitoramento CSV -------------------- */

int cpu_monitorar_pid_csv(pid_t pid, int intervalo_ms, int amostras, FILE *saida) {
    if (pid <= 0) {
        fprintf(stderr, "PID inválido: %d\n", pid);
        return -1;
    }
    if (amostras <= 0) {
        fprintf(stderr, "Número de amostras inválido: %d\n", amostras);
        return -1;
    }
    if (intervalo_ms < 1) {
        fprintf(stderr, "Intervalo inválido (ms): %d\n", intervalo_ms);
        return -1;
    }

    if (!saida) saida = stdout;

    cpu_times_t sys_antes, sys_depois;
    proc_cpu_t  proc_antes, proc_depois;

    if (cpu_ler_times_sistema(&sys_antes) != 0) {
        fprintf(stderr, "Falha na leitura inicial do sistema\n");
        return -1;
    }
    if (cpu_ler_processo(pid, &proc_antes) != 0) {
        fprintf(stderr, "Processo %d não encontrado ou sem permissão\n", pid);
        return -1;
    }

    /* Cabeçalho do CSV */
    fprintf(saida, "timestamp,amostra,cpu_processo_percent,cpu_sistema_percent\n");
    fflush(saida);

    if (saida != stdout) {
        fprintf(stderr, "Iniciando monitoramento do PID %d (%d amostras, intervalo: %dms)\n",
                pid, amostras, intervalo_ms);
    }

    for (int i = 0; i < amostras; i++) {
        dormir_ms(intervalo_ms);

        if (cpu_ler_times_sistema(&sys_depois) != 0) {
            fprintf(stderr, "Falha ao ler /proc/stat na amostra %d\n", i);
            return -1;
        }
        if (cpu_ler_processo(pid, &proc_depois) != 0) {
            fprintf(stderr, "Processo %d terminou durante o monitoramento (amostra %d)\n", pid, i);
            return -1;
        }

        double cpu_sistema  = cpu_calculo_percentual(&sys_antes, &sys_depois);
        double cpu_processo = cpu_calculo_percentual_processo(&proc_antes, &proc_depois,
                                                              &sys_antes, &sys_depois);

        char ts[64];
        obter_timestamp(ts, sizeof(ts));
        fprintf(saida, "%s,%d,%.2f,%.2f\n", ts, i, cpu_processo, cpu_sistema);
        fflush(saida);

        /* Atualiza bases para próxima iteração */
        sys_antes  = sys_depois;
        proc_antes = proc_depois;

        /* Feedback (não poluir CSV) */
        if (saida != stdout && (amostras <= 10 || (i + 1) % 10 == 0)) {
            fprintf(stderr, "Amostra %d/%d: processo=%.2f%%, sistema=%.2f%%\n",
                    i + 1, amostras, cpu_processo, cpu_sistema);
        }
    }

    if (saida != stdout) {
        fprintf(stderr, "Monitoramento de CPU concluído: %d amostras coletadas\n", amostras);
    }
    return 0;
}

/* -------------------- Uso instantâneo -------------------- */

double cpu_obter_uso_instantaneo(pid_t pid) {
    if (pid <= 0) {
        fprintf(stderr, "PID inválido para uso instantâneo: %d\n", pid);
        return -1.0;
    }

    cpu_times_t sys_depois;
    proc_cpu_t  proc_depois;

    /* Se mudou de PID ou primeira vez, inicializa estado e forma delta */
    if (!monitor_state.iniciado || monitor_state.pid != pid) {
        if (cpu_ler_times_sistema(&monitor_state.sys_antes) != 0) {
            fprintf(stderr, "Falha na leitura inicial do sistema para uso instantâneo\n");
            return -1.0;
        }
        if (cpu_ler_processo(pid, &monitor_state.proc_antes) != 0) {
            fprintf(stderr, "Falha na leitura inicial do processo %d para uso instantâneo\n", pid);
            return -1.0;
        }
        monitor_state.pid = pid;
        monitor_state.iniciado = 1;

        /* pequena espera para formar delta */
        dormir_ms(100);
        fprintf(stderr, "Estado do monitor inicializado para PID %d\n", pid);
    }

    if (cpu_ler_times_sistema(&sys_depois) != 0) {
        fprintf(stderr, "Falha na leitura do sistema para uso instantâneo\n");
        return -1.0;
    }
    if (cpu_ler_processo(pid, &proc_depois) != 0) {
        fprintf(stderr, "Processo %d não encontrado para uso instantâneo\n", pid);
        return -1.0;
    }

    double uso = cpu_calculo_percentual_processo(&monitor_state.proc_antes, &proc_depois,
                                                 &monitor_state.sys_antes, &sys_depois);

    /* Atualiza estado */
    monitor_state.sys_antes  = sys_depois;
    monitor_state.proc_antes = proc_depois;

    return uso;
}

void cpu_resetar_estado(void) {
    monitor_state.pid = -1;
    monitor_state.iniciado = 0;
    memset(&monitor_state.sys_antes, 0, sizeof(cpu_times_t));
    memset(&monitor_state.proc_antes, 0, sizeof(proc_cpu_t));
    fprintf(stderr, "Estado do monitor de CPU resetado\n");
}

/* -------------------- Utilitário de existência de processo -------------------- */

int processo_existe(pid_t pid) {
    if (pid <= 0) return 0;
    char caminho[64];
    snprintf(caminho, sizeof(caminho), "/proc/%d", pid);
    return access(caminho, F_OK) == 0;
}
