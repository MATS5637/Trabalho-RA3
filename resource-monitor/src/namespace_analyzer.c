// namespace_analyzer.c - VERSÃO COMPLETA E CORRIGIDA
#define _GNU_SOURCE             // para unshare()
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <sys/wait.h>
#include <sched.h>
#include "../include/namespace.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* -------------------- Tipos de namespaces comuns -------------------- */

static const char *NS_TYPES[] = {
    "cgroup",
    "ipc",
    "mnt",
    "net",
    "pid",
    "pid_for_children",
    "time",
    "time_for_children",
    "user",
    "uts"
};

static const size_t NS_TYPES_COUNT = sizeof(NS_TYPES) / sizeof(NS_TYPES[0]);

/* -------------------- Funções auxiliares internas -------------------- */

static int ler_link_ns(const char *caminho, char *dest, size_t size) {
    ssize_t n = readlink(caminho, dest, size - 1);
    if (n < 0) {
        return -1;
    }
    dest[n] = '\0';
    return 0;
}

static unsigned long long extrair_inode_ns(const char *link_target) {
    const char *abre  = strchr(link_target, '[');
    const char *fecha = strchr(link_target, ']');
    if (!abre || !fecha || fecha <= abre + 1) {
        return 0;
    }

    char buf[64];
    size_t len = (size_t)(fecha - abre - 1);
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;

    memcpy(buf, abre + 1, len);
    buf[len] = '\0';

    char *endptr = NULL;
    unsigned long long inode = strtoull(buf, &endptr, 10);
    if (endptr == buf) {
        return 0;
    }
    return inode;
}

static int obter_inode_namespace(pid_t pid, const char *tipo, unsigned long long *inode) {
    if (!tipo || !inode || pid <= 0) {
        return -1;
    }

    char caminho[PATH_MAX];
    char alvo[PATH_MAX];

    snprintf(caminho, sizeof(caminho), "/proc/%d/ns/%s", pid, tipo);

    if (ler_link_ns(caminho, alvo, sizeof(alvo)) != 0) {
        return -1;
    }

    *inode = extrair_inode_ns(alvo);
    return (*inode > 0) ? 0 : -1;
}

/* -------------------- API PRINCIPAL -------------------- */

int ns_listar_para_pid(pid_t pid, FILE *saida) {
    if (pid <= 0) {
        fprintf(stderr, "ns_listar_para_pid: PID inválido: %d\n", pid);
        return -1;
    }
    if (!saida) {
        saida = stdout;
    }

    char base[64];
    snprintf(base, sizeof(base), "/proc/%d/ns", pid);

    DIR *dir = opendir(base);
    if (!dir) {
        if (errno == ENOENT) {
            fprintf(stderr, "Processo %d não existe\n", pid);
        } else {
            fprintf(stderr, "Erro ao abrir %s: %s\n", base, strerror(errno));
        }
        return -1;
    }

    fprintf(saida, "\n=== Namespaces do PID %d ===\n", pid);

    struct dirent *ent;
    int encontrados = 0;

    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".")  == 0 ||
            strcmp(ent->d_name, "..") == 0) {
            continue;
        }

        char caminho[PATH_MAX];
        char alvo[PATH_MAX];

        snprintf(caminho, sizeof(caminho), "%s/%s", base, ent->d_name);

        if (ler_link_ns(caminho, alvo, sizeof(alvo)) != 0) {
            fprintf(saida, "%-20s -> [erro: %s]\n",
                    ent->d_name, strerror(errno));
            continue;
        }

        unsigned long long inode = extrair_inode_ns(alvo);
        if (inode > 0) {
            fprintf(saida, "%-20s -> %s (inode=%llu)\n",
                    ent->d_name, alvo, inode);
        } else {
            fprintf(saida, "%-20s -> %s\n", ent->d_name, alvo);
        }
        encontrados++;
    }

    closedir(dir);

    if (encontrados == 0) {
        fprintf(saida, "Nenhum namespace encontrado\n");
    }

    fprintf(saida, "==========================================\n");
    return 0;
}

