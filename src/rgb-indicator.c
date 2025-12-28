/*
 * Copyright 2025 LooUQ Incorporated
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys_clock.h>

#include "rgb-indicator.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(rgb_indicator);

const uint8_t dot_current[] = { 128, 128, 128 };                                // relative brightness of each RGB channel

/* Private service function declarations
 * -------------------------------------------------------------------------------------------- */
static int lp5817_init(const struct i2c_dt_spec *rgb_ctrllr);
static void lp5817_setColor(const struct i2c_dt_spec *rgb_ctrllr, uint8_t red, uint8_t green, uint8_t blue);
static void flashTimerExpiry(struct k_timer *flashTimer);                       // ISR for timer expiry
static void flashDisplay_handler(struct k_work *work);                          // workqueue handler for flash actions
static inline bool isFlashing(rgb_indicator_t * indicator);                     // quick check for active flash session


/* ------------------------------------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------------------------------- */

/**
 * @brief Initialize RGB indicator device and driver
 * 
 * @param rgb_dev TI LP5817 RGB controller device 
 * @param rgbi RGB indicator struct holding display parameters
 * @return int 0 = success
 */
int rgbi_init(const struct i2c_dt_spec *rgb_dev, rgb_indicator_t * rgbi)
{
    int ret = -1;

    if (lp5817_init(rgb_dev) == 0)                              // initialize/validate LP5817 before building object
    {
        ret = 0;                                                // clear err return
        rgbi->rgbdev = rgb_dev;

        k_timer_init(&(rgbi->flashTimer), flashTimerExpiry, NULL);
        k_timer_user_data_set(&(rgbi->flashTimer), rgbi);
        k_work_init(&(rgbi->flashWork), flashDisplay_handler);
    }

    return ret;
}


/**
 * @brief Set the RGB contoller to display a color from a RGB struct
 * 
 * @param rgbi Indicator device to control
 * @param channels RBG structure containing red, green, and blue channels 
 */
void rgbi_setColor(rgb_indicator_t *rgbi, const struct led_rgb * channels)
{
    lp5817_setColor(rgbi->rgbdev, channels->r, channels->g, channels->b);
}


/**
 * @brief Set the RGB contoller to display a color using 3 channel values
 * 
 * @param rgbi Indicator device to control
 * @param red The red channel intensity (scale is 0=off to 255=full intensity)
 * @param green The green channel intensity
 * @param blue The blue channel intensity
 */
void rgbi_setColorFromPixels(rgb_indicator_t *rgbi, rgbi_color red, rgbi_color green, rgbi_color blue)
{
    lp5817_setColor(rgbi->rgbdev, red, green, blue);
}


/**
 * @brief Turn indicator off
 *
 * @param rgbi The RGB indicator to actuate
 */
void rgbi_off(rgb_indicator_t *rgbi)
{
    if (!isFlashing(rgbi))                                       // do not change indicator if flash sequence underway
    {
        rgbi_setColorFromPixels(rgbi, 0, 0, 0);
    }
}


/**
 * @brief Setup/initiate a indicator flash sequence
 * 
 * @param rgbi The RGB indicator to actuate
 * @param colors The color set to display when flash indicates ON
 * @param onDuration The amount of time the indicator is on
 * @param offDuration The amount of time the indicator is off
 * @param count The number of "on" flashes to display, if count==0 the flash sequence continues until cancelled
 */
void rgbi_flash(rgb_indicator_t *rgbi, struct led_rgb * pixels, k_timeout_t onDuration, k_timeout_t offDuration, uint8_t count)
{
    rgbi->onDuration = onDuration;
    rgbi->offDuration = offDuration;
    rgbi->flashesAsked = count;                                        // number of ON pulses OR 0==continuous
    rgbi->flashesPerformed = 0;
    memcpy(&(rgbi->pixels), pixels, sizeof(struct led_rgb));           // copy pixels into object (for next flash ON event)

    rgbi_setColor(rgbi, pixels);                                       // start the sequence, the flashExpiry() callback will take it from here
    k_timer_start(&(rgbi->flashTimer), rgbi->onDuration, K_NO_WAIT);
    rgbi->flashState = 1;
}


