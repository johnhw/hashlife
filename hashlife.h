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


typedef uint64_t node_id;
static const node_id UNUSED = 0;
static const node_id OFF = 1;
static const node_id ON = 2;


/* Base level node */
typedef struct node
{
    node_id id;
    uint64_t level;
    node_id a, b, c, d;                                                     // children    
    uint64_t pop;     
    // piggy back cache
    // same size as the table, but keyed on (from,j)
    node_id from;
    node_id to;
    uint64_t j;    
} node;


typedef struct node_table
{
    node *index;      
    uint64_t size;  // number of slots (always a power of 2)
    uint64_t count; // number of allocated slots            
} node_table;



/* Hash functions */
uint64_t mix64(uint64_t x);
uint64_t hash_quad(const uint64_t a, const uint64_t b, const uint64_t c, const uint64_t d);
void vacuum(node_table *table, node_id top);

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