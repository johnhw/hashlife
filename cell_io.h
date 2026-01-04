#include "hashlife.h"

/* RLE and pattern */
bool is_tok(char ch);
char *read_one(char *s, char *state, int *count);
node_id from_rle(node_table *table, char *rle_str);
void to_rle(node_table *table, node_id id, char *buf);
node_id from_text(node_table *table, char *text);
char *to_text(node_table *table, node_id id); // caller frees
void rasterise(node_table *table, node_id id, float *buf, uint64_t buf_width, uint64_t buf_height, uint64_t x, uint64_t y, uint64_t width, uint64_t height, uint64_t min_level);
uint64_t hash_life_text(char *text);
