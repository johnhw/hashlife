#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "hashlife.h"
#include <assert.h>



static void init_zeros(node_table *table)
{
    table->zeros[0] = table->off;
    for(int i=1;i<ZERO_CACHE_MAX_SIZE;i++)
    {
        node *z = table->zeros[i-1];
        table->zeros[i] = join_nodes(table, z, z, z, z);
    }
}

/* 
    Create a new table with the on and off nodes, and the zero cache 
    Tables are create with a fixed initial size, and will be expanded 
    as required when the table is full.
*/
node_table *create_table()
{
    node_table *table = (node_table *)malloc(sizeof(node_table));
    
    table->size = INIT_TABLE_SIZE;
    table->count = 0;
    table->nodes = (node *)calloc(table->size, sizeof(node));
    
    // create the on and off root nodes
    node off = {.k = 0, .a=0, .b=0, .c=0, .d=0, .pop = 0, .id = 1};
    node on = {.k = 0, .a=0, .b=0, .c=0, .d=0, .pop = 1, .id = 2};    
        
    table->off = insert_table(table, &off);
    table->on= insert_table(table, &on);    
    // cache the empty nodes (up to the max size)
    init_zeros(table);    
    return table;
}

/* Linear probing to find the next empty slot in the table */
uint64_t probe(node_table *table, uint64_t hash)
{
    uint64_t mask = table->size - 1;
    uint64_t index = hash & mask;
    while(table->nodes[index].id != 0 && table->nodes[index].id != hash) index = (index + 1) & (mask);
    return index;
}

/* Insert a node into the table, using linear probing to resolve collisions */
node *insert_table(node_table *table, node *n)
{
    uint64_t index = probe(table, n->id);
    // didn't already exist? insert it
    if(table->nodes[index].id == 0)
    {
        table->nodes[index] = *n;
        table->count++;
        return &(table->nodes[index]);
    }
    if(table->nodes[index].id == n->id)
        return &table->nodes[index];
    return NULL;
}

/* Get a node from the table, using linear probing to resolve collisions */
node *get_table(node_table *table, uint64_t hash)
{
    uint64_t index = probe(table, hash); 
    // found it?    
    if(table->nodes[index].id == hash) return &table->nodes[index];
    return NULL;
}

/* Double the size of the table and re-insert all nodes 
    Updates the fixed references to the "on" and "off" nodes
    and the zero cache.
*/
void expand_table(node_table *table)
{
    uint64_t new_size = table->size * 2;
    node *new_nodes = (node *)calloc(new_size, sizeof(node));
    uint64_t mask = new_size - 1;
    uint64_t index;
    node *n, *new_n;    
    for(int i=0;i<table->size;i++)
    {
        n = &table->nodes[i];
        // perform re-insertion of all nodes
        if(n->id != 0)
        {
            index = n->id & mask;
            while(new_nodes[index].id != 0) index = (index + 1) & (mask);
            new_nodes[index] = *n;
            new_n = &new_nodes[index];
        }
        // store the references to the on and off nodes
        if(table->nodes[i].id==1) table->off = new_n;
        if(table->nodes[i].id==2) table->on = new_n;        
        if(table->nodes[i].pop==0 && table->nodes[i].id !=0 && table->nodes[i].k < ZERO_CACHE_MAX_SIZE)
            table->zeros[table->nodes[i].k] = new_n;
    }    
    free(table->nodes);
    table->nodes = new_nodes;    
    table->size = new_size;
}


node *join_hash(node_table *table, uint64_t a_hash, uint64_t b_hash, uint64_t c_hash, uint64_t d_hash)
{
    node_id hash = 0x765b23899067874 * a_hash  + 0x788b239358d80cba *  b_hash + 0x114aefbc97f18777 * c_hash + 0x2a748ed22de145df * d_hash;
    node *result = get_table(table, hash);
    if(result)
        return result;
    node *a = get_table(table, a_hash);
    node *b = get_table(table, b_hash);
    node *c = get_table(table, c_hash);
    node *d = get_table(table, d_hash);
    assert(a && b && c && d);
    return join_nodes(table, a, b, c, d);
}

