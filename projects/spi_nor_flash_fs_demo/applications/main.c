/*
 * Copyright (c) 2021 HPMicro
 *
 * Change Logs:
 * Date         Author          Notes
 * 2021-08-13   Fan YANG        first version
 *
 */

#include <rtthread.h>
#include <rtdevice.h>
#include "rtt_board.h"

#include "dev_spi_flash.h"
#include "dev_spi_flash_sfud.h"
#include "drv_spi.h"
#include "fal_cfg.h"
#include <drv_gpio.h>
#include <dfs_fs.h>
#include <fal.h>

void thread_entry(void *arg);

#define SPI_FLASH_DEVICE_NAME   "qspi_flash"  // qspi + SPI number + CS number
#define SPI_FLASH_BUS_NAME      "qspi0"   // SPI bus name (SPI2 in QUAD mode)
#define SPI_FLASH_CS_PIN        GET_PIN(A, 11)      // CS pin for software chip select

int main(void)
{

    app_init_led_pins();

    static uint32_t led_thread_arg = 0;
    rt_thread_t led_thread = rt_thread_create("led_th", thread_entry, &led_thread_arg, 1024, 1, 10);
    rt_thread_startup(led_thread);

    return 0;
}


void thread_entry(void *arg)
{
    while(1){
#ifdef APP_LED0
        app_led_write(APP_LED0, APP_LED_ON);
        rt_thread_mdelay(500);
        app_led_write(APP_LED0, APP_LED_OFF);
        rt_thread_mdelay(500);
#endif
#ifdef APP_LED1
        app_led_write(APP_LED1, APP_LED_ON);
        rt_thread_mdelay(500);
        app_led_write(APP_LED1, APP_LED_OFF);
        rt_thread_mdelay(500);
#endif
#ifdef APP_LED2
        app_led_write(APP_LED2, APP_LED_ON);
        rt_thread_mdelay(500);
        app_led_write(APP_LED2, APP_LED_OFF);
        rt_thread_mdelay(500);
#endif
    }
}

// Software CS control callback
static void spi_flash_cs_ctrl(uint32_t state)
{
    if (state)
        rt_pin_write(SPI_FLASH_CS_PIN, PIN_HIGH);
    else
        rt_pin_write(SPI_FLASH_CS_PIN, PIN_LOW);
}

static int rt_hw_spi_flash_init(void)
{
    rt_err_t result;
    
    /* 1. Attach SPI device to bus (using software CS) */
    rt_pin_mode(SPI_FLASH_CS_PIN, PIN_MODE_OUTPUT);
    result = rt_hw_spi_device_attach(SPI_FLASH_BUS_NAME, SPI_FLASH_DEVICE_NAME, spi_flash_cs_ctrl);
    if (result != RT_EOK)
    {
        rt_kprintf("spi device attach failed!\n");
        return -RT_ERROR;
    }
    
    /* 2. Probe flash using SFUD (auto-detect chip via SFDP) */
    if (RT_NULL == rt_sfud_flash_probe(SPI_NOR_FLASH_DEV_NAME, SPI_FLASH_DEVICE_NAME))
    {
        rt_kprintf("sfud flash probe failed!\n");
        return -RT_ERROR;
    }
    
    rt_kprintf("SPI Flash initialized successfully!\n");
    return RT_EOK;
}
INIT_COMPONENT_EXPORT(rt_hw_spi_flash_init);

static int filesystem_init(void)
{
    /* 1. Initialize FAL (Note: FAL auto-initializes, but explicit call is also acceptable) */
    fal_init();
    
    /* 2. Create block device on FAL partition - using 'fs' partition */
    struct rt_device *flash_dev = fal_blk_device_create("fs");
    if (flash_dev == NULL)
    {
        rt_kprintf("Failed to create block device!\n");
        return -RT_ERROR;
    }
    
    /* 3. Mount file system */
    /* Using elmfat: */
    if (dfs_mount("fs", "/", "elm", 0, 0) == 0)
    {
        rt_kprintf("File system mounted to /\n");
    }
    else
    {
        /* First time use requires formatting */
        rt_kprintf("mount failed, try to format...\n");
        if (dfs_mkfs("elm", "fs") == 0)
        {
            if (dfs_mount("fs", "/", "elm", 0, 0) == 0)
            {
                rt_kprintf("File system formatted and mounted!\n");
            }
        }
        else
        {
            rt_kprintf("format failed!\n");
        }
    }
    
    return RT_EOK;
}
INIT_ENV_EXPORT(filesystem_init);
