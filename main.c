#include "hashlife.h"
#include "cell_io.h"


/* Simple main. 
   Expects arguments of the form <file.rle> <generations>
   Reads RLE from stdin, writes RLE to stdout.
*/
int main(int argc, char **argv)
{
    if (argc != 3)
    {
        printf("Usage: %s <file.rle> <generations>\n", argv[0]);
        return 1;
    }
    char *filename = argv[1];
    uint64_t generations = strtoull(argv[2], NULL, 10);
    node_table *table = create_table(INIT_TABLE_SIZE);    
    node_id pattern = read_rle(table, filename);
    pattern = advance(table, pattern, generations);
    char *rle_out = to_rle(table, pattern);
    printf("%s\n", rle_out);
    free(rle_out);
    return 0;
}