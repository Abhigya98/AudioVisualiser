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
void draw_box(int x_start, int y_start, int x_finish, int y_finish)
{
    matrix_clear(); 
    for(int x = x_start; x <=x_finish; x++)
    {
        for(int y=y_start; y <=y_finish; y++)
        {
            if(x== x_start || x == x_finish || y==y_start||y==y_finish)
            matrix_set_pixel(x,y,5,0,20);
            // matrix_set_pixel(x, y, x * 10, y * 10, 0);
        }
    }
    led_strip_refresh(led_strip);
    vTaskDelay(pdMS_TO_TICKS(1000));
}
void concentric_squares()
{
    while(1)
    {
        int x = 3;
        int y =3;
        int x_max = x*4;
        int y_max = y*4;

        while(x <= 8 && y <= 8)
        {
            draw_box(x++, y++,x_max--,y_max--);

        }
        // draw_box();
    }

}
void bouncing_dot()
{
    int x = 0;
    int dir = 3;

    while(1)
    {
        int y = rand()%MATRIX_HEIGHT;
        matrix_clear();

        matrix_set_pixel(x, y, 255, 0, 255);

        led_strip_refresh(led_strip);
        vTaskDelay(pdMS_TO_TICKS(100));

        x += dir;

        if(x == 0 || x == MATRIX_WIDTH - 1)
            dir = -dir;
    }
}

void moving_vertical_bar()
{ 
    while(1)
    {
        
        for(int y =0; y<MATRIX_HEIGHT;y++)
        {
            matrix_clear();
            for(int x = 0; x < MATRIX_WIDTH; x++)
            {
                matrix_set_pixel(x, y, 50, 50, 60);
            }
            led_strip_refresh(led_strip);
        vTaskDelay(pdMS_TO_TICKS(100));
        }
        

    }
}
typedef struct
{
    int x;
    int y;
} raindrop_t;
#define MAX_DROPS 20

raindrop_t drops[MAX_DROPS];

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
