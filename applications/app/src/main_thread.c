//
// Created by pkj on 2025/10/15.
//

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/__assert.h>
#include <string.h>

#define STACK_SIZE 1024
#define PRIORITY 7

#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)


#if !DT_NODE_HAS_STATUS_OKAY(LED0_NODE)
#error "led0 not support"
#endif

#if !DT_NODE_HAS_STATUS_OKAY(LED1_NODE)
#error "led1 not support"
#endif

struct printk_data_t
{
    void* fifo_reserved;
    uint32_t led;
    uint32_t cnt;
};

K_FIFO_DEFINE(printk_fifo);

struct led
{
    struct gpio_dt_spec spec;
    uint8_t num;
};

static const struct led led0 = {
    .spec = GPIO_DT_SPEC_GET_OR(LED0_NODE, gpios, {0}),
    .num = 0
};

static const struct led led1 = {
    .spec = GPIO_DT_SPEC_GET_OR(LED1_NODE, gpios, {0}),
    .num = 1
};

void blink(const struct led* led, uint32_t sleep_ms, uint32_t id)
{
    const struct gpio_dt_spec* spec = &led->spec;
    int cnt = 0;
    int ret;
    if (!device_is_ready(spec->port))
    {
        printk("%s: device is not ready\n", __FUNCTION__);
        return;
    }
    ret = gpio_pin_configure_dt(spec,GPIO_OUTPUT);
    if (ret < 0)
    {
        printk("%s: gpio_pin_configure_dt failed\n", __FUNCTION__);
        return;
    }

    while (1)
    {
        gpio_pin_set_dt(spec, cnt % 2);
        struct printk_data_t tx_data = {. led = id, .cnt = cnt};
        size_t size = sizeof(struct printk_data_t);
        char*mem_ptr = k_malloc(size);
        __ASSERT_NO_MSG(mem_ptr != NULL);
        memcpy(mem_ptr, &tx_data, sizeof(struct printk_data_t));
        k_fifo_put(&printk_fifo, mem_ptr);
        k_msleep(sleep_ms);
        cnt++;
    }
}

void blink0(void)
{
    blink(&led0, 1000, 0);
}

void blink1(void)
{
    blink(&led1, 100, 1);
}

void uart_out(void)
{
    while (1)
    {
        struct printk_data_t * rx_data = k_fifo_get(&printk_fifo,K_FOREVER);
        printk("Toggled led%d, counter=%d\n",rx_data->led,rx_data->cnt);
        k_free(rx_data);
    }
}


K_THREAD_DEFINE(blink0_id,STACK_SIZE,blink0,NULL,NULL,NULL,PRIORITY,0,0);
K_THREAD_DEFINE(blink1_id,STACK_SIZE,blink1,NULL,NULL,NULL,PRIORITY,0,0);
K_THREAD_DEFINE(blink2_id,STACK_SIZE,uart_out,NULL,NULL,NULL,PRIORITY,0,0);
