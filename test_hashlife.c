#include "hashlife.h"
#include "cell_io.h"
#include <stdbool.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// macro to print test in green, with a tick at the start

#define TEST_OK(msg) printf("\033[0;32m * %s\033[0m\n", msg);
// yellow
#define TEST_START(msg) printf("\033[0;33m... %s\033[0m\n", msg);

void verify_hashtable(node_table *table)
{
    uint64_t entries = 0;
    TEST_START("Verifying hashtable");
    for (uint64_t i = 0; i < table->size; i++)
    {
        node *n = &table->index[i];
        if (n->id != 0)
        {
            entries++;
            node *lookup_n = lookup(table, n->id);
            assert(lookup_n == n); // should find itself
        }
    }
    assert(entries == table->count);
    // verify load is < 25%
    assert(table->count * 4 < table->size);
    TEST_OK("Hashtable verified");
}

void verify_children(node_table *table)
{
    TEST_START("Verifying children");
    // scan the entire table.
    // check: all nodes with level > 0 have valid children (non-zero, right level)
    // all nodes with level > 1 have valid grandchildren (non-zero, right level)
    for (uint64_t i = 0; i < table->size; i++)
    {
        node *n = &table->index[i];
        if (n->id != 0)
        {
            if (n->level > 0)
            {
                node *a = lookup(table, n->a);
                node *b = lookup(table, n->b);
                node *c = lookup(table, n->c);
                node *d = lookup(table, n->d);
                assert(a != NULL && a->level == n->level - 1);
                assert(b != NULL && b->level == n->level - 1);
                assert(c != NULL && c->level == n->level - 1);
                assert(d != NULL && d->level == n->level - 1);
            }
            if (n->level > 1)
            {
                node *a = lookup(table, n->a);
                node *b = lookup(table, n->b);
                node *c = lookup(table, n->c);
                node *d = lookup(table, n->d);
                node *aa = lookup(table, n->aa);
                node *ab = lookup(table, n->ab);
                node *ac = lookup(table, n->ac);
                node *ad = lookup(table, n->ad);
                node *ba = lookup(table, n->ba);
                node *bb = lookup(table, n->bb);
                node *bc = lookup(table, n->bc);
                node *bd = lookup(table, n->bd);
                node *ca = lookup(table, n->ca);
                node *cb = lookup(table, n->cb);
                node *cc = lookup(table, n->cc);
                node *cd = lookup(table, n->cd);
                node *da = lookup(table, n->da);
                node *db = lookup(table, n->db);
                node *dc = lookup(table, n->dc);
                node *dd = lookup(table, n->dd);
                assert(aa != NULL && aa->level == n->level - 2);
                assert(ab != NULL && ab->level == n->level - 2);
                assert(ac != NULL && ac->level == n->level - 2);
                assert(ad != NULL && ad->level == n->level - 2);
                assert(ba != NULL && ba->level == n->level - 2);
                assert(bb != NULL && bb->level == n->level - 2);
                assert(bc != NULL && bc->level == n->level - 2);
                assert(bd != NULL && bd->level == n->level - 2);
                assert(ca != NULL && ca->level == n->level - 2);
                assert(cb != NULL && cb->level == n->level - 2);
                assert(cc != NULL && cc->level == n->level - 2);
                assert(cd != NULL && cd->level == n->level - 2);
                assert(da != NULL && da->level == n->level - 2);
                assert(db != NULL && db->level == n->level - 2);
                assert(dc != NULL && dc->level == n->level - 2);
                assert(dd != NULL && dd->level == n->level - 2);
                assert(aa == lookup(table, a->a));
                assert(ab == lookup(table, a->b));
                assert(ac == lookup(table, a->c));
                assert(ad == lookup(table, a->d));
                assert(ba == lookup(table, b->a));
                assert(bb == lookup(table, b->b));
                assert(bc == lookup(table, b->c));
                assert(bd == lookup(table, b->d));
                assert(ca == lookup(table, c->a));
                assert(cb == lookup(table, c->b));
                assert(cc == lookup(table, c->c));
                assert(cd == lookup(table, c->d));
                assert(da == lookup(table, d->a));
                assert(db == lookup(table, d->b));
                assert(dc == lookup(table, d->c));
                assert(dd == lookup(table, d->d));
            }
        }
    }
    TEST_OK("Children verified");
}

