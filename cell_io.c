#include "hashlife.h"

/* Read a .* style plain text pattern
    and return the corresponding hashlife node
*/
node_id from_text(node_table *table, char *txt)
{
    node_id root = table->zeros[1];
    uint64_t x = 0, y = 0;
    char *p = txt;
    while (*p)
    {
        switch (*p++)
        {
        case 'O':
            root = set_cell(table, root, x, y, 1);
            x++;
            break;
        case '.':
            x++;
            break;
        case '\n':
            y++;
            x = 0;
            break;
        }
    }
    return root;
}

/* Convert a hashlife node to a .* style plain text pattern
    Allocates a buffer; caller must free it.
*/
char *to_text(node_table *table, node_id id)
{
    uint64_t size = 1ULL << lookup(table, id)->level;
    char *p = malloc(size * (size + 1) + 1); // include newlines and null terminator
    char *start = p;
    for (uint64_t y = 0; y < size; y++)
    {
        for (uint64_t x = 0; x < size; x++)
        {
            float v = get_cell(table, id, x, y, 0);
            *p++ = v > 0.5f ? 'O' : '.';
        }
        *p++ = '\n';
    }
    *p++ = 0;
    return start;
}

uint64_t hash_life_text(char *text)
{
    uint64_t seed = mix64(0xdeadbeef);
    char *p = text;
    uint64_t x = 0, y = 0, ax = 0, ay = 0, anchored = 0;
    uint64_t dx, dy;
    while (*p)
    {
        switch (*p++)
        {
        case 'O':
            if (!anchored)
            {
                anchored = 1;
                ax = x;
                ay = y;
            }
            dx = x - ax;
            dy = y - ay;
            seed = mix64(dx ^ seed);
            seed = mix64(dy ^ seed);
            x++;
            break;
        case '.':
            x++;
            break;
        case '\n':
            y++;
            x = 0;
            break;
        }
    }
    return seed;
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
    node_id root = table->zeros[2];
    int pop = 0;
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
                pop++;
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