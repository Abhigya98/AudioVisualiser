#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "freertos/queue.h"
#include "esp_timer.h"

#define BLINK_LED_0 5
#define QUEUE_LENGTH 10

QueueHandle_t sampleQueue;

// void blink_led_task(void *pvParameter)
// {
//     char buffer[512];
//     gpio_set_direction(BLINK_LED_0, GPIO_MODE_OUTPUT);
//     while(1)
//     {
//         vTaskList(buffer);
//         printf("TaskList: \n%s\n", buffer);
//         gpio_set_level(BLINK_LED_0,1);
//         vTaskDelay(500 / portTICK_PERIOD_MS);
//         gpio_set_level(BLINK_LED_0, 0);
//         vTaskDelay(500/ portTICK_PERIOD_MS);
//     }

// }

void print_taskList_task(void *pvParameter)
{
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(50);
    xLastWakeTime = xTaskGetTickCount();
    while(1)
    {
        int64_t t = esp_timer_get_time();
        printf("[%lld ms] Hello from the other task\n", t/1000);
        vTaskDelayUntil(&xLastWakeTime,xFrequency);
    }
    
}
void monitor_task(void *pvParameter)
{
    char buffer[512];
    while(1)
    {
        vTaskList(buffer);
        printf("Task List:\n%s\n", buffer);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void producer_task(void *pvParameter)
{
    int sample =0;

    while(1)
    {
        
        if(xQueueSend(sampleQueue, &sample, portMAX_DELAY) == pdPASS)
        {
            int64_t t = esp_timer_get_time(); // time in microseconds
            printf("[%lld ms] Produced: %d\n", t/1000, sample);
            sample++;
        }

            vTaskDelay(1000/portTICK_PERIOD_MS);
    }
}

void consumer_task(void *pvParameter)
{
    int receivedSample = 0;
    while(1)
    {
        
        if(xQueueReceive(sampleQueue, &receivedSample, portMAX_DELAY)==pdPASS)
        {
            int64_t t = esp_timer_get_time();
            printf("[%lld ms] Consumed: %d\n", t/1000, receivedSample);
        }
    }
}

void metronome_task(void *pvParameter)
{
    gpio_set_direction(BLINK_LED_0, GPIO_MODE_OUTPUT);
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(50);
    int state =0;

    xLastWakeTime = xTaskGetTickCount();
    while(1)
    {
        int64_t t = esp_timer_get_time();
        printf("[%lld ms] Tick\n", t/1000);
        gpio_set_level(BLINK_LED_0,state);
        vTaskDelayUntil(&xLastWakeTime,xFrequency);
        state = !state;
    }
}
void app_main(void)
{
    // xTaskCreate(blink_led_task, "blink_led_0", 2048, NULL,1,NULL);
    xTaskCreate(print_taskList_task, "print_task", 1024, NULL,1,NULL);

    // sampleQueue = xQueueCreate(QUEUE_LENGTH,sizeof(int));

    // xTaskCreate(producer_task,"producer_Task", 2048,NULL,1,NULL);
    // xTaskCreate(consumer_task,"consumer_Task", 2048,NULL,1,NULL);
    // xTaskCreate(monitor_task,"monitor_Task", 2048,NULL,1,NULL);
    xTaskCreate(metronome_task,"metronome_Task", 2048,NULL,2,NULL);
}
