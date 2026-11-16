typedef struct banker {
    int *available;
    int **maximum;
    int **allocation;
    int **need;

    int customers;
    int resources;
} banker;