int ns_comparar_processos(pid_t pid1, pid_t pid2, FILE *saida) {
    if (pid1 <= 0 || pid2 <= 0) {
        fprintf(stderr,
                "ns_comparar_processos: PIDs inválidos: %d, %d\n",
                pid1, pid2);
        return -1;
    }
    if (!saida) {
        saida = stdout;
    }

    fprintf(saida, "\n=== Comparação de Namespaces ===\n");
    fprintf(saida, "PID A = %d\nPID B = %d\n\n", pid1, pid2);

    int compartilhados  = 0;
    int diferentes      = 0;
    int indisponiveis   = 0;

    for (size_t i = 0; i < NS_TYPES_COUNT; i++) {
        const char *tipo = NS_TYPES[i];
        unsigned long long inode1 = 0, inode2 = 0;

        int ok1 = (obter_inode_namespace(pid1, tipo, &inode1) == 0);
        int ok2 = (obter_inode_namespace(pid2, tipo, &inode2) == 0);

        if (!ok1 || !ok2) {
            fprintf(saida, "%-20s -> INDISPONÍVEL\n", tipo);
            indisponiveis++;
            continue;
        }

        if (inode1 == inode2) {
            fprintf(saida, "%-20s -> COMPARTILHADO (inode=%llu)\n",
                    tipo, inode1);
            compartilhados++;
        } else {
            fprintf(saida, "%-20s -> DIFERENTE (A=%llu, B=%llu)\n",
                    tipo, inode1, inode2);
            diferentes++;
        }
    }

    fprintf(saida, "\n--- Resumo ---\n");
    fprintf(saida, "Compartilhados:  %d\n", compartilhados);
    fprintf(saida, "Diferentes:      %d\n", diferentes);
    fprintf(saida, "Indisponíveis:   %d\n", indisponiveis);
    fprintf(saida, "====================================\n");

    return 0;
}

int ns_resumo_atual(FILE *saida) {
    if (!saida) {
        saida = stdout;
    }
    pid_t self = getpid();
    fprintf(saida,
            "=== Namespaces do Processo Atual (PID=%d) ===\n",
            self);
    return ns_listar_para_pid(self, saida);
}

/* -------------------- FUNÇÕES AVANÇADAS -------------------- */

int ns_encontrar_por_tipo(const char *tipo_namespace,
                          unsigned long long inode_alvo,
                          FILE *saida) {
    if (!tipo_namespace) {
        fprintf(stderr,
                "ns_encontrar_por_tipo: tipo_namespace nulo\n");
        return -1;
    }
    if (!saida) {
        saida = stdout;
    }

    /* Se inode_alvo é 0, usamos o namespace do processo atual
       como referência. */
    if (inode_alvo == 0) {
        pid_t self = getpid();
        if (obter_inode_namespace(self, tipo_namespace, &inode_alvo) != 0) {
            fprintf(stderr,
                    "Erro ao obter namespace %s do processo atual\n",
                    tipo_namespace);
            return -1;
        }
    }

    fprintf(saida,
            "\n=== Processos no namespace %s:[%llu] ===\n",
            tipo_namespace, inode_alvo);

    DIR *proc_dir = opendir("/proc");
    if (!proc_dir) {
        perror("Erro ao abrir /proc");
        return -1;
    }

    struct dirent *ent;
    int encontrados = 0;

    while ((ent = readdir(proc_dir)) != NULL) {
        if (ent->d_type != DT_DIR && ent->d_type != DT_UNKNOWN) {
            continue;
        }

        char *endptr = NULL;
        pid_t pid = (pid_t)strtol(ent->d_name, &endptr, 10);
        if (*endptr != '\0') {
            continue; // não é PID numérico
        }

        unsigned long long inode;
        if (obter_inode_namespace(pid, tipo_namespace, &inode) == 0 &&
            inode == inode_alvo) {
            fprintf(saida, "PID %d\n", pid);
            encontrados++;
        }
    }

    closedir(proc_dir);

    fprintf(saida, "--- Total: %d processos ---\n", encontrados);
    fprintf(saida, "====================================\n");

    return encontrados;
}

