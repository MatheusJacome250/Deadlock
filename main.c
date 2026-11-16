#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
typedef struct banker {
    int *available;
    int **maximum;
    int **allocation;
    int **need;

    int customers;
    int resources;
} banker;
typedef enum {
    PEDIDO_ACEITO,
    PEDIDO_EXCEDE_NECESSIDADE,
    PEDIDO_SEM_RECURSOS,
    PEDIDO_INSEGURO,
    PEDIDO_ERRO
} resultado_pedido;

int **criar_matriz(int linhas, int colunas) {
    if (linhas <= 0 || colunas <= 0) {
        return NULL;
    }

    int **matriz = calloc(linhas, sizeof *matriz);
    if (matriz == NULL) {
        return NULL;
    }

    for (int i = 0; i < linhas; i++) {
        matriz[i] = calloc(colunas, sizeof *matriz[i]);

        if (matriz[i] == NULL) {
            for (int j = 0; j < i; j++) {
                free(matriz[j]);
            }

            free(matriz);
            return NULL;
        }
    }

    return matriz;
}

void liberar_matriz(int **matriz, int linhas) {
    if (matriz == NULL) {
        return;
    }

    for (int i = 0; i < linhas; i++) {
        free(matriz[i]);
    }

    free(matriz);
}

int converter_numero(const char *texto, int *numero) {
    if (texto == NULL || numero == NULL) {
        return 0;
    }

    while (isspace((unsigned char)*texto)) {
        texto++;
    }

    if (!isdigit((unsigned char)*texto)) {
        return 0;
    }

    errno = 0;
    char *fim;
    long valor = strtol(texto, &fim, 10);

    if (errno != 0 || valor > INT_MAX) {
        return 0;
    }

    while (isspace((unsigned char)*fim)) {
        fim++;
    }

    if (*fim != '\0') {
        return 0;
    }

    *numero = (int)valor;
    return 1;
}

