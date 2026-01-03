#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include "hashlife.h"


/* Create a buffer for storing coordinates */
coord_set *create_coord_set(uint64_t capacity)
{
    coord_set *set = (coord_set *)malloc(sizeof(coord_set));
    set->xy = (coord *)calloc(capacity, sizeof(*set->xy));
    set->size = capacity;
    clear_coord_set(set);
    return set;
}

uint64_t probe_set(coord_set *set, uint64_t x, uint64_t y)
{    
    uint64_t index = (x * 0x114aefbc97f18777 + y * 0x2a748ed22de145df ) % set->size;        
    while(set->xy[index].active && (set->xy[index].x != x && set->xy[index].y != y)) 
    {index = (index + 1) % set->size;}
    return index;
}

/* Insert a coordinate pair into the buffer */
void insert_coord_set(coord_set *buf, uint64_t x, uint64_t y, float grey, node_id hash)
{
    uint64_t index = probe_set(buf, x, y);    
    if(!buf->xy[index].active) buf->count++;
    buf->xy[index].active = true;
    buf->xy[index].hash = hash;
    buf->xy[index].grey = grey;
    buf->xy[index].x = x;
    buf->xy[index].y = y;    
    if(x < buf->min_x) buf->min_x = x;
    if(x > buf->max_x) buf->max_x = x;
    if(y < buf->min_y) buf->min_y = y;
    if(y > buf->max_y) buf->max_y = y;
    
    if(buf->count > buf->size / 3) { expand_coord_set(buf);};

}

/* Remove a coordinate pair from the buffer */
void delete_coord_set(coord_set *buf, uint64_t x, uint64_t y)
{
    uint64_t index = probe_set(buf, x, y);
    buf->xy[index].active = 0;
    buf->xy[index].hash = 0;
    buf->xy[index].grey = 0.0f;
    buf->count--;
}

/* Check if a coordinate pair is in the buffer */
float get_coord_set(coord_set *buf, uint64_t x, uint64_t y, node_id *hash)
{
    uint64_t index = probe_set(buf, x, y);    
    if(!buf->xy[index].active) {
        if(hash) *hash = 0;
        return 0.0f;    
    }
    if(hash) *hash = buf->xy[index].hash;
    return buf->xy[index].grey;
}

/* Double the size of the buffer and re-insert all coordinates */
void expand_coord_set(coord_set *buf)
{    
    coord_set *new_buf = create_coord_set(buf->size*2);
    coord *c;
    for(int i=0;i<buf->size;i++)
    {
        c = &buf->xy[i];        
        if(c->active)
            insert_coord_set(new_buf, c->x, c->y, c->grey, c->hash);                       
    }    
    free(buf->xy);
    buf->xy = new_buf->xy;    
    buf->size = new_buf->size;    
    buf->count = new_buf->count; 
    buf->min_x = new_buf->min_x;
    buf->min_y = new_buf->min_y;
    buf->max_x = new_buf->max_x;
    buf->max_y = new_buf->max_y;
    free(new_buf);        
}


/* Clear the buffer, but don't free the memory */
void clear_coord_set(coord_set *buf)
{
    memset(buf->xy, 0, buf->size * sizeof(coord));
    buf->count = 0;
    buf->min_x = UINT64_MAX;
    buf->min_y = UINT64_MAX;
    buf->max_x = 0;
    buf->max_y = 0;
}

/* Free the memory used by the buffer */
void free_coord_set(coord_set *buf)
{
    free(buf->xy);
    free(buf);
}


/* Dump the contents of a coord_buf to stdout */
void print_coord_set(coord_set *buf)
{
    for(int i=0;i<buf->size;i++)
    {
        if(buf->xy[i].active)
            printf("(%d, %d) %f\n", (int)buf->xy[i].x, (int)buf->xy[i].y, buf->xy[i].grey);
    }
}


