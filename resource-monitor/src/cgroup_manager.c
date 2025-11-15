// src/cgroup_manager.c
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#include <dirent.h>

#include "../include/cgroup.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* ==================== DETECÇÃO DE VERSÃO ==================== */

static int g_cgroup_version = -1;

/* Retorna:
 *   2 -> cgroup v2
 *   1 -> cgroup v1
 *   0 -> não encontrado
 */
static int detectar_cgroup_version_internal(void) {
    /* Heurística simples: cgroup v2 expõe "cgroup.controllers" na raiz */
    if (access("/sys/fs/cgroup/cgroup.controllers", F_OK) == 0) {
        return 2; // cgroup v2
    }

    /* Para v1, verificamos se alguns controladores clássicos existem */
    const char *controllers[] = {"cpu", "memory", "blkio", "pids"};
    for (int i = 0; i < 4; i++) {
        char path[PATH_MAX];
        snprintf(path, sizeof(path), "/sys/fs/cgroup/%s", controllers[i]);
        if (access(path, F_OK) == 0) {
            return 1; // cgroup v1
        }
    }

    return 0; // não encontrado
}

int cgroup_detectar_versao(void) {
    if (g_cgroup_version < 0) {
        g_cgroup_version = detectar_cgroup_version_internal();
    }
    return g_cgroup_version;
}

/* ==================== HELPERS DE ARQUIVO ==================== */

static int escrever_arquivo(const char *path, const char *conteudo) {
    FILE *fp = fopen(path, "w");
    if (!fp) {
        return -1;
    }
    /* gravamos com '\n' para ficar mais próximo do uso via shell */
    int rc = (fprintf(fp, "%s\n", conteudo) > 0) ? 0 : -1;
    fclose(fp);
    return rc;
}

static int ler_ull_arquivo(const char *path, unsigned long long *valor) {
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;
    int rc = (fscanf(fp, "%llu", valor) == 1) ? 0 : -1;
    fclose(fp);
    return rc;
}

/* ==================== FUNÇÕES DE CAMINHO ==================== */

/* cgroup v2: tudo sob /sys/fs/cgroup (unificado) */
static void path_v2(const char *cgroup, const char *file, char *out, size_t size) {
    if (!cgroup || cgroup[0] == '\0') {
        if (file && file[0] != '\0')
            snprintf(out, size, "/sys/fs/cgroup/%s", file);
        else
            snprintf(out, size, "/sys/fs/cgroup");
    } else {
        if (file && file[0] != '\0')
            snprintf(out, size, "/sys/fs/cgroup/%s/%s", cgroup, file);
        else
            snprintf(out, size, "/sys/fs/cgroup/%s", cgroup);
    }
}

/* cgroup v1 – serve para encontrar arquivos de controladores
 * Já supõe que o diretório do controlador existe. Para CRIAR diretórios
 * de cgroup v1, não use esta função (veja cgroup_criar). */
static int path_v1(const char *controller,
                   const char *cgroup,
                   const char *file,
                   char *out,
                   size_t size)
{
    if (!controller) return -1;

    /* tenta caminho direto primeiro */
    if (!cgroup || cgroup[0] == '\0') {
        if (file && file[0] != '\0')
            snprintf(out, size, "/sys/fs/cgroup/%s/%s", controller, file);
        else
            snprintf(out, size, "/sys/fs/cgroup/%s", controller);
    } else {
        if (file && file[0] != '\0')
            snprintf(out, size, "/sys/fs/cgroup/%s/%s/%s", controller, cgroup, file);
        else
            snprintf(out, size, "/sys/fs/cgroup/%s/%s", controller, cgroup);
    }

    if (access(out, F_OK) == 0) return 0;

    /* Fallback: alguns sistemas usam "cpu,cpuacct" em vez de "cpu" puro */
    if (strcmp(controller, "cpu") == 0) {
        if (!cgroup || cgroup[0] == '\0') {
            if (file && file[0] != '\0')
                snprintf(out, size, "/sys/fs/cgroup/cpu,cpuacct/%s", file);
            else
                snprintf(out, size, "/sys/fs/cgroup/cpu,cpuacct");
        } else {
            if (file && file[0] != '\0')
                snprintf(out, size, "/sys/fs/cgroup/cpu,cpuacct/%s/%s", cgroup, file);
            else
                snprintf(out, size, "/sys/fs/cgroup/cpu,cpuacct/%s", cgroup);
        }
        return (access(out, F_OK) == 0) ? 0 : -1;
    }

    return -1;
}

/* ==================== LISTAR CGROUPS ATIVOS ==================== */

