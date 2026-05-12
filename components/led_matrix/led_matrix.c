#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "led_matrix.h"

#define MATRIX_WIDTH    16     /**< Number of LED columns in the matrix [0-15 valid x coords] */
#define MATRIX_HEIGHT   16    /**< Number of LED rows in the matrix [0-15 valid y coords] */
#define NUM_LEDS        MATRIX_WIDTH * MATRIX_HEIGHT
#define MATRIX_PIN      5      

static led_strip_handle_t s_led_strip = NULL;

static int xy_to_index(int x, int y)
{
    // Bounds check
    if (x < 0 || x >= MATRIX_WIDTH ||
        y < 0 || y >= MATRIX_HEIGHT)
    {
        return -1;
    }

    if ((y % 2) == 0)
    {
        // Even row
        return y * MATRIX_WIDTH + x;
    }
    else
    {
        // Odd row (reverse direction)
        return y * MATRIX_WIDTH + (MATRIX_WIDTH - 1 - x);
    }
}


void matrix_init(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = MATRIX_PIN,
        .max_leds = NUM_LEDS
    };

    led_strip_spi_config_t spi_config = {
        .spi_bus = SPI2_HOST,
        .flags.with_dma = true
    };

    ESP_ERROR_CHECK(
    led_strip_new_spi_device(&strip_config, &spi_config, &s_led_strip)
    );

    led_strip_clear(s_led_strip);
    led_strip_refresh(s_led_strip);
}

void matrix_set_pixel(int x, int y, int r, int g, int b)
{
    int index = xy_to_index(x, y);

    if (index < 0)
    {
        return;
    }

    ESP_ERROR_CHECK(
        led_strip_set_pixel(
            s_led_strip,
            index,
            r,
            g,
            b
        )
    );
}

/*
 * Clear all pixels.
 */
void matrix_clear(void)
{
    ESP_ERROR_CHECK(
        led_strip_clear(s_led_strip)
    );
}

/*
 * Push pixel buffer to the LEDs.
 */
void matrix_show(void)
{
    ESP_ERROR_CHECK(
        led_strip_refresh(s_led_strip)
    );
}