/* Verify that the graph terminates in either on or off nodes at level 0, and has
 monotonically decreasing levels down the tree and that population sums
 are in range and sum to the correct levels */
uint64_t verify_tree(node_table *table, node_id id, uint64_t level)
{
    node *n = lookup(table, id);
    assert(n->pop <= (1ULL << (2 * n->level)));
    assert(n->level == level);
    
    assert(n != NULL);
    if (n->level == 0)
    {
        assert(id == table->on || id == table->off);
        return n->pop;
    }
    else
    {
        uint64_t pop = 0;
        pop += verify_tree(table, n->a, n->level - 1);
        pop += verify_tree(table, n->b, n->level - 1);
        pop += verify_tree(table, n->c, n->level - 1);
        pop += verify_tree(table, n->d, n->level - 1);
        assert(n->pop == pop);
        return pop;
    }
}

void verify_whole_tree(node_table *table)
{

    TEST_START("Verifying whole tree");
    for (uint64_t i = 0; i < table->size; i++)
    {
        node *n = &table->index[i];
        if (n->id != 0 && n->level <= 8)
            verify_tree(table, n->id, n->level);
    }
    TEST_OK("Whole tree verified");
}

void verify_whole_population(node_table *table)
{
    TEST_START("Verifying whole population");

    for (uint64_t i = 0; i < table->size; i++)
    {
        node *n = &table->index[i];
        if (n->id != 0 && n->level <= 8)
        {
            bool ok = verify_tree(table, n->id, n->level);
            assert(ok);
        }
    }
    TEST_OK("Whole population verified");
}

void test_inner(node_table *table, node_id id)
{
    TEST_START("Testing inner node creation");

    node_id in = inner(table, id);
    node *n = lookup(table, id);
    node *in_n = lookup(table, in);
    assert(in_n->level == n->level - 1);
    node_id centre_id = centre(table, id);
    node *centre_n = lookup(table, centre_id);
    assert(centre_n->level == n->level + 1);
    assert(centre_n->pop == n->pop);
    assert(is_padded(table, centre_id));
    assert(inner(table, centre_id) == id);
    assert(!is_padded(table, crop(table, id)));
    assert(inner(table, inner(table, centre(table, (centre(table, id))))) == id);
    TEST_OK("Inner node creation verified");
}

void print_node(node_table *table, node_id id)
{
    node *n = lookup(table, id);
    printf("Node ID: %llu, Level: %llu, Pop: %llu, Children: [%llu, %llu, %llu, %llu]\n", n->id, n->level, n->pop, n->a, n->b, n->c, n->d);
}

void test_init()
{
    TEST_START("Testing table initialisation...");
    // force expansions
    node_table *table = create_table(131072);
    printf("Table has %llu entries\n", table->count);
    printf("Table size: %llu\n", table->size);
    assert(table != NULL);
    printf("Table was not NULL\n");
    assert(lookup(table, table->on) != NULL);
    assert(lookup(table, table->off) != NULL);
    printf("On and Off nodes were not NULL\n");
    node *on, *off;
    on = lookup(table, table->on);
    off = lookup(table, table->off);
    assert(on->pop == 1);
    assert(off->pop == 0);
    assert(on->id == 2);
    assert(off->id == 1);

    // verify all zero nodes are zero population and have correct level
    for (int i = 0; i < ZERO_CACHE_MAX_SIZE; i++)
    {
        node *z = lookup(table, table->zeros[i]);
        assert(z != NULL);
        assert(z->pop == 0);
        assert(z->level == (uint64_t)i);
    }
    printf("All zero nodes verified\n");
    verify_hashtable(table);

    verify_children(table);
    verify_whole_tree(table);

    // all 4x4 nodes have a valid next pointer to a 2x2 node
    for (uint64_t i = 0; i < table->size; i++)
    {
        node *n = &table->index[i];
        if (n->id != 0 && n->level == 2)
        {
            // should have a valid next pointer to a 2x2 node
            node *next_n = lookup(table, n->next);
            assert(next_n != NULL);
            assert(next_n->level == 1);
        }
    }
    printf("All 4x4 nodes have valid next pointers\n");
    for (uint64_t i = 0; i < table->size; i++)
    {
        node *n = &table->index[i];
        if (n->id != 0)
        {
            // all nodes should be immortal at this point
            assert(n->ref_count == -1);
        }
    }
    printf("All nodes are immortal\n");
    TEST_OK("Table initialisation verified");
}