int ns_listar_ativos_sistema(FILE *saida) {
    if (!saida) {
        saida = stdout;
    }

    fprintf(saida, "\n=== Namespaces Ativos no Sistema ===\n");

    for (size_t i = 0; i < NS_TYPES_COUNT; i++) {
        const char *tipo = NS_TYPES[i];

        fprintf(saida, "\n--- %s namespaces ---\n", tipo);

        // Guarda até 1000 inodes únicos (pra fins didáticos tá ótimo)
        unsigned long long inodes[1000];
        int count = 0;

        DIR *proc_dir = opendir("/proc");
        if (!proc_dir) {
            perror("Erro ao abrir /proc");
            continue;
        }

        struct dirent *ent;
        while ((ent = readdir(proc_dir)) != NULL && count < 1000) {
            if (ent->d_type != DT_DIR && ent->d_type != DT_UNKNOWN) {
                continue;
            }

            char *endptr = NULL;
            pid_t pid = (pid_t)strtol(ent->d_name, &endptr, 10);
            if (*endptr != '\0') {
                continue;
            }

            unsigned long long inode;
            if (obter_inode_namespace(pid, tipo, &inode) == 0) {
                int ja_existe = 0;
                for (int j = 0; j < count; j++) {
                    if (inodes[j] == inode) {
                        ja_existe = 1;
                        break;
                    }
                }
                if (!ja_existe) {
                    inodes[count++] = inode;
                }
            }
        }

        closedir(proc_dir);

        fprintf(saida,
                "Total de %s namespaces únicos: %d\n",
                tipo, count);

        for (int j = 0; j < count && j < 5; j++) {
            fprintf(saida, "  %s:[%llu]\n", tipo, inodes[j]);
        }
        if (count > 5) {
            fprintf(saida, "  ... e mais %d\n", count - 5);
        }
    }

    fprintf(saida, "\n====================================\n");
    return 0;
}

int ns_medir_overhead_criacao(FILE *saida) {
    if (!saida) {
        saida = stdout;
    }

    fprintf(saida,
            "\n=== Medição de Overhead de Criação de Namespaces ===\n");

    const int iteracoes = 100;
    long long total_ns = 0;

    for (int i = 0; i < iteracoes; i++) {
        struct timespec inicio, fim;
        clock_gettime(CLOCK_MONOTONIC, &inicio);

        pid_t child = fork();
        if (child == 0) {
            // Filho: cria novos namespaces e sai
            if (unshare(CLONE_NEWNS | CLONE_NEWUTS | CLONE_NEWIPC) != 0) {
                // Mesmo se falhar (sem permissão), só sai
                _exit(1);
            }
            _exit(0);
        } else if (child > 0) {
            // Pai: espera filho
            waitpid(child, NULL, 0);
            clock_gettime(CLOCK_MONOTONIC, &fim);

            long long ns = (fim.tv_sec  - inicio.tv_sec) * 1000000000LL +
                           (fim.tv_nsec - inicio.tv_nsec);
            total_ns += ns;
        } else {
            perror("fork");
            return -1;
        }
    }

    double media_us = (double)total_ns / (double)iteracoes / 1000.0;

    fprintf(saida, "Iterações: %d\n", iteracoes);
    fprintf(saida, "Tempo médio por criação: %.2f µs\n", media_us);
    fprintf(saida,
            "Overhead aproximado por namespace: %.2f µs (considerando 3 namespaces)\n",
            media_us / 3.0);
    fprintf(saida, "====================================\n");

    return 0;
}

int ns_relatorio_sistema(FILE *saida) {
    if (!saida) {
        saida = stdout;
    }

    fprintf(saida,
            "\n=== RELATÓRIO COMPLETO DE NAMESPACES DO SISTEMA ===\n\n");

    // 1. Namespaces ativos
    ns_listar_ativos_sistema(saida);

    // 2. Overhead de criação de namespaces
    ns_medir_overhead_criacao(saida);

    // 3. Processo init (PID 1) como referência
    fprintf(saida, "\n=== Processo Init (PID 1) como Referência ===\n");
    ns_listar_para_pid(1, saida);

    // 4. Processo atual para comparação
    fprintf(saida, "\n=== Processo Atual para Comparação ===\n");
    ns_resumo_atual(saida);

    fprintf(saida, "=== FIM DO RELATÓRIO ===\n");
    return 0;
}
