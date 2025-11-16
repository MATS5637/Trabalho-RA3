#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#include "../include/monitor.h"

int main(int argc, char *argv[]) {
    pid_t pid;
    int intervalo_ms;
    int amostras;
    const char *rotulo;

    if (argc == 1) {
        pid = getpid();
        intervalo_ms = 100;
        amostras = 5;
        rotulo = "auto";
    } else if (argc == 5) {
        pid = (pid_t)atoi(argv[1]);
        intervalo_ms = atoi(argv[2]);
        amostras = atoi(argv[3]);
        rotulo = argv[4];
    } else {
        fprintf(stderr, "Uso: %s <pid> <intervalo_ms> <amostras> <rotulo>\n", argv[0]);
        fprintf(stderr, "Ou rode sem argumentos para usar PID atual e parâmetros padrão.\n");
        return 1;
    }

    if (pid <= 0 || intervalo_ms <= 0 || amostras <= 0) {
        fprintf(stderr, "Parâmetros inválidos.\n");
        return 1;
    }

    if (!processo_existe(pid)) {
        fprintf(stderr, "Processo %d não existe.\n", pid);
        return 1;
    }

    printf("# Teste de I/O para PID %d (%s)\n", pid, rotulo);
    return io_monitorar_pid_csv(pid, intervalo_ms, amostras, stdout);
}
