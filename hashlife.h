#include <stdint.h>
#include <stdbool.h>
#define INIT_TABLE_SIZE 4096
#define ZERO_CACHE_MAX_SIZE 256 // max size of the zero cache = 2^256

typedef uint64_t node_id;

typedef struct node {
    uint64_t k;
    node_id a, b, c, d;
    uint64_t pop;
    node_id id;
} node;

typedef struct node_table
{
    node *nodes;
    uint64_t size;
    uint64_t count;    
    node *on, *off; // fixed references to the on and off nodes at k=0
    node *zeros[ZERO_CACHE_MAX_SIZE]; // cache of zero nodes up to MAX_SIZE
} node_table;


typedef struct coord
{
    uint64_t x, y;
    float grey;
    node_id hash;    
    bool active;
} coord;


typedef struct coord_set
{
    coord *xy;
    uint64_t size;
    uint64_t count;
    uint64_t min_x, min_y, max_x, max_y;
} coord_set;

typedef struct grey_buf
{
    float *grey;
    uint64_t rows;
    uint64_t cols;
} grey_buf;

node *base_life(node_table *table, node *a, node *b, node *c, node *d, node *e, node *f, node *g, node *h, node *i);
node_table *create_table();
node *insert_table(node_table *table, node *n);
node *get_table(node_table *table, uint64_t hash);
void expand_table(node_table *table);
node *join_nodes(node_table *table, node *a, node *b, node *c, node *d);
node *get_zero(node_table *table, int k);
node *centre(node_table *table, node *m);
node *inner(node_table *table, node *n);
void *pad(node_table *table, node *n);
bool is_padded(node_table *table, node *n);
node *crop(node_table *table, node *n);
node *life_4x4(node_table *table, node *m);
node *join_hash(node_table *table, uint64_t a_hash, uint64_t b_hash, uint64_t c_hash, uint64_t d_hash);
coord_set *create_coord_set(uint64_t capacity);
uint64_t probe_set(coord_set *set, uint64_t x, uint64_t y);
void insert_coord_set(coord_set *buf, uint64_t x, uint64_t y, float grey, node_id hash);
void delete_coord_set(coord_set *buf, uint64_t x, uint64_t y);
float get_coord_set(coord_set *buf, uint64_t x, uint64_t y, node_id *hash);
void expand_coord_set(coord_set *buf);
void clear_coord_set(coord_set *buf);
void print_coord_set(coord_set *buf);
void full_expansion(node_table *table, node *n, coord_set *buf);
void expand_to_points(node_table *table, node *n, uint64_t x, uint64_t y, coord_set *buf, uint64_t level, uint64_t *clip);
void rasterise_coord_set(coord_set *buf, grey_buf *raster, int64_t x_offset, int64_t y_offset);
grey_buf *create_grey_buf(uint64_t rows, uint64_t cols);
grey_buf *fully_rasterise(coord_set *buf);
void print_grey_buf(grey_buf *buf);
void rle_grey_buf(grey_buf *buf, char *str, int len, bool header);
void free_coord_set(coord_set *buf);
void print_coord_set(coord_set *buf);


coord_set *rle_to_coords(char *rle);
node *construct_node(node_table *table, coord_set *buf);
node_id get_hash_coord(coord_set *buf, uint64_t x, uint64_t y, node_id default_hash);
void insert_hash_coord(coord_set *buf, uint64_t x, uint64_t y, node_id hash);
