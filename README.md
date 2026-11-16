# Algoritmo do Banqueiro

Programa em C que implementa o algoritmo do banqueiro para controlar pedidos e liberações de recursos. A quantidade de clientes e de tipos de recurso é definida pelos dados de entrada.

## Compilação

```bash
make
```

Isso gera o executável `banker`.

## Arquivos de entrada

Os arquivos devem estar na pasta de onde o programa será executado:

- `customer.txt`: cada linha representa um cliente e contém suas demandas máximas, separadas por vírgulas.
- `commands.txt`: contém os comandos `RQ` para solicitar recursos, `RL` para liberá-los e `*` para mostrar o estado atual.

Cada cliente e comando deve informar uma quantidade de valores compatível com os tipos de recurso passados na execução.

## Execução

Passe um número para cada tipo de recurso. Por exemplo, para três tipos:

```bash
./banker 10 5 7
```

Os números e os arquivos de entrada podem variar. O programa grava as respostas em `result.txt` e informa erros de entrada no terminal.

## Limpeza

```bash
make clean
```

Esse comando remove o executável `banker`.