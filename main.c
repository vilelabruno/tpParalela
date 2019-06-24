#include "count.h"
#include "lk.h"
#include "mpi.h"
#include <dirent.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MASTER 0
#define DEBUG 0
#define VERBOSE 0

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int ws;


    MPI_Comm_size(MPI_COMM_WORLD, &ws);
    int wr;
    MPI_Comm_rank(MPI_COMM_WORLD, &wr);

    if (wr == MASTER) {
        char *pathDir = argv[1];
        DIR *dir = opendir(pathDir);
        struct dirent *sd;

        if (dir == NULL) {
            printf("dir not found.\n");
            abort();
        }

        printf("1... \n");

        int totalStars = 0;

        int imStars = 0;
        int block_y, block_x;
        block_y = atoi(argv[2]);
        block_x = atoi(argv[3]);
        int stream_max_size = block_y * block_x;
        MPI_Bcast(&stream_max_size, 1, MPI_INT, MASTER, MPI_COMM_WORLD);

        // MPI Persistent mode configuration
        MPI_Request req_send_stars_block[(ws - 1)];
        MPI_Status stts_send_stars_block[(ws - 1)];
        MPI_Request req_recv_count[(ws - 1)];
        MPI_Status stts_recv_count[(ws - 1)];

        MPI_Request req_send_block_size[(ws - 1)];
        MPI_Status stts_send_block_size[(ws - 1)];

        png_byte **block = initiate_blocks((ws - 1), block_y, block_x);
        int **block_size = (int **) malloc(sizeof(int *) * (ws - 1));
        for (int p = 0; p < ws - 1; p++)
            block_size[p] = (int *) malloc(sizeof(int) * 2);

        int *stars_counted = (int *) calloc((ws - 1), sizeof(int));

        for (int p = 1; p < ws; p++) {
            MPI_Send_init(block_size[(p - 1)], 2, MPI_INT, p, 0, MPI_COMM_WORLD, &req_send_block_size[(p - 1)]);
            MPI_Send_init(block[(p - 1)], (block_x * block_y * 3), MPI_UNSIGNED_CHAR, p, 0, MPI_COMM_WORLD,
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
            block_lking(&l, height, width, block_y, block_x);

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
        int stream_max_size;
        MPI_Bcast(&stream_max_size, 1, MPI_INT, MASTER, MPI_COMM_WORLD);

        // MPI Persistent mode configuration
        MPI_Request req_recv_block_size;
        MPI_Status stts_recv_block_size;

        MPI_Request req_recv_block;
        MPI_Status stts_recv_block;

        MPI_Request req_send_count;
        MPI_Status stts_send_count;

        png_byte *stars_received = create_block(stream_max_size);
        int *block_size = (int *) malloc(sizeof(int) * 2);
        int stars_send = 0;

        MPI_Recv_init(block_size, 2, MPI_INT, MASTER, 0, MPI_COMM_WORLD, &req_recv_block_size);
        MPI_Recv_init(stars_received, (stream_max_size * 4), MPI_UNSIGNED_CHAR, MASTER, 0, MPI_COMM_WORLD,
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