#define TEST_CELLS 256
void test_set_get()
{
    node_table *table = create_table(8);
    uint64_t test_cells[TEST_CELLS][2];

    // generate random test cells
    srand(42);
    for (int i = 0; i < TEST_CELLS; i++)
    {
        test_cells[i][0] = rand() % 1024;
        test_cells[i][1] = rand() % 1024;
    }
    TEST_START("Testing set and get cells");
    // set cells
    node_id node = table->zeros[2]; // 4x4 block
    // set 3, 7, check it, clear it, check it
    node = set_cell(table, node, 3, 7, 1);
    assert(lookup(table, node)->pop == 1);
    float grey = get_cell(table, node, 3, 7, 0);
    assert(grey == 1.0f);
    node = set_cell(table, node, 3, 7, 0);
    assert(lookup(table, node)->pop == 0);
    grey = get_cell(table, node, 3, 7, 0);
    assert(grey == 0.0f);
    printf("Single cell set and get verified\n");
    // check some empty cells
    for (int i = 0; i < TEST_CELLS; i++)
    {
        float grey = get_cell(table, node, test_cells[i][0], test_cells[i][1], 0);
        assert(grey == 0.0f);
    }
    printf("All empty cells verified\n");
    for (int i = 0; i < TEST_CELLS; i++)
    {
        node = set_cell(table, node, test_cells[i][0], test_cells[i][1], 1);
    }
    assert(lookup(table, node)->pop == TEST_CELLS);
    printf("Node size after set cells: %d\n", 1 << lookup(table, node)->level);
    printf("Grey level after setting %d cells: %f\n", TEST_CELLS, get_cell(table, node, 0, 0, lookup(table, node)->level));

    // get cells
    for (int i = 0; i < TEST_CELLS; i++)
    {
        float grey = get_cell(table, node, test_cells[i][0], test_cells[i][1], 0);
        assert(grey == 1.0f);
    }
    // check some empty cells
    for (int i = 0; i < TEST_CELLS; i++)
    {
        float grey = get_cell(table, node, test_cells[i][0] + 1024, test_cells[i][1] + 1024, 0);
        assert(grey == 0.0f);
    }
    printf("All set cells verified\n");
    TEST_OK("Set and Get cells verified");
}

bool verify_same(node_table *table, node_id id1, char *original)
{
    char *buf = to_text(table, id1);
    bool result = hash_life_text(buf) == hash_life_text(original);
    free(buf);
    return result;
}

void test_still_life()
{
    TEST_START("Testing still life pattern");
    node_table *table = create_table(64);
    char *mickey_mouse = ".OO....OO\nO..O..O..O\nO..OOOO..O\n.OO....OO\n...OOOO\n...O..O\n....OO";
    node_id mickey = from_text(table, mickey_mouse);    

    /* Generate the still life pattern and verify it never changes */
    mickey = centre(table, centre(table, mickey));
    verify_children(table);
    verify_tree(table, mickey, lookup(table, mickey)->level);
    
    node_id succ = next(table, mickey, 0);
    assert(verify_same(table, succ, mickey_mouse));
    
    printf("Still life test one passed\n");
    succ = next(table, succ, 0);
    assert(verify_same(table, succ, mickey_mouse));
    printf("Still life test two passed\n");    
    node_id next_1 = advance(table, mickey, 8);
    assert(verify_same(table, next_1, mickey_mouse));
    printf("Still life variable step with advance passed\n");
    TEST_OK("Still life pattern verified");
}

