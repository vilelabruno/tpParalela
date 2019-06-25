#include "mpi.h"
#include <dirent.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MASTER 0
#define DEBUG 0
#define VERBOSE 0

typedef struct q_comp_ {
    struct pix_ *c;
    struct q_comp_ *last;
} q_comp;

typedef struct fifo_queue_ {
    int size;
    q_comp *head;
} fifo_queue;

typedef struct block_link_ {
    int y_init, y_end;
    int x_init, x_end;
    struct block_link_ *next;
} block_link;

typedef struct block_list_ {
    int size;
    struct block_link_ *first;
} block_list;

typedef struct pix_ {
    int y;
    int x;
    int value;
    int label;
} pixel;

typedef struct figure_ {
    int dim_y;
    int dim_x;
    pixel **pixels;
} figure;

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
void read_png_file(char *file_path, int *width, int *height, png_byte *color_type, png_byte *bit_depth,
              png_byte ***row_pointers) {
    FILE *fp = fopen(file_path, "rb");

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png)
        abort();

    png_infop info = png_create_info_struct(png);
    if (!info)
        abort();

    if (setjmp(png_jmpbuf(png)))
        abort();

    png_init_io(png, fp);

    png_read_info(png, info);

    *width = png_get_image_width(png, info);
    *height = png_get_image_height(png, info);
    *color_type = png_get_color_type(png, info);
    *bit_depth = png_get_bit_depth(png, info);

    if (*bit_depth == 16)
        png_set_strip_16(png);

    if (*color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png);

    if (*color_type == PNG_COLOR_TYPE_GRAY && *bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png);

    if (png_get_valid(png, info, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png);

    if (*color_type == PNG_COLOR_TYPE_RGB || *color_type == PNG_COLOR_TYPE_GRAY ||
        *color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);

    if (*color_type == PNG_COLOR_TYPE_GRAY || *color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);

    png_read_update_info(png, info);

    *row_pointers = (png_byte **) malloc(sizeof(png_byte *) * *width);
    for (int y = 0; y < *height; y++)
        (*row_pointers)[y] = (png_byte *) malloc(png_get_rowbytes(png, info));

    png_read_image(png, *row_pointers);

    png_destroy_read_struct(&png, &info, NULL);

    fclose(fp);
}

void malloc_components(figure **src) {
    (*src)->pixels = (pixel **) malloc(sizeof(pixel *) * (*src)->dim_y);
    for (int y = 0; y < (*src)->dim_y; y++)
        (*src)->pixels[y] = (pixel *) malloc(sizeof(pixel) * (*src)->dim_x);
}

void binarizer(png_byte *row_pointers, figure **binary_comps) {
    // malloc components, transform the image to gray levels, and divide the range into binary to a factor
    malloc_components(&(*binary_comps));
    rec_601_gray(row_pointers, *(*binary_comps));
    divide_range(*binary_comps, 0.4);
}

void rec_601_gray(png_byte *row_pointers, figure binary_comps) {
#pragma omp parallel for
    for (int y = 0; y < binary_comps.dim_y; y++) {
        for (int x = 0; x < binary_comps.dim_x; x++) {
            png_byte *px = &(row_pointers[(y * binary_comps.dim_x * 3 + x * 3)]);
            binary_comps.pixels[y][x].value = (0.2989 * px[0]) + (0.5870 * px[1]) + (0.1140 * px[2]);
            binary_comps.pixels[y][x].label = -1;
            binary_comps.pixels[y][x].y = y;
            binary_comps.pixels[y][x].x = x;
        }
    }
}

fifo_queue *create_queue() {
    fifo_queue *q = (fifo_queue *) malloc(sizeof(fifo_queue));
    q->size = 0;
    return q;
}

void divide_range(figure *binary_figure, double factor) {
#pragma omg parallel for
    for (int y = 0; y < (*binary_figure).dim_y; y++) {
        for (int x = 0; x < (*binary_figure).dim_x; x++) {
            // binarize value to a factor chosen
            if ((*binary_figure).pixels[y][x].value > 255 * factor)
                (*binary_figure).pixels[y][x].value = 1;
            else
                (*binary_figure).pixels[y][x].value = 0;
        }
    }
}

