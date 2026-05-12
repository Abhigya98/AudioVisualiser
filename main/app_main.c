#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "audio.h"
#include "led_matrix.h"
#include "visualiser.h"

void app_main(void)
{
    audio_init();
    matrix_init();

    while (1)
    {
        int level = audio_get_level();
        // int level = audio_get_sample(); // For testing, use raw sample instead of level
        printf("%d\n", level);
        visualizer_update(level);
        vTaskDelay(pdMS_TO_TICKS(100));   // 50 FPS
    }
}