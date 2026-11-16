#include <stdlib.h>

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