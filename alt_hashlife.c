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

/* Find the successor of the given node, 2^level-2 steps in the future */
node_id successor(node_table *table, node_id id)
{
    node *n = lookup(table, id);

    node *c1, *c2, *c3, *c4, *c5, *c6, *c7, *c8, *c9;
    if (n->pop == 0)
        return n->a;
    if (n->level == 2)
        return n->next; // guaranteed to be cached
    c1 = join(table, n->aa, n->ab, n->ac, n->ad);
    c2 = join(table, n->ab, n->ba, n->ad, n->bc);
    c3 = join(table, n->ba, n->bb, n->bc, n->bd);
    c4 = join(table, n->ac, n->ad, n->ca, n->cb);
    c5 = join(table, n->ad, n->bc, n->cb, n->da);
    c6 = join(table, n->bc, n->bd, n->da, n->db);
    c7 = join(table, n->ca, n->cb, n->cc, n->cd);
    c8 = join(table, n->cb, n->da, n->cd, n->dc);
    c9 = join(table, n->da, n->db, n->dc, n->dd);
    return join(table,
                successor(table, join(table, c1, c2, c4, c5)),
                successor(table, join(table, c2, c3, c5, c6)),
                successor(table, join(table, c4, c5, c7, c8)),
                successor(table, join(table, c5, c6, c8, c9)));
}

/* Return the successor for this node, caching it as required */
node_id next(node_table *table, node_id id)
{
    node *n = lookup(table, id);
    if (n->next)
        return n->next;
    n->next = successor(table, id);
    return n->next;
}

/* Advance time by j steps.
    - Find the MSB of j
    - Make sure that the node has a level at least big enough for the MSB of j + 2
    - Repeatedly apply successor for each bit set in j (e.g. applying successor for 2^k-2 steps at level k, then moving to all nodes at level k-1, etc.)
*/
node_id advance(node_table *table, node_id id, uint64_t j)
{
    if (j == 0)
        return id;

    node *n = lookup(table, id);

    if (n->pop == 0)
        return id;

    // find the position of the MSB of j
    uint64_t msb = 0;
    uint64_t temp = j;
    while (temp >>= 1)
        msb++;

    // ensure the node is big enough
    while (n->level <= msb + 1)
    {
        id = centre(table, id);
        n = lookup(table, id);
    }

    node_id next_j = next(table, id);

    // check if j is now zero after knocking out the MSB
    j = j ^ (1ULL << msb);
    if (j == 0)
        return next_j;

    // otherwise, advance the next node by the remaining j
    return join(table,
                advance(table, join(table, n->a, n->b, n->c, n->d), j),
                advance(table, join(table, n->b, n->c, n->d, n->a), j),
                advance(table, join(table, n->c, n->d, n->a, n->b), j),
                advance(table, join(table, n->d, n->a, n->b, n->c), j));
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
    node *m = lookup(table, m_h);
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

    // expand the node until x < 1<< (level-1) and y < 1 << (level-1)
    while (x >= (1ULL << n->level) || y >= (1ULL << n->level))
    {
        id = centre(table, id);
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
float get_cell(node_table *table, node_id id, uint64_t x, uint64_t y, int level)
{
    node *n = lookup(table, id);
    if (n->level == 0 || n->level == level)
        return n->pop / (float)(1 << (n->level * 2));
    uint64_t size = 1 << n->level;
    // bounds test
    if (x < 0 || y < 0 || x >= size || y >= size)
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

/* Return true if the character is valid RLE */
bool is_tok(char ch)
{
    return ch == 'b' || ch == 'o' || ch == '$' || ch == '!' || isdigit(ch);
}

/* Read a single RLE element from a stream, skipping irrelevant material */
char *read_one(char *s, char *state, int *count)
{
    int n = 0;
    while (*s)
    {
        while (*s && isspace((unsigned char)*s))
            s++;
        if (!*s || is_tok(*s))
            break;

        while (*s && *s != '\n')
            s++;
        if (*s == '\n')
            s++;
    }

    if (!*s)
    {
        *state = '!';
        *count = 1;
        return s;
    }

    while (isdigit((unsigned char)*s))
        n = n * 10 + (*s++ - '0');

    *count = n ? n : 1;
    *state = *s++;
    return s;
}

/*
    Take an RLE string, ignore any size or comment information
    and insert the live cells into a hashlife node table, returning the root node_id
*/
node_id from_rle(node_table *table, char *rle_str)
{
    char *s = rle_str;
    char state;
    int count;
    uint64_t x = 0, y = 0;
    node_id root = table->off->id;

    while (1)
    {
        s = read_one(s, &state, &count);
        if (state == '!' || !state)
            break;
        else if (state == 'b')
        {
            x += count;
        }
        else if (state == 'o')
        {
            for (int i = 0; i < count; i++)
            {
                root = set_cell(table, root, x, y, true);
                x++;
            }
        }
        else if (state == '$')
        {
            y += count;
            x = 0;
        }
    }
    return root;
}

/*
    Take a hashlife node and output an RLE string into the given buffer.
    The buffer must be large enough to hold the output.
*/
void to_rle(node_table *table, node_id id, char *buf)
{
    char *p = buf;
    uint64_t size = 1ULL << lookup(table, id)->level;
    uint64_t count;
    p += sprintf(p, "x=%llu,y=%llu, rule = B3/S23\n", size, size);

    float cell_value;
    for (uint64_t y = 0; y < size; y++)
    {
        count = 0;
        cell_value = -1.0f; // invalid
        for (uint64_t x = 0; x < size; x++)
        {
            float v = get_cell(table, id, x, y, 0);
            if (v == cell_value)
            {
                count++;
            }
            else
            {
                // flush previous
                if (cell_value >= 0.0f)
                {
                    if (count > 1)
                        p += sprintf(p, "%d%c", (int)count, cell_value > 0.5f ? 'o' : 'b');
                    else
                        p += sprintf(p, "%c", cell_value > 0.5f ? 'o' : 'b');
                }
                // start new
                cell_value = v;
                count = 1;
            }
        }
        // flush end of line
        if (cell_value >= 0.0f)
        {
            if (count > 1)
                p += sprintf(p, "%d%c", (int)count, cell_value > 0.5f ? 'o' : 'b');
            else
                p += sprintf(p, "%c", cell_value > 0.5f ? 'o' : 'b');
        }
        // line end
        p += sprintf(p, "$");
    }
    // end marker
    sprintf(p, "!");
}