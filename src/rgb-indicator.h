/*
 * Copyright 2025 LooUQ Incorporated
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef RGB_INDICATOR 
#define RGB_INDICATOR 

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <zephyr/sys_clock.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>


/* Temporary channel assignments, board issue
 * Out-0 = Green, Out-1 = Blue, Out-2 = Red
 * Normal is Red/Green/Blue
 */

#define LP5817_REG_CHIPENABLE 0x00
#define LP5817_REG_MAXCURRENT 0x01
#define LP5817_REG_OUTENABLE 0x02
#define LP5817_REG_DOTCURRENT_0 0x14
#define LP5817_REG_DOTCURRENT_1 0x15
#define LP5817_REG_DOTCURRENT_2 0x16
#define LP5817_REG_UPDATE 0x0f

#define LP5817_REG_INTENSITY0 0x18
#define LP5817_REG_INTENSITY1 0x19
#define LP5817_REG_INTENSITY2 0x1a

#define LP5817_CMD_CHIPENABLE 0x01
#define LP5817_CMD_MAXCURRENT 0x01
#define LP5817_CMD_MAXCURRENT_0 0xff
#define LP5817_CMD_MAXCURRENT_1 0xcc
#define LP5817_CMD_MAXCURRENT_2 0xcc
#define LP5817_CMD_OUTENABLE 0X07
#define LP5817_CMD_UPDATE 0x55

struct led_rgb 
{
	uint8_t r;              // red channel
	uint8_t g;              // green channel
	uint8_t b;              // blue channel
};

#define RGB(_r, _g, _b) {.r = (_r), .g = (_g), .b = (_b)}
#define CREATE_RGB_PIXELS(_p,_r,_g,_b)  struct led_rgb _p = { .r = _r, .g = _g, .b = _b };


typedef uint8_t rgbi_color;

typedef struct _rgb_indicator
{
    const struct i2c_dt_spec *rgbdev;
    uint8_t brightness;                                 // 0-255, adj color channel 0-99 input to physical chip intensity
    struct led_rgb pixels;                              // stay consistent with Zephyr library led_strip.h
    uint8_t flashesAsked;
    uint8_t flashesPerformed;
    k_timeout_t onDuration;
    k_timeout_t offDuration;
    struct k_timer flashTimer;
    uint8_t flashState;
    struct k_work flashWork;
} rgb_indicator_t;


/**
 * @brief Initialize RGB indicator hardware and driver 
 * 
 * @param rgb_ctrllr Physical RGB controller devices
 * @param rgbi RGB indicator structure
 * @return int Error indicator, 0=success
 */
int rgbi_init(const struct i2c_dt_spec *rgb_ctrllr, rgb_indicator_t * rgbi);


/**
 * @brief Set the color of the display using a led_rgb struct
 * 
 * @param indicator Device spec pointer to the indicator to operate
 * @param pixels The structure containing the red, green, and blue color pixels.
 */
void rgbi_setColor(rgb_indicator_t * rgbi, const struct led_rgb * pixels);                                 // set color and display


/**
 * @brief Set the color of the display using individual color values
 * 
 * @param indicator Device spec pointer to the indicator to operate
 * @param red Brightness of the red channel 0 (off) to 255 (full brightness)
 * @param green Brightness of the green channel 0 (off) to 255 (full brightness)
 * @param blue Brightness of the blue channel 0 (off) to 255 (full brightness)
 */
void rgbi_setColorFromPixels(rgb_indicator_t * rgbi, rgbi_color red, rgbi_color green, rgbi_color blue);   // set color and display


/**
 * @brief Shut indicator off, all channels to 0
 * 
 * @param indicator Device spec pointer to the indicator to operate
 */
void rgbi_off(rgb_indicator_t * rgbi);                                                                     // set all channels to 0 = OFF


/**
 * @brief Perform (display) a flash sequence on the indicator
 * 
 * @param indicator Device spec pointer to the indicator to operate
 * @param pixels The color to display when the indicator is in the ON state
 * @param onDuration The period of time the indicator should be ON in a flash iteration
 * @param offDuration The period of time the indicator should be OFF in a flash iteration
 * @param count The number of flashes, ON/OFF sequences, that should be performed.
 */
void rgbi_flash(rgb_indicator_t * rgbi, struct led_rgb * pixels, k_timeout_t onDuration, k_timeout_t offDuration, uint8_t count);


/**
 * @brief Perform a continuous flash sequence until manually cancelled
 * 
 * @param indicator Device spec pointer to the indicator to operate
 * @param pixels The color to display when the indicator is in the ON state
 * @param onDuration The period of time the indicator should be ON in a flash iteration
 * @param offDuration The period of time the indicator should be OFF in a flash iteration
 */
void rgbi_flash_continuous(rgb_indicator_t * rgbi, struct led_rgb pixels, k_timeout_t onDuration, k_timeout_t offDuration);


/**
 * @brief Stop/cancel a flash sequence. Required to end a continuous flash sequence, but can cut a normal count flash short. 
 * 
 * @param indicator Device spec pointer to the indicator to operate
 */
void rgbi_cancel(rgb_indicator_t * rgbi);


/**
 * @brief Determine if a flash sequence is underway. 
 * 
 * @param indicator Device spec pointer to the indicator to operate
 * @return true A flash sequence is running, can be a normal count flash or continuous
 * @return false The indicator is idle and not flashing.
 * 
 * @note The indicator may be ON (solid) or OFF. If your application needs to know ON
 * state color, you will need to record this information in your application.
 */
bool rgbi_isBusy(rgb_indicator_t * rgbi);

#endif