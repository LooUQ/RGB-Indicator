/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(rgbi, LOG_LEVEL_INF);

#include "rgb_indicator.h"

#define SLEEP_TIME_MS 250

#define HXRQST_NODE DT_NODELABEL(hxrqst)
#define PWRGOOD_NODE DT_NODELABEL(pwrgood)
#define RGBCTRL_NODE DT_NODELABEL(rgbctrl)

static const struct gpio_dt_spec hxrqst = GPIO_DT_SPEC_GET(HXRQST_NODE, gpios);
static const struct gpio_dt_spec pwrgood = GPIO_DT_SPEC_GET(PWRGOOD_NODE, gpios);
static const struct i2c_dt_spec rgb_ctrl = I2C_DT_SPEC_GET(RGBCTRL_NODE);

// #define BMP_NODE DT_NODELABEL(bmp)
// #define SHT_NODE DT_NODELABEL(sht)
// static const struct i2c_dt_spec bmp_snsr = I2C_DT_SPEC_GET(BMP_NODE);
// static const struct i2c_dt_spec sht_snsr = I2C_DT_SPEC_GET(SHT_NODE);

static const struct led_rgb LED_OFF = RGB(0,0,0);
static const struct led_rgb LED_RED = RGB(100,0,0);
static const struct led_rgb LED_GREEN = RGB(0,100,0);
static const struct led_rgb LED_BLUE = RGB(0,0,100);

static const struct led_rgb colors[] = {
    RGB(0, 0, 0),
    RGB(100, 0, 0),          /* red */
    RGB(0, 0, 0),
    RGB(0, 100, 0),         /* green */
    RGB(0, 0, 0),
    RGB(0, 0, 100),         /* blue */
    RGB(0, 0, 0),
    RGB(100, 100, 100),     /* white */
    RGB(0, 0, 0),
    RGB(0, 0, 0),
    RGB(100, 100, 0),
    RGB(0, 100, 100)
};

rgb_indicator_t rgbi;

int main(void)
{
    int ret;
    int loopcount = 0;

    printf("Hello %s, welcome to the IoT world and watch out for green flashes on the horizon! \r\n", CONFIG_BOARD_TARGET);

    if (!gpio_is_ready_dt(&hxrqst) ||
        !gpio_is_ready_dt(&pwrgood) ||
        !device_is_ready(rgb_ctrl.bus) 
        // !device_is_ready(bmp_snsr.bus) ||
        // !device_is_ready(sht_snsr.bus)
       )
    {
        LOG_ERR("Required devices not ready");
        return 0;
    }

    ret  = gpio_pin_configure_dt(&hxrqst, GPIO_OUTPUT_ACTIVE) < 0 ? 1 : 0;
    ret += gpio_pin_configure_dt(&pwrgood, GPIO_OUTPUT_ACTIVE) < 0 ? 1 : 0;
    if (ret != 0)
    {
        LOG_ERR("Unable to configure I/O");
        return 0;
    }

    ret = rgbi_init(&rgb_ctrl, &rgbi);
    if (ret != 0)
    {
        printk("Failed to initalize the RGB indicator, err=%d\r\n", ret);
    }

    rgbi_setColor(&rgbi, &LED_OFF);
    rgbi_setColor(&rgbi, &LED_RED);                 // got green, blue, red
    k_msleep(1000);
    rgbi_setColor(&rgbi, &LED_GREEN);
    k_msleep(1000);
    rgbi_setColor(&rgbi, &LED_BLUE);
    k_msleep(1000);
    rgbi_setColor(&rgbi, &LED_OFF);

    while (1)
    {
        ret =  gpio_pin_toggle_dt(&hxrqst) < 0 ? 1 : 0;
        ret += gpio_pin_toggle_dt(&pwrgood) < 0 ? 1 : 0;
        if (ret != 0)
        {
            LOG_ERR("I/O error on pin output");
            return 0;
        }

        loopcount++;
        printf("Loops: %d\n", loopcount);
        k_msleep(SLEEP_TIME_MS);
    }
    return 0;
}
