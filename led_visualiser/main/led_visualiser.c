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

void draw_diagonal()
{
    matrix_clear();

    for(int i=0;i<MATRIX_WIDTH;i++)
    {
        matrix_set_pixel(i,i,10,20,2);
    }

    led_strip_refresh(led_strip);
}
void test_leds()
{
    for(int i = 0; i < NUM_LEDS; i++)
    {
        led_strip_set_pixel(led_strip, i, 0, 255, 0);
    }

    led_strip_refresh(led_strip);
}
void draw_horizontal_line(int y)
{
    matrix_clear();

    for(int x = 0; x < MATRIX_WIDTH; x++)
    {
        matrix_set_pixel(x, y, 50, 50, 60);
    }

    led_strip_refresh(led_strip);
}

void moving_dot()
{
    for(int x =0; x <MATRIX_WIDTH; x++)
    {
        matrix_clear();
        matrix_set_pixel(x,5,0,0,20);
        led_strip_refresh(led_strip);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
void app_main(void)
{
   
    led_init();
    test_leds();

    // while(1)
    // {
    //     ESP_LOGI(TAG, "Starting LED test");
    //     draw_diagonal();
    //     vTaskDelay(pdMS_TO_TICKS(1000));
    // }
    while(1)
    {
        // for(int y = 0; y < MATRIX_HEIGHT; y++)
        // {
        //     draw_horizontal_line(y);
        //     vTaskDelay(pdMS_TO_TICKS(100));
        // }
        moving_dot();
    }
    

}