/* Construct a coord set from an RLE string */
coord_set *rle_to_coords(char *rle)
{
    char *p = rle;
    int x = 0, y = 0;
    int count = 0;
    coord_set *buf = create_coord_set(256);
    while(*p)
    {
        // skip comments etc.
        if(*p=='#' || *p=='x') while(*p && *p!='\n') p++;        
        if(*p >= '0' && *p <= '9')
        {
            count = count * 10 + (*p - '0');
        }
        else
        {
            if(*p == 'o') 
            {
                if(count==0) count = 1;                
                for(int i=0;i<count;i++)
                {
                    insert_coord_set(buf, x, y, 1.0f, 2);                    
                    x++;
                }
            }   
            if(*p == 'b') 
            {
                if(count==0) count = 1;                
                for(int i=0;i<count;i++)
                {                    
                    x++;
                }
            }         
            if(*p == '$')
            {
                y++;
                x = 0;                
            }            
            count = 0;
        }
        p++;
    }
    return buf;
}



/* Write elements in a coord_buf into a grey_buf (raster),
with a specified offset; only write points in bounds of the raster. */
void rasterise_coord_set(coord_set *buf, grey_buf *raster, int64_t x_offset, int64_t y_offset)
{
    memset(raster->grey, 0, raster->rows * raster->cols * sizeof(float));    
    for(int i=0;i<buf->size;i++)
    {
        int64_t x = buf->xy[i].x + x_offset;
        int64_t y = buf->xy[i].y + y_offset;
        if(x < raster->cols && y < raster->rows)
            raster->grey[y * raster->cols + x] = buf->xy[i].grey;
    }
}

/* Create a new grey buffer with the specified rows and columns */
grey_buf *create_grey_buf(uint64_t rows, uint64_t cols)
{
    grey_buf *raster = (grey_buf *)malloc(sizeof(grey_buf));
    raster->rows = rows;
    raster->cols = cols;
    raster->grey = (float *)calloc(rows * cols, sizeof(float));
    return raster;
}

/* Fully rasterise a coordinate buffer, allocating a new grey buffer */
grey_buf *fully_rasterise(coord_set *buf)
{
    grey_buf *raster = create_grey_buf(buf->max_y - buf->min_y + 1, buf->max_x - buf->min_x + 1);
    rasterise_coord_set(buf, raster, -buf->min_x, -buf->min_y);
    return raster;
}

/* Print a grey buffer to stdout */
void print_grey_buf(grey_buf *buf)
{
    char *chars = " .:-=+*#%@";
    int c;
    int n = strlen(chars);
    char ch;
    for(int i=0;i<buf->rows;i++)
    {
        for(int j=0;j<buf->cols;j++)
        {
            c = (int)(buf->grey[i * buf->cols + j] * (n-1));
            ch = c < n ? chars[c] : '?';
            printf("%c", ch);
        }
        printf("\n");
    }
}

/* Print a grey buffer to stdout in RLE format */
void rle_grey_buf(grey_buf *buf, char *str, int len, bool header)
{
    int count = 1;
    int last;
    char *end = str + len;
    // print the size
    if(header)
        str += snprintf(str, end-str, "x=%d,y=%d\n", (int)buf->cols, (int)buf->rows);
    for(int j=0;j<buf->rows;j++)
    {
        // each line, reset the count and last value
        last = buf->grey[j * buf->cols] > 0.5;
        count = 1;
        for(int i=1;i<buf->cols;i++)
        {
            int k = j * buf->cols + i;
            if((buf->grey[k] > 0.5) == last)
                count++;
            else
            {
                if(count>1)
                    str +=  snprintf(str, end-str, "%d%c", count, last > 0.5 ? 'o' : 'b');
                else
                    str +=  snprintf(str, end-str, "%c", last > 0.5 ? 'o' : 'b');
                count = 1;
                last = buf->grey[k] > 0.5;
            }
        }
        str += snprintf(str, end-str, "%d%c", count, last > 0.5 ? 'o' : 'b');
        str += snprintf(str, end-str, "$");        
    }
    snprintf(str, end-str, "!");    
}

