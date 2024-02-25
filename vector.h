#ifndef VECTOR_H
#define VECTOR_H

#include <stdio.h>
#include <stdlib.h>

struct vector {
    int *data;
    size_t size;
};

void init(struct vector *vec) {
    vec->data = NULL;
    vec->size = 0;
}

void push(struct vector *vec, int datum) {
    vec->size++;
    vec->data = (int*)realloc(vec->data, sizeof(int) * vec->size);
    if (vec->data == NULL) {
        perror("Realloc error\n");
        exit(EXIT_FAILURE);
    }
    vec->data[vec->size - 1] = datum;
}

void free_vec(struct vector *vec) {
    free(vec->data);
    vec->data = NULL;
    vec->size = 0;
}

#endif