node *join_nodes(node_table *table, node *a, node *b, node *c, node *d)
{
    node n, *result;
    n.k = a->k + 1;
    n.id = 0x765b23899067874 * a->id  + 0x788b239358d80cba *  b->id + 0x114aefbc97f18777 * c->id + 0x2a748ed22de145df * d->id;
    
    // check for collisions in the full 64-bit hash space
    bool possible_collision = true;
    // Warning: this method of collision resolution
    // is *not* stable in the presence of deletions!
    while(possible_collision)
    {
        result = get_table(table, n.id);
        if(result)
        {
            // same hash, but is it the same node?
            if(result->a == a->id && result->b == b->id && result->c == c->id && result->d == d->id && result->k == n.k)
                return result; // already exists, no problem            
            n.id = n.id + 0x1d1c8b8a9d491ce; // try another hash
        }
        else
            possible_collision = false;
    }
    // it's a new node, so insert it
    n.pop = a->pop + b->pop + c->pop + d->pop;
    n.a = a->id;
    n.b = b->id;
    n.c = c->id;
    n.d = d->id;    
    result = insert_table(table, &n);
    // max 25% capacity; resize if required
    if(table->count > table->size / 4) 
    {   expand_table(table);
        // note: we need to get, because the resize may have invalidated the pointer
        return get_table(table, n.id); 
    }
    return result; 
}



/* Return the zero node of the given size, using the zero cache 
    If it's not cached, construct it from the next smaller size
*/
node *get_zero(node_table *table, int k)
{
    if(k<ZERO_CACHE_MAX_SIZE)
        return table->zeros[k];
    else
        {            
            node *sub_zero = get_zero(table, k-1);
            return join_nodes(table, sub_zero, sub_zero, sub_zero, sub_zero);            
        }
}

/* Centre a node, by surrounding it with zeros of the same size.
    Returns the new node.
 */
node *centre(node_table *table, node *m)
{
    node_id z;
    z = table->zeros[m->k - 1]->id;
    node *a = join_hash(table, z, z, z, m->a);
    node *b = join_hash(table, z, z, m->b, z);
    node *c = join_hash(table, z, m->c, z, z);
    node *d = join_hash(table, m->d, z, z, z);
    return join_nodes(table, a, b, c, d);
}   

/* Compute the base life rule for bottom level blocks, where k=0 
    All arguments are pointers to level 0 nodes, with population 1 or 0. 
    Returns a pointer to either of the root level "on" or "off" nodes */
node *base_life(node_table *table, node *a, node *b, node *c, node *d, node *e, node *f, node *g, node *h, node *i)
{
    int pop_sum = a->pop + b->pop + c->pop + d->pop + e->pop + f->pop + g->pop + h->pop + i->pop;
    bool alive = (pop_sum == 3 || (pop_sum == 2 && e->pop));
    return alive ? table->on : table->off;
}


#define SUB(N, X, Y) get_table(table, get_table(table, N->X)->Y)

/* Compute the life rule for a 4x4 block, where k=2 
    Returns the new node that would be the successor in
    the central block, one generation ahead
*/
node *life_4x4(node_table *table, node *m)
{
    // k = 2
    node *aa = SUB(m, a, a);
    node *ab = SUB(m, a, b);
    node *ac = SUB(m, a, c);
    node *ad = SUB(m, a, d);
    node *ba = SUB(m, b, a);
    node *bb = SUB(m, b, b);
    node *bc = SUB(m, b, c);
    node *bd = SUB(m, b, d);
    node *ca = SUB(m, c, a);
    node *cb = SUB(m, c, b);
    node *cc = SUB(m, c, c);
    node *cd = SUB(m, c, d);
    node *da = SUB(m, d, a);
    node *db = SUB(m, d, b);
    node *dc = SUB(m, d, c);
    node *dd = SUB(m, d, d);

    node *na = base_life(table, aa, ab, ba, ac, ad, bc, ca, cb, da);
    node *nb = base_life(table, ab, ba, bb, ad, bc, bd, cb, da, db);
    node *nc = base_life(table, ac, ad, bc, ca, cb, da, cc, cd, dc);
    node *nd = base_life(table, ad, bc, bd, cb, da, db, cd, dc, dd);

    return join_nodes(table, na, nb, nc, nd);
}


bool is_padded(node_table *table, node *n)
{
    node *a = get_table(table, n->a);
    node *b = get_table(table, n->b);
    node *c = get_table(table, n->c);
    node *d = get_table(table, n->d);
    node *a_d_d = get_table(table, get_table(table, a->d)->d);
    node *b_c_c = get_table(table, get_table(table, b->c)->c);
    node *c_b_b = get_table(table, get_table(table, c->b)->b);
    node *d_a_a = get_table(table, get_table(table, d->a)->a);
    return a->pop == a_d_d->pop && b->pop == b_c_c->pop && c->pop == c_b_b->pop && d->pop == d_a_a->pop;
}

node *inner(node_table *table, node *n)
{
    node *a_d = get_table(table, get_table(table, n->a)->d);
    node *b_c = get_table(table, get_table(table, n->b)->c);
    node *c_b = get_table(table, get_table(table, n->c)->b);
    node *d_a = get_table(table, get_table(table, n->d)->a);
    return join_nodes(table, a_d, b_c, c_b, d_a);
}