int scc_count(figure **src) {
    fifo_queue *q = create_queue();
    int label = 0;

    for (int y = 0; y < (*src)->dim_y; y++) {
        for (int x = 0; x < (*src)->dim_x; x++) {
            if (((*src)->pixels[y][x].value == 1) && ((*src)->pixels[y][x].label == -1)) {
                pixel *p = &(*src)->pixels[y][x];
                enqueue(&q, p);

                while (q->size != 0) {
                    pixel *c = dequeue(&q);
                    c->label = label + 1;

                    pixel *l, *r, *u, *d, *ru, *rd, *lu, *ld;
                    if ((c->y - 1) >= 0) {
                        u = &(*src)->pixels[c->y - 1][c->x];
                        if (u->value == 1 && u->label == -1) {
                            u->label = label;
                            enqueue(&q, u);
                        }

                        if ((c->x + 1) < (*src)->dim_x) {
                            ru = &(*src)->pixels[c->y - 1][c->x + 1];
                            if (ru->value == 1 && ru->label == -1) {
                                ru->label = label;
                                enqueue(&q, ru);
                            }
                        }

                        if ((c->x - 1) < (*src)->dim_x) {
                            lu = &(*src)->pixels[c->y - 1][c->x - 1];
                            if (lu->value == 1 && lu->label == -1) {
                                lu->label = label;
                                enqueue(&q, lu);
                            }
                        }
                    }

                    if ((c->x - 1) >= 0) {
                        l = &(*src)->pixels[c->y][c->x - 1];
                        if (l->value == 1 && l->label == -1) {
                            l->label = label;
                            enqueue(&q, l);
                        }
                    }

                    if ((c->x + 1) < (*src)->dim_x) {
                        r = &(*src)->pixels[c->y][c->x + 1];
                        if (r->value == 1 && r->label == -1) {
                            r->label = label;
                            enqueue(&q, r);
                        }
                    }

                    if ((c->y + 1) < (*src)->dim_y) {
                        d = &(*src)->pixels[c->y + 1][c->x];
                        if (d->value == 1 && d->label == -1) {
                            d->label = label;
                            enqueue(&q, d);
                        }

                        if ((c->x - 1) < (*src)->dim_x) {
                            ld = &(*src)->pixels[c->y + 1][c->x - 1];
                            if (ld->value == 1 && ld->label == -1) {
                                ld->label = label;
                                enqueue(&q, ld);
                            }
                        }

                        if ((c->x + 1) < (*src)->dim_x) {
                            rd = &(*src)->pixels[c->y + 1][c->x + 1];
                            if (rd->value == 1 && rd->label == -1) {
                                rd->label = label;
                                enqueue(&q, rd);
                            }
                        }
                    }
                }
                label++;
            }
        }
    }
    free(q);
    return label;
}

void destroy_figure(figure *src) {
    destroy_components(src->pixels, src->dim_y);
    free(src);
}

void destroy_components(pixel **src, int dim_y) {
    for (int y = 0; y < dim_y; y++)
        free(src[y]);

    free(src);
}

png_byte **initiate_blocks(int size, int qtdPixY, int qtdPixX) {
    png_byte **blocks = (png_byte **) malloc(sizeof(png_byte *) * size);
    for (int y = 0; y < size; y++)
        blocks[y] = (png_byte *) malloc(sizeof(png_byte) * (qtdPixY * qtdPixX * 3));

    return blocks;
}

png_byte *create_block(int max_size) {
    // allocate memory only for values RGB of pixels, ignoring A values
    png_byte *block = (png_byte *) malloc(sizeof(png_byte) * (max_size * 3));

    return block;
}

void divide_block(png_byte **src, png_byte *block, int y_init, int y_end, int x_init, int x_end) {
    int dim_y = (y_end - y_init);
    int dim_x = (x_end - x_init);

#pragma omg parallel for
    for (int y = 0; y < dim_y; y++) {
        for (int x = 0; x < dim_x; x++) {
            // create block divisions ignoring A values, using only RGB then
            png_byte *ptr = &(src[y_init + y][(x_init + x * 4)]);
            block[(y * dim_x * 3 + x * 3)] = ptr[0];
            block[(y * dim_x * 3 + x * 3 + 1)] = ptr[1];
            block[(y * dim_x * 3 + x * 3 + 2)] = ptr[2];
        }
    }
}