int cgroup_listar_ativos(void) {
    int versao = cgroup_detectar_versao();
    if (versao == 0) {
        fprintf(stderr, "cgroup não disponível neste sistema\n");
        return -1;
    }

    const char *path = "/sys/fs/cgroup";
    DIR *dir = opendir(path);
    if (!dir) {
        perror("opendir /sys/fs/cgroup");
        return -1;
    }

    printf("=== Cgroups Ativos (v%d) ===\n", versao);

    struct dirent *ent;
    int count = 0;
    while ((ent = readdir(dir)) != NULL) {
        // Ignora "." e ".."
if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
    continue;

char fullpath[PATH_MAX];
snprintf(fullpath, sizeof(fullpath), "%s/%s", path, ent->d_name);

struct stat st;
if (stat(fullpath, &st) == 0 && S_ISDIR(st.st_mode)) {
    printf("  %s\n", ent->d_name);
    count++;
}

        {
            printf("  %s\n", ent->d_name);
            count++;
        }
    }

    closedir(dir);
    printf("Total: %d entradas em /sys/fs/cgroup\n", count);
    return 0;
}

/* ==================== LEITURA DE MÉTRICAS ==================== */

int cgroup_ler_cpu_usage(const char *cgroup_name, unsigned long long *cpu_usage) {
    if (!cpu_usage) return -1;

    int versao = cgroup_detectar_versao();
    char path[PATH_MAX];

    if (versao == 2) {
        /* cgroup v2: cpu.stat → usage_usec */
        path_v2(cgroup_name, "cpu.stat", path, sizeof(path));
        FILE *fp = fopen(path, "r");
        if (!fp) return -1;

        char key[64];
        unsigned long long value;
        while (fscanf(fp, "%63s %llu", key, &value) == 2) {
            if (strcmp(key, "usage_usec") == 0) {
                *cpu_usage = value * 1000ULL; /* converte usec → nsec */
                fclose(fp);
                return 0;
            }
        }
        fclose(fp);
        return -1;
    } else if (versao == 1) {
        /* v1: cpuacct.usage em nano-segundos */
        if (path_v1("cpu", cgroup_name, "cpuacct.usage", path, sizeof(path)) == 0 ||
            path_v1("cpuacct", cgroup_name, "cpuacct.usage", path, sizeof(path)) == 0)
        {
            return ler_ull_arquivo(path, cpu_usage);
        }
        return -1;
    }

    return -1;
}

int cgroup_ler_memory_usage(const char *cgroup_name, unsigned long long *memory_usage) {
    if (!memory_usage) return -1;

    int versao = cgroup_detectar_versao();
    char path[PATH_MAX];

    if (versao == 2) {
        path_v2(cgroup_name, "memory.current", path, sizeof(path));
    } else if (versao == 1) {
        if (path_v1("memory", cgroup_name, "memory.usage_in_bytes",
                    path, sizeof(path)) != 0)
        {
            return -1;
        }
    } else {
        return -1;
    }

    return ler_ull_arquivo(path, memory_usage);
}

int cgroup_ler_io_stats(const char *cgroup_name,
                        unsigned long long *read_bytes,
                        unsigned long long *write_bytes)
{
    if (!read_bytes || !write_bytes) return -1;

    *read_bytes  = 0;
    *write_bytes = 0;

    int versao = cgroup_detectar_versao();
    char path[PATH_MAX];

    if (versao == 2) {
        /* v2: io.stat – pode ter várias linhas (um por device) */
        path_v2(cgroup_name, "io.stat", path, sizeof(path));
        FILE *fp = fopen(path, "r");
        if (!fp) return -1;

        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            unsigned long long r = 0, w = 0;
            char *rpos = strstr(line, "rbytes=");
            char *wpos = strstr(line, "wbytes=");
            if (rpos) sscanf(rpos + 7, "%llu", &r);
            if (wpos) sscanf(wpos + 7, "%llu", &w);
            *read_bytes  += r;
            *write_bytes += w;
        }
        fclose(fp);
    } else if (versao == 1) {
        /* v1: blkio.io_service_bytes */
        if (path_v1("blkio", cgroup_name, "blkio.io_service_bytes",
                    path, sizeof(path)) == 0)
        {
            FILE *fp = fopen(path, "r");
            if (fp) {
                char dev[32], op[32];
                unsigned long long value;
                while (fscanf(fp, "%31s %31s %llu",
                              dev, op, &value) == 3)
                {
                    if (strcmp(op, "Read") == 0)
                        *read_bytes  += value;
                    else if (strcmp(op, "Write") == 0)
                        *write_bytes += value;
                }
                fclose(fp);
            }
        }
    } else {
        return -1;
    }

    return 0;
}

