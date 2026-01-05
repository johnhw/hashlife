#ifndef HASHLIFE_H
#define HASHLIFE_H
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

#define min(a, b) ((a) < (b) ? (a) : (b))
#define INIT_TABLE_SIZE 4096
#define ZERO_CACHE_MAX_SIZE 256
#define MAX_J_CACHE 16

typedef uint64_t node_id;

/* Base level node */
typedef struct node
{
    node_id id;
    uint64_t level;
    node_id a, b, c, d;                                                     // children
    node_id aa, ab, ac, ad, ba, bb, bc, bd, ca, cb, cc, cd, da, db, dc, dd; // grandchildren
    node_id next;                                                           // cached natural successor
    uint64_t pop;
    int32_t ref_count; // 0 = ready to free, negative = immortal
    uint64_t last_used;
    node_id nexts[MAX_J_CACHE]; // alternatively, we could hash this into a global pool
} node;

typedef struct node_table
{
    node *index;
    uint64_t time;  // current time for LRU
    uint64_t size;  // number of slots (always a power of 2)
    uint64_t count; // number of allocated slots
    uint64_t mask; // 2^n - 1 for indexing
    node_id on, off;                   // base on and off nodes
    node_id zeros[ZERO_CACHE_MAX_SIZE]; // all zero nodes up to max size    
} node_table;

/* Hash functions */
uint64_t mix64(uint64_t x);
uint64_t hash_quad(const uint64_t a, const uint64_t b, const uint64_t c, const uint64_t d);

/* Reference counting */
void decref(node_table *table, node_id id);
void incref(node_table *table, node_id id);
void gc(node_table *table);

/* Table operations */
node_id get_zero(node_table *table, uint64_t k);
node *lookup(node_table *table, node_id hash);
node_id join(node_table *table, node_id a_hash, node_id b_hash, node_id c_hash, node_id d_hash);

/* Initalisation */
void init_table(node_table *table);
node_table *create_table(uint64_t initial_size);
node_id base_life(node_id a, node_id b, node_id c, node_id d, node_id e, node_id f, node_id g, node_id h, node_id i);
node_id life_4x4(node_table *table, node_id m_h);

/* Advance helpers */
node_id successor(node_table *table, node_id id, uint64_t j);
node_id next(node_table *table, node_id id, uint64_t j);

/* Node operations */
node_id centre(node_table *table, node_id m_h);
node_id advance(node_table *table, node_id id, uint64_t j);
node_id skip(node_table *table, node_id id, uint64_t j);
node_id inner(node_table *table, node_id id);
bool is_padded(node_table *table, node_id id);
node_id crop(node_table *table, node_id id);
node_id pad(node_table *table, node_id id);

/* Cell access */
node_id set_cell(node_table *table, node_id id, uint64_t x, uint64_t y, bool state);
float get_cell(node_table *table, node_id id, uint64_t x, uint64_t y, uint64_t level);

#endif // HASHLIFE_H