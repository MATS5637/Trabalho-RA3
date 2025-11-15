#ifndef NAMESPACE_H
#define NAMESPACE_H

#include <stdio.h>
#include <sys/types.h>

/* ==================== ESTRUTURAS DE DADOS (para uso futuro/relatórios) ==================== */

typedef struct {
    char tipo[32];              // "pid", "net", "mnt", etc.
    unsigned long long inode;   // Identificador único do namespace
    int compartilhado;          // Quantos processos compartilham este namespace
} namespace_info_t;

typedef struct {
    pid_t pid;
    int namespace_count;
    namespace_info_t namespaces[12]; // Tipos comuns de namespaces
} processo_namespaces_t;

/* ==================== API PRINCIPAL (usada diretamente no trabalho) ==================== */

/**
 * Lista todos os namespaces de um processo específico.
 *
 * Ex:
 *   ns_listar_para_pid(1234, stdout);
 */
int ns_listar_para_pid(pid_t pid, FILE *saida);

/**
 * Compara os namespaces de dois processos.
 *
 * Ex:
 *   ns_comparar_processos(1234, 5678, stdout);
 */
int ns_comparar_processos(pid_t pid1, pid_t pid2, FILE *saida);

/**
 * Mostra resumo dos namespaces do processo atual (self).
 *
 * Ex:
 *   ns_resumo_atual(stdout);
 */
int ns_resumo_atual(FILE *saida);

/* ==================== FUNÇÕES AVANÇADAS / EXTRA ==================== */

/**
 * Encontra processos que compartilham um namespace específico.
 *
 * Se inode_alvo == 0, usa o namespace do processo atual como referência.
 *
 * Ex:
 *   ns_encontrar_por_tipo("net", 0, stdout);
 */
int ns_encontrar_por_tipo(const char *tipo_namespace,
                          unsigned long long inode_alvo,
                          FILE *saida);

/**
 * Lista todos os namespaces ativos no sistema (por tipo),
 * contando inodes únicos.
 */
int ns_listar_ativos_sistema(FILE *saida);

/**
 * Mede overhead de criação de namespaces usando unshare()
 * (útil como experimento no relatório).
 */
int ns_medir_overhead_criacao(FILE *saida);

/**
 * Gera um relatório completo de isolamento do sistema
 * usando as funções acima.
 */
int ns_relatorio_sistema(FILE *saida);

#endif /* NAMESPACE_H */