figure *initiate_figure(int dim_y, int dim_x) {
    figure *fig = (figure *) malloc(sizeof(figure));

    fig->dim_y = dim_y;
    fig->dim_x = dim_x;

    return fig;
}

void destroy_blocks(void **blocks, int size) {
    for (int y = 0; y < size; y++)
        free(blocks[y]);

    free(blocks);
}

void block_lking(block_list **l, int dim_y, int dim_x, int qtdPixY, int qtdPixX) {
    for (int y = 0; y < dim_y; y += qtdPixY) {
        for (int x = 0; x < dim_x; x += qtdPixX) {
            // generate y boundaries
            int y_init = y;
            int y_end = y + qtdPixY;
            if (y_end > dim_y)
                y_end = dim_y;

            // generate x boundaries
            int x_init = x;
            int x_end = x + qtdPixX;
            if (x_end > dim_x)
                x_end = dim_x;

            // insert block into queue
            inlink((*l), y_init, y_end, x_init, x_end);
        }
    }
}


int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int ws;


    MPI_Comm_size(MPI_COMM_WORLD, &ws);
    int wr;
    MPI_Comm_rank(MPI_COMM_WORLD, &wr);

    if (wr == MASTER) { //if is this the main computer
        char *pathDir = argv[1];
        DIR *dir = opendir(pathDir);
        struct dirent *sd;

        if (dir == NULL) {
            printf("dir not found.\n");
            abort();
        }

        int imStars = 0;

        int qtdPixY = 1000, qtdPixX = 1000;

        int totalStars = 0;
        int max_size = qtdPixY * qtdPixX;
        MPI_Bcast(&max_size, 1, MPI_INT, MASTER, MPI_COMM_WORLD);

        // MPI Persistent mode configuration
        MPI_Request req_send_stars_block[(ws - 1)];
        MPI_Status stts_send_stars_block[(ws - 1)];
        MPI_Request req_recv_count[(ws - 1)];
        MPI_Status stts_recv_count[(ws - 1)];
        MPI_Request req_send_block_size[(ws - 1)];
        MPI_Status stts_send_block_size[(ws - 1)];

        png_byte **block = initiate_blocks((ws - 1), qtdPixY, qtdPixX);
        int **block_size = (int **) malloc(sizeof(int *) * (ws - 1));
        for (int p = 0; p < ws - 1; p++)
            block_size[p] = (int *) malloc(sizeof(int) * 2);

        int *stars_counted = (int *) calloc((ws - 1), sizeof(int));

        for (int p = 1; p < ws; p++) {
            MPI_Send_init(block_size[(p - 1)], 2, MPI_INT, p, 0, MPI_COMM_WORLD, &req_send_block_size[(p - 1)]);
            MPI_Send_init(block[(p - 1)], (qtdPixX * qtdPixY * 3), MPI_UNSIGNED_CHAR, p, 0, MPI_COMM_WORLD,
                          &req_send_stars_block[(p - 1)]);
            MPI_Recv_init(&stars_counted[(p - 1)], 1, MPI_INT, p, 0, MPI_COMM_WORLD, &req_recv_count[(p - 1)]);
            stts_send_stars_block[(p - 1)]._cancelled = 0;
        }

        printf("Done.\n");
        while ((sd = readdir(dir)) != NULL) {
            // skip directory self pointer and exit to parent
            if (!(strcmp(sd->d_name, ".") && strcmp(sd->d_name, "..")))
                continue;

            int width, height;
            char *file_path = (char *) malloc(sizeof(char) * strlen(pathDir) + strlen(sd->d_name));
            strcpy(file_path, pathDir);
            strcat(file_path, sd->d_name);

            png_byte color_type, bit_depth;
            png_byte **row_pointers;
            read_png_file(file_path, &width, &height, &color_type, &bit_depth, &row_pointers);

            block_list *l = (block_list *) malloc(sizeof(block_list));
            l->size = 0;
            block_lking(&l, height, width, qtdPixY, qtdPixX);

            /* Execution */
            do {
                int p;
                for (p = 0; (p < (ws - 1)) && (l->size > 0); p++) {
                    block_link *bl = outlink(l);
                    block_size[p][0] = (bl->y_end - bl->y_init);
                    block_size[p][1] = (bl->x_end - bl->x_init);
                    divide_block(row_pointers, block[p], bl->y_init, bl->y_end, bl->x_init, bl->x_end);
                    if (VERBOSE >= 3) {
                        printf("send y_init = %d\n", bl->y_init);
                        printf("send y_end = %d\n", bl->y_end);
                        printf("send x_init = %d\n", bl->x_init);
                        printf("send x_end = %d\n", bl->x_end);
                    }
                    if (DEBUG)
                        getchar();
                    free(bl);
                }
                if (p == (ws - 1)) {
                    MPI_Startall((ws - 1), req_send_block_size);
                    MPI_Startall((ws - 1), req_send_stars_block);
                    MPI_Startall((ws - 1), req_recv_count);

                    MPI_Waitall((ws - 1), req_send_block_size, stts_send_block_size);
                    MPI_Waitall((ws - 1), req_send_stars_block, stts_send_stars_block);
                    MPI_Waitall((ws - 1), req_recv_count, stts_recv_count);
                } else {
                    for (int pc = 0; pc < p; pc++) {
                        MPI_Start(&req_send_block_size[pc]);
                        MPI_Start(&req_send_stars_block[pc]);
                        MPI_Start(&req_recv_count[pc]);

                        MPI_Wait(&req_send_block_size[pc], &stts_send_block_size[pc]);
                        MPI_Wait(&req_send_stars_block[pc], &stts_send_stars_block[pc]);
                        MPI_Wait(&req_recv_count[pc], &stts_recv_count[pc]);
                    }
                }
                for (int pc = 0; pc < p; pc++)
                    imStars += stars_counted[pc];
            } while (l->size > 0);

            if (VERBOSE >= 1)
                printf("Image stars count = %d\n", imStars);
            totalStars += imStars;

            // program reset of needed variables
            imStars = 0;
            destroy_blocks((void **) row_pointers, height);
            free(file_path);
            free(l);
        }

        for (int p = 0; p < (ws - 1); p++) {
            MPI_Request_free(&req_send_block_size[p]);
            MPI_Wait(&req_send_block_size[p], &stts_send_block_size[p]);

            MPI_Request_free(&req_send_stars_block[p]);
            MPI_Wait(&req_send_stars_block[p], &stts_send_stars_block[p]);

            MPI_Request_free(&req_recv_count[p]);
            MPI_Wait(&req_recv_count[p], &stts_recv_count[p]);
        }

        printf("Result of stars found = %d\n", totalStars);
        closedir(dir);

        // abort execution
        MPI_Abort(MPI_COMM_WORLD, 0);
    } else {
        // needed block parameters receiving
        int max_size;
        MPI_Bcast(&max_size, 1, MPI_INT, MASTER, MPI_COMM_WORLD);

        // MPI Persistent mode configuration
        MPI_Request req_recv_block_size;
        MPI_Status stts_recv_block_size;

        MPI_Request req_recv_block;
        MPI_Status stts_recv_block;

        MPI_Request req_send_count;
        MPI_Status stts_send_count;

        png_byte *stars_received = create_block(max_size);
        int *block_size = (int *) malloc(sizeof(int) * 2);
        int stars_send = 0;

        MPI_Recv_init(block_size, 2, MPI_INT, MASTER, 0, MPI_COMM_WORLD, &req_recv_block_size);
        MPI_Recv_init(stars_received, (max_size * 4), MPI_UNSIGNED_CHAR, MASTER, 0, MPI_COMM_WORLD,
                      &req_recv_block);
        MPI_Send_init(&stars_send, 1, MPI_INT, MASTER, 0, MPI_COMM_WORLD, &req_send_count);

        /* Execution */
        while (1) {
            MPI_Start(&req_recv_block_size);
            MPI_Wait(&req_recv_block_size, &stts_recv_block_size);

            figure *binary_fig = initiate_figure(block_size[0], block_size[1]);

            MPI_Start(&req_recv_block);
            MPI_Wait(&req_recv_block, &stts_recv_block);

            binarizer(stars_received, &binary_fig);
            stars_send = scc_count(&binary_fig);

            MPI_Start(&req_send_count);
            MPI_Wait(&req_send_count, &stts_send_count);

            destroy_figure(binary_fig);
        }
    }

    // MPI finalizer
    MPI_Finalize();
    return 0;
}
