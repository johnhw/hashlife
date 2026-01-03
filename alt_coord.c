
typedef struct coord 
{
    int64_t x;
    int64_t y;
    float grey;    
} coord;

typedef struct coord_set
{
    coord *xy;
    int64_t size;
    int64_t count;
    uint64_t min_x, min_y, max_x, max_y;
} coord_set;

coord_set *create_coord_set(uint64_t capacity)
{
    coord_set *set = (coord_set *)malloc(sizeof(coord_set));
    set->xy = (coord *)calloc(capacity, sizeof(*set->xy));
    set->size = capacity;
    set->count = 0;
    set->min_x = 0;
    set->min_y = 0;
    set->max_x = 0;
    set->max_y = 0;
    return set;
}

coord_set *free_coord_set(coord_set *buf)
{
    free(buf->xy);
    free(buf);
    return NULL;
}

void insert_coord_set(coord_set **buf_ptr, uint64_t x, uint64_t y, float grey)
{
    coord_set *buf = *buf_ptr;
    uint64_t hash_buf[4];
    if(buf->count >= buf->size / 4)
    {
        *buf_ptr = expand_set(buf);
        buf = *buf_ptr;
    }    
    hash_buf[0] = (uint64_t) x;
    hash_buf[1] = (uint64_t) y;
    hash_buf[2] = (uint64_t) (grey * 1000000.0f);
    hash_buf[3] = 0;
    uint64_t index = mix64(hash_quad(hash_buf[0], hash_buf[1], hash_buf[2], hash_buf[3])) % buf->size;
    while(buf->xy[index].grey != 0.0f && (buf->xy[index].x != x || buf->xy[index].y != y || buf->xy[index].grey != grey)) 
        index = (index + 1) % buf->size;
    buf->xy[index].x = x;
    buf->xy[index].y = y;
    buf->xy[index].grey = grey;
    buf->count++;
    if(x < buf->min_x) buf->min_x = x;
    if(x > buf->max_x) buf->max_x = x;
    if(y < buf->min_y) buf->min_y = y;
    if(y > buf->max_y) buf->max_y = y;
}

coord_set *expand_set(coord_set *buf)
{
    coord_set *new_buf = create_coord_set(buf->size*2);
    for(int i=0;i<buf->size;i++)
        insert_coord_set(new_buf, buf->xy[i].x, buf->xy[i].y, buf->xy[i].grey);        
    new_buf->count = buf->count;
    free_coord_set(buf);
    return new_buf;
}