void print_table_stats(node_table *table)
{
    uint64_t used = 0;
    for (uint64_t i = 0; i < table->size; i++)
    {
        node *n = &table->index[i];
        if (n->id != 0)
            used++;
    }
    printf("Hashtable usage: %llu / %llu (%.2f%%)\n", used, table->size, (used * 100.0) / table->size);
}

void test_gun()
{
    node_table *table = create_table(128);
    /* Check the glider gun does in fact change */
    char *gosper_gun = "........................O\n......................O.O\n............OO......OO............OO\n...........O...O....OO............OO\nOO........O.....O...OO\nOO........O...O.OO....O.O\n..........O.....O.......O\n...........O...O\n............OO";
    
    node_id gun = from_text(table, gosper_gun);
    for(int i=1;i<30;i++)
    {
        node_id g = advance(table, gun, i);        
        assert(!verify_same(table, g, gosper_gun));
    }
    printf("Verified variable pattern changes with advance\n");

    /* repeatedly advance by big steps, check that population increases */
    for(int i=0;i<6;i++)
    {
        gun = centre(table, centre(table, gun));
    }
    uint64_t last_pop = lookup(table, gun)->pop;
    for(int i=0;i<10;i++)
    {
        gun = next(table,  centre(table, centre(table, gun)), 0);
        
        uint64_t pop = lookup(table, gun)->pop;
        assert(pop >= last_pop);
        last_pop = pop;
        
    }
    gun = from_text(table, gosper_gun);
    /* verify that advancing by a big step is the same as advancing with several smaller steps */
    int test_steps[] = {2, 16, 5, 31, 255, 256};
    for(int i=0;i<6;i++)
    {
        int steps = test_steps[i];
        node_id big_step = advance(table, gun, steps);
        node_id small_step = gun;
        for(int j=0;j<steps;j++)
        {
            small_step = advance(table, small_step, 1);
        }
        char *big_text = to_text(table, big_step);
        verify_same(table, small_step, big_text);
        free(big_text);
    }
    
    printf("Verified big step advance matches multiple small steps\n");
    print_table_stats(table);
    TEST_OK("Variable pattern verified");
}

void test_advance()
{
    TEST_START("Testing pattern advancement");
    test_still_life();
    test_gun();
    TEST_OK("Pattern advancement verified");
}


void test_rle()
{
    TEST_START("Testing RLE import/export");
    node_table *table = create_table(64);
    char *gosper_gun = "........................O\n......................O.O\n............OO......OO............OO\n...........O...O....OO............OO\nOO........O.....O...OO\nOO........O...O.OO....O.O\n..........O.....O.......O\n...........O...O\n............OO";
    char *gosper_rle = "#N Gosper glider gun\n#O Bill Gosper\n#C A true period 30 glider gun.\n#C The first known gun and the first known finite pattern with unbounded growth.\n#C www.conwaylife.com/wiki/index.php?title=Gosper_glider_gun\nx = 36, y = 9, rule = B3/S23\n24bo11b$22bobo11b$12b2o6b2o12b2o$11bo3bo4b2o12b2o$2o8bo5bo3b2o14b$2o8b\no3bob2o4bobo11b$10bo5bo7bo11b$11bo3bo20b$12b2o!";
    /* Check that the RLE matches the plain text format */
    node_id rle_gun = from_rle(table, gosper_rle);
    assert(lookup(table, rle_gun)->pop == 36);
    assert(verify_same(table, rle_gun, gosper_gun));

    /* check that from_rle -> to_rle -> from_rle does not change the pattern */
    char rle_buf[4096];
    to_rle(table, rle_gun, rle_buf);
    node_id cycle_rle_gun = from_rle(table, rle_buf);
    assert(verify_same(table, cycle_rle_gun, gosper_gun));
    TEST_OK("RLE import/export verified");

}

