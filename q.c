#include "q.h"

/**
 *
 * @param q
 * @param c
 */
void enqueue(fifo_queue **q, pixel *c) {
    q_comp *new_comp = (q_comp *) malloc(sizeof(q_comp));
    new_comp->c = c;
    if ((*q)->size == 0) {
        new_comp->last = NULL;
        (*q)->head = new_comp;
    } else {
        new_comp->last = (*q)->head;
        (*q)->head = new_comp;
    }
    (*q)->size++;
}

/**
 *
 * @param q
 * @return
 */
pixel *dequeue(fifo_queue **q) {
    if ((*q)->size == 0) {
        return NULL;
    } else {
        q_comp *tmp = (*q)->head;
        (*q)->head = tmp->last;
        struct pix_ *comp = tmp->c;
        free(tmp);
        (*q)->size--;
        return comp;
    }
}