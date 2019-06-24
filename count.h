#ifndef FINAL_PARALLEL_PROCESSING_count_H
#define FINAL_PARALLEL_PROCESSING_count_H

#include "q.h"
#include "lk.h"
#include <png.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

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

/**
 *
 * @param file_path
 * @param width
 * @param height
 * @param color_type
 * @param bit_depth
 * @param row_pointers
 */
void read_png_file(char *file_path, int *width, int *height, png_byte *color_type, png_byte *bit_depth,
                   png_byte ***row_pointers);

void binarizer(png_byte *row_pointers, figure **binary_comps);

void rec_601_gray(png_byte *row_pointers, figure binary_comps);

void divide_range(figure *binary_figure, double factor);

void divide_block(png_byte **src, png_byte *block, int y_init, int y_end, int x_init, int x_end);

int scc_count(figure **src);

void malloc_components(figure **src);

figure *initiate_figure(int dim_y, int dim_x);

png_byte *create_block(int stream_max_size);

png_byte **initiate_blocks(int size, int block_y, int block_x);

void block_lking(block_list **l, int dim_y, int dim_x, int block_y, int block_x);

void destroy_blocks(void **blocks, int size);

void destroy_figure(figure *src);

void destroy_components(pixel **src, int dim_y);

#endif //FINAL_PARALLEL_PROCESSING_count_H
