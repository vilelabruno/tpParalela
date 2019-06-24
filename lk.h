
#ifndef PARALLEL_COUNTING_STARS_lk_H
#define PARALLEL_COUNTING_STARS_lk_H

#include <stdlib.h>

typedef struct block_link_ {
    int y_init, y_end;
    int x_init, x_end;
    struct block_link_ *next;
} block_link;

typedef struct block_list_ {
    int size;
    struct block_link_ *first;
} block_list;

void
inlink(struct block_list_ *l, int y_init, int y_end, int x_init, int x_end);

struct block_link_ *
outlink(struct block_list_ *l);

#endif //PARALLEL_COUNTING_STARS_lk_H
