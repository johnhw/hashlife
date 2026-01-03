#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

typedef struct htable 
{
    void *ptr;
    uint64_t element_size;
    uint64_t size;
    uint64_t count;
    /* hash function */
    uint64_t (*hash_func)(void *element);
    /* equality function */ 
    bool (*equals_func)(void *a, void *b);
} htable;