/**
 * @brief Let caller know if the indicator in busy displaying a flash sequence
 * 
 * @param rgbi The RGB indicator to actuate
 * @return true Flash sequence is underway, could be continuous
 * @return false The indicator is idle (not executing a flash sequence)
 */
bool rgbi_isBusy(rgb_indicator_t *rgbi)
{
    return isFlashing(rgbi);
}


/**
 * @brief Cancel flash sequence, if underway, and turn indicator off
 * 
 * @param rgbi The RGB indicator to actuate
 */
void rgbi_cancel(rgb_indicator_t *rgbi)
{
    k_timer_stop(&(rgbi->flashTimer));                         // stop timer, prevent expiry
    rgbi->onDuration = K_NO_WAIT;                              // clear flashing: onDuration==0 indicates idle
    rgbi_off(rgbi);                                            // idle state is LED OFF
}


/* Private service functions definitions
 * --------------------------------------------------------------------------------------------- */

 /**
 * @brief Initialize TI LP5817 chip for use
 * 
 * @param rgb_ctrl The device spec for the RGB LED controller 
 * @return int 0=success
 */
static int lp5817_init(const struct i2c_dt_spec *rgb_ctrllr)
{
	int ret;
    uint8_t cmd[2];

    /* Write CHIP_EN = 1 to enable controller
     * Set max current (chip and ouputs)
     * Enable outputs
     * Send UPDATE_CMD to activate configuration
     */

    if (!device_is_ready(rgb_ctrllr->bus)) {
        LOG_ERR("I2C bus %s is not ready!\n\r",rgb_ctrllr->bus->name);
        return 0;
    }

    cmd[0] = LP5817_REG_CHIPENABLE;
    cmd[1] = LP5817_CMD_CHIPENABLE;
    ret = i2c_write_dt(rgb_ctrllr, cmd, sizeof(cmd));
    if (ret != 0)
    {
        LOG_ERR("Failed to write chip enable (%x) to LP5817@%x err=%d\r\n", cmd[0], rgb_ctrllr->addr, ret);
    }

    cmd[0] = LP5817_REG_MAXCURRENT;
    cmd[1] = LP5817_CMD_MAXCURRENT;
    ret = i2c_write_dt(rgb_ctrllr, cmd, sizeof(cmd));
    if (ret != 0)
    {
        LOG_ERR("Failed to set LP5817 chip-level max current\r\n");
    }

    ret = 0;
    for (size_t i = 0; i < 3; i++)
    {
        cmd[0] = LP5817_REG_DOTCURRENT_0 + i;
        cmd[1] = dot_current[i];                                        // apply dot (channel) configured current (brightness adjustment)
        ret += i2c_write_dt(rgb_ctrllr, cmd, sizeof(cmd));
    }
    if (ret != 0)
    {
        LOG_ERR("Failed to set LP5817 DOT (channel) current\r\n");
    }

    cmd[0] = LP5817_REG_OUTENABLE;
    cmd[1] = LP5817_CMD_OUTENABLE;
    ret = i2c_write_dt(rgb_ctrllr, cmd, sizeof(cmd));
    if (ret != 0)
    {
        LOG_ERR("Failed to enable channels\r\n");
    }


    lp5817_setColor(rgb_ctrllr, 0, 0, 0);

    cmd[0] = LP5817_REG_UPDATE;
    cmd[1] = LP5817_CMD_UPDATE;
    ret = i2c_write_dt(rgb_ctrllr, cmd, sizeof(cmd));
    if (ret != 0)
    {
        LOG_ERR("Failed to apply settings to LP5817\r\n");
    }

    // uint8_t val;
    // i2c_reg_read_byte_dt(rgb_ctrllr, LP5817_REG_OUTENABLE, &val);
    // if (val != 0)
    // {
    //     LOG_WRN("Warning: LP5817 is not enabled\r\n");
    // }

    return ret;
}


