#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <ctype.h>

#include "../include/monitor.h"
#include "../include/cgroup.h"
#include "../include/namespace.h"

static void imprimir_uso_geral(const char *progname) {
    fprintf(stderr,
        "Uso (modo linha de comando):\n"
        "  %s cpu <pid> <intervalo_ms> <amostras>\n"
        "  %s mem <pid> <intervalo_ms> <amostras>\n"
        "  %s io  <pid> <intervalo_ms> <amostras>\n"
        "  %s cgroup-create <nome> <cpu_cores> <mem_mb>\n"
        "  %s cgroup-add    <nome> <pid>\n"
        "  %s cgroup-stats  <nome>\n"
        "  %s ns-pid     <pid>\n"
        "  %s ns-compare <pid1> <pid2>\n"
        "  %s ns-report\n"
        "\n"
        "Sem argumentos, o programa entra em modo interativo (menu).\n",
        progname, progname, progname,
        progname, progname, progname,
        progname, progname, progname
    );
}

static int cmd_cpu(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Uso: %s cpu <pid> <intervalo_ms> <amostras>\n", argv[0]);
        return 1;
    }

    pid_t pid = (pid_t)atoi(argv[2]);
    int intervalo_ms = atoi(argv[3]);
    int amostras = atoi(argv[4]);

    if (pid <= 0 || intervalo_ms <= 0 || amostras <= 0) {
        fprintf(stderr, "Parâmetros inválidos em comando cpu.\n");
        return 1;
    }

    if (!processo_existe(pid)) {
        fprintf(stderr, "Processo %d não existe.\n", pid);
        return 1;
    }

    fprintf(stderr, "\n===== Monitoramento de CPU =====\n");
    fprintf(stderr, "PID monitorado      : %d\n", pid);
    fprintf(stderr, "Intervalo entre leituras (ms): %d\n", intervalo_ms);
    fprintf(stderr, "Número de amostras  : %d\n\n", amostras);

    fprintf(stderr, "Cada linha gerada possui os campos:\n");
    fprintf(stderr, "  timestamp            -> data/hora da coleta\n");
    fprintf(stderr, "  amostra              -> número sequencial da amostra (0..N-1)\n");
    fprintf(stderr, "  cpu_processo_percent -> uso de CPU do processo monitorado (em %%)\n");
    fprintf(stderr, "  cpu_sistema_percent  -> uso total de CPU do sistema (em %%)\n\n");

    return cpu_monitorar_pid_csv(pid, intervalo_ms, amostras, stdout);
}


static int cmd_mem(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Uso: %s mem <pid> <intervalo_ms> <amostras>\n", argv[0]);
        return 1;
    }

    pid_t pid = (pid_t)atoi(argv[2]);
    int intervalo_ms = atoi(argv[3]);
    int amostras = atoi(argv[4]);

    if (pid <= 0 || intervalo_ms <= 0 || amostras <= 0) {
        fprintf(stderr, "Parâmetros inválidos em comando mem.\n");
        return 1;
    }

    if (!processo_existe(pid)) {
        fprintf(stderr, "Processo %d não existe.\n", pid);
        return 1;
    }

    return mem_monitorar_pid_csv(pid, intervalo_ms, amostras, stdout);
}

static int cmd_io(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Uso: %s io <pid> <intervalo_ms> <amostras>\n", argv[0]);
        return 1;
    }

    pid_t pid = (pid_t)atoi(argv[2]);
    int intervalo_ms = atoi(argv[3]);
    int amostras = atoi(argv[4]);

    if (pid <= 0 || intervalo_ms <= 0 || amostras <= 0) {
        fprintf(stderr, "Parâmetros inválidos em comando io.\n");
        return 1;
    }

    if (!processo_existe(pid)) {
        fprintf(stderr, "Processo %d não existe.\n", pid);
        return 1;
    }

    return io_monitorar_pid_csv(pid, intervalo_ms, amostras, stdout);
}