void test_pattern()
{
    TEST_START("Testing pattern import/export");
    node_table *table = create_table(64);
    
    char *mickey_mouse = ".OO....OO\nO..O..O..O\nO..OOOO..O\n.OO....OO\n...OOOO\n...O..O\n....OO";
    node_id mickey = from_text(table, mickey_mouse);
    node_id centered = centre(table, mickey);
    assert(verify_same(table, centered, mickey_mouse));
    node_id cropped = crop(table, mickey);
    assert(verify_same(table, cropped, mickey_mouse));
    
    printf("Testing inner/center/pad/crop functions on pattern\n");
    test_inner(table, mickey);
    TEST_OK("Pattern import/export verified");
}

/* Load and return an RLE of a pattern */
node_id load_rle(node_table *table, char *filename)
{
    FILE *f = fopen(filename, "r");
    if (!f)
    {
        printf("Failed to open RLE file: %s\n", filename);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = malloc(fsize + 1);
    fread(buf, 1, fsize, f);
    buf[fsize] = 0;
    fclose(f);

    node_id id = from_rle(table, buf);
    free(buf);
    return id;
}

/* static for timing access */
static node_table *timing_table;
static node_id timing_pattern;

int time_next()
{
    node_id pattern = timing_pattern;
    for(int i=0;i<16;i++)
        pattern = next(timing_table, centre(timing_table, centre(timing_table, pattern)), 0);
    return lookup(timing_table, pattern)->pop;
}

int time_advance_1()
{
    node_id pattern = timing_pattern;
    
    pattern = advance(timing_table, pattern, 1);
    return lookup(timing_table, pattern)->pop;
}

int time_advance_64()
{
    node_id pattern = timing_pattern;
    
    pattern = advance(timing_table, pattern, 64);
    return lookup(timing_table, pattern)->pop;
}

int time_advance_256()
{
    node_id pattern = timing_pattern;
    
    pattern = advance(timing_table, pattern, 256);
    return lookup(timing_table, pattern)->pop;
}

int time_advance_65535()
{
    node_id pattern = timing_pattern;
    pattern = advance(timing_table, pattern, 65535);
    return lookup(timing_table, pattern)->pop;
}

int time_advance_65536()
{
    node_id pattern = timing_pattern;
    pattern = advance(timing_table, pattern, 65536);
    return lookup(timing_table, pattern)->pop;
}


int load_rle_time()
{
    
    char *gosper_rle = "24bo11b$22bobo11b$12b2o6b2o12b2o$11bo3bo4b2o12b2o$2o8bo5bo3b2o14b$2o8b\no3bob2o4bobo11b$10bo5bo7bo11b$11bo3bo20b$12b2o!";
    node_id pattern = from_rle(timing_table, gosper_rle);
    return lookup(timing_table, pattern)->pop;
    
}

void timeit(int (*func)(void), const char *name, int iters, int warm, int trials);

int main()
{
    test_init();   
    test_set_get();
    test_pattern();
    test_advance();
    test_rle();

    /* timing tests */
    timing_table = create_table(131072);
    timing_pattern = load_rle(timing_table, "pat/breeder.rle");
    timeit(time_next, "Advance breeder ", 500, 50, 7);

    timeit(time_advance_1, "Advance breeder by 1", 500, 50, 7);
    timeit(time_advance_64, "Advance breeder by 64", 500, 50, 7);
    timeit(time_advance_256, "Advance breeder by 256", 500, 50, 7);
    timeit(time_advance_65535, "Advance breeder by 65535", 500, 50, 7);
    timeit(time_advance_65536, "Advance breeder by 65536", 500, 50, 7);
    timeit(load_rle_time, "Load Gosper glider gun RLE", 1000, 100, 7);



}

