// tests/test_cpu.c

// Programa de teste para o módulo de monitoramento de CPU (cpu_monitor.c)
#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>
#include <string.h>

#include "../include/monitor.h"

/* ==================== WORKLOAD CPU-INTENSIVE ==================== */

/**
 * Gera uma carga de CPU intensa por aproximadamente "segundos".
 */
static void workload_cpu_intensivo(int segundos)
{
    volatile double x = 0.0;
    time_t inicio = time(NULL);

    if (inicio == (time_t)-1) {
        perror("time");
        return;
    }

    printf("[WORKLOAD] Gerando carga por %d segundos...\n", segundos);

    while (time(NULL) - inicio < segundos) {
        for (int i = 0; i < 1000000; i++) {
            x += i * 0.000001;
            if (x > 1e9) x = 0.0;
        }
    }

    (void)x; // evita warning
    printf("[WORKLOAD] Carga concluída.\n");
}

/* ==================== TESTE 1: USO INSTANTÂNEO DO PRÓPRIO PROCESSO ==================== */

static int teste_uso_instantaneo_self(void)
{
    printf("=== TESTE 1: cpu_obter_uso_instantaneo no próprio processo ===\n");

    pid_t pid = getpid();
    printf("Monitorando PID: %d\n", pid);

    for (int i = 0; i < 5; i++) {
        // Alterna entre idle e carga
        if (i % 2 == 0) {
            printf("Amostra %d: Estado idle...\n", i + 1);
            sleep(1);
        } else {
            printf("Amostra %d: Gerando carga...\n", i + 1);
            workload_cpu_intensivo(1);
        }

        double uso = cpu_obter_uso_instantaneo(pid);
        if (uso < 0.0) {
            fprintf(stderr, "Falha em cpu_obter_uso_instantaneo (iteracao %d)\n", i);
            return -1;
        }

        printf("Amostra %d: uso de CPU = %.2f%%\n", i + 1, uso);
    }

    printf("Teste 1 concluído.\n\n");
    return 0;
}

/* ==================== TESTE 2: MONITORAR PROCESSO FILHO VIA CSV ==================== */

static int teste_monitorar_processo_filho(void)
{
    printf("=== TESTE 2: cpu_monitorar_pid_csv em processo filho ===\n");

    int intervalo_ms   = 800;  // 800 ms entre amostras
    int amostras       = 8;    // número de amostras
    int duracao_filho  = (intervalo_ms * amostras) / 1000 + 5; // +5s de folga
    const char *arquivo_csv = "cpu_test_child.csv";

    pid_t filho = fork();
    if (filho < 0) {
        perror("fork");
        return -1;
    }

    if (filho == 0) {
        // ----- Processo filho: gera carga de CPU -----
        printf("[FILHO %d] Iniciando workload de CPU por %d segundos...\n",
               getpid(), duracao_filho);
        workload_cpu_intensivo(duracao_filho);
        printf("[FILHO %d] Workload concluído. Saindo.\n", getpid());
        _exit(0);
    }

    // ----- Processo pai: monitora o filho -----
    printf("[PAI %d] Criado filho PID %d\n", getpid(), filho);

    // Dá 1s pro filho começar a trabalhar
    sleep(1);

    if (!processo_existe(filho)) {
        fprintf(stderr, "ERRO: Processo filho %d não existe mais!\n", filho);
        return -1;
    }

    printf("[PAI] Iniciando monitoramento (PID %d)\n", filho);
    printf("[PAI] Salvando em %s (%d amostras, intervalo: %dms)\n",
           arquivo_csv, amostras, intervalo_ms);

    FILE *fp = fopen(arquivo_csv, "w");
    if (!fp) {
        fprintf(stderr, "Erro ao abrir %s: %s\n", arquivo_csv, strerror(errno));
        kill(filho, SIGTERM);
        return -1;
    }

    int rc = cpu_monitorar_pid_csv(filho, intervalo_ms, amostras, fp);
    fclose(fp);

    if (rc != 0) {
        fprintf(stderr, "cpu_monitorar_pid_csv retornou erro (%d)\n", rc);
    } else {
        printf("[PAI] Monitoramento concluído. Dados salvos em %s\n", arquivo_csv);
    }

    // Espera o filho terminar naturalmente, com timeout
    int status = 0;
    int timeout = 10; // segundos

    for (int i = 0; i < timeout; i++) {
        pid_t r = waitpid(filho, &status, WNOHANG);
        if (r == filho) {
            break; // terminou
        } else if (r == -1) {
            perror("waitpid");
            break;
        }
        sleep(1);
    }

    // Se ainda estiver vivo, força término
    if (waitpid(filho, &status, WNOHANG) == 0) {
        printf("[PAI] Filho ainda rodando após timeout, enviando SIGTERM...\n");
        kill(filho, SIGTERM);
        sleep(1);
        if (waitpid(filho, &status, WNOHANG) == 0) {
            printf("[PAI] Ainda vivo, enviando SIGKILL...\n");
            kill(filho, SIGKILL);
            waitpid(filho, &status, 0);
        }
    }

    if (WIFEXITED(status)) {
        printf("[PAI] Filho terminou com código %d\n", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        printf("[PAI] Filho terminou por sinal %d\n", WTERMSIG(status));
    }

    printf("Teste 2 concluído.\n\n");
    return rc;
}

/* ==================== TESTE 3: VERIFICAÇÃO DE PROCESSO INEXISTENTE ==================== */

static int teste_processo_inexistente(void)
{
    printf("=== TESTE 3: Verificação de processo inexistente ===\n");

    // Usa um PID inválido por definição (negativo) para o teste
    pid_t pid_invalido = -1;

    printf("Verificando PID %d (inválido)...\n", pid_invalido);

    if (processo_existe(pid_invalido)) {
        printf("ERRO: processo_existe retornou verdadeiro para PID inválido\n");
        return -1;
    }

    double uso = cpu_obter_uso_instantaneo(pid_invalido);
    if (uso >= 0.0) {
        printf("ERRO: cpu_obter_uso_instantaneo não sinalizou erro para PID inválido\n");
        return -1;
    }

    printf("PID inválido corretamente tratado como erro.\n");
    printf("Teste 3 concluído.\n\n");
    return 0;
}

/* ==================== FUNÇÃO PRINCIPAL ==================== */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("=============================================\n");
    printf("  TESTES DO MÓDULO DE CPU - RESOURCE MONITOR\n");
    printf("=============================================\n\n");

    // Garante estado limpo do módulo de CPU
    cpu_resetar_estado();

    int erro = 0;

    if (teste_uso_instantaneo_self() != 0) {
        fprintf(stderr, "ERRO no Teste 1\n");
        erro = 1;
    }

    if (teste_monitorar_processo_filho() != 0) {
        fprintf(stderr, "ERRO no Teste 2\n");
        erro = 1;
    }

    if (teste_processo_inexistente() != 0) {
        fprintf(stderr, "ERRO no Teste 3\n");
        erro = 1;
    }

    if (!erro) {
        printf("✅ Todos os testes de CPU foram executados com sucesso!\n");
        printf("📊 Confira o arquivo 'cpu_test_child.csv' para analisar as métricas.\n");
    } else {
        printf("❌ Alguns testes falharam. Verifique as mensagens acima.\n");
    }

    return erro ? EXIT_FAILURE : EXIT_SUCCESS;
}
