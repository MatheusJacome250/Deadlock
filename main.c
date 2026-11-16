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
char *pular_bom(char *linha) {
    if (linha != NULL && strlen(linha) >= 3 && (unsigned char)linha[0] == 0xEF && (unsigned char)linha[1] == 0xBB && (unsigned char)linha[2] == 0xBF) {
        return linha + 3;
    }

    return linha;
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

        char *conteudo = linha;
        if (estado->customers == 0) {
            conteudo = pular_bom(linha);
        }

        if (!ler_linha_cliente(conteudo, estado->resources, valores)) {
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
void imprimir_vetor(FILE *saida, const int valores[], int quantidade) {
    for (int i = 0; i < quantidade; i++) {
        if (i > 0) {
            fputc(' ', saida);
        }

        fprintf(saida, "%d", valores[i]);
    }
}
int largura_vetor(const int valores[], int quantidade) {
    int largura = 0;

    for (int i = 0; i < quantidade; i++) {
        if (i > 0) {
            largura++;
        }

        int caracteres = snprintf(NULL, 0, "%d", valores[i]);
        if (caracteres < 0) {
            return -1;
        }

        largura += caracteres;
    }

    return largura;
}
int imprimir_estado(FILE *saida, const banker *estado) {
    if (saida == NULL || estado == NULL || estado->maximum == NULL || estado->allocation == NULL || estado->need == NULL || estado->available == NULL) {
        return 0;
    }

    int largura_maximo = 10;
    int largura_alocacao = 12;

    for (int i = 0; i < estado->customers; i++) {
        int maximo = largura_vetor(estado->maximum[i], estado->resources);
        int alocacao = largura_vetor(estado->allocation[i], estado->resources);

        if (maximo < 0 || alocacao < 0) {
            return 0;
        }

        if (maximo + 1 > largura_maximo) {
            largura_maximo = maximo + 1;
        }

        if (alocacao + 1 > largura_alocacao) {
            largura_alocacao = alocacao + 1;
        }
    }

    fprintf(saida, "%-*s|   %-*s|   NEED\n",
            largura_maximo, "MAXIMUM",
            largura_alocacao, "ALLOCATION");

    for (int i = 0; i < estado->customers; i++) {
        int maximo = largura_vetor(estado->maximum[i], estado->resources);
        int alocacao = largura_vetor(estado->allocation[i], estado->resources);

        imprimir_vetor(saida, estado->maximum[i], estado->resources);
        for (int espacos = maximo; espacos < largura_maximo; espacos++) {
            fputc(' ', saida);
        }

        fputs("|   ", saida);

        imprimir_vetor(saida, estado->allocation[i], estado->resources);
        for (int espacos = alocacao; espacos < largura_alocacao; espacos++) {
            fputc(' ', saida);
        }

        fputs("|   ", saida);
        imprimir_vetor(saida, estado->need[i], estado->resources);
        fputs(" \n", saida);
    }

    fputs("AVAILABLE ", saida);
    imprimir_vetor(saida, estado->available, estado->resources);
    fputs(" \n", saida);

    return !ferror(saida);
}
int registrar_pedido(FILE *saida, const banker *estado, int cliente, const int pedido[], resultado_pedido resultado) {
    if (saida == NULL || estado == NULL || pedido == NULL || estado->available == NULL || cliente < 0 ||
        cliente >= estado->customers) {
        return 0;
    }

    if (resultado == PEDIDO_ACEITO) {
        fprintf(saida, "Allocate to customer %d the resources ", cliente);
        imprimir_vetor(saida, pedido, estado->resources);
        fputc('\n', saida);
    } else if (resultado == PEDIDO_SEM_RECURSOS) {
        fputs("The resources ", saida);
        imprimir_vetor(saida, estado->available, estado->resources);
        fprintf(saida, " are not enough to customer %d request ", cliente);
        imprimir_vetor(saida, pedido, estado->resources);
        fputs(" \n", saida);
    } else if (resultado == PEDIDO_INSEGURO) {
        fprintf(saida, "The customer %d request ", cliente);
        imprimir_vetor(saida, pedido, estado->resources);
        fputs(" was denied because result in an unsafe state\n", saida);
    } else if (resultado == PEDIDO_EXCEDE_NECESSIDADE) {
        fprintf(saida, "The customer %d request ", cliente);
        imprimir_vetor(saida, pedido, estado->resources);
        fputs(" was denied because exceed its maximum allocation\n", saida);
    } else {
        return 0;
    }

    return !ferror(saida);
}
int registrar_liberacao(FILE *saida, const banker *estado, int cliente, const int liberacao[]) {
    if (saida == NULL || estado == NULL || liberacao == NULL || cliente < 0 || cliente >= estado->customers) {
        return 0;
    }

    fprintf(saida, "Release from customer %d the resources ", cliente);
    imprimir_vetor(saida, liberacao, estado->resources);
    fputc('\n', saida);

    return !ferror(saida);
}
int interpretar_comando(char *linha, const banker *estado, char *tipo, int *cliente, int valores[]) {
    if (linha == NULL || estado == NULL || tipo == NULL || cliente == NULL || valores == NULL || estado->customers <= 0 || estado->resources <= 0) {
        return 0;
    }

    char *contexto = NULL;
    char *parte = strtok_r(linha, " \t\r\n", &contexto);

    if (parte == NULL) {
        return 0;
    }

    if (strcmp(parte, "*") == 0) {
        if (strtok_r(NULL, " \t\r\n", &contexto) != NULL) {
            return 0;
        }

        *tipo = '*';
        return 1;
    }

    if (strcmp(parte, "RQ") == 0) {
        *tipo = 'Q';
    } else if (strcmp(parte, "RL") == 0) {
        *tipo = 'L';
    } else {
        return 0;
    }

    parte = strtok_r(NULL, " \t\r\n", &contexto);
    if (!converter_numero(parte, cliente) || *cliente >= estado->customers) {
        return 0;
    }

    for (int j = 0; j < estado->resources; j++) {
        parte = strtok_r(NULL, " \t\r\n", &contexto);

        if (!converter_numero(parte, &valores[j])) {
            return 0;
        }
    }

    if (strtok_r(NULL, " \t\r\n", &contexto) != NULL) {
        return 0;
    }

    return 1;
}
int processar_comandos(banker *estado, const char *nome_arquivo, FILE *saida) {
    if (estado == NULL || nome_arquivo == NULL || saida == NULL || estado->customers <= 0 || estado->resources <= 0) {
        fprintf(stderr, "Erro: estado invalido para processar comandos.\n");
        return 0;
    }

    FILE *arquivo = fopen(nome_arquivo, "r");
    if (arquivo == NULL) {
        fprintf(stderr, "Erro: nao foi possivel abrir %s.\n", nome_arquivo);
        return 0;
    }

    int *valores = malloc(estado->resources * sizeof *valores);
    if (valores == NULL) {
        fprintf(stderr, "Erro: memoria insuficiente.\n");
        fclose(arquivo);
        return 0;
    }

    char *linha = NULL;
    size_t capacidade = 0;
    int numero_linha = 0;
    int leitura_ok = 1;

    while (getline(&linha, &capacidade, arquivo) != -1) {
        numero_linha++;

        char *conteudo = linha;
        if (numero_linha == 1) {
            conteudo = pular_bom(linha);
        }

        char tipo;
        int cliente = 0;

        if (!interpretar_comando(conteudo, estado, &tipo, &cliente, valores)) {
            fprintf(stderr, "Erro: comando invalido na linha %d de %s.\n", numero_linha, nome_arquivo);
            leitura_ok = 0;
            break;
        }

        if (tipo == '*') {
            if (!imprimir_estado(saida, estado)) {
                fprintf(stderr, "Erro ao escrever o resultado.\n");
                leitura_ok = 0;
                break;
            }

            continue;
        }

        if (tipo == 'Q') {
            resultado_pedido resultado = request_resources(estado, cliente, valores);

            if (resultado == PEDIDO_ERRO || !registrar_pedido(saida, estado, cliente, valores, resultado)) {
                fprintf(stderr, "Erro ao processar o pedido da linha %d.\n", numero_linha);
                leitura_ok = 0;
                break;
            }
        } else {
            if (!release_resources(estado, cliente, valores)) {
                fprintf(stderr, "Erro: liberacao invalida na linha %d.\n",
                        numero_linha);
                leitura_ok = 0;
                break;
            }

            if (!registrar_liberacao(saida, estado, cliente, valores)) {
                fprintf(stderr, "Erro ao escrever o resultado.\n");
                leitura_ok = 0;
                break;
            }
        }
    }

    if (ferror(arquivo)) {
        fprintf(stderr, "Erro ao ler %s.\n", nome_arquivo);
        leitura_ok = 0;
    }

    if (numero_linha == 0 && leitura_ok) {
        fprintf(stderr, "Erro: %s esta vazio.\n", nome_arquivo);
        leitura_ok = 0;
    }

    free(linha);
    free(valores);
    fclose(arquivo);

    return leitura_ok;
}
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: ./banker <quantidade de cada recurso>\n");
        return EXIT_FAILURE;
    }

    banker estado = {0};
    estado.resources = argc - 1;

    estado.available = calloc(estado.resources, sizeof *estado.available);
    if (estado.available == NULL) {
        fprintf(stderr, "Erro: memoria insuficiente.\n");
        return EXIT_FAILURE;
    }

    for (int i = 0; i < estado.resources; i++) {
        if (!converter_numero(argv[i + 1], &estado.available[i])) {
            fprintf(stderr, "Erro: quantidade invalida para o recurso %d.\n", i);
            liberar_banqueiro(&estado);
            return EXIT_FAILURE;
        }
    }

    if (!carregar_clientes(&estado, "customer.txt")) {
        liberar_banqueiro(&estado);
        return EXIT_FAILURE;
    }

    FILE *saida = fopen("result.txt", "w");
    if (saida == NULL) {
        fprintf(stderr, "Erro: nao foi possivel criar result.txt.\n");
        liberar_banqueiro(&estado);
        return EXIT_FAILURE;
    }

    int sucesso = processar_comandos(&estado, "commands.txt", saida);

    if (fclose(saida) != 0) {
        fprintf(stderr, "Erro ao fechar result.txt.\n");
        sucesso = 0;
    }

    liberar_banqueiro(&estado);

    if (sucesso) {
        return EXIT_SUCCESS;
    }

    return EXIT_FAILURE;
}