static int cmd_cgroup_create(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr,
                "Uso: %s cgroup-create <nome> <cpu_cores> <mem_mb>\n",
                argv[0]);
        return 1;
    }

    const char *nome = argv[2];
    double cpu_cores = atof(argv[3]);
    unsigned long mem_mb = strtoul(argv[4], NULL, 10);

    if (cpu_cores < 0.0 || mem_mb == 0) {
        fprintf(stderr, "Parâmetros inválidos em cgroup-create.\n");
        return 1;
    }

    int rc = cgroup_criar(nome, cpu_cores, mem_mb);
    if (rc != 0) {
        fprintf(stderr, "Falha ao criar/configurar cgroup '%s'.\n", nome);
        return 1;
    }

    printf("Cgroup '%s' criado/configurado.\n", nome);
    return 0;
}

static int cmd_cgroup_add(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr,
                "Uso: %s cgroup-add <nome> <pid>\n",
                argv[0]);
        return 1;
    }

    const char *nome = argv[2];
    pid_t pid = (pid_t)atoi(argv[3]);

    if (pid <= 0) {
        fprintf(stderr, "PID inválido.\n");
        return 1;
    }

    if (!processo_existe(pid)) {
        fprintf(stderr, "Processo %d não existe.\n", pid);
        return 1;
    }

    int rc = cgroup_adicionar_processo(nome, pid);
    if (rc != 0) {
        fprintf(stderr, "Falha ao adicionar PID %d ao cgroup '%s'.\n", pid, nome);
        return 1;
    }

    printf("PID %d adicionado ao cgroup '%s'.\n", pid, nome);
    return 0;
}

static int cmd_cgroup_stats(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr,
                "Uso: %s cgroup-stats <nome>\n",
                argv[0]);
        return 1;
    }

    const char *nome = argv[2];
    return cgroup_relatorio_usage(nome);
}

static int cmd_ns_pid(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s ns-pid <pid>\n", argv[0]);
        return 1;
    }

    pid_t pid = (pid_t)atoi(argv[2]);
    if (pid <= 0) {
        fprintf(stderr, "PID inválido.\n");
        return 1;
    }

    if (!processo_existe(pid)) {
        fprintf(stderr, "Processo %d não existe.\n", pid);
        return 1;
    }

    return ns_listar_para_pid(pid, stdout);
}

static int cmd_ns_compare(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Uso: %s ns-compare <pid1> <pid2>\n", argv[0]);
        return 1;
    }

    pid_t pid1 = (pid_t)atoi(argv[2]);
    pid_t pid2 = (pid_t)atoi(argv[3]);

    if (pid1 <= 0 || pid2 <= 0) {
        fprintf(stderr, "PIDs inválidos.\n");
        return 1;
    }

    if (!processo_existe(pid1) || !processo_existe(pid2)) {
        fprintf(stderr, "Um dos processos não existe.\n");
        return 1;
    }

    return ns_comparar_processos(pid1, pid2, stdout);
}

static int cmd_ns_report(int argc, char *argv[]) {
    (void)argv;
    if (argc != 2) {
        fprintf(stderr, "Uso: %s ns-report\n", argv[0]);
        return 1;
    }

    return ns_relatorio_sistema(stdout);
}

static void listar_pids_disponiveis() {
    DIR *d = opendir("/proc");
    if (!d) {
        perror("Não foi possível abrir /proc");
        return;
    }

    struct dirent *ent;

    printf("\n=== PIDs disponíveis no sistema ===\n");

    while ((ent = readdir(d)) != NULL) {
        const char *nome = ent->d_name;

        int somente_numeros = 1;
        for (int i = 0; nome[i] != '\0'; i++) {
            if (!isdigit((unsigned char)nome[i])) {
                somente_numeros = 0;
                break;
            }
        }

        if (somente_numeros) {
            char caminho[256];
            int n = snprintf(caminho, sizeof(caminho), "/proc/%.200s/comm", nome);
            if (n < 0 || n >= (int)sizeof(caminho)) {
                fprintf(stderr, "Erro ao montar caminho para PID %s\n", nome);
                continue;
            }

            FILE *f = fopen(caminho, "r");
            char comm[256] = "(nome indisponível)";

            if (f) {
                if (fgets(comm, sizeof(comm), f) == NULL) {
                    fclose(f);
                    continue;
                }
                fclose(f);
                comm[strcspn(comm, "\n")] = '\0';
            }

            printf("PID %s – %s\n", nome, comm);
        }
    }

    closedir(d);
    printf("====================================\n\n");
}


