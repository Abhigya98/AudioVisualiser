#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "esp_err.h"
#include "esp_log.h"

#define MATRIX_WIDTH 16
#define MATRIX_HEIGHT 16
#define LED_PIN 5
#define NUM_LEDS 256

led_strip_handle_t led_strip;
static const char *TAG = "matrix";

typedef struct
{
    int x;
    int y;
} raindrop_t;
#define MAX_DROPS 20

raindrop_t drops[MAX_DROPS];

void led_init()
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_PIN,
        .max_leds = NUM_LEDS
    };

    led_strip_spi_config_t spi_config = {
        .spi_bus = SPI2_HOST,
        .flags.with_dma = true
    };

    ESP_ERROR_CHECK(
    led_strip_new_spi_device(&strip_config, &spi_config, &led_strip)
    );

    led_strip_clear(led_strip);
    led_strip_refresh(led_strip);
}

int xy_to_index(int x, int y)
{
    if (y % 2 == 0)
        return y * MATRIX_WIDTH + x;
    else
        return y * MATRIX_WIDTH + (MATRIX_WIDTH - 1 - x);
}


void matrix_set_pixel(int x, int y, int r, int g, int b)
{
    int index = xy_to_index(x, y);

    led_strip_set_pixel(
        led_strip,
        index,
        r,
        g,
        b
    );
}

void matrix_clear()
{
    led_strip_clear(led_strip);
}

void rain_init()
{
    for(int i = 0; i < MAX_DROPS; i++)
    {
        drops[i].x = rand() % MATRIX_WIDTH;
        drops[i].y = 0;//rand() % MATRIX_HEIGHT;
    }
}

void rain_update()
{
    for(int i = 0; i < MAX_DROPS; i++)
    {
        drops[i].y++;

        // If it goes off screen → respawn
        if(drops[i].y >= MATRIX_HEIGHT)
        {
            drops[i].y = 0;
            drops[i].x = rand() % MATRIX_WIDTH;
        }

        // Random respawn (adds natural variation)
        else if(rand() % 10 == 0)
        {
            drops[i].y = 0;
            drops[i].x = rand() % MATRIX_WIDTH;
        }
    }
}
void rain_draw()
{
    matrix_clear();

    for(int i = 0; i < MAX_DROPS; i++)
    {
        int x = drops[i].x;
        int y = drops[i].y;

        // head
        matrix_set_pixel(x, y, 10, 10, 50);

        // tail
        if(y - 1 >= 0)
            matrix_set_pixel(x, y - 1, 5, 5, 20);

        if(y - 2 >= 0)
            matrix_set_pixel(x, y - 2, 2, 2, 10);
    }

    led_strip_refresh(led_strip);
}
void rain_effect()
{
    rain_init();

    while(1)
    {
        rain_update();
        rain_draw();

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
void app_main(void)
{
   
    led_init();
    test_leds();

    // moving_vertical_bar();
    rain_effect();
    

}
