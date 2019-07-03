#include "mpi.h"
#include <dirent.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct complement_ {
    struct pix_ *c;
    struct complement_ *last;
} complement;

typedef struct fila_ {
    int size;
    complement *head;
} fila;

typedef struct subimage_link_ {
    int y_init, y_end;
    int x_init, x_end;
    struct subimage_link_ *next;
} subimage_link;

typedef struct subimage_list_ {
    int size;
    struct subimage_link_ *first;
} subimage_list;

typedef struct pix_ {
    int y;
    int x;
    int value;
    int label;
} pixel;

typedef struct image_ {
    int dim_y;
    int dim_x;
    pixel **pixels;
} image;

void addQ(fila **q, pixel *c) {
    complement *new_comp = (complement *) malloc(sizeof(complement));
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

pixel *remQ(fila **q) {
    if ((*q)->size == 0) {
        return NULL;
    } else {
        complement *tmp = (*q)->head;
        (*q)->head = tmp->last;
        struct pix_ *comp = tmp->c;
        free(tmp);
        (*q)->size--;
        return comp;
    }
}
void inlink(struct subimage_list_ *l, int y_init, int y_end, int x_init, int x_end) {
    subimage_link *new = (subimage_link *) malloc(sizeof(subimage_link));

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

struct subimage_link_ * outlink(struct subimage_list_ *l) {
    if (l->size == 0) {
        exit(1);
    } else {
        struct subimage_link_ *ret = l->first;
        l->first = ret->next;
        l->size--;
        return ret;
    }
}
void loadImg(char *file_path, int *width, int *height, png_byte *color_type, png_byte *bit_depth,
              png_byte ***row_pointers) {
    FILE *fp = fopen(file_path, "rb");

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png)
        exit(1);

    png_infop info = png_create_info_struct(png);
    if (!info)
        exit(1);

    if (setjmp(png_jmpbuf(png)))
        exit(1);

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

void mallocPx(image **src) {
    (*src)->pixels = (pixel **) malloc(sizeof(pixel *) * (*src)->dim_y);
    for (int y = 0; y < (*src)->dim_y; y++)
        (*src)->pixels[y] = (pixel *) malloc(sizeof(pixel) * (*src)->dim_x);
}

void bin(png_byte *row_pointers, image **binary_comps) {
    mallocPx(&(*binary_comps));
    transGray(row_pointers, *(*binary_comps));
    divide_range(*binary_comps, 0.4);
}

void transGray(png_byte *row_pointers, image binary_comps) {
    for (int y = 0; y < binary_comps.dim_y; y++) {
        for (int x = 0; x < binary_comps.dim_x; x++) {
            png_byte *px = &(row_pointers[(y * binary_comps.dim_x * 3 + x * 3)]);
            binary_comps.pixels[y][x].value = (0.3 * px[0]) + (0.59 * px[1]) + (0.1 * px[2]); 
            binary_comps.pixels[y][x].label = -1;
            binary_comps.pixels[y][x].y = y;
            binary_comps.pixels[y][x].x = x;
        }
    }
}

fila *create_queue() {
    fila *q = (fila *) malloc(sizeof(fila));
    q->size = 0;
    return q;
}

void divide_range(image *binary_image, dousbe factor) {
    for (int y = 0; y < (*binary_image).dim_y; y++) {
        for (int x = 0; x < (*binary_image).dim_x; x++) {
            if ((*binary_image).pixels[y][x].value > 255 * factor)
                (*binary_image).pixels[y][x].value = 1;
            else
                (*binary_image).pixels[y][x].value = 0;
        }
    }
}

int count(image **src) {
    fila *q = create_queue();
    int label = 0;

    for (int y = 0; y < (*src)->dim_y; y++) {
        for (int x = 0; x < (*src)->dim_x; x++) {
            if (((*src)->pixels[y][x].value == 1) && ((*src)->pixels[y][x].label == -1)) {
                pixel *p = &(*src)->pixels[y][x];
                addQ(&q, p);

                while (q->size != 0) {
                    pixel *c = remQ(&q);
                    c->label = label + 1;

                    pixel *l, *r, *u, *d, *ru, *rd, *lu, *ld;
                    if ((c->y - 1) >= 0) {
                        u = &(*src)->pixels[c->y - 1][c->x];
                        if (u->value == 1 && u->label == -1) {
                            u->label = label;
                            addQ(&q, u);
                        }

                        if ((c->x + 1) < (*src)->dim_x) {
                            ru = &(*src)->pixels[c->y - 1][c->x + 1];
                            if (ru->value == 1 && ru->label == -1) {
                                ru->label = label;
                                addQ(&q, ru);
                            }
                        }

                        if ((c->x - 1) < (*src)->dim_x) {
                            lu = &(*src)->pixels[c->y - 1][c->x - 1];
                            if (lu->value == 1 && lu->label == -1) {
                                lu->label = label;
                                addQ(&q, lu);
                            }
                        }
                    }

                    if ((c->x - 1) >= 0) {
                        l = &(*src)->pixels[c->y][c->x - 1];
                        if (l->value == 1 && l->label == -1) {
                            l->label = label;
                            addQ(&q, l);
                        }
                    }

                    if ((c->x + 1) < (*src)->dim_x) {
                        r = &(*src)->pixels[c->y][c->x + 1];
                        if (r->value == 1 && r->label == -1) {
                            r->label = label;
                            addQ(&q, r);
                        }
                    }

                    if ((c->y + 1) < (*src)->dim_y) {
                        d = &(*src)->pixels[c->y + 1][c->x];
                        if (d->value == 1 && d->label == -1) {
                            d->label = label;
                            addQ(&q, d);
                        }

                        if ((c->x - 1) < (*src)->dim_x) {
                            ld = &(*src)->pixels[c->y + 1][c->x - 1];
                            if (ld->value == 1 && ld->label == -1) {
                                ld->label = label;
                                addQ(&q, ld);
                            }
                        }

                        if ((c->x + 1) < (*src)->dim_x) {
                            rd = &(*src)->pixels[c->y + 1][c->x + 1];
                            if (rd->value == 1 && rd->label == -1) {
                                rd->label = label;
                                addQ(&q, rd);
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

void free_fig(image *src) {
    free_px(src->pixels, src->dim_y);
    free(src);
}

void free_px(pixel **src, int dim_y) {
    for (int y = 0; y < dim_y; y++)
        free(src[y]);

    free(src);
}


png_byte *create_subimage(int max_size) {
    png_byte *subimage = (png_byte *) malloc(sizeof(png_byte) * (max_size * 3));

    return subimage;
}

void divide_subimage(png_byte **src, png_byte *subimage, int y_init, int y_end, int x_init, int x_end) {
    int dim_y = (y_end - y_init);
    int dim_x = (x_end - x_init);
    for (int y = 0; y < dim_y; y++) {
        for (int x = 0; x < dim_x; x++) {
            png_byte *ptr = &(src[y_init + y][(x_init + x * 4)]);
            subimage[(y * dim_x * 3 + x * 3)] = ptr[0];
            subimage[(y * dim_x * 3 + x * 3 + 1)] = ptr[1];
            subimage[(y * dim_x * 3 + x * 3 + 2)] = ptr[2];
        }
    }
}

image *initiate_image(int dim_y, int dim_x) {
    image *fig = (image *) malloc(sizeof(image));

    fig->dim_y = dim_y;
    fig->dim_x = dim_x;

    return fig;
}

void free_subimg(void **subimages, int size) {
    for (int y = 0; y < size; y++)
        free(subimages[y]);

    free(subimages);
}

void subimage_lking(subimage_list **l, int dim_y, int dim_x, int qtdPixY, int qtdPixX) {
    for (int y = 0; y < dim_y; y += qtdPixY) {
        for (int x = 0; x < dim_x; x += qtdPixX) {
            int y_init = y;
            int y_end = y + qtdPixY;
            if (y_end > dim_y)
                y_end = dim_y;
            int x_init = x;
            int x_end = x + qtdPixX;
            if (x_end > dim_x)
                x_end = dim_x;
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

    if (wr == 0) { //if is this the main computer
        char *pathDir = "../imagens/";
        DIR *dir = opendir(pathDir);
        struct dirent *sd;

        if (dir == NULL) {
            printf("dir not found.\n");
            exit(1);
        }

        int imStars = 0;

        int qtdPixY = 1000, qtdPixX = 1000;

        int totalStars = 0;
        int max_size = qtdPixY * qtdPixX;
        MPI_Bcast(&max_size, 1, MPI_INT, 0, MPI_COMM_WORLD);

        MPI_Request req_send_stars_subimage[(ws - 1)];
        MPI_Status stts_send_stars_subimage[(ws - 1)];
        MPI_Request req_recv_count[(ws - 1)];
        MPI_Status stts_recv_count[(ws - 1)];
        MPI_Request req_send_subimage_size[(ws - 1)];
        MPI_Status stts_send_subimage_size[(ws - 1)];

        png_byte **subimage = (png_byte **) malloc(sizeof(png_byte *) * (ws - 1));
        for (int y = 0; y < (ws - 1); y++)
            subimage[y] = (png_byte *) malloc(sizeof(png_byte) * (qtdPixY * qtdPixX * 3));

        int **subimage_size = (int **) malloc(sizeof(int *) * (ws - 1));
        for (int p = 0; p < ws - 1; p++)
            subimage_size[p] = (int *) malloc(sizeof(int) * 2);

        int *stars_counted = (int *) calloc((ws - 1), sizeof(int));

        for (int p = 1; p < ws; p++) {
            MPI_Send_init(subimage_size[(p - 1)], 2, MPI_INT, p, 0, MPI_COMM_WORLD, &req_send_subimage_size[(p - 1)]);
            MPI_Send_init(subimage[(p - 1)], (qtdPixX * qtdPixY * 3), MPI_UNSIGNED_CHAR, p, 0, MPI_COMM_WORLD,
                          &req_send_stars_subimage[(p - 1)]);
            MPI_Recv_init(&stars_counted[(p - 1)], 1, MPI_INT, p, 0, MPI_COMM_WORLD, &req_recv_count[(p - 1)]);
            stts_send_stars_subimage[(p - 1)]._cancelled = 0;
        }

        printf("Done.\n");
        while ((sd = readdir(dir))) {
            if (!(strcmp(sd->d_name, ".") && strcmp(sd->d_name, "..")))
                continue;

            int width, height;
            char *file_path = (char *) malloc(sizeof(char) * strlen(pathDir) + strlen(sd->d_name));
            strcpy(file_path, pathDir);
            strcat(file_path, sd->d_name);

            png_byte color_type, bit_depth;
            png_byte **row_pointers;
            loadImg(file_path, &width, &height, &color_type, &bit_depth, &row_pointers);

            subimage_list *l = (subimage_list *) malloc(sizeof(subimage_list));
            l->size = 0;
            subimage_lking(&l, height, width, qtdPixY, qtdPixX);

            printf("lets go...");
            do {
                int p;
                for (p = 0; (p < (ws - 1)) && (l->size > 0); p++) {
                    subimage_link *sb = outlink(l);
                    subimage_size[p][0] = (sb->y_end - sb->y_init);
                    subimage_size[p][1] = (sb->x_end - sb->x_init);
                    divide_subimage(row_pointers, subimage[p], sb->y_init, sb->y_end, sb->x_init, sb->x_end);

                    
                    free(sb);
                }
                if (p == (ws - 1)) {
                    MPI_Startall((ws - 1), req_send_subimage_size);
                    MPI_Startall((ws - 1), req_send_stars_subimage);
                    MPI_Startall((ws - 1), req_recv_count);

                    MPI_Waitall((ws - 1), req_send_subimage_size, stts_send_subimage_size);
                    MPI_Waitall((ws - 1), req_send_stars_subimage, stts_send_stars_subimage);
                    MPI_Waitall((ws - 1), req_recv_count, stts_recv_count);
                } else {
                    for (int pc = 0; pc < p; pc++) {
                        MPI_Start(&req_send_subimage_size[pc]);
                        MPI_Start(&req_send_stars_subimage[pc]);
                        MPI_Start(&req_recv_count[pc]);

                        MPI_Wait(&req_send_subimage_size[pc], &stts_send_subimage_size[pc]);
                        MPI_Wait(&req_send_stars_subimage[pc], &stts_send_stars_subimage[pc]);
                        MPI_Wait(&req_recv_count[pc], &stts_recv_count[pc]);
                    }
                }
                for (int pc = 0; pc < p; pc++)
                    imStars += stars_counted[pc];
            } while (l->size > 0);

            totalStars += imStars;

            imStars = 0;
            free_subimg((void **) row_pointers, height);
            free(file_path);
            free(l);
        }

        for (int p = 0; p < (ws - 1); p++) {
            MPI_Request_free(&req_send_subimage_size[p]);
            MPI_Wait(&req_send_subimage_size[p], &stts_send_subimage_size[p]);

            MPI_Request_free(&req_send_stars_subimage[p]);
            MPI_Wait(&req_send_stars_subimage[p], &stts_send_stars_subimage[p]);

            MPI_Request_free(&req_recv_count[p]);
            MPI_Wait(&req_recv_count[p], &stts_recv_count[p]);
        }

        printf("estrelas = %d\n", totalStars);
        closedir(dir);

        MPI_Abort(MPI_COMM_WORLD, 0);
    } else {
        int max_size;
        MPI_Bcast(&max_size, 1, MPI_INT, 0, MPI_COMM_WORLD);

        MPI_Request req_recv_subimage_size;
        MPI_Status stts_recv_subimage_size;

        MPI_Request req_recv_subimage;
        MPI_Status stts_recv_subimage;

        MPI_Request req_send_count;
        MPI_Status stts_send_count;

        png_byte *stars_received = create_subimage(max_size);
        int *subimage_size = (int *) malloc(sizeof(int) * 2);
        int stars_send = 0;

        MPI_Recv_init(subimage_size, 2, MPI_INT, 0, 0, MPI_COMM_WORLD, &req_recv_subimage_size);
        MPI_Recv_init(stars_received, (max_size * 4), MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD,
                      &req_recv_subimage);
        MPI_Send_init(&stars_send, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &req_send_count);

        printf("2 lets go...");
        while (1) {
            MPI_Start(&req_recv_subimage_size);
            MPI_Wait(&req_recv_subimage_size, &stts_recv_subimage_size);

            image *binary_fig = (image *) malloc(sizeof(image));
        
            binary_fig->dim_y = subimage_size[0];
            binary_fig->dim_x = subimage_size[1];
        

            MPI_Start(&req_recv_subimage);
            MPI_Wait(&req_recv_subimage, &stts_recv_subimage);

            bin(stars_received, &binary_fig);
            stars_send = count(&binary_fig);

            MPI_Start(&req_send_count);
            MPI_Wait(&req_send_count, &stts_send_count);

            free_fig(binary_fig);
        }
    }

    MPI_Finalize();
    return 0;
}
