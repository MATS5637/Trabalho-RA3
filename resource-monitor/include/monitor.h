#ifndef MONITOR_H
#define MONITOR_H

#include <stdio.h>
#include <sys/types.h>  // pid_t

/* -------------------- Estruturas de dados -------------------- */

/* Tempos agregados do sistema (linha "cpu" do /proc/stat) */
typedef struct {
    unsigned long long user;
    unsigned long long nice;
    unsigned long long system;
    unsigned long long idle;
    unsigned long long iowait;
    unsigned long long irq;
    unsigned long long softirq;
    unsigned long long steal;
    unsigned long long total;
    unsigned long long active;
} cpu_times_t;

/* Tempos de CPU de um processo (linha /proc/<pid>/stat) */
typedef struct {
    unsigned long long utime;
    unsigned long long stime;
    unsigned long long total_time;
} proc_cpu_t;

/* -------------------- API de monitoramento de CPU -------------------- */

/* Leitura da linha "cpu" de /proc/stat */
int    cpu_ler_times_sistema(cpu_times_t *out);

/* Percentual de uso de CPU do sistema entre duas leituras */
double cpu_calculo_percentual(const cpu_times_t *antes, const cpu_times_t *depois);

/* Leitura de tempos de CPU de um processo específico (PID) */
int    cpu_ler_processo(pid_t pid, proc_cpu_t *out);

/* Percentual de CPU de um processo em relação ao total da máquina */
double cpu_calculo_percentual_processo(const proc_cpu_t *antes, const proc_cpu_t *depois,
                                       const cpu_times_t *sys_antes, const cpu_times_t *sys_depois);

/* Loop de monitoramento que grava CSV (timestamp, amostra, cpu_proc, cpu_sys) */
int    cpu_monitorar_pid_csv(pid_t pid, int intervalo_ms, int amostras, FILE *saida);

/* Uso "instantâneo" de CPU de um processo (mantém estado interno por PID) */
double cpu_obter_uso_instantaneo(pid_t pid);

/* Zera o estado interno usado por cpu_obter_uso_instantaneo */
void   cpu_resetar_estado(void);

/* Verifica se um processo ainda existe (checa /proc/<pid>) */
int    processo_existe(pid_t pid);

#endif /* MONITOR_H */
