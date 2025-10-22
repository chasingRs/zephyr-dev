//
// Created by pkj on 2025/10/15.
//

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>

static const struct pwm_dt_spec pwm_led0 = PWM_DT_SPEC_GET(DT_ALIAS(pwm_led0));

#define MIN_PERIDO PWM_SEC(1U) / 128U
#define MAX_PERIDO PWM_SEC(1U)

int main(void)
{
    uint32_t max_period, period;
    bool dir = 0U;
    int ret;

    printk("PWM-base blinky\n");
    if (!pwm_is_ready_dt(&pwm_led0))
    {
        printk("Error: PWM led:%s not ready\n", pwm_led0.dev->name);
        return -1;
    }

    max_period = MAX_PERIDO;
    while (pwm_set_dt(&pwm_led0, max_period, max_period / 2U))
    {
        max_period /= 2;
        if (max_period < 4U * MIN_PERIDO)
        {
            printk("Error: PWM device does not support a period at least %lu\n", 4U * MIN_PERIDO);
            return -1;
        }
    }
    printk(" Calibrate done: max period=%d\n", max_period);
    period = max_period;
    while (true)
    {
        ret = pwm_set_dt(&pwm_led0, period, period / 2U);
        if (ret < 0)
        {
            printk("Error: PWM led:%s failed (%d)\n", pwm_led0.dev->name, ret);
            return -1;
        }
        printk("PWM led:%s done\n", pwm_led0.dev->name);
        period = dir ? (period / 2U) : (period * 2U);
        if (period > max_period)
        {
            period = max_period;
            dir = 1;
        }
        else if (period < MIN_PERIDO)
        {
            period = MIN_PERIDO;
            dir = 0;
        }
        k_sleep(K_SECONDS(3U));
    }
    return 0;
}
