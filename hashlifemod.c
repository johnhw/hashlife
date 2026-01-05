/*
    Hashlife.

    Design:
    - All nodes are referenced by an opaque identifier.
    - There is an *intern index* that maps from identifiers to pointers to nodes.
    - It matches on (a, b, c, d) to ensure uniqueness.
    - Nodes are reference counted.

*/
#include "hashlife.h"


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

/* Centre a node, by surrounding it with zeros of the same size.
    Returns the new node.
 */
node_id centre(node_table *table, node_id m_h)
{
    node_id z;
    node *m = lookup(table, m_h);
    z = table->zeros[m->level - 1];
    node_id a = join(table, z, z, z, m->a);
    node_id b = join(table, z, z, m->b, z);
    node_id c = join(table, z, m->c, z, z);
    node_id d = join(table, m->d, z, z, z);
    return join(table, a, b, c, d);
}

/* Return the zero node of the given size */
node_id get_zero(node_table *table, uint64_t k)
{
    assert(k < ZERO_CACHE_MAX_SIZE);
    if (k < ZERO_CACHE_MAX_SIZE)
        return table->zeros[k];
    return 0; // assert will catch this case
}

void decref(node_table *table, node_id id)
{
    if (id == 0)
        return;
    node *n = lookup(table, id);
    if (n->ref_count > 0)
        n->ref_count--;
    if (n->ref_count == 0)
    {
        decref(table, n->a);
        decref(table, n->b);
        decref(table, n->c);
        decref(table, n->d);
        decref(table, n->next);
        // mark as free
        n->id = 0;
    }
}

void incref(node_table *table, node_id id)
{
    node *n = lookup(table, id);
    if (n->ref_count >= 0)
        n->ref_count++;
}

node *lookup(node_table *table, node_id id)
{
    assert(id != 0);
    // lookup the node in the intern table
    // print an error and exit if not found
    uint64_t index = id & table->mask;
    node *n = &table->index[index];
    while (n->id != id)
    {
        assert(n->id != 0); // not found!
        index = (index + 1) & table->mask;
        n = &table->index[index];
    }
    n->last_used = table->time;
    return n;
}

/* Join four nodes.
   - If the node exists, return it from the table.
   - Otherwise, intern it.
   - Increment the reference counts of the child nodes.
   - Returns the hash of the joined node.
*/

node_id join(node_table *table, node_id a_hash, node_id b_hash, node_id c_hash, node_id d_hash)
{
    uint64_t hash = hash_quad(a_hash, b_hash, c_hash, d_hash);
    uint64_t index = hash & table->mask;
    table->time++;
    node *n = &table->index[index];

    // look for an existing node
    while (n->id != 0)
    {
        if (n->id == hash)
        {
            if (n->a == a_hash && n->b == b_hash && n->c == c_hash && n->d == d_hash)
            {
                return n->id; // found it
            }
            else
            {
                // doppleganger! we must create a new unique id and start again
                hash = mix64(hash);
                index = hash & table->mask;
                n = &table->index[index];
            }
        }
        else
        {
            // keep looking
            index = (index + 1) & table->mask;
            n = &table->index[index];
        }
    }

    // not found, so create it
    n->id = hash;
    n->last_used = table->time;
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
    n->ref_count = 0;
    #ifdef GRANDCHILDREN_CACHE
    // set grandchildren
    if (n->level > 1)
    {
        n->aa = a->a;
        n->ab = a->b;
        n->ac = a->c;
        n->ad = a->d;
        n->ba = b->a;
        n->bb = b->b;
        n->bc = b->c;
        n->bd = b->d;
        n->ca = c->a;
        n->cb = c->b;
        n->cc = c->c;
        n->cd = c->d;
        n->da = d->a;
        n->db = d->b;
        n->dc = d->c;
        n->dd = d->d;
    }
    #endif 
    // increment reference counts of children
    if (a->ref_count >= 0)
        a->ref_count++;
    if (b->ref_count >= 0)
        b->ref_count++;
    if (c->ref_count >= 0)
        c->ref_count++;
    if (d->ref_count >= 0)
        d->ref_count++;
    table->count++;

    // resize the table if necessary
    if (table->count * 4 >= table->size)
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
            if (old_n->id != 0)
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
        table->mask = new_mask;
    }
    return hash;
}

static inline node_id sucjoin(node_table *table, node_id a, node_id b, node_id c, node_id d, uint64_t j)
{
    return next(table, join(table, a, b, c, d), j);
}

