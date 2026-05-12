#include "visualiser.h"
#include "led_matrix.h"

#define MATRIX_WIDTH  16
#define MATRIX_HEIGHT 16

void visualizer_update(int level)
{
    matrix_clear();

    int height = (level * MATRIX_HEIGHT) / 200;
    if (height > MATRIX_HEIGHT)
        height = MATRIX_HEIGHT;

    for (int x = 0; x < MATRIX_WIDTH; x++)
    {
        for (int y = 0; y < height; y++)
        {
            matrix_set_pixel(x,
                             MATRIX_HEIGHT - 1 - y,
                             80, 0, 100);
        }
    }

    matrix_show();
}