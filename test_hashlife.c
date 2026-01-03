#include "hashlife.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#define TEST_PASSED(MSG) printf("%s passed\n", MSG)

void test_create_table()
{
    node_table *table = create_table();
    assert(get_table(table, 1) == table->off);
    assert(get_table(table, 2) == table->on);
    assert(table->count == 1 + ZERO_CACHE_MAX_SIZE);
    assert(table->size == INIT_TABLE_SIZE);
    assert(get_zero(table, 0) == table->off);
    assert(get_zero(table, 1) == table->zeros[1]);
    node *n;
    for(int i=0;i<ZERO_CACHE_MAX_SIZE;i++)
    {
        n = get_zero(table, i);
        assert(n->k == i);
        assert(n->pop == 0);
        assert(n->id!=0);
    }    
    assert(get_zero(table, ZERO_CACHE_MAX_SIZE)->pop==0);
    assert(get_zero(table, ZERO_CACHE_MAX_SIZE+64)->pop==0);
    TEST_PASSED("test_create_table");
}

void test_coord_buf()
{
    coord_set *buf = create_coord_set(10);
    assert(buf->size == 10);
    assert(buf->count == 0);
    node_id on = 1;
    node_id ret;
    insert_coord_set(buf, 1, 2, 0.5, on);
    
    assert(buf->count == 1);
    assert(get_coord_set(buf, 1, 2, &ret) == 0.5);    
    assert(ret == on);
    delete_coord_set(buf, 1, 2);
    assert(buf->count == 0);
    assert(get_coord_set(buf, 1, 2, &ret) == 0.0);
    insert_coord_set(buf, 2, 3, 0.5, on);
    insert_coord_set(buf, 3, 4, 0.25, on);
    assert(get_coord_set(buf, 2, 3, &ret) == 0.5);    
    expand_coord_set(buf);
    //assert(buf->size == 30);
    assert(buf->count == 2);    
    assert(get_coord_set(buf, 2, 3, &ret) == 0.5);
    assert(get_coord_set(buf, 3, 4, &ret) == 0.25);
    insert_coord_set(buf, 2, 3, 0.25, on);
    assert(get_coord_set(buf, 2, 3, &ret) == 0.25);

    assert(buf->min_x == 2);    
    assert(buf->max_x == 3); 
    assert(buf->min_y == 3);    
    assert(buf->max_y == 4); 
       
    clear_coord_set(buf);    
    assert(buf->count == 0);
    free(buf->xy);
    free(buf);
    TEST_PASSED("test_coord_buf");
}

void test_base_life()
{
    node_table *table = create_table();
    node *on, *off;
    on = table->on;
    off = table->off;    
    assert(base_life(table, on, off, off, off, off, off, off, off, off)==off);
    assert(base_life(table, on, off, on, off, off, off, on, off, off)==on);
    assert(base_life(table, on, off, on, off, on, off, off, off, off)==on);
    assert(base_life(table, on, off, on, off, on, off, off, on, off)==off);    
    assert(base_life(table, on, off, on, off, on, off, off, on, on)==off);
    assert(base_life(table, on, off, off, off, off, off, off, off, off)==off);
    assert(base_life(table, off, off, off, off, off, off, off, off, off)==off);

    TEST_PASSED("test_base_life");
}


void test_centre_pad_crop()
{
    node_table *table = create_table();
    node *on, *off;
    on = table->on;
    off = table->off;
    node *block = join_nodes(table, on, on, on, on);
    node *off_block = join_nodes(table, off, off, off, off);
    assert(block->pop==4);
    assert(off_block->pop==0);
    node *centre_block = centre(table, block);
    node *off_centre_block = centre(table, off_block);    
    assert(centre_block->pop == 4); 
    assert(centre_block->k == 2);
    assert(off_centre_block->k == 2);
    assert(off_centre_block->pop == 0);
    node *double_centre_block = centre(table, centre_block);
    assert(double_centre_block->pop == 4);
    assert(double_centre_block->k == 3);


    assert(inner(table, centre_block) == block);
    assert(inner(table, off_centre_block) == off_block);
    assert(inner(table, double_centre_block) == centre_block);

    assert(is_padded(table, block)==false);    
    assert(is_padded(table, double_centre_block)==true);    
    
    
    node *pad_block = pad(table, block);
    assert(is_padded(table, pad_block)==true);
    assert(pad_block->k == 4);
    assert(pad_block->pop == 4);

    node *deep_centre = centre(table, centre(table, centre(table, pad_block)));
    assert(deep_centre->k == 7);
    assert(deep_centre->pop == 4);
    assert(is_padded(table, deep_centre)==true);
    assert(crop(table, deep_centre)->k == 3);
    assert(crop(table, deep_centre)==inner(table, pad_block));

    TEST_PASSED("test_centre_pad_crop");
}