/* ==================== MÉTRICAS COMPLETAS ==================== */

int cgroup_ler_metricas_completas(const char *cgroup_name,
                                  cgroup_metrics_t *metrics)
{
    if (!metrics) return -1;

    memset(metrics, 0, sizeof(*metrics));
    if (cgroup_name) {
        strncpy(metrics->name, cgroup_name, sizeof(metrics->name) - 1);
        metrics->name[sizeof(metrics->name) - 1] = '\0';
    }

    if (cgroup_ler_cpu_usage(cgroup_name, &metrics->cpu_usage) != 0)
        metrics->cpu_usage = 0;

    if (cgroup_ler_memory_usage(cgroup_name, &metrics->memory_usage) != 0)
        metrics->memory_usage = 0;

    if (cgroup_ler_io_stats(cgroup_name,
                            &metrics->io_read_bytes,
                            &metrics->io_write_bytes) != 0)
    {
        metrics->io_read_bytes  = 0;
        metrics->io_write_bytes = 0;
    }

    /* Limite de memória e failcnt */
    int versao = cgroup_detectar_versao();
    char path[PATH_MAX];

    if (versao == 2) {
        path_v2(cgroup_name, "memory.max", path, sizeof(path));
        FILE *fp = fopen(path, "r");
        if (fp) {
            char buf[64];
            if (fscanf(fp, "%63s", buf) == 1) {
                if (strcmp(buf, "max") == 0)
                    metrics->memory_limit = 0;  /* sem limite */
                else
                    metrics->memory_limit = strtoull(buf, NULL, 10);
            }
            fclose(fp);
        }
    } else if (versao == 1) {
        if (path_v1("memory", cgroup_name, "memory.limit_in_bytes",
                    path, sizeof(path)) == 0)
        {
            ler_ull_arquivo(path, &metrics->memory_limit);
        }
        if (path_v1("memory", cgroup_name, "memory.failcnt",
                    path, sizeof(path)) == 0)
        {
            unsigned long long failcnt = 0;
            if (ler_ull_arquivo(path, &failcnt) == 0) {
                metrics->memory_failcnt = (unsigned long)failcnt;
            }
        }
    }

    return 0;
}

/* ==================== CRIAÇÃO E CONFIGURAÇÃO ==================== */

int cgroup_criar(const char *cgroup_name,
                 double cpu_limit_cores,
                 unsigned long memory_limit_mb)
{
    if (!cgroup_name || cgroup_name[0] == '\0') {
        fprintf(stderr, "Nome do cgroup inválido\n");
        return -1;
    }

    int versao = cgroup_detectar_versao();
    if (versao == 0) {
        fprintf(stderr, "cgroup não disponível neste sistema\n");
        return -1;
    }

    char path[PATH_MAX];

    if (versao == 2) {
        /* ---------- cgroup v2: único filesystem ---------- */
        path_v2(cgroup_name, NULL, path, sizeof(path));
        if (mkdir(path, 0755) != 0 && errno != EEXIST) {
            perror("mkdir cgroup v2");
            return -1;
        }

        /* Limite de CPU: cpu.max = "<quota> <period>" */
        if (cpu_limit_cores > 0.0) {
            path_v2(cgroup_name, "cpu.max", path, sizeof(path));
            unsigned long quota = (unsigned long)(cpu_limit_cores * 100000.0);
            char buf[64];
            snprintf(buf, sizeof(buf), "%lu 100000", quota);
            (void)escrever_arquivo(path, buf);
        }

        /* Limite de memória: memory.max em bytes */
        if (memory_limit_mb > 0) {
            path_v2(cgroup_name, "memory.max", path, sizeof(path));
            unsigned long long bytes =
                (unsigned long long)memory_limit_mb * 1024ULL * 1024ULL;
            char buf[64];
            snprintf(buf, sizeof(buf), "%llu", bytes);
            (void)escrever_arquivo(path, buf);
        }

    } else { /* versao == 1 */
        /* ---------- cgroup v1: vários controladores ---------- */
        const char *controllers[] = {"cpu", "memory", "blkio", "pids"};
        for (int i = 0; i < 4; i++) {
            /* Monta o caminho do diretório do cgroup dentro do controlador */
            if (!cgroup_name || cgroup_name[0] == '\0') {
                snprintf(path, sizeof(path),
                         "/sys/fs/cgroup/%s", controllers[i]);
            } else {
                snprintf(path, sizeof(path),
                         "/sys/fs/cgroup/%s/%s",
                         controllers[i], cgroup_name);
            }

            if (mkdir(path, 0755) != 0 && errno != EEXIST) {
                fprintf(stderr,
                        "Aviso: não foi possível criar %s: %s\n",
                        path, strerror(errno));
            }
        }

        /* Limite de CPU: cpu.cfs_quota_us */
        if (cpu_limit_cores > 0.0) {
            if (path_v1("cpu", cgroup_name,
                        "cpu.cfs_quota_us", path, sizeof(path)) == 0)
            {
                unsigned long quota =
                    (unsigned long)(cpu_limit_cores * 100000.0);
                char buf[32];
                snprintf(buf, sizeof(buf), "%lu", quota);
                (void)escrever_arquivo(path, buf);
            }
        }

        /* Limite de memória: memory.limit_in_bytes */
        if (memory_limit_mb > 0) {
            if (path_v1("memory", cgroup_name,
                        "memory.limit_in_bytes", path, sizeof(path)) == 0)
            {
                unsigned long long bytes =
                    (unsigned long long)memory_limit_mb * 1024ULL * 1024ULL;
                char buf[32];
                snprintf(buf, sizeof(buf), "%llu", bytes);
                (void)escrever_arquivo(path, buf);
            }
        }
    }

    printf("Cgroup '%s' criado/configurado com sucesso (v%d)\n",
           cgroup_name, versao);
    return 0;
}