static int menu_interativo(const char *progname) {
    while (1) {
        int opcao;
        printf("==== Resource Monitor (modo interativo) ====\n");
        printf("1) Monitorar CPU\n");
        printf("2) Monitorar Memória\n");
        printf("3) Monitorar I/O\n");
        printf("4) Namespaces de um PID\n");
        printf("5) Comparar namespaces de dois PIDs\n");
        printf("6) Relatório geral de namespaces\n");
        printf("7) Criar cgroup\n");
        printf("8) Adicionar processo a cgroup\n");
        printf("9) Estatísticas de cgroup\n");
        printf("10) Listar PIDs disponíveis\n");
        printf("0) Sair\n");
        printf("Escolha uma opção: ");
        if (scanf("%d", &opcao) != 1) {
            fprintf(stderr, "Entrada inválida.\n");
            return 1;
        }

        if (opcao == 0) {
            printf("Encerrando.\n");
            return 0;
        }

        if (opcao == 1) {
            pid_t pid;
            int intervalo, amostras;
            char buf_pid[32], buf_int[32], buf_ams[32];
            char *argv_cpu[6];

            printf("PID: ");
            if (scanf("%d", &pid) != 1) return 1;
            printf("Intervalo (ms): ");
            if (scanf("%d", &intervalo) != 1) return 1;
            printf("Amostras: ");
            if (scanf("%d", &amostras) != 1) return 1;

            snprintf(buf_pid, sizeof(buf_pid), "%d", pid);
            snprintf(buf_int, sizeof(buf_int), "%d", intervalo);
            snprintf(buf_ams, sizeof(buf_ams), "%d", amostras);

            argv_cpu[0] = (char *)progname;
            argv_cpu[1] = "cpu";
            argv_cpu[2] = buf_pid;
            argv_cpu[3] = buf_int;
            argv_cpu[4] = buf_ams;
            argv_cpu[5] = NULL;

            cmd_cpu(5, argv_cpu);
        } else if (opcao == 2) {
            pid_t pid;
            int intervalo, amostras;
            char buf_pid[32], buf_int[32], buf_ams[32];
            char *argv_mem[6];

            printf("PID: ");
            if (scanf("%d", &pid) != 1) return 1;
            printf("Intervalo (ms): ");
            if (scanf("%d", &intervalo) != 1) return 1;
            printf("Amostras: ");
            if (scanf("%d", &amostras) != 1) return 1;

            snprintf(buf_pid, sizeof(buf_pid), "%d", pid);
            snprintf(buf_int, sizeof(buf_int), "%d", intervalo);
            snprintf(buf_ams, sizeof(buf_ams), "%d", amostras);

            argv_mem[0] = (char *)progname;
            argv_mem[1] = "mem";
            argv_mem[2] = buf_pid;
            argv_mem[3] = buf_int;
            argv_mem[4] = buf_ams;
            argv_mem[5] = NULL;

            cmd_mem(5, argv_mem);
        } else if (opcao == 3) {
            pid_t pid;
            int intervalo, amostras;
            char buf_pid[32], buf_int[32], buf_ams[32];
            char *argv_io[6];

            printf("PID: ");
            if (scanf("%d", &pid) != 1) return 1;
            printf("Intervalo (ms): ");
            if (scanf("%d", &intervalo) != 1) return 1;
            printf("Amostras: ");
            if (scanf("%d", &amostras) != 1) return 1;

            snprintf(buf_pid, sizeof(buf_pid), "%d", pid);
            snprintf(buf_int, sizeof(buf_int), "%d", intervalo);
            snprintf(buf_ams, sizeof(buf_ams), "%d", amostras);

            argv_io[0] = (char *)progname;
            argv_io[1] = "io";
            argv_io[2] = buf_pid;
            argv_io[3] = buf_int;
            argv_io[4] = buf_ams;
            argv_io[5] = NULL;

            cmd_io(5, argv_io);
        } else if (opcao == 4) {
            pid_t pid;
            char buf_pid[32];
            char *argv_ns[4];

            printf("PID: ");
            if (scanf("%d", &pid) != 1) return 1;

            snprintf(buf_pid, sizeof(buf_pid), "%d", pid);

            argv_ns[0] = (char *)progname;
            argv_ns[1] = "ns-pid";
            argv_ns[2] = buf_pid;
            argv_ns[3] = NULL;

            cmd_ns_pid(3, argv_ns);
        } else if (opcao == 5) {
            pid_t pid1, pid2;
            char buf_p1[32], buf_p2[32];
            char *argv_cmp[5];

            printf("PID 1: ");
            if (scanf("%d", &pid1) != 1) return 1;
            printf("PID 2: ");
            if (scanf("%d", &pid2) != 1) return 1;

            snprintf(buf_p1, sizeof(buf_p1), "%d", pid1);
            snprintf(buf_p2, sizeof(buf_p2), "%d", pid2);

            argv_cmp[0] = (char *)progname;
            argv_cmp[1] = "ns-compare";
            argv_cmp[2] = buf_p1;
            argv_cmp[3] = buf_p2;
            argv_cmp[4] = NULL;

            cmd_ns_compare(4, argv_cmp);
        } else if (opcao == 6) {
            char *argv_rep[3];

            argv_rep[0] = (char *)progname;
            argv_rep[1] = "ns-report";
            argv_rep[2] = NULL;

            cmd_ns_report(2, argv_rep);
        } else if (opcao == 7) {
            char nome[128];
            double cpu_cores;
            unsigned long mem_mb;
            char buf_cpu[32], buf_mem[32];
            char *argv_cg[6];

            printf("Nome do cgroup: ");
            if (scanf("%127s", nome) != 1) return 1;
            printf("CPU (em núcleos, ex: 0.5, 1.0, 2.0): ");
            if (scanf("%lf", &cpu_cores) != 1) return 1;
            printf("Memória (MB): ");
            if (scanf("%lu", &mem_mb) != 1) return 1;

            snprintf(buf_cpu, sizeof(buf_cpu), "%.3f", cpu_cores);
            snprintf(buf_mem, sizeof(buf_mem), "%lu", mem_mb);

            argv_cg[0] = (char *)progname;
            argv_cg[1] = "cgroup-create";
            argv_cg[2] = nome;
            argv_cg[3] = buf_cpu;
            argv_cg[4] = buf_mem;
            argv_cg[5] = NULL;

            cmd_cgroup_create(5, argv_cg);
        } else if (opcao == 8) {
            char nome[128];
            pid_t pid;
            char buf_pid[32];
            char *argv_cg[5];

            printf("Nome do cgroup: ");
            if (scanf("%127s", nome) != 1) return 1;
            printf("PID: ");
            if (scanf("%d", &pid) != 1) return 1;

            snprintf(buf_pid, sizeof(buf_pid), "%d", pid);

            argv_cg[0] = (char *)progname;
            argv_cg[1] = "cgroup-add";
            argv_cg[2] = nome;
            argv_cg[3] = buf_pid;
            argv_cg[4] = NULL;

            cmd_cgroup_add(4, argv_cg);
        } else if (opcao == 9) {
            char nome[128];
            char *argv_cg[4];

            printf("Nome do cgroup: ");
            if (scanf("%127s", nome) != 1) return 1;

            argv_cg[0] = (char *)progname;
            argv_cg[1] = "cgroup-stats";
            argv_cg[2] = nome;
            argv_cg[3] = NULL;

            cmd_cgroup_stats(3, argv_cg);
        } else if (opcao == 10) {
            listar_pids_disponiveis();
        } else {
            printf("Opção inválida.\n");
        }

        printf("\n");
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        return menu_interativo(argv[0]);
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "cpu") == 0) {
        return cmd_cpu(argc, argv);
    } else if (strcmp(cmd, "mem") == 0) {
        return cmd_mem(argc, argv);
    } else if (strcmp(cmd, "io") == 0) {
        return cmd_io(argc, argv);
    } else if (strcmp(cmd, "cgroup-create") == 0) {
        return cmd_cgroup_create(argc, argv);
    } else if (strcmp(cmd, "cgroup-add") == 0) {
        return cmd_cgroup_add(argc, argv);
    } else if (strcmp(cmd, "cgroup-stats") == 0) {
        return cmd_cgroup_stats(argc, argv);
    } else if (strcmp(cmd, "ns-pid") == 0) {
        return cmd_ns_pid(argc, argv);
    } else if (strcmp(cmd, "ns-compare") == 0) {
        return cmd_ns_compare(argc, argv);
    } else if (strcmp(cmd, "ns-report") == 0) {
        return cmd_ns_report(argc, argv);
    } else {
        fprintf(stderr, "Comando desconhecido: %s\n\n", cmd);
        imprimir_uso_geral(argv[0]);
        return 1;
    }
}
