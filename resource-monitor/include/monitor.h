#ifndef MONITOR_H
#define MONITOR_H

#include <stdio.h>
#include <sys/types.h> // pid_t

/* ==================== ESTRUTURAS DE DADOS ==================== */

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

/* Estatísticas de I/O de processo/sistema */
typedef struct {
unsigned long long read_bytes;
unsigned long long write_bytes;
unsigned long long read_syscalls;
unsigned long long write_syscalls;
unsigned long long disk_operations;
} io_stats_t;

/* Estatísticas de memória de processo */
typedef struct {
unsigned long long rss_kb; // Resident Set Size
unsigned long long vsz_kb; // Virtual Memory Size
unsigned long long shared_kb; // Memória compartilhada
unsigned long long swap_kb; // Memória em swap
unsigned long long minor_faults; // Page faults menores
unsigned long long major_faults; // Page faults maiores
} mem_proc_stats_t;

/* Estatísticas de memória do sistema */
typedef struct {
unsigned long long mem_total_kb;
unsigned long long mem_free_kb;
unsigned long long mem_available_kb;
unsigned long long buffers_kb;
unsigned long long cached_kb;
unsigned long long swap_total_kb;
unsigned long long swap_free_kb;
} mem_sys_stats_t;

/* ==================== API DE MONITORAMENTO DE CPU ==================== */

/* Leitura da linha "cpu" de /proc/stat */
int cpu_ler_times_sistema(cpu_times_t *out);

/* Percentual de uso de CPU do sistema entre duas leituras */
double cpu_calculo_percentual(const cpu_times_t *antes, const cpu_times_t *depois);

/* Leitura de tempos de CPU de um processo específico (PID) */
int cpu_ler_processo(pid_t pid, proc_cpu_t *out);

/* Percentual de CPU de um processo em relação ao total da máquina */
double cpu_calculo_percentual_processo(const proc_cpu_t *antes, const proc_cpu_t *depois,
const cpu_times_t *sys_antes, const cpu_times_t *sys_depois);

/* Loop de monitoramento que grava CSV (timestamp, amostra, cpu_proc, cpu_sys) */
int cpu_monitorar_pid_csv(pid_t pid, int intervalo_ms, int amostras, FILE *saida);

/* Uso "instantâneo" de CPU de um processo (mantém estado interno por PID) */
double cpu_obter_uso_instantaneo(pid_t pid);

/* Zera o estado interno usado por cpu_obter_uso_instantaneo */
void cpu_resetar_estado(void);

/* Relatório formatado de CPU (modo painel) */
int cpu_gerar_relatorio(pid_t pid, FILE *out);

/* ==================== API DE MONITORAMENTO DE I/O ==================== */

/* Leitura de estatísticas de I/O de um processo */
int io_ler_stats_processo(pid_t pid, io_stats_t *stats);

/* Cálculo de taxas de I/O entre duas leituras */
int io_calcular_taxas(const io_stats_t *antes, const io_stats_t *depois,
int intervalo_ms, io_stats_t *taxas);

/* Monitoramento contínuo com saída CSV */
int io_monitorar_pid_csv(pid_t pid, int intervalo_ms, int amostras, FILE *saida);

/* Uso instantâneo de I/O (mantém estado interno) */
int io_obter_uso_instantaneo(pid_t pid, io_stats_t *taxas);

/* Leitura de estatísticas de I/O do sistema */
int io_ler_stats_sistema(io_stats_t *stats);

/* Criar workload de I/O para testes */
int io_criar_workload(const char *arquivo, size_t tamanho_mb, int operacoes);

/* Zera o estado interno do monitor I/O */
void io_resetar_estado(void);

/* Imprime estatísticas de I/O formatadas */
void io_imprimir_stats(const io_stats_t *stats, const char *rotulo);

/* Relatório formatado de I/O (modo painel) */
int io_gerar_relatorio(pid_t pid, FILE *saida);

/* ==================== API DE MONITORAMENTO DE MEMÓRIA ==================== */

/* Leitura de estatísticas de memória de um processo */
int mem_ler_processo(pid_t pid, mem_proc_stats_t *out);

/* Leitura de estatísticas de memória do sistema */
int mem_ler_sistema(mem_sys_stats_t *out);

/* Cálculo de percentual de uso de memória */
double mem_calcular_percentual_uso(const mem_proc_stats_t *proc,
const mem_sys_stats_t *sys);

/* Monitoramento contínuo com saída CSV */
int mem_monitorar_pid_csv(pid_t pid, int intervalo_ms, int amostras, FILE *saida);

/* Gera relatório formatado de memória */
int mem_gerar_relatorio(pid_t pid, FILE *saida);

/* ==================== UTILITÁRIOS GLOBAIS ==================== */

/* Verifica se um processo ainda existe (checa /proc/<pid>) */
int processo_existe(pid_t pid);

#endif /* MONITOR_H */