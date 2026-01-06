#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

#define min(a, b) ((a) < (b) ? (a) : (b))
#define INIT_TABLE_SIZE 0x100000

typedef uint64_t node_id;
static const node_id UNUSED = 0ULL;
static const node_id ON = 1ULL;
static const node_id OFF = 2ULL;

/* Base level node */
typedef struct node
{
    node_id id;
    uint64_t level;
    node_id a, b, c, d; // children
    node_id next;       // cached natural successor
    uint64_t pop;
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

/* Table operations */
node_id get_zero(node_table *table, uint64_t k);
node *lookup(node_table *table, node_id hash);
node_id join(node_table *table, node_id a_hash, node_id b_hash, node_id c_hash, node_id d_hash);

/* Initalisation */
node_table *create_table(uint64_t size);
node_id base_life(node_id a, node_id b, node_id c, node_id d, node_id e, node_id f, node_id g, node_id h, node_id i);
node_id life_4x4(node_table *table, node_id m_h);

/* Advance helpers */
node_id successor(node_table *table, node_id id, uint64_t j);

/* Node operations */
node_id centre(node_table *table, node_id m_h);
node_id advance(node_table *table, node_id id, uint64_t j);
node_id inner(node_table *table, node_id id);
bool is_padded(node_table *table, node_id id);
node_id crop(node_table *table, node_id id);
node_id pad(node_table *table, node_id id);

/* Cell access */
node_id set_cell(node_table *table, node_id id, uint64_t x, uint64_t y, bool state);
float get_cell(node_table *table, node_id id, uint64_t x, uint64_t y, uint64_t level);

/* SplitMix64 mixing function */
uint64_t mix64(uint64_t x)
{
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

/* Hash four values into one */
uint64_t hash_quad(const uint64_t a, const uint64_t b, const uint64_t c, const uint64_t d)
{
    uint64_t h = 0x243f6a8885a308d3ULL;
    h ^= mix64(a + 0x9e3779b97f4a7c15ULL * 1);
    h = (h * 0x9e3779b97f4a7c15ULL) ^ (h >> 32);
    h ^= mix64(b + 0x9e3779b97f4a7c15ULL * 2);
    h = (h * 0x9e3779b97f4a7c15ULL) ^ (h >> 32);
    h ^= mix64(c + 0x9e3779b97f4a7c15ULL * 3);
    h = (h * 0x9e3779b97f4a7c15ULL) ^ (h >> 32);
    h ^= mix64(d + 0x9e3779b97f4a7c15ULL * 4);
    h = (h * 0x9e3779b97f4a7c15ULL) ^ (h >> 32);
    return mix64(h);
}

/* Return a zero node at level k */
node_id get_zero(node_table *table, uint64_t k)
{
    if (k == 0)
        return OFF;
    node_id z = OFF;
    for (uint64_t i = 0; i < k; i++)
        z = join(table, z, z, z, z);
    return z;
}

/* Centre a node, by surrounding it with zeros of the same size.
    Returns the new node.
 */
node_id centre(node_table *table, node_id m_h)
{
    node_id z;
    node *m = lookup(table, m_h);
    z = get_zero(table, m->level - 1);
    node_id a = join(table, z, z, z, m->a);
    node_id b = join(table, z, z, m->b, z);
    node_id c = join(table, z, m->c, z, z);
    node_id d = join(table, m->d, z, z, z);
    return join(table, a, b, c, d);
}

node *lookup(node_table *table, node_id id)
{
    uint64_t mask = table->size - 1;
    uint64_t index = id & mask;
    node *n = &table->index[index];
    while (n->id != UNUSED && n->id != id)
    {
        // linear probing
        index = (index + 1) & mask;
        n = &table->index[index];
    }
    return n;
}

/* Double the size of the table, reinserting nodes */
void resize_table(node_table *table)
{
    uint64_t new_size = table->size * 2;
    node *new_index = (node *)calloc(new_size, sizeof(node));
    uint64_t new_mask = new_size - 1;
    uint64_t new_index_pos;
    node *old_n;
    for (uint64_t i = 0; i < table->size; i++)
    {
        old_n = &table->index[i];
        // perform re-insertion of all nodes
        if (old_n->id != UNUSED)
        {
            new_index_pos = old_n->id & new_mask;
            while (new_index[new_index_pos].id != 0)
                new_index_pos = (new_index_pos + 1) & new_mask;
            new_index[new_index_pos] = *old_n;
        }
    }
    free(table->index);
    table->index = new_index;
    table->size = new_size;
}


/* Join four nodes.
   - If the node exists, return it from the table.
   - Otherwise, intern it.
   - Returns the hash of the joined node.
   NOTE: calling this may resize the table, rendering any
   existing pointers INVALID! We must look up by IDs (which are stable)
   after any call that might resize the table.
*/

node_id join(node_table *table, node_id a_hash, node_id b_hash, node_id c_hash, node_id d_hash)
{
    uint64_t hash = hash_quad(a_hash, b_hash, c_hash, d_hash);
    node *n = lookup(table, hash);
    while (n->id != UNUSED)
    {
        if (n->a == a_hash && n->b == b_hash && n->c == c_hash && n->d == d_hash)
            return n->id;   // found it
        hash = mix64(hash); // doppleganger, create a new unique id
        n = lookup(table, hash);
    }
    // not found, so create it
    n->id = hash;
    // set children
    n->a = a_hash;
    n->b = b_hash;
    n->c = c_hash;
    n->d = d_hash;
    // set population
    node *a = lookup(table, a_hash);
    node *b = lookup(table, b_hash);
    node *c = lookup(table, c_hash);
    node *d = lookup(table, d_hash);
    n->level = a->level + 1;
    n->pop = a->pop + b->pop + c->pop + d->pop;

    // resize the table if necessary
    if (table->count * 4 >= table->size)
        resize_table(table);

    return hash;
}

static inline node_id sucjoin(node_table *table, node_id a, node_id b, node_id c, node_id d, uint64_t j)
{
    return successor(table, join(table, a, b, c, d), j);
}

/* Find the successor of the given node, 2^level-2 steps in the future */
node_id successor(node_table *table, node_id id, uint64_t j)
{
    node *n = lookup(table, id);
    uint64_t level = n->level;
    if (n->pop == 0) // empty
        return n->a;
    if (level == 2) // base case
        return life_4x4(table, id);
    if (j == 0)
        j = level - 2;
    if (j == level - 2 && n->next) // cached
        return n->next;

    node_id c1, c2, c3, c4, c5, c6, c7, c8, c9;
    // copy the actual nodes to prevent changes during lookups
    node a = *lookup(table, n->a);
    node b = *lookup(table, n->b);
    node c = *lookup(table, n->c);
    node d = *lookup(table, n->d);

    c1 = sucjoin(table, a.a, a.b, a.c, a.d, j);
    c2 = sucjoin(table, a.b, b.a, a.d, b.c, j);
    c3 = sucjoin(table, b.a, b.b, b.c, b.d, j);
    c4 = sucjoin(table, a.c, a.d, c.a, c.b, j);
    c5 = sucjoin(table, a.d, b.c, c.b, d.a, j);
    c6 = sucjoin(table, b.c, b.d, d.a, d.b, j);
    c7 = sucjoin(table, c.a, c.b, c.c, c.d, j);
    c8 = sucjoin(table, c.b, d.a, c.d, d.c, j);
    c9 = sucjoin(table, d.a, d.b, d.c, d.d, j);

    /* Not the natural successor; combine parts */
    if (j < level - 2)
    {
        node *c1n = lookup(table, c1);
        node *c2n = lookup(table, c2);
        node *c3n = lookup(table, c3);
        node *c4n = lookup(table, c4);
        node *c5n = lookup(table, c5);
        node *c6n = lookup(table, c6);
        node *c7n = lookup(table, c7);
        node *c8n = lookup(table, c8);
        node *c9n = lookup(table, c9);

        return join(table,
                    join(table, c1n->d, c2n->c, c4n->b, c5n->a),
                    join(table, c2n->d, c3n->c, c5n->b, c6n->a),
                    join(table, c4n->d, c5n->c, c7n->b, c8n->a),
                    join(table, c5n->d, c6n->c, c8n->b, c9n->a));
    }
    else
    {
        /* Natural successor */
        node_id next = join(table,
                            sucjoin(table, c1, c2, c4, c5, j),
                            sucjoin(table, c2, c3, c5, c6, j),
                            sucjoin(table, c4, c5, c7, c8, j),
                            sucjoin(table, c5, c6, c8, c9, j));
        lookup(table, id)->next = next;
        return next;
    }
}

/* Advance time by j steps.
    - Find the MSB of j
    - Make sure that the node has a level at least big enough

*/
node_id advance(node_table *table, node_id id, uint64_t steps)
{
    node *n = lookup(table, id);
    uint64_t level = n->level;
    // ensure the node is big enough; n->level must be > log2(steps)+2
    while ((1ULL << (level - 2)) < steps)
    {
        // pad the node
        id = centre(table, id);
        level++;
    }
    // pad to the centre
    id = centre(table, id);
    uint64_t j = 1;
    while (steps > 0)
    {
        if (steps & 1)
            id = centre(table, successor(table, id, j));
        j++;
        steps >>= 1;
    }
    // crop for the caller
    return crop(table, id);
}

/* Compute the life rule on the 3x3 neighbourhood

    a b c
    d e f
    g h i

    Returns either the hash of the "on" (=2) or "off" (=1) base node.
*/
node_id base_life(node_id a, node_id b, node_id c, node_id d, node_id e, node_id f, node_id g, node_id h, node_id i)
{
    /* hash 1 = off, hash 2 = on, by definition*/
    int pop_sum = (a - 1) + (b - 1) + (c - 1) + (d - 1) + (f - 1) + (g - 1) + (h - 1) + (i - 1);
    return pop_sum == 3 || (pop_sum == 2 && e == 2) ? ON : OFF;
}

node_id life_4x4(node_table *table, node_id id)
{
    node *n = lookup(table, id);
    // k = 2
    node_id na, nb, nc, nd;
    node *a = lookup(table, n->a);
    node *b = lookup(table, n->b);
    node *c = lookup(table, n->c);
    node *d = lookup(table, n->d);
    na = base_life(a->a, a->b, b->a, a->c, a->d, b->b, c->a, c->b, d->a);
    nb = base_life(a->b, b->a, b->b, a->d, b->b, b->d, c->b, d->a, d->b);
    nc = base_life(a->c, a->d, b->b, c->a, c->b, d->a, c->c, c->d, d->c);
    nd = base_life(a->d, b->b, b->d, c->b, d->a, d->b, c->d, d->c, d->d);
    return join(table, na, nb, nc, nd);
}

node_table *create_table(uint64_t size)
{
    node_table *table = (node_table *)malloc(sizeof(node_table));
    table->size = size;
    table->index = (node *)calloc(table->size, sizeof(node));
    table->index[1] = (node){.level = 0, .a = 0, .b = 0, .c = 0, .d = 0, .pop = 0, .id = 1}; // off node
    table->index[2] = (node){.level = 0, .a = 0, .b = 0, .c = 0, .d = 0, .pop = 1, .id = 2}; // on node
    table->count = 2;
    return table;
}

/* Return the inner node of half the size in each dimension */
node_id inner(node_table *table, node_id id)
{
    node *n = lookup(table, id);
    node *a = lookup(table, n->a);
    node *b = lookup(table, n->b);
    node *c = lookup(table, n->c);
    node *d = lookup(table, n->d);
    return join(table, a->d, b->c, c->b, d->a);
}

/* Return true if all outer regions are zero (i.e. only inner inset is non-zero)*/
bool is_padded(node_table *table, node_id id)
{
    node *n = lookup(table, id);
    node *a = lookup(table, n->a);
    node *b = lookup(table, n->b);
    node *c = lookup(table, n->c);
    node *d = lookup(table, n->d);
    bool ad = lookup(table, n->a)->pop == lookup(table, a->d)->pop;
    bool bc = lookup(table, n->b)->pop == lookup(table, b->c)->pop;
    bool cb = lookup(table, n->c)->pop == lookup(table, c->b)->pop;
    bool da = lookup(table, n->d)->pop == lookup(table, d->a)->pop;
    return ad && bc && cb && da;
}

/* Repeatedly take the inner node, until no more can be removed */
node_id crop(node_table *table, node_id id)
{
    node *n = lookup(table, id);
    while (n->level > 2 && is_padded(table, id))
    {
        id = inner(table, id);
        n = lookup(table, id);
    }
    return id;
}

/* Repeatedly centre the node, until the node is fully padded */
node_id pad(node_table *table, node_id id)
{
    node *n = lookup(table, id);
    while (n->level < 3 || is_padded(table, id))
    {
        id = centre(table, id);
        n = lookup(table, id);
    }
    return id;
}

/* Set the cell at the given position, returning the new node */
node_id set_cell(node_table *table, node_id id, uint64_t x, uint64_t y, bool state)
{
    node *n = lookup(table, id);
    if (n->level == 0)
        return state ? ON : OFF;
    // expand the node until x < 1<< (level-1) and y < 1 << (level-1)
    while (x >= (1ULL << n->level) || y >= (1ULL << n->level))
    {
        node_id z = get_zero(table, n->level);
        id = join(table, id, z, z, z);
        n = lookup(table, id);
    }
    uint64_t offset = 1 << (n->level - 1);
    node_id a = n->a;
    node_id b = n->b;
    node_id c = n->c;
    node_id d = n->d;
    if (x < offset && y < offset)
        a = set_cell(table, a, x, y, state);
    else if (x >= offset && y < offset)
        b = set_cell(table, b, x - offset, y, state);
    else if (x < offset && y >= offset)
        c = set_cell(table, c, x, y - offset, state);
    else
        d = set_cell(table, d, x - offset, y - offset, state);
    return join(table, a, b, c, d);
}

/* Get the grey level at the given position and level */
float get_cell(node_table *table, node_id id, uint64_t x, uint64_t y, uint64_t level)
{
    node *n = lookup(table, id);
    if (n->level == 0 || n->level == level)
        return n->pop / (float)(1 << (n->level * 2));
    uint64_t size = 1 << n->level;
    // bounds test
    if (x >= size || y >= size)
        return 0.0f;
    // recursive descent
    uint64_t offset = 1 << (n->level - 1);
    if (x < offset && y < offset)
        return get_cell(table, n->a, x, y, level);
    else if (x >= offset && y < offset)
        return get_cell(table, n->b, x - offset, y, level);
    else if (x < offset && y >= offset)
        return get_cell(table, n->c, x, y - offset, level);
    else
        return get_cell(table, n->d, x - offset, y - offset, level);
}
