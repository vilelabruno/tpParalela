
#ifndef PARALLEL_COUNTING_STARS_q_H
#define PARALLEL_COUNTING_STARS_q_H

#include "count.h"

typedef struct q_comp_ {
    struct pix_ *c;
    struct q_comp_ *last;
} q_comp;

typedef struct fifo_queue_ {
    int size;
    q_comp *head;
} fifo_queue;

void enqueue(fifo_queue **q, struct pix_ *c);

struct pix_ *
dequeue(fifo_queue **q);

#endif //PARALLEL_COUNTING_STARS_q_H
