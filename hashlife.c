/*
    An implementation of HashLife
    A conversion of the Python version at https://github.com/johnhw/hashlife

    John H. Williamson, 2026
    MIT License

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
uint64_t hash_quad(uint64_t a, uint64_t b, uint64_t c, uint64_t d)
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

/*  One attempt lookup in the successor cache
    (no probing, collisions cause recomputation)
*/
node_id lookup_next(node_table *table, node_id from, uint64_t j)
{
    uint64_t mask = (table->size) - 1;
    uint64_t hash = hash_quad(from, j, from, j);
    node *n = &table->index[hash & mask];
    if (n->from == from && n->j == j)
        return n->to;
    return UNUSED;
}

/* Cache the successor result,
    kicking out any existing entry.
*/
void cache_next(node_table *table, node_id from, node_id to, uint64_t j)
{
    uint64_t mask = (table->size) - 1;
    uint64_t hash = hash_quad(from, j, from, j);
    node *n = &table->index[hash & mask];
    n->from = from;
    n->to = to;
    n->j = j;
}

/* Centre a node, by surrounding it with zeros of the same size.
    Returns the new node.
 */
node_id centre(node_table *table, node_id m_h)
{
    node_id z;
    node m = *lookup(table, m_h);
    z = get_zero(table, LEVEL(m_h) - 1);
    node_id a = join(table, z, z, z, m.a);
    node_id b = join(table, z, z, m.b, z);
    node_id c = join(table, z, m.c, z, z);
    node_id d = join(table, m.d, z, z, z);
    return join(table, a, b, c, d);
}

/* Return the zero node of the given size */
node_id get_zero(node_table *table, uint64_t k)
{
    if (k == 0)
        return table->off;
    // Try the systematic name for a zero first
    node_id z = (0ULL << 63) | (1ULL << 62) | (k << 46) | (HASH_MASK(mix64(k)));
    if (lookup(table, z)->id == z)
        return z;
    return join(table, get_zero(table, k - 1), get_zero(table, k - 1), get_zero(table, k - 1), get_zero(table, k - 1));
}

node *lookup(node_table *table, node_id id)
{
    uint64_t mask = table->size - 1;
    uint64_t index = id;
    node *n = &table->index[index & mask];
    while (n->id != UNUSED && n->id != id)
        n = &table->index[index++ & mask];
    return n;
}


