#ifndef CGROUP_H
#define CGROUP_H

#include <sys/types.h>

/* Estruturas para métricas de cgroup */
typedef struct {
    char name[64];
    unsigned long long cpu_usage;
    unsigned long long memory_usage;
    unsigned long long memory_limit;
    unsigned long long io_read_bytes;
    unsigned long long io_write_bytes;
    unsigned long memory_failcnt;
} cgroup_metrics_t;

/* API do Control Group Manager */

// Detecção e informações
int cgroup_detectar_versao(void);
int cgroup_listar_ativos(void);
// Leitura de métricas
int cgroup_ler_cpu_usage(const char *cgroup_name, unsigned long long *cpu_usage);
int cgroup_ler_memory_usage(const char *cgroup_name, unsigned long long *memory_usage);
int cgroup_ler_io_stats(const char *cgroup_name, unsigned long long *read_bytes, unsigned long long *write_bytes);
int cgroup_ler_metricas_completas(const char *cgroup_name, cgroup_metrics_t *metrics);

// Gerenciamento de cgroups
int cgroup_criar(const char *cgroup_name, double cpu_limit_cores, unsigned long memory_limit_mb);
int cgroup_adicionar_processo(const char *cgroup_name, pid_t pid);
int cgroup_remover(const char *cgroup_name);

// Relatórios
int cgroup_relatorio_usage(const char *cgroup_name);

#endif /* CGROUP_H */