void test_4x4()
{
    node_table *table = create_table();
    uint64_t on, off;
    on = table->on->id;
    off = table->off->id;
    // generate all possible 4x4 life patterns
    for(int i=0;i<65536;i++)
    {
        uint64_t aa = (i & 1) ? on : off;
        uint64_t ab = (i & 2) ? on : off;
        uint64_t ac = (i & 4) ? on : off;
        uint64_t ad = (i & 8) ? on : off;
        uint64_t ba = (i & 16) ? on : off;
        uint64_t bb = (i & 32) ? on : off;
        uint64_t bc = (i & 64) ? on : off;
        uint64_t bd = (i & 128) ? on : off;
        uint64_t ca = (i & 256) ? on : off;
        uint64_t cb = (i & 512) ? on : off;
        uint64_t cc = (i & 1024) ? on : off;
        uint64_t cd = (i & 2048) ? on : off;
        uint64_t da = (i & 4096) ? on : off;
        uint64_t db = (i & 8192) ? on : off;
        uint64_t dc = (i & 16384) ? on : off;
        uint64_t dd = (i & 32768) ? on : off;
        node *a = join_hash(table, aa, ab, ac, ad);
        node *b = join_hash(table, ba, bb, bc, bd);
        node *c = join_hash(table, ca, cb, cc, cd);
        node *d = join_hash(table, da, db, dc, dd);        
        node *m = join_hash(table, a->id, b->id, c->id, d->id);
        assert(m->pop == __builtin_popcount(i));
        assert(m->k == 2);
        node *life = life_4x4(table, m);        
        assert(life->k == 1);        

    }
    node *block = join_hash(table, on, on, on, on);
    assert(life_4x4(table, table->zeros[2])==table->zeros[1]);
    node *block_next = life_4x4(table, centre(table, block));
    assert(block_next==block);

    
    TEST_PASSED("test_4x4");
}

void test_expand_construct()
{
    node_table *table = create_table();
    node *on, *off;
    on = table->on;
    off = table->off;
    /* Create a block and centre it */
    node *block = join_nodes(table, on, on, on, on);
    node *off_block = join_nodes(table, off, off, off, off);
    node *centre_block = centre(table, block);
    coord_set *buf = create_coord_set(256);
    full_expansion(table, block, buf);
    full_expansion(table, centre_block, buf);

    /* Test rasterisation */
    grey_buf *grey = create_grey_buf(8,8);
    rasterise_coord_set(buf, grey, 0, 0);
    assert(grey->grey[8+1] == 1.0f);
    assert(grey->grey[8+2] == 1.0f);
    assert(grey->grey[8+8+1] == 1.0f);
    assert(grey->grey[8+8+2] == 1.0f);
    assert(grey->grey[0] == 0.0f);
    assert(grey->grey[8+3] == 0.0f);
    /* Test that RLE generation works */
    char rle_buf[512], rle_buf2[512];
    rle_grey_buf(grey, rle_buf, 511, true);        

    /* Parse RLE, then test round trip */
    coord_set *glider_gun = rle_to_coords("24bo11b$22bobo11b$12b2o6b2o12b2o$11bo3bo4b2o12b2o$2o8bo5bo3b2o14b$2o8bo3bob2o4bobo11b$10bo5bo7bo11b$11bo3bo20b$12b2o!");    
    grey_buf *gun_buf = fully_rasterise(glider_gun);    
    rle_grey_buf(gun_buf, rle_buf, 511, false);    
    coord_set *gun = rle_to_coords(rle_buf);
    grey_buf *gun_buf2 = fully_rasterise(gun);
    rle_grey_buf(gun_buf2, rle_buf2, 511, false);
    assert(strcmp(rle_buf, rle_buf2)==0);

    /* Construct a node from the coord set */
    node_table *new_table = create_table();
    node *n = construct_node(new_table, glider_gun);
    coord_set *gun_coords = create_coord_set(256);
    assert(gun_coords->count == 0);
    full_expansion(new_table, n, gun_coords);
    print_coord_set(gun_coords);
    assert(gun_coords->count == glider_gun->count);
    grey_buf *gun_buf3 = fully_rasterise(gun_coords);
    print_grey_buf(gun_buf3);
    

    TEST_PASSED("test_expand_construct");
}

int main(int argc, char **argv)
{
    test_create_table();
    //test_coord_buf();
    test_base_life();
    //test_centre_pad_crop();
    test_expand_construct();
    test_4x4();
}