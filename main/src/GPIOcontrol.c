#include "common.h"
#include "gatt_svc.h"
#include "GPIOcontrol.h"

static QueueHandle_t gpio_evt_queue = NULL;

// 中断函数内只记录当前时间，其余事件在task里完成
static void IRAM_ATTR gpio_isr_handler()
{
    gettimeofday(&timeval_s, NULL);
    xQueueSendFromISR(gpio_evt_queue, &timeval_s, NULL);
}

// 算是真正的中断处理函数
static void gpio_task(void *arg)
{
    for (;;)
    {
        if (xQueueReceive(gpio_evt_queue, &timeval_s, portMAX_DELAY))
        {
            timeinfo.local_ts = (uint64_t)timeval_s.tv_sec * 1000000L + (uint64_t)timeval_s.tv_usec + timeinfo.current_bias;
            if(conninfo.is_connect){
                ESP_LOGI(TAG, "get IO event, print time: %lld", timeinfo.local_ts);
                notify_data_to_central();
            }
        }
    }
}

// 初始化gpio
void gpio_init()
{
    // 初始化led为输出模式
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << LED1 | 1ULL << LED2,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 1,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    gpio_set_level(LED1, 0);
    gpio_set_level(LED2, 0);

    // 初始化IO为中断模式
    io_conf.pin_bit_mask = 1ULL << IO_ISR;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    gpio_config(&io_conf);

    // 初始化中断任务
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    xTaskCreate(gpio_task, "gpio_task", 2048, NULL, 10, NULL);

    // 添加中断
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(IO_ISR, gpio_isr_handler, (void *)IO_ISR);

    ESP_LOGI(TAG, "Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());
}

// 改变LED状态
void set_led_state(uint8_t state)
{
    gpio_set_level(LED1, state);
    gpio_set_level(LED2, state);
}