int cgroup_adicionar_processo(const char *cgroup_name, pid_t pid) {
    if (!cgroup_name || pid <= 0) return -1;

    int versao = cgroup_detectar_versao();
    char path[PATH_MAX];
    char pid_str[32];

    snprintf(pid_str, sizeof(pid_str), "%d", pid);

    if (versao == 2) {
        path_v2(cgroup_name, "cgroup.procs", path, sizeof(path));
        return escrever_arquivo(path, pid_str);
    } else if (versao == 1) {
        const char *controllers[] = {"cpu", "memory", "blkio"};
        int success = 0;
        for (int i = 0; i < 3; i++) {
            if (path_v1(controllers[i], cgroup_name,
                        "tasks", path, sizeof(path)) == 0)
            {
                if (escrever_arquivo(path, pid_str) == 0) {
                    success = 1;
                }
            }
        }
        return success ? 0 : -1;
    }

    return -1;
}

int cgroup_remover(const char *cgroup_name) {
    if (!cgroup_name || cgroup_name[0] == '\0') return -1;

    int versao = cgroup_detectar_versao();

    if (versao == 2) {
        char path[PATH_MAX];
        path_v2(cgroup_name, NULL, path, sizeof(path));
        return (rmdir(path) == 0 || errno == ENOENT) ? 0 : -1;
    } else if (versao == 1) {
        const char *controllers[] = {"cpu", "memory", "blkio", "pids"};
        int success = 1;
        for (int i = 0; i < 4; i++) {
            char path[PATH_MAX];
            if (!cgroup_name || cgroup_name[0] == '\0') {
                snprintf(path, sizeof(path),
                         "/sys/fs/cgroup/%s", controllers[i]);
            } else {
                snprintf(path, sizeof(path),
                         "/sys/fs/cgroup/%s/%s",
                         controllers[i], cgroup_name);
            }
            if (rmdir(path) != 0 && errno != ENOENT) {
                success = 0;
            }
        }
        return success ? 0 : -1;
    }

    return -1;
}

/* ==================== RELATÓRIO ==================== */

int cgroup_relatorio_usage(const char *cgroup_name) {
    cgroup_metrics_t metrics;
    if (cgroup_ler_metricas_completas(cgroup_name, &metrics) != 0) {
        fprintf(stderr, "Erro ao ler métricas do cgroup '%s'\n",
                cgroup_name ? cgroup_name : "(root)");
        return -1;
    }

    printf("\n=== Relatório CGroup: %s ===\n",
           metrics.name[0] ? metrics.name : "(root)");
    printf("CPU Usage:    %llu ns\n", metrics.cpu_usage);
    printf("Memory Usage: %llu bytes", metrics.memory_usage);
    if (metrics.memory_limit > 0) {
        double pct = (double)metrics.memory_usage /
                     (double)metrics.memory_limit * 100.0;
        printf(" (%.1f%% de %llu bytes de limite)",
               pct, metrics.memory_limit);
    }
    printf("\n");
    printf("Memory Fails: %lu\n", metrics.memory_failcnt);
    printf("I/O Read:     %llu bytes\n", metrics.io_read_bytes);
    printf("I/O Write:    %llu bytes\n", metrics.io_write_bytes);
    printf("====================================\n");

    return 0;
}