int ler_linha_cliente(char *linha, int recursos, int *valores) {
    if (linha == NULL || valores == NULL || recursos <= 0) {
        return 0;
    }

    char *campo = linha;

    for (int i = 0; i < recursos; i++) {
        char *virgula = strchr(campo, ',');

        if (i < recursos - 1) {
            if (virgula == NULL) {
                return 0;
            }

            *virgula = '\0';
        } else {
            if (virgula != NULL) {
                return 0;
            }
        }

        if (!converter_numero(campo, &valores[i])) {
            return 0;
        }

        if (i < recursos - 1) {
            campo = virgula + 1;
        }
    }

    return 1;
}
int carregar_clientes(banker *estado, const char *nome_arquivo) {
    if (estado == NULL || nome_arquivo == NULL || estado->resources <= 0 || estado->customers != 0 || estado->maximum != NULL || estado->allocation != NULL || estado->need != NULL) {
        fprintf(stderr, "Erro: estado invalido para ler os clientes.\n");
        return 0;
    }

    FILE *arquivo = fopen(nome_arquivo, "r");
    if (arquivo == NULL) {
        fprintf(stderr, "Erro: nao foi possivel abrir %s.\n", nome_arquivo);
        return 0;
    }

    char *linha = NULL;
    size_t capacidade = 0;
    int leitura_ok = 1;

    while (getline(&linha, &capacidade, arquivo) != -1) {
        if (estado->customers == INT_MAX) {
            fprintf(stderr, "Erro: clientes demais em %s.\n", nome_arquivo);
            leitura_ok = 0;
            break;
        }

        int *valores = calloc(estado->resources, sizeof *valores);
        if (valores == NULL) {
            fprintf(stderr, "Erro: memoria insuficiente.\n");
            leitura_ok = 0;
            break;
        }

        if (!ler_linha_cliente(linha, estado->resources, valores)) {
            fprintf(stderr, "Erro: linha %d invalida em %s.\n", estado->customers + 1, nome_arquivo);
            free(valores);
            leitura_ok = 0;
            break;
        }

        int **nova_matriz = realloc(
            estado->maximum,
            (estado->customers + 1) * sizeof *nova_matriz
        );

        if (nova_matriz == NULL) {
            fprintf(stderr, "Erro: memoria insuficiente.\n");
            free(valores);
            leitura_ok = 0;
            break;
        }

        estado->maximum = nova_matriz;
        estado->maximum[estado->customers] = valores;
        estado->customers++;
    }

    if (ferror(arquivo)) {
        fprintf(stderr, "Erro ao ler %s.\n", nome_arquivo);
        leitura_ok = 0;
    }

    free(linha);
    fclose(arquivo);

    if (estado->customers == 0 && leitura_ok) {
        fprintf(stderr, "Erro: %s esta vazio.\n", nome_arquivo);
        leitura_ok = 0;
    }

    if (!leitura_ok) {
        liberar_matriz(estado->maximum, estado->customers);
        estado->maximum = NULL;
        estado->customers = 0;
        return 0;
    }

    estado->allocation = criar_matriz(estado->customers, estado->resources);
    estado->need = criar_matriz(estado->customers, estado->resources);

    if (estado->allocation == NULL || estado->need == NULL) {
        fprintf(stderr, "Erro: memoria insuficiente.\n");
        liberar_matriz(estado->maximum, estado->customers);
        liberar_matriz(estado->allocation, estado->customers);
        liberar_matriz(estado->need, estado->customers);
        estado->maximum = NULL;
        estado->allocation = NULL;
        estado->need = NULL;
        estado->customers = 0;
        return 0;
    }

    for (int i = 0; i < estado->customers; i++) {
        for (int j = 0; j < estado->resources; j++) {
            estado->need[i][j] = estado->maximum[i][j];
        }
    }

    return 1;
}
int estado_seguro(const banker *estado) {
    if (estado == NULL || estado->available == NULL || estado->allocation == NULL || estado->need == NULL || estado->customers <= 0 || estado->resources <= 0) {
        return -1;
    }

    int *work = malloc(estado->resources * sizeof *work);
    int *finish = calloc(estado->customers, sizeof *finish);

    if (work == NULL || finish == NULL) {
        free(work);
        free(finish);
        return -1;
    }

    for (int j = 0; j < estado->resources; j++) {
        work[j] = estado->available[j];
    }

    int concluidos = 0;

    while (concluidos < estado->customers) {
        int avancou = 0;

        for (int i = 0; i < estado->customers; i++) {
            if (finish[i]) {
                continue;
            }

            int pode_terminar = 1;

            for (int j = 0; j < estado->resources; j++) {
                if (estado->need[i][j] > work[j]) {
                    pode_terminar = 0;
                    break;
                }
            }

            if (pode_terminar) {
                for (int j = 0; j < estado->resources; j++) {
                    work[j] += estado->allocation[i][j];
                }

                finish[i] = 1;
                concluidos++;
                avancou = 1;
            }
        }

        if (!avancou) {
            break;
        }
    }

    int seguro = (concluidos == estado->customers);

    free(work);
    free(finish);

    return seguro;
}
resultado_pedido request_resources(banker *estado, int cliente, const int pedido[]) {
    if (estado == NULL || pedido == NULL || estado->available == NULL ||estado->allocation == NULL || estado->need == NULL || cliente < 0 || cliente >= estado->customers) {
        return PEDIDO_ERRO;
    }

    for (int j = 0; j < estado->resources; j++) {
        if (pedido[j] < 0) {
            return PEDIDO_ERRO;
        }

        if (pedido[j] > estado->need[cliente][j]) {
            return PEDIDO_EXCEDE_NECESSIDADE;
        }
    }

    for (int j = 0; j < estado->resources; j++) {
        if (pedido[j] > estado->available[j]) {
            return PEDIDO_SEM_RECURSOS;
        }
    }

    for (int j = 0; j < estado->resources; j++) {
        estado->available[j] -= pedido[j];
        estado->allocation[cliente][j] += pedido[j];
        estado->need[cliente][j] -= pedido[j];
    }

    int seguro = estado_seguro(estado);

    if (seguro != 1) {
        for (int j = 0; j < estado->resources; j++) {
            estado->available[j] += pedido[j];
            estado->allocation[cliente][j] -= pedido[j];
            estado->need[cliente][j] += pedido[j];
        }

        if (seguro == 0) {
            return PEDIDO_INSEGURO;
        }

        return PEDIDO_ERRO;
    }

    return PEDIDO_ACEITO;
}
int release_resources(banker *estado, int cliente, const int liberacao[]) {
    if (estado == NULL || liberacao == NULL || estado->available == NULL || estado->allocation == NULL || estado->need == NULL || cliente < 0 || cliente >= estado->customers) {
        return 0;
    }

    for (int j = 0; j < estado->resources; j++) {
        if (liberacao[j] < 0 || liberacao[j] > estado->allocation[cliente][j]) {
            return 0;
        }
    }

    for (int j = 0; j < estado->resources; j++) {
        estado->available[j] += liberacao[j];
        estado->allocation[cliente][j] -= liberacao[j];
        estado->need[cliente][j] += liberacao[j];
    }

    return 1;
}

void liberar_banqueiro(banker *estado) {
    if (estado == NULL) {
        return;
    }

    liberar_matriz(estado->maximum, estado->customers);
    liberar_matriz(estado->allocation, estado->customers);
    liberar_matriz(estado->need, estado->customers);
    free(estado->available);

    estado->maximum = NULL;
    estado->allocation = NULL;
    estado->need = NULL;
    estado->available = NULL;
    estado->customers = 0;
    estado->resources = 0;
}