#include "count.h"

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

    // Read any color_type into 8bit depth, RGBA format.
    // See http://www.libpng.org/pub/png/libpng-manual.txt

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

/**
 *
 * @param src
 */
void destroy_figure(figure *src) {
    destroy_components(src->pixels, src->dim_y);
    free(src);
}

/**
 *
 * @param src
 * @param dim_y
 */
void destroy_components(pixel **src, int dim_y) {
    for (int y = 0; y < dim_y; y++)
        free(src[y]);

    free(src);
}

/**
 *
 * @param size
 * @param block_y
 * @param block_x
 * @return
 */
png_byte **initiate_blocks(int size, int block_y, int block_x) {
    png_byte **blocks = (png_byte **) malloc(sizeof(png_byte *) * size);
    for (int y = 0; y < size; y++)
        blocks[y] = (png_byte *) malloc(sizeof(png_byte) * (block_y * block_x * 3));

    return blocks;
}

/**
 *
 * @param stream_max_size
 * @return
 */
png_byte *create_block(int stream_max_size) {
    // allocate memory only for values RGB of pixels, ignoring A values
    png_byte *block = (png_byte *) malloc(sizeof(png_byte) * (stream_max_size * 3));

    return block;
}

/**
 *
 * @param src
 * @param block
 * @param y_init
 * @param y_end
 * @param x_init
 * @param x_end
 */
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

/**
 *
 * @param dim_y
 * @param dim_x
 * @return
 */
figure *initiate_figure(int dim_y, int dim_x) {
    figure *fig = (figure *) malloc(sizeof(figure));

    fig->dim_y = dim_y;
    fig->dim_x = dim_x;

    return fig;
}

/**
 *
 * @param blocks
 * @param size
 */
void destroy_blocks(void **blocks, int size) {
    for (int y = 0; y < size; y++)
        free(blocks[y]);

    free(blocks);
}

/**
 *
 * @param l
 * @param dim_y
 * @param dim_x
 * @param block_y
 * @param block_x
 */
void block_lking(block_list **l, int dim_y, int dim_x, int block_y, int block_x) {
    for (int y = 0; y < dim_y; y += block_y) {
        for (int x = 0; x < dim_x; x += block_x) {
            // generate y boundaries
            int y_init = y;
            int y_end = y + block_y;
            if (y_end > dim_y)
                y_end = dim_y;

            // generate x boundaries
            int x_init = x;
            int x_end = x + block_x;
            if (x_end > dim_x)
                x_end = dim_x;

            // insert block into queue
            inlink((*l), y_init, y_end, x_init, x_end);
        }
    }
}