node *crop(node_table *table, node *n)
{
    if(n->k <= 3 || !is_padded(table, n))
        return n;
    else
        return crop(table, inner(table, n));
}

void *pad(node_table *table, node *n)
{
    if(n->k <= 3 || !is_padded(table, n))
        return pad(table, centre(table, n));
    else
        return n;
}   



// #define JOIN(N, X1, Y1, X2, Y2, X3, Y3, X4, Y4) join_table(table, SUB(N, X1, Y1), SUB(N, X2, Y2), SUB(N, X3, Y3), SUB(N, X4, Y4))


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


/* Conversion to and from coordinate buffers */

/* Expand a node into an *empty* coord_buf, with optional clipping and level of detail */
void expand_to_points(node_table *table, node *n, uint64_t x, uint64_t y, coord_set *buf, uint64_t level, uint64_t *clip)
{    
    if(n->pop == 0) return;

    uint64_t size = 1 << n->k;
    uint64_t offset = size >> 1;
    uint64_t x1 = x + offset;
    uint64_t y1 = y + offset;
    if(clip)
    {
        if(x + size < clip[0] || x > clip[1] || y + size < clip[2] || y > clip[3])
            return;
    }
    if(n->k == level)
    {
        float grey = (float)n->pop / (size * size);
        insert_coord_set(buf, x, y, grey, n->id);
    }
    else
    {
        expand_to_points(table, get_table(table, n->a), x, y, buf, level, clip);
        expand_to_points(table, get_table(table, n->b), x + offset, y, buf, level, clip);
        expand_to_points(table, get_table(table, n->c), x, y + offset, buf, level, clip);
        expand_to_points(table, get_table(table, n->d), x + offset, y + offset, buf, level, clip);
    }
}

/* Fully expand a node into a coord_buf */
void full_expansion(node_table *table, node *n, coord_set *buf)
{
    clear_coord_set(buf);
    expand_to_points(table, n, 0, 0, buf, 0, NULL);
}

node_id get_coord_hash(coord_set *buf, uint64_t x, uint64_t y, node_id def)
{
    node_id hash = 0;
    uint64_t index = probe_set(buf, x, y);
    if(buf->xy[index].active)
        return buf->xy[index].hash;
    return def;
}



node *construct_node(node_table *table, coord_set *buf)
{    
    uint64_t count = buf->count;
    uint64_t index = 0;
    uint64_t k = 0;
    node *n = NULL;

    /* create two buffers; and fill the first with 
    cells in the right position */
    coord_set *cur_level = create_coord_set(count*2);
    coord_set *next_level = create_coord_set(count*2);
    for(int i=0;i<buf->size;i++)
    {
        if(buf->xy[i].active)
            insert_coord_set(cur_level, buf->xy[i].x, buf->xy[i].y, buf->xy[i].grey, buf->xy[i].hash);
    }
    printf("count=%d\n", cur_level->count);
    /* Keep merging until we have a single node */
    while(cur_level->count > 1)
    {                       
        node_id z =  table->zeros[k]->id;
        int total_pop = 0;
        int countr = 0;
        for(int i=0;i<cur_level->size;i++)
        {
            if(cur_level->xy[i].active)
            {
                uint64_t x = cur_level->xy[i].x;
                uint64_t y = cur_level->xy[i].y;
                x = x - (x & 1);
                y = y - (y & 1);
                node_id a, b, c, d;                                
                /* Read the neighbours, and get the hash of each */
                
                a = get_coord_hash(cur_level, x, y, z);
                b = get_coord_hash(cur_level, x+1, y, z);
                c = get_coord_hash(cur_level, x, y+1, z);
                d = get_coord_hash(cur_level, x+1, y+1, z);                
                n = join_hash(table, a, b, c, d);                                
                insert_coord_set(next_level, x >> 1, y >> 1, 1.0f, n->id);                                
                total_pop += n->pop;
                countr ++;
            }
        }
        printf("countr=%d count=%d\n", countr, next_level->count);
        printf("k=%d, pop=%d\n", k, total_pop);
        coord_set *temp = cur_level;
        cur_level = next_level;
        free_coord_set(temp);
        next_level = create_coord_set(count*2);        
        k++;
    }

    /* There were no cells; return the all zeros node at k=2 */
    if(n==NULL)
        return table->zeros[2];

    /* Get the last node and pad it */
    node *result = pad(table, get_table(table, n->id));
    free_coord_set(cur_level);
    free_coord_set(next_level);
    return result;
}