/* Duplicate a table */
node_table *copy_table(node_table *old_table)
{
    node_table *new_table = create_table(old_table->size);
    memcpy(new_table->index, old_table->index, old_table->size * sizeof(node));
    new_table->count = old_table->count;    
    return new_table;
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

/* Given four node_ids, compute the parent node ID */
uint64_t merge(node_id a, node_id b, node_id c, node_id d)
{
    // format: [flag:1] [zero:1] [level:16] [hash:46]
    uint64_t h = hash_quad(a, b, c, d);
    // extract level bits from a
    uint64_t level = ((a >> 46) + 1) & 0xFFFF;
    uint64_t all_zero = IS_ZERO(a) && IS_ZERO(b) && IS_ZERO(c) && IS_ZERO(d);
    if (all_zero)
        return (0ULL << 63) | (1ULL << 62) | (level << 46) | (HASH_MASK(mix64(level)));
    else
        return (0ULL << 63) | (0ULL << 62) | (level << 46) | (HASH_MASK(h));
}

/* Join four nodes.
   - If the node exists, return it from the table.
   - Otherwise, intern it.
   - Increment the reference counts of the child nodes.
   - Returns the hash of the joined node.
*/

node_id join(node_table *table, node_id a_hash, node_id b_hash, node_id c_hash, node_id d_hash)
{
    uint64_t hash = merge(a_hash, b_hash, c_hash, d_hash);
    node *n = lookup(table, hash);
    while (n->id != UNUSED)
    {
        if (n->a == a_hash && n->b == b_hash && n->c == c_hash && n->d == d_hash)
            return n->id;                // found it
        uint64_t new_hash = mix64(hash); // doppleganger, create a new unique id
        hash ^= HASH_MASK(new_hash);     // XOR in low 46 bits only
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
    n->pop = a->pop + b->pop + c->pop + d->pop;
    table->count++;

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

    uint64_t level = LEVEL(id);

    if (j == 0 || j >= level - 2)
        j = level - 2;

    if (IS_ZERO(id)) // empty
        return lookup(table, id)->a;

    node_id next = lookup_next(table, id, j);
    if (next != UNUSED)
        return next;

    if (level == 2) // base case
    {
        next = life_4x4(table, id);
        cache_next(table, id, next, j);
        return next;
    }

    node *n = lookup(table, id);

    node_id c1, c2, c3, c4, c5, c6, c7, c8, c9;
    // copy the actual nodes to prevent changes during lookups
    node a = *lookup(table, n->a);
    node b = *lookup(table, n->b);
    node c = *lookup(table, n->c);
    node d = *lookup(table, n->d);

    c1 = successor(table, a.id, j); // same as sucjoin(table, a.a, a.b, a.c, a.d, j);
    c2 = sucjoin(table, a.b, b.a, a.d, b.c, j);
    c3 = successor(table, b.id, j);
    c4 = sucjoin(table, a.c, a.d, c.a, c.b, j);
    c5 = sucjoin(table, a.d, b.c, c.b, d.a, j);
    c6 = sucjoin(table, b.c, b.d, d.a, d.b, j);
    c7 = successor(table, c.id, j);
    c8 = sucjoin(table, c.b, d.a, c.d, d.c, j);
    c9 = successor(table, d.id, j);

    /* Not the natural successor; combine parts */
    if (j < level - 2)
    {
        node c1n = *lookup(table, c1);
        node c2n = *lookup(table, c2);
        node c3n = *lookup(table, c3);
        node c4n = *lookup(table, c4);
        node c5n = *lookup(table, c5);
        node c6n = *lookup(table, c6);
        node c7n = *lookup(table, c7);
        node c8n = *lookup(table, c8);
        node c9n = *lookup(table, c9);

        next = join(table,
                    join(table, c1n.d, c2n.c, c4n.b, c5n.a),
                    join(table, c2n.d, c3n.c, c5n.b, c6n.a),
                    join(table, c4n.d, c5n.c, c7n.b, c8n.a),
                    join(table, c5n.d, c6n.c, c8n.b, c9n.a));
        cache_next(table, id, next, j);
        return next;
    }
    else
    {
        /* Natural successor */
        next = join(table,
                    sucjoin(table, c1, c2, c4, c5, j),
                    sucjoin(table, c2, c3, c5, c6, j),
                    sucjoin(table, c4, c5, c7, c8, j),
                    sucjoin(table, c5, c6, c8, c9, j));

        cache_next(table, id, next, j);
        return next;
    }
}

/* Advance time by j steps.
    - Find the MSB of j
    - Make sure that the node has a level at least big enough

*/
node_id advance(node_table *table, node_id id, uint64_t steps)
{
    // ensure the node is big enough; n->level must be > log2(steps)+2
    while ((1ULL << (LEVEL(id) - 2)) < steps)
        id = centre(table, id);
    // pad to the centre
    id = centre(table, id);
    // advance by 2^j on each set bit j
    for (uint64_t j = 1; steps > 0; j++, steps >>= 1)
        if (steps & 1)
            id = centre(table, successor(table, id, j));
    // crop for the caller
    return crop(table, id);
}

/* Fast forward by repeated application of the HashLife step */
node_id ffwd(node_table *table, node_id id, uint64_t steps, uint64_t *generations)
{
    *generations = 0;
    for (uint64_t i = 0; i < steps; i++)
    {
        id = centre(table, centre(table, pad(table, id)));
        id = successor(table, id, 0);
        *generations += 1ULL << (LEVEL(id) - 2);
    }
    return crop(table, id);
}

/* Compute the life rule on the 3x3 neighbourhood

    a b c
    d E f
    g h i

    Returns either the hash of the "on" (=2) or "off" (=1) base node.
*/
node_id base_life(node_id a, node_id b, node_id c, node_id d, node_id e, node_id f, node_id g, node_id h, node_id i, node_id ON, node_id OFF)
{

    int pop_sum = (a == ON) + (b == ON) + (c == ON) + (d == ON) + (f == ON) + (g == ON) + (h == ON) + (i == ON);
    return (pop_sum == 3 || (pop_sum == 2 && e == ON)) ? ON : OFF;
}

node_id life_4x4(node_table *table, node_id id)
{
    node *n = lookup(table, id);
    node_id na, nb, nc, nd;
    node *a = lookup(table, n->a);
    node *b = lookup(table, n->b);
    node *c = lookup(table, n->c);
    node *d = lookup(table, n->d);
    uint64_t on = table->on;
    uint64_t off = table->off;
    na = base_life(a->a, a->b, b->a, a->c, a->d, b->c, c->a, c->b, d->a, on, off);
    nb = base_life(a->b, b->a, b->b, a->d, b->c, b->d, c->b, d->a, d->b, on, off);
    nc = base_life(a->c, a->d, b->c, c->a, c->b, d->a, c->c, c->d, d->c, on, off);
    nd = base_life(a->d, b->c, b->d, c->b, d->a, d->b, c->d, d->c, d->d, on, off);
    return join(table, na, nb, nc, nd);
}

/* Mark every child node, recursively */
void set_flag(node_table *table, node_id id)
{
    if (LEVEL(id) <= 2)
        return;
    node *n = lookup(table, id);

    // set the high bit of pop
    n->id = MARK(n->id);

    set_flag(table, n->a);
    set_flag(table, n->b);
    set_flag(table, n->c);
    set_flag(table, n->d);
}

/* Remove all nodes not a child of top */
void vacuum(node_table *table, node_id top)
{
    // walk the tree, marking all reachable nodes
    set_flag(table, top);
    node *old_index = table->index;
    table->index = (node *)calloc(table->size, sizeof(node));
    table->count = 0;
    for (uint64_t i = 0; i < table->size; i++)
    {
        node *n = &old_index[i];                
        bool marked = IS_MARKED(n->id);
        n->id = UNMARK(n->id);
        if (n->id != UNUSED && (LEVEL(n->id) <= 2 || marked))
        {            
            node *slot = lookup(table, n->id);            
            assert(slot->id == UNUSED); // should not already exist
            *slot = *n;            
            table->count++;
        }        
    }
    /* now clear up the successor cache */
    for (uint64_t i = 0; i < table->size; i++)
    {
        node *n = &table->index[i];        
        /* 
        check; are we mapping to a successor? 
        make sure that successor actually still exists! 
        */
        if (n->to != UNUSED)
        {
            if (lookup(table, n->to)->id != n->to || lookup(table, n->from)->id != n->from)
            {
                // invalid node, delete it
                n->from = UNUSED;
                n->to = UNUSED;
                n->j = 0;
            }
        }
    }
}

node_table *create_table(uint64_t initial_size)
{
    node_table *table = (node_table *)malloc(sizeof(node_table));
    table->size = initial_size < 16 ? 16 : initial_size;
    table->index = (node *)calloc(table->size, sizeof(node));
    table->off = (0ULL << 63) | (1ULL << 62) | (0ULL << 46) | HASH_MASK(mix64(0));
    table->on = (0ULL << 63) | (0ULL << 62) | (0ULL << 46) | HASH_MASK(mix64(1));

    uint64_t mask = table->size - 1;
    /* cell level nodes */
    table->index[(table->off) & mask] = (node){.a = 0, .b = 0, .c = 0, .d = 0, .pop = 0, .id = table->off}; // off node
    table->index[(table->on) & mask] = (node){.a = 0, .b = 0, .c = 0, .d = 0, .pop = 1, .id = table->on};   // on node
    table->count = 2;
    return table;
}

void free_table(node_table *table)
{
    free(table->index);
    free(table);
}   

/* Return the inner node of half the size in each dimension */
node_id inner(node_table *table, node_id id)
{
    node *n = lookup(table, id);
    node a = *lookup(table, n->a);
    node b = *lookup(table, n->b);
    node c = *lookup(table, n->c);
    node d = *lookup(table, n->d);
    return join(table, a.d, b.c, c.b, d.a);
}

/* Return true if all outer regions are zero (i.e. only inner inset is non-zero)*/
bool is_padded(node_table *table, node_id id)
{
    node *n = lookup(table, id);
    node *a = lookup(table, n->a);
    node *b = lookup(table, n->b);
    node *c = lookup(table, n->c);
    node *d = lookup(table, n->d);
    bool ad = a->pop == lookup(table, a->d)->pop;
    bool bc = b->pop == lookup(table, b->c)->pop;
    bool cb = c->pop == lookup(table, c->b)->pop;
    bool da = d->pop == lookup(table, d->a)->pop;
    return ad && bc && cb && da;
}

/* Repeatedly take the inner node, until no more can be removed */
node_id crop(node_table *table, node_id id)
{
    while (LEVEL(id) > 3 && is_padded(table, id))
        id = inner(table, id);
    return id;
}

/* Repeatedly centre the node, until the node is fully padded */
node_id pad(node_table *table, node_id id)
{
    while (LEVEL(id) < 3 || is_padded(table, id))
        id = centre(table, id);
    return id;
}

/* Set the cell at the given position, returning the new node */
node_id set_cell(node_table *table, node_id id, uint64_t x, uint64_t y, bool state)
{

    if (LEVEL(id) == 0)
    {
        if (state)
            return table->on;
        else
            return table->off;
    }
    // expand the node until x < 1<< (level-1) and y < 1 << (level-1)
    while (x >= (1ULL << LEVEL(id)) || y >= (1ULL << LEVEL(id)))
    {
        node_id z = get_zero(table, LEVEL(id));
        id = join(table, id, z, z, z);
    }
    node *n = lookup(table, id);
    uint64_t offset = 1 << (LEVEL(id) - 1);
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
    if (LEVEL(id) == 0 || LEVEL(id) == level)
        return n->pop / (float)(1 << (2 * LEVEL(id)));
    uint64_t size = 1 << LEVEL(id);
    // bounds test
    if (x >= size || y >= size)
        return 0.0f;
    // recursive descent

    uint64_t offset = 1 << (LEVEL(id) - 1);
    if (x < offset && y < offset)
        return get_cell(table, n->a, x, y, level);
    else if (x >= offset && y < offset)
        return get_cell(table, n->b, x - offset, y, level);
    else if (x < offset && y >= offset)
        return get_cell(table, n->c, x, y - offset, level);
    else
        return get_cell(table, n->d, x - offset, y - offset, level);
}
