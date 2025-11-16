#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#include "../include/monitor.h"

int main(int argc, char *argv[]) {
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