static void lp5817_setColor(const struct i2c_dt_spec *rgb_ctrllr, uint8_t red, uint8_t green, uint8_t blue)
{
   	int ret;
    uint8_t cmd[2];

    // per TI documentation 0x18, 0x 19, 0x1a. But doesn't seem to be that way
    // cmd[0] = LP5817_REG_INTENSITY0;      // red
    // cmd[0] = LP5817_REG_INTENSITY1;      // green
    // cmd[0] = LP5817_REG_INTENSITY2;      // blue

    cmd[0] = LP5817_REG_INTENSITY2;
    cmd[1] = red;
    ret = i2c_write_dt(rgb_ctrllr, cmd, sizeof(cmd));

    cmd[0] = LP5817_REG_INTENSITY0;
    cmd[1] = green;
    ret = i2c_write_dt(rgb_ctrllr, cmd, sizeof(cmd));

    cmd[0] = LP5817_REG_INTENSITY1;
    cmd[1] = blue;
    ret = i2c_write_dt(rgb_ctrllr, cmd, sizeof(cmd));

    if (ret != 0)
    {
    	LOG_ERR("Could not update indicator");
    }

}


/**
 * @brief ISR to service end of flash duration
 * 
 * @param flashTimer Timer object
 */
static void flashTimerExpiry(struct k_timer *flashTimer)
{
    rgb_indicator_t * rgbi = (rgb_indicator_t *)k_timer_user_data_get(flashTimer);     // get rgb indicator object from timer
    k_work_submit(&(rgbi->flashWork));                                                 // submit the work item to system WQ to update flash sequence
}


/**
 * @brief Handler to perform RGB indicator update (flash ON>>OFF or OFF>>ON)
 * 
 * @param work workqueue item to process
 */
static void flashDisplay_handler(struct k_work *work)
{
    rgb_indicator_t *rgbi = CONTAINER_OF(work, rgb_indicator_t, flashWork);            // get parent structure: rgb indicator struct
    // rgb_indicator_t * indicator = (rgb_indicator_t *)k_timer_user_data_get(flashTimer);

    if (rgbi->onDuration.ticks > 0)
    {
        if (rgbi->flashState)                                              // is ON currently
        {
            rgbi_setColorFromPixels(rgbi, 0, 0, 0);                        // can't use rgbi_off inside flash sequence
            rgbi->flashState = 0;
            rgbi->flashesPerformed++;                                      // completed an ON flash

            if (rgbi->flashesPerformed < rgbi->flashesAsked ||        // still flashes to perform
                rgbi->flashesAsked == 0)                                   // OR IF continuous flash sequence
            {
                k_timer_start(&(rgbi->flashTimer), rgbi->offDuration, K_NO_WAIT);    // wait out OFF (for next ON event)
            }
            else                                                                // done with flash sequence, reset
            {
                rgbi->flashesAsked = 0;                                    
                rgbi->flashesPerformed = 0;
                rgbi->onDuration = K_NO_WAIT;                              // signal done
            }
        }
        else                                                                    // indicator is OFF
        {
            rgbi->flashState = 1;
            if (rgbi->flashesPerformed < rgbi->flashesAsked)
            {
                rgbi_setColor(rgbi, &(rgbi->pixels));                 // turn indicator ON
                k_timer_start(&(rgbi->flashTimer), rgbi->onDuration, K_NO_WAIT);
            }
        }
    }
}


/**
 * @brief Inline function to determine if the indicator is currently busy with previously issued flash sequence command
 * 
 * @param rgbi RGB indicator device
 * @return true Indicator is busy 
 * @return false Indicator is idle
 */
static inline bool isFlashing(rgb_indicator_t * rgbi)
{
    return rgbi->onDuration.ticks > 0;
}
