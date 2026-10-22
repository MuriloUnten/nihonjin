#include <stdio.h>
#include <assert.h>
#include "cores.h"
#include "imagem.h"
#include "pdi.h"

#define TESTS_COUNT 5

typedef struct TestParams {
    char filename[64];
    int  count;
} TestParams;

const TestParams tests[TESTS_COUNT] = {
    { .filename = "60.bmp",  .count = 60  },
    { .filename = "82.bmp",  .count = 82  },
    { .filename = "114.bmp", .count = 114 },
    { .filename = "150.bmp", .count = 150 },
    { .filename = "205.bmp", .count = 205 },
};

void default_dilate(Imagem *in, Imagem *out);
void default_erode(Imagem *in, Imagem *out);

int count(Imagem *img, const char *identifier) {
    assert(img != NULL);

    Imagem *grayscale = criaImagem(img->largura, img->altura, 1);
    RGBParaCinza(img, grayscale);
    
    float threshold = thresholdOtsu(grayscale) / 3.0f;
    Imagem *binary = criaImagem(img->largura, img->altura, 1);
    binarizaAdapt(grayscale, binary, 51, threshold, NULL);
    // binariza(grayscale, binary, threshold);

    Imagem *eroded = criaImagem(img->largura, img->altura, 1);
    default_erode(binary, eroded);
    default_dilate(eroded, binary);

    char output_path[64];
    sprintf(output_path, "./output/%s_binarized.bmp", identifier);
    salvaImagem(binary, output_path);
    return 0;
}

int main() {
    for (int i = 0; i < TESTS_COUNT; ++i) {
        TestParams test = tests[i];
        Imagem *img = abreImagem(test.filename, 3);
        char identifier[5];
        sprintf(identifier, "%d", test.count);

        int rice_count = count(img, identifier);
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