/* Find the successor of the given node, 2^level-2 steps in the future */
node_id successor(node_table *table, node_id id, uint64_t j)
{
    node *n = lookup(table, id);
    if (n->pop == 0)
        return n->a;

    // store the node locally to ensure it does not change during processing
    node ncopy;
    ncopy = *n;
    n = &ncopy;

    node_id c1, c2, c3, c4, c5, c6, c7, c8, c9;
    #ifndef GRANDCHILDREN_CACHE
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
    #else
    c1 = sucjoin(table, n->aa, n->ab, n->ac, n->ad, j);
    c2 = sucjoin(table, n->ab, n->ba, n->ad, n->bc, j);
    c3 = sucjoin(table, n->ba, n->bb, n->bc, n->bd, j);
    c4 = sucjoin(table, n->ac, n->ad, n->ca, n->cb, j);
    c5 = sucjoin(table, n->ad, n->bc, n->cb, n->da, j);
    c6 = sucjoin(table, n->bc, n->bd, n->da, n->db, j);
    c7 = sucjoin(table, n->ca, n->cb, n->cc, n->cd, j);
    c8 = sucjoin(table, n->cb, n->da, n->cd, n->dc, j);
    c9 = sucjoin(table, n->da, n->db, n->dc, n->dd, j);
    #endif 

    /* Not the natural successor; combine parts */
    if (j < n->level - 2)
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
        /* Natural successor */
        return join(table,
                    sucjoin(table, c1, c2, c4, c5, j),
                    sucjoin(table, c2, c3, c5, c6, j),
                    sucjoin(table, c4, c5, c7, c8, j),
                    sucjoin(table, c5, c6, c8, c9, j));
}

/* Return the successor for this node, caching it as required */
node_id next(node_table *table, node_id id, uint64_t j)
{
    node *n = lookup(table, id);
    if(n->pop==0)
        return n->a;
    
    uint64_t level = n->level;
    if (j == 0 || j >= level - 2)
    {
        // cache the natural successor        
        if (!n->next)
        {
            node_id succ = successor(table, id, level - 2);
            n = lookup(table, id); 
            n->next = succ;
            incref(table, n->next);
        }
        return n->next;
    }
    else
    {        
        if(j>=MAX_J_CACHE)
            return successor(table, id, j);
        if(n->nexts[j])
            return n->nexts[j];
        node_id succ = successor(table, id, j);
        n = lookup(table, id);
        n->nexts[j] = succ;
        incref(table, n->nexts[j]);
        return succ;
        
    }
}

