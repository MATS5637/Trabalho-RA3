#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include "monitor.h"

int cpu_ler_times_sistema(cpu_times_t *out){
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) return -1;
    char buffer[256];
    if (!fgets(buffer, sizeof(buffer), fp)) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    cpu_times_t tmp = {0};
    int lidos = sscanf(buffer, "cpu %llu %llu %llu %llu", &tmp.user, &tmp.nice, &tmp.system, &tmp.idle);
    if (lidos < 4) return -1;
    tmp.total = tmp.user + tmp.nice + tmp.system + tmp.idle;
    tmp.active = tmp.user + tmp.nice + tmp.system;
    *out = tmp;
    return 0;
}

double cpu_calculo_percentual(const cpu_times_t *antes, const cpu_times_t *depois){
    unsigned long long delta_total = (depois->total > antes->total) ? (depois->total - antes->total) : 0;
    unsigned long long delta_active = (depois->active > antes->active) ? (depois->active - antes->active) : 0;
    if (delta_total == 0) return 0.0;
    return (double)delta_active / (double)delta_total * 100.0;
}

int cpu_ler_processo(pid_t pid, proc_cpu_t *out){
    char caminho[64];
    snprintf(caminho, sizeof(caminho), "/proc/%d/stat", pid);
    FILE *fp = fopen(caminho, "r");
    if (!fp) return -1;
    char linha[512];
    if (!fgets(linha, sizeof(linha), fp)) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    char *p =strrchr(linha, ')');
    if (!p) return -1;
    p++;
    unsigned long utime, stime;
    int lidos = sscanf(p, " %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu", &utime, &stime);
    if (lidos !=2) return -1;
    out->utime = utime;
    out->stime = stime;
    out->total_time = utime + stime;
    return 0;
}

double cpu_calculo_percentual_processo(const proc_cpu_t *antes, const proc_cpu_t *depois, const cpu_times_t *sys_antes, const cpu_times_t *sys_depois){
    unsigned long long delta_proc = (depois->total_time > antes->total_time) ? (depois->total_time - antes->total_time) : 0;
    unsigned long long delta_sys = (sys_depois->total > sys_antes->total) ? (sys_depois->total - sys_antes->total) : 0;
    if (delta_sys == 0) return 0.0;
    return (double)delta_proc / (double)delta_sys * 100.0;
}

int cpu_monitorar_pid_csv(pid_t pid, int intervalo_ms, int amostras, FILE *saida){
    if (!saida) saida = stdout;
    cpu_times_t sys_antes, sys_depois;
    proc_cpu_t proc_antes, proc_depois;
    if (cpu_ler_times_sistema(&sys_antes) != 0) return -1;
    if (cpu_ler_processo(pid, &proc_antes) != 0) return -1;
    fprintf(saida, "timestamp, cpu_processo, cpu_sistema\n");
    for (int i =0; i<amostras; i++){
        usleep(intervalo_ms * 1000);
        if (cpu_ler_times_sistema(&sys_depois) !=0)return -1;
        if (cpu_ler_processo(pid, &proc_depois) !=0)return -1;
        double cpu_sistema = cpu_calculo_percentual(&sys_antes, &sys_depois);
        double cpu_processo = cpu_calculo_percentual_processo(&proc_antes, &proc_depois, &sys_antes, &sys_depois);
        fprintf(saida, "%d,%.2f,%.2f\n", i, cpu_processo, cpu_sistema);
        sys_antes = sys_depois;
        proc_antes = proc_depois;
    }
    return 0;
}