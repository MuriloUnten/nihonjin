#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include "cores.h"
#include "pdi.h"

#define TESTS_COUNT 5

#define RICE_MIN_PIXEL_COUNT 20 // reason: just used to ignore random specs when finding the median rice
#define RICE_MIN_LENGTH 7       // reason: less than the smallest rice i could find
#define RICE_MAX_LENGTH 40      // reason: a lot bigger than the biggest rice. used to ignore connected blobs when finding the median rice

typedef struct TestParams {
    char filename[64];
    int  count;
} TestParams;

/* *************** test params *************** */
const bool save_images = true;
const TestParams tests[TESTS_COUNT] = {
    { .filename = "60.bmp",  .count = 60  },
    { .filename = "82.bmp",  .count = 82  },
    { .filename = "114.bmp", .count = 114 },
    { .filename = "150.bmp", .count = 150 },
    { .filename = "205.bmp", .count = 205 },
};
/* ******************************************* */

const Cor color_red = {
    .canais = { 247, 38, 38 },
};

void default_dilate(Imagem *in, Imagem *out);
void default_erode(Imagem *in, Imagem *out);
int compare_int(const void *a, const void *b);
double median_int(int arr[], int n);

// // had this idea, but without implementing proper watershed, it didn't really work.
// float distance_find(Imagem *img, int y, int x) {
//     for (int step = 1; step < 100; ++step) {
//         int start_y = MAX(y - step, 0);
//         int end_y   = MIN(y + step, img->altura - 1);
//         for (int y_prime = start_y; y_prime <= end_y; ++y_prime) {
//             int start_x = MAX(x - step, 0);
//             int end_x = MIN(x + step, img->largura - 1);
//             for (int x_prime = start_x; x_prime <= end_x; ++x_prime) {
//                 if (img->dados[0][y_prime][x_prime] == 0.0f) {
//                     int y_distance = y - y_prime;
//                     int x_distance = x - x_prime;
//                     float distance = sqrt((y_distance * y_distance) + (x_distance * x_distance));
//                     return distance;
//                 }
//             }
//         }
//     }
//     return 100.0f;
// }
//
// void distance_field(Imagem *img, Imagem *out) {
//     assert(img != NULL);
//     assert(out != NULL);
//     assert(img->n_canais == 1);
//     assert(out->n_canais == 1);
//
//     float max_found = 1.0f;
//     for (int y = 0; y < img->altura; ++y) {
//         for (int x = 0; x < img->largura; ++x) {
//             if (img->dados[0][y][x] == 0.0f) {
//                 out->dados[0][y][x] = 0.0f;
//                 continue;
//             }
//
//             float distance = distance_find(img, y, x);
//             out->dados[0][y][x] = distance;
//             max_found = fmax(max_found, distance);
//         }
//     }
//
//     float normalizing_factor = 1 / max_found;
//     for (int y = 0; y < img->altura; ++y) {
//         for (int x = 0; x < img->largura; ++x) {
//             out->dados[0][y][x] *= normalizing_factor;
//         }
//     }
// }

int count(Imagem *img, const char *identifier, bool save_images) {
    assert(img != NULL);

    Imagem *grayscale = criaImagem(img->largura, img->altura, 1);
    RGBParaCinza(img, grayscale);
    
    // float threshold = thresholdOtsu(grayscale) / 3.0f;
    Imagem *binary = criaImagem(img->largura, img->altura, 1);
    binarizaAdapt(grayscale, binary, 75, 0.1f, NULL); // 75 because it's a lot bigger than the biggest rice
    // binariza(grayscale, binary, threshold);

    Imagem *eroded = criaImagem(img->largura, img->altura, 1);
    default_erode(binary, eroded);
    default_dilate(eroded, binary);

    ComponenteConexo *clusters = NULL;
    int cluster_count = rotulaUnionFind(binary, &clusters, RICE_MIN_LENGTH, RICE_MIN_LENGTH, RICE_MIN_PIXEL_COUNT);
    int *n_pixels = (int*)malloc(cluster_count * sizeof(int));
    int *widths   = (int*)malloc(cluster_count * sizeof(int));
    int *heights  = (int*)malloc(cluster_count * sizeof(int));
    int considered_clusters = 0;
    for (int i = 0; i < cluster_count; ++i) {
        ComponenteConexo cluster = clusters[i];
        if (cluster.roi.b - cluster.roi.c > RICE_MAX_LENGTH) continue;
        if (cluster.roi.d - cluster.roi.e > RICE_MAX_LENGTH) continue;

        n_pixels[considered_clusters] = cluster.n_pixels;
        considered_clusters++;
    }

    int median_pixel_count = median_int(n_pixels, considered_clusters);
    int median_width       = median_int(widths, considered_clusters);
    int median_height      = median_int(heights, considered_clusters);

    int count = 0;
    Imagem *bounds = clonaImagem(img);
    for (int i = 0; i < cluster_count; ++i) {
        ComponenteConexo cluster = clusters[i];
        desenhaRetangulo(cluster.roi, color_red, bounds);

        int pixel_count_variance_threshold = (int)(median_pixel_count * 1.3f);
        int width_variance_threshold       = (int)(median_width * 1.3f);
        int height_variance_threshold      = (int)(median_height * 1.3f);

        if (cluster.n_pixels > pixel_count_variance_threshold) {
            float estimate_in_cluster = cluster.n_pixels / (median_pixel_count * 0.85f); // 0.85 as a correction factor for overlapping rice pixels
            count += estimate_in_cluster;
        } else {
            count++;
            continue;
        }
    }

    if (save_images) {
        char segmented_path[64];
        snprintf(segmented_path, 64, "./output/%s_bounds.bmp", identifier);
        salvaImagem(bounds, segmented_path);

        char output_path[64];
        snprintf(output_path, 64, "./output/%s_binarized.bmp", identifier);
        salvaImagem(binary, output_path);
    }

    destroiImagem(binary);
    destroiImagem(eroded);
    destroiImagem(grayscale);
    destroiImagem(bounds);
    free(clusters);
    free(n_pixels);
    free(widths);
    free(heights);
    return count;
}

int main() {
    for (int i = 0; i < TESTS_COUNT; ++i) {
        TestParams test = tests[i];
        Imagem *img = abreImagem(test.filename, 3);
        char identifier[5];
        sprintf(identifier, "%d", test.count);

        int rice_count = count(img, identifier, save_images);
        printf("Image %s: expected %d, found %d\n", test.filename, test.count, rice_count);
        destroiImagem(img);
    }
}

void default_dilate(Imagem *in, Imagem *out) {
    assert(in != NULL);
    assert(out != NULL);

    Imagem *kernel = criaImagem(3, 3, 1);
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 3; ++x) {
            kernel->dados[0][y][x] = (float)((x + y) % 2);
        }
    }
    kernel->dados[0][1][1] = 1.0f;

    Coordenada center = { .x = 1, .y = 1 };
    dilata(in, kernel, center, out);
    destroiImagem(kernel);
}

void default_erode(Imagem *in, Imagem *out) {
    assert(in != NULL);
    Imagem *kernel = criaImagem(3, 3, 1);
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 3; ++x) {
            kernel->dados[0][y][x] = (float)((x + y) % 2);
        }
    }
    kernel->dados[0][1][1] = 1.0f;

    Coordenada center = { .x = 1, .y = 1 };
    erode(in, kernel, center, out);
    destroiImagem(kernel);
}

int compare_int(const void *a, const void *b) {
    return (*(int*)a - *(int*)b);
}

double median_int(int arr[], int n) {
    qsort(arr, n, sizeof(int), compare_int);
    if (n % 2 == 0)
        return (arr[n / 2 - 1] + arr[n / 2]) / 2.0;
    return arr[n / 2];
}