/* Advance time by j steps.
    - Find the MSB of j
    - Make sure that the node has a level at least big enough

*/
node_id advance(node_table *table, node_id id, uint64_t steps)
{
    node *n = lookup(table, id);
    // ensure the node is big enough; n->level must be > log2(steps)+2
    while ((1ULL << (n->level - 2)) < steps)
    {
        // pad the node
        id = centre(table, id);
        n = lookup(table, id);
    }

    // pad to the centre
    id = centre(table, id);
    n = lookup(table, id);

    uint64_t j = 1;
    while (steps > 0)
    {
        if (steps & 1)
            id = centre(table, next(table, id, j));
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
    bool alive = pop_sum == 3 || (pop_sum == 2 && e == 2);
    return alive + 1;
}

node_id life_4x4(node_table *table, node_id m_h)
{
    node *m = lookup(table, m_h);
    // k = 2
    node_id na, nb, nc, nd;
    #ifdef GRANDCHILDREN_CACHE
    na = base_life(m->aa, m->ab, m->ba, m->ac, m->ad, m->bc, m->ca, m->cb, m->da);
    nb = base_life(m->ab, m->ba, m->bb, m->ad, m->bc, m->bd, m->cb, m->da, m->db);
    nc = base_life(m->ac, m->ad, m->bc, m->ca, m->cb, m->da, m->cc, m->cd, m->dc);
    nd = base_life(m->ad, m->bc, m->bd, m->cb, m->da, m->db, m->cd, m->dc, m->dd);
    #else 
    node a = *lookup(table, m->a);
    node b = *lookup(table, m->b);
    node c = *lookup(table, m->c);
    node d = *lookup(table, m->d);
    na = base_life(a.a, a.b, b.a, a.c, a.d, b.c, c.a, c.b, d.a);
    nb = base_life(a.b, b.a, b.b, a.d, b.c, b.d, c.b, d.a, d.b);
    nc = base_life(a.c, a.d, b.c, c.a, c.b, d.a, c.c, c.d, d.c);
    nd = base_life(a.d, b.c, b.d, c.b, d.a, d.b, c.d, d.c, d.d);    
    #endif 
    
    
    return join(table, na, nb, nc, nd);
}

/* Initialise the zero cache and the on and off base nodes */
void init_table(node_table *table)
{
    node_id base_tile[16]; // all 2x2 combinations of on and off cells

    /* cell level nodes */

    table->index[1] = (node){.level = 0, .a = 0, .b = 0, .c = 0, .d = 0, .pop = 0, .id = 1, .ref_count = -1}; // off node
    table->index[2] = (node){.level = 0, .a = 0, .b = 0, .c = 0, .d = 0, .pop = 1, .id = 2, .ref_count = -1}; // on node
    table->count = 2;
    table->on = 2;
    table->off = 1;

    /* all empty nodes */
    table->zeros[0] = table->off;
    for (int i = 1; i < ZERO_CACHE_MAX_SIZE; i++)
    {
        /* Create every zero and make them immortal */
        node_id z = table->zeros[i - 1];
        table->zeros[i] = join(table, z, z, z, z);
        lookup(table, table->zeros[i])->ref_count = -1;
    }

    /* 2x2 nodes */
    for (int i = 0; i < 16; i++)
    {
        node_id a = (i & 0x8) ? table->on : table->off;
        node_id b = (i & 0x4) ? table->on : table->off;
        node_id c = (i & 0x2) ? table->on : table->off;
        node_id d = (i & 0x1) ? table->on : table->off;
        node_id id = join(table, a, b, c, d);
        base_tile[i] = id;
        // mark as immortal
        lookup(table, id)->ref_count = -1;
    }

    /* 4x4 nodes */
    for (int i = 0; i < 65536; i++)
    {
        node_id a = base_tile[(i >> 12) & 0xf];
        node_id b = base_tile[(i >> 8) & 0xf];
        node_id c = base_tile[(i >> 4) & 0xf];
        node_id d = base_tile[(i >> 0) & 0xf];
        node_id id = join(table, a, b, c, d);
        node_id next_generation = life_4x4(table, id);
        node *n = lookup(table, id);
        // mark as immortal
        n->ref_count = -1;
        // map the successor for 4x4 -> 2x2 nodes
        n->next = next_generation;
    }
}

node_table *create_table(uint64_t initial_size)
{
    node_table *table = (node_table *)malloc(sizeof(node_table));
    table->size = initial_size;
    table->count = 0;
    table->mask = initial_size - 1;
    table->time = 0;
    table->index = (node *)calloc(table->size, sizeof(node));
    init_table(table);
    return table;
}

/* Return the inner node of half the size in each dimension */
node_id inner(node_table *table, node_id id)
{
    node *n = lookup(table, id);
    #ifdef GRANDCHILDREN_CACHE
    return join(table, n->ad, n->bc, n->cb, n->da);
    #else
    node a = *lookup(table, n->a);
    node b = *lookup(table, n->b);
    node c = *lookup(table, n->c);
    node d = *lookup(table, n->d);
    return join(table, a.d, b.c, c.b, d.a);
    #endif


}

/* Return true if all outer regions are zero (i.e. only inner inset is non-zero)*/
bool is_padded(node_table *table, node_id id)
{
    node *n = lookup(table, id);
    #ifdef GRANDCHILDREN_CACHE
    bool ad = lookup(table, n->a)->pop == lookup(table, n->ad)->pop;
    bool bc = lookup(table, n->b)->pop == lookup(table, n->bc)->pop;
    bool cb = lookup(table, n->c)->pop == lookup(table, n->cb)->pop;
    bool da = lookup(table, n->d)->pop == lookup(table, n->da)->pop;
    #else 
    node a = *lookup(table, n->a);
    node b = *lookup(table, n->b);
    node c = *lookup(table, n->c);
    node d = *lookup(table, n->d);
    bool ad = a.pop == lookup(table, a.d)->pop;
    bool bc = b.pop == lookup(table, b.c)->pop;
    bool cb = c.pop == lookup(table, c.b)->pop;
    bool da = d.pop == lookup(table, d.a)->pop;
    #endif
    return ad && bc && cb && da;
}

/* Repeatedly take the inner node, until no more can be removed */
node_id crop(node_table *table, node_id id)
{
    node *n = lookup(table, id);
    while (n->level > 3 && is_padded(table, id))
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
    {
        if (state)
            return table->on;
        else
            return table->off;
    }

    // expand the node until x < 1<< (level-1) and y < 1 << (level-1)
    while (x >= (1ULL << n->level) || y >= (1ULL << n->level))
    {
        node_id z = table->zeros[n->level];
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
