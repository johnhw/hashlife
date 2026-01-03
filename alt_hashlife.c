/*
    Hashlife.

    Design:
    - All nodes are referenced by an opaque identifier.
    - There is an *intern index* that maps from identifiers to pointers to nodes.
    - It matches on (level, a, b, c, d) to ensure uniqueness.
    - Nodes are reference counted.

*/

#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#define INIT_TABLE_SIZE 4096
#define ZERO_CACHE_MAX_SIZE 64

typedef uint64_t node_id;

uint64_t make_index(uint32_t generation, uint32_t index)
{
    return ((uint64_t)generation << 32) | index;
}

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

/* Base level node */
typedef struct node
{
    node_id id;
    uint64_t level;
    node_id a, b, c, d;                                                     // children
    node_id aa, ab, ac, ad, ba, bb, bc, bd, ca, cb, cc, cd, da, db, dc, dd; // grandchildren
    node_id next;                                                           // cached successor
    uint64_t pop;
    int32_t ref_count; // 0 = ready to free, negative = immortal
    uint64_t last_used;
} node;

typedef struct node_table
{
    node *index;
    uint64_t time;
    uint64_t size;
    uint64_t count;
    uint64_t mask;
    node *on, *off;                   // base on and off nodes
    node *zeros[ZERO_CACHE_MAX_SIZE]; // all zero nodes up to max size
    node *base_tile[16];              // all 2x2 combinations of on and off cells
} node_table;

/* Centre a node, by surrounding it with zeros of the same size.
    Returns the new node.
 */
node_id centre(node_table *table, node_id m_h)
{
    node_id z;
    node *m = get_table(table, m_h);
    z = table->zeros[m->level - 1]->id;
    node_id a = join(table, z, z, z, m->a);
    node_id b = join(table, z, z, m->b, z);
    node_id c = join(table, z, m->c, z, z);
    node_id d = join(table, m->d, z, z, z);
    return join(table, a, b, c, d);
}

/* Return the zero node of the given size */
node_id get_zero(node_table *table, uint64_t k)
{
    assert(k >= 0 && k < ZERO_CACHE_MAX_SIZE);
    if (k < ZERO_CACHE_MAX_SIZE)
        return table->zeros[k]->id;
}

void decref(node_table *table, node_id id)
{
    if (id == 0)
        return;
    node *n = get_table(table, id);
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
    node *n = get_table(table, id);
    if (n->ref_count >= 0)
        n->ref_count++;
}

