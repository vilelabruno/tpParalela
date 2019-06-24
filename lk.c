
#include "lk.h"

/**
 *
 * @param l
 * @param y_init
 * @param y_end
 * @param x_init
 * @param x_end
 */
void inlink(struct block_list_ *l, int y_init, int y_end, int x_init, int x_end) {
    block_link *new = (block_link *) malloc(sizeof(block_link));

    new->y_init = y_init;
    new->y_end = y_end;
    new->x_init = x_init;
    new->x_end = x_end;
    if (l->size == 0) {
        new->next = NULL;
        l->first = new;
    } else {
        new->next = l->first;
        l->first = new;
    }
    l->size++;
}

/**
 *
 * @param l
 * @return
 */
struct block_link_ * outlink(struct block_list_ *l) {
    if (l->size == 0) {
        abort();
    } else {
        struct block_link_ *ret = l->first;
        l->first = ret->next;
        l->size--;
        return ret;
    }
}
