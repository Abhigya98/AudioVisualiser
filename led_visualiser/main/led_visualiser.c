#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define BLINK_LED_0 5

void blink_led_task(void *pvParameter)
{
    char buffer[512];
    gpio_set_direction(BLINK_LED_0, GPIO_MODE_OUTPUT);
    while(1)
    {
        vTaskList(buffer);
        printf("TaskList: \n%s\n", buffer);
        gpio_set_level(BLINK_LED_0,1);
        vTaskDelay(500 / portTICK_PERIOD_MS);
        gpio_set_level(BLINK_LED_0, 0);
        vTaskDelay(500/ portTICK_PERIOD_MS);
    }

}

void print_taskList_task(void *pvParameter)
{
    while(1)
    {
        printf("Hello from the other task");
        vTaskDelay(100/portTICK_PERIOD_MS);
    }
    
}
void app_main(void)
{
    xTaskCreate(blink_led_task, "blink_led_0", 2048, NULL,1,NULL);
    xTaskCreate(print_taskList_task, "print_task", 1024, NULL,1,NULL);
}
