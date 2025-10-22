//
// Created by pkj on 2025/10/15.
//

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>

#define PWM_LED_ALIAS(i) DT_ALIAS(__CONCAT(pwm_led,i))
#define PWM_LED_IS_OKAY(i) DT_NODE_HAS_STATUS_OKAY(DT_PARENT(PWM_LED_ALIAS(i)))
#define PWM_LED(i,_) IF_ENABLED(PWM_LED_IS_OKAY(i), (PWM_DT_SPEC_GET(PWM_LED_ALIAS(i)),))

#define MAX_LEDS 10
static const struct pwm_dt_spec pwm_leds[] = {LISTIFY(MAX_LEDS, PWM_LED, ())};

#define NUM_STEPS  50U
#define SLEEP_MSEC 25U

int main(void)
{
    uint32_t pulse_width[ARRAY_SIZE(pwm_leds)];
    uint32_t steps[ARRAY_SIZE(pwm_leds)];
    uint32_t dirs[ARRAY_SIZE(pwm_leds)];
    int ret;

    printk("Found %d leds\n", ARRAY_SIZE(pwm_leds));
    for (size_t i = 0; i < ARRAY_SIZE(pwm_leds); i++)
    {
        pulse_width[i] = 0;
        steps[i] = pwm_leds[i].period / NUM_STEPS;
        dirs[i] = 1U;
        if (!pwm_is_ready_dt(&pwm_leds[i]))
        {
            printk("Error: PWM device %s is not read\n", pwm_leds[i].dev->name);
            return -1;
        }
    }
    while (true)
    {
        for (size_t i = 0; i < ARRAY_SIZE(pwm_leds); i++)
        {
            ret = pwm_set_pulse_dt(&pwm_leds[i], pulse_width[i]);
            if (ret)
            {
                printk("Error: PWM device %s failed to set pulse width\n", pwm_leds[i].dev->name);
            }
            printk("LED %d: Using pulse width %d%%\n", i, 100 * pulse_width[i] / pwm_leds[i].period);
            if (dirs[i] == 1)
            {
                if (pulse_width[i] + steps[i] >= pwm_leds[i].period)
                {
                    dirs[i] = 0U;
                }
                else
                {
                    pulse_width[i] += steps[i];
                }
            }
            else
            {
                if (pulse_width[i] - steps[i] <= 0)
                {
                    dirs[i] = 1U;
                }
                else
                {
                    pulse_width[i] -= steps[i];
                }
            }
        }
        k_sleep(K_MSEC(SLEEP_MSEC));
    }
    return 0;
}