node *lookup(node_table *table, node_id hash)
{
    assert(hash != 0);
    // lookup the node in the intern table
    // print an error and exit if not found
    uint64_t index = hash & table->mask;
    node *n = &table->index[index];
    while (n->id != hash)
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
    n->level = lookup(table, a_hash)->level + 1;
    // set population
    node *a = lookup(table, a_hash);
    node *b = lookup(table, b_hash);
    node *c = lookup(table, c_hash);
    node *d = lookup(table, d_hash);
    n->pop = a->pop + b->pop + c->pop + d->pop;
    n->ref_count = 0;
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
    // increment reference counts of children
    if (a_hash >= 0)
        a->ref_count++;
    if (b_hash >= 0)
        b->ref_count++;
    if (c_hash >= 0)
        c->ref_count++;
    if (d_hash >= 0)
        d->ref_count++;
    table->count++;

    // resize the table if necessary
    if (table->count * 4 >= table->size)
    {
        uint64_t new_size = table->size * 2;
        node *new_index = (node *)calloc(new_size, sizeof(node));
        uint64_t new_mask = new_size - 1;
        uint64_t new_index_pos;
        node *old_n, *new_n;
        for (int i = 0; i < table->size; i++)
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
}



// node *successor(node_table *table, node *node, uint64_t j)
// {
//     if(node->pop==0)
//         return node->a;
//     else if(node->k==2)
//         return life_4x4(table, node);
//     else
//     {
//         j = node->k - 2 ? j : min(j, node->k - 2);
//         node *c1 = successor(table, JOIN(node, a, a, a, b, a, c, a, d), j);
//         node *c2 = successor(table, JOIN(node, a, b, b, a, a, d, b, c), j);
//         node *c3 = successor(table, JOIN(node, b, a, b, b, b, c, b, d), j);
//         node *c4 = successor(table, JOIN(node, a, c, a, d, c, a, c, b), j);
//         node *c5 = successor(table, JOIN(node, a, d, b, c, c, b, d, a), j);
//         node *c6 = successor(table, JOIN(node, b, c, b, d, d, a, d, b), j);
//         node *c7 = successor(table, JOIN(node, c, a, c, b, c, c, c, d), j);
//         node *c8 = successor(table, JOIN(node, c, b, d, a, c, d, d, c), j);
//         node *c9 = successor(table, JOIN(node, d, a, d, b, d, c, d, d), j);
//         if(j < node->k - 2)
//         {
//             return join_table(table,
//                 join_table(table, get_table(table, c1->d), get_table(table, c2->c), get_table(table, c4->b), get_table(table, c5->a)),
//                 join_table(table, get_table(table, c2->d), get_table(table, c3->c), get_table(table, c5->b), get_table(table, c6->a)),
//                 join_table(table, get_table(table, c4->d), get_table(table, c5->c), get_table(table, c7->b), get_table(table, c8->a)),
//                 join_table(table, get_table(table, c5->d), get_table(table, c6->c), get_table(table, c8->b), get_table(table, c9->a))
                
//             );
//         }
//         else
//         {
//             return join_table(table,
//                 successor(table, join_table(table, c1, c2, c4, c5), j),
//                 successor(table, join_table(table, c2, c3, c5, c6), j),
//                 successor(table, join_table(table, c4, c5, c7, c8), j),
//                 successor(table, join_table(table, c5, c6, c8, c9), j)
//             );
//         }
        
//     }
// }


node_id successor(node_table *table, node_id id)
{
    node *n = lookup(table, id);
    if (n->pop == 0)
        return n->a;
    else if (n->level == 2)
        return life_4x4(table, id);
    else
    {
        node_id a = successor(table, join(table, n->a, n->b, n->c, n->d));
        return a;
    }
}


/* Compute the life rule on the 3x3 neighbourhood

    a b c
    d e f
    g h i

    Returns either the hash of the "on" (=2) or "off" (=1) base node.
*/
node *base_life(node_table *table, node_id a, node_id b, node_id c, node_id d, node_id e, node_id f, node_id g, node_id h, node_id i)
{
    /* hash 1 = off, hash 2 = on, by definition*/
    int pop_sum = (a - 1) + (b - 1) + (c - 1) + (d - 1) + (e - 1) + (f - 1) + (g - 1) + (h - 1) + (i - 1);
    bool alive = (pop_sum == 3 || (pop_sum == 2 && e == 2));
    return alive + 1;
}

node_id *life_4x4(node_table *table, node_id m_h)
{
    node *m = get_table(table, m_h);
    // k = 2
    node_id na, nb, nc, nd;
    na = base_life(table, m->aa, m->ab, m->ba, m->ac, m->ad, m->bc, m->ca, m->cb, m->da);
    nb = base_life(table, m->ab, m->ba, m->bb, m->ad, m->bc, m->bd, m->cb, m->da, m->db);
    nc = base_life(table, m->ac, m->ad, m->bc, m->ca, m->cb, m->da, m->cc, m->cd, m->dc);
    nd = base_life(table, m->ad, m->bc, m->bd, m->cb, m->da, m->db, m->cd, m->dc, m->dd);
    node_id new_node = join(table, na, nb, nc, nd);
    return new_node;
}

/* Initialise the zero cache and the on and off base nodes */
void init_table(node_table *table)
{
    /* cell level nodes */
    table->on = insert_table(table, &(node){.level = 0, .a = 0, .b = 0, .c = 0, .d = 0, .pop = 1, .id = 2, .ref_count = -1});
    table->off = insert_table(table, &(node){.level = 0, .a = 0, .b = 0, .c = 0, .d = 0, .pop = 0, .id = 1, .ref_count = -1});

    /* all empty nodes */
    table->zeros[0] = table->off;
    for (int i = 1; i < ZERO_CACHE_MAX_SIZE; i++)
    {
        /* Create every zero and make them immortal */
        node_id z = table->zeros[i - 1]->id;
        table->zeros[i] = lookup(table, join(table, z, z, z, z));
        table->zeros[i]->ref_count = -1;
    }

    /* 2x2 nodes */
    for (int i = 0; i < 16; i++)
    {
        node_id a = (i & 0x8) ? table->on->id : table->off->id;
        node_id b = (i & 0x4) ? table->on->id : table->off->id;
        node_id c = (i & 0x2) ? table->on->id : table->off->id;
        node_id d = (i & 0x1) ? table->on->id : table->off->id;
        node_id id = join(table, a, b, c, d);
        table->base_tile[i] = lookup(table, id);
        // mark as immortal
        lookup(table, id)->ref_count = -1;
    }

    node **base = table->base_tile;
    /* 4x4 nodes */
    for (int i = 0; i < 65536; i++)
    {
        node_id a = base[(i >> 12) & 0xf]->id;
        node_id b = base[(i >> 8) & 0xf]->id;
        node_id c = base[(i >> 4) & 0xf]->id;
        node_id d = base[(i >> 0) & 0xf]->id;
        node_id id = join(table, a, b, c, d);
        node *n = lookup(table, id);
        // mark as immortal
        n->ref_count = -1;
        // map the successor for 4x4 -> 2x2 nodes
        node_id next_generation = life_4x4(table, id);
        n->next = next_generation;
    }
}

/* Return the inner node of half the size in each dimension */
node_id inner(node_table *table, node_id id)
{
    node *n = lookup(table, id);
    return join(table, n->ad, n->bc, n->cb, n->da);
}

/* Return true if all outer regions are zero (i.e. only inner inset is non-zero)*/
bool is_padded(node_table *table, node_id id)
{
    node *n = lookup(table, id);
    bool ad = lookup(table, n->a)->pop == lookup(table, n->ad)->pop;
    bool bc = lookup(table, n->b)->pop == lookup(table, n->bc)->pop;
    bool cb = lookup(table, n->c)->pop == lookup(table, n->cb)->pop;
    bool da = lookup(table, n->d)->pop == lookup(table, n->da)->pop;
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
            return table->on->id;
        else
            return table->off->id;
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
float get_cell(node_table *table, node_id id, uint64_t x, uint64_t y, int level)
{
    node *n = lookup(table, id);
    if (n->level == 0 || n->level == level)
        return n->pop / (float)(1 << (n->level * 2));
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

/* Rasterise to an image */
void rasterise(node_table *table, node_id id, float *buf, uint64_t buf_width, uint64_t buf_height, uint64_t x, uint64_t y, uint64_t width, uint64_t height, uint64_t min_level)
{
    /* Verify that the size is compatible */
    uint64_t pixel_width = width >> min_level;
    uint64_t pixel_height = height >> min_level;
    assert(pixel_width <= buf_width && pixel_height <= buf_height);

    for (uint64_t j = 0; j < pixel_height; j++)
        for (uint64_t i = 0; i < pixel_width; i++)
            buf[j * buf_width + i] = get_cell(table, id, x + (i << min_level), y + (j << min_level), min_level);
}