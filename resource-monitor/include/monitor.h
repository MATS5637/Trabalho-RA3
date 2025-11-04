#ifndef MONITOR_H
#define MONITOR_H

#include <stdio.h>
#include <sys/types.h>

typedef struct {
    unsigned long long user;
    unsigned long long nice;
    unsigned long long system;
    unsigned long long idle;
    unsigned long long total;
    unsigned long long active;
} cpu_times_t;

typedef struct {
    unsigned long utime;
    unsigned long stime;
    unsigned long total_time;
} proc_cpu_t;

int cpu_ler_times_sistema(cpu_times_t *out);
double cpu_calculo_percentual(const cpu_times_t *antes, const cpu_times_t *depois);
int cpu_ler_processo(pid_t pid, proc_cpu_t *out);
double cpu_calculo_percentual_processo(const proc_cpu_t *antes, const proc_cpu_t *depois, const cpu_times_t *sys_antes, const cpu_times_t *sys_depois);
int cpu_monitorar_pid_csv(pid_t pid, int intervalo_ms, int amostras, FILE *saida);
#endif