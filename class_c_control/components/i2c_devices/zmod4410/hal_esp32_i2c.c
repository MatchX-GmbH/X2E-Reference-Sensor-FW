/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-11-15     Sherman      first version
 */

#include "hal_esp32_i2c.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_err.h"

#include "assert.h"
#include "rom/ets_sys.h"
#include "freertos/task.h"

#define TAG "hal_esp32_i2c.c"

#define ZMOD4410_NAME      "i2c1"

#define AIR_INT (GPIO_NUM_14)


#define ACK_EN true
struct rt_i2c_bus_device *i2c_bus;

#define USER_INPUT  "P105"
static uint8_t is_key = 0;
#define ZMOD_ESP_HAL_SCAN_I2C_ADDRS \
  { 0x32 }
static xQueueHandle gpio_zmod_irq_queue = NULL;
/**
 * @brief Sleep for some time. Depending on target and application this can \n
 *        be used to go into power down or to do task switching.
 * @param [in] ms will sleep for at least this number of milliseconds
 */
static void esp_sleep(uint32_t ms)
{
    vTaskDelay(ms / portTICK_PERIOD_MS);
}

/* I2C communication */
/**
 * @brief Read a register over I2C
 * @param [in] i2c_addr 7-bit I2C slave address of the ZMOD45xx
 * @param [in] reg_addr address of internal register to read
 * @param [out] buf destination buffer; must have at least a size of len*uint8_t
 * @param [in] len number of bytes to read
 * @return error code
 */
static int8_t esp_i2c_read(uint8_t i2c_addr, uint8_t reg_addr, uint8_t *buf, uint8_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    ESP_ERROR_CHECK(i2c_master_start(cmd));
    ESP_ERROR_CHECK(i2c_master_write_byte(
                        cmd, (i2c_addr << 1) | I2C_MASTER_WRITE, ACK_EN));
    ESP_ERROR_CHECK(i2c_master_write_byte(cmd, reg_addr, ACK_EN));
    ESP_ERROR_CHECK(i2c_master_start(cmd));
    ESP_ERROR_CHECK(i2c_master_write_byte(
                        cmd, (i2c_addr << 1) | I2C_MASTER_READ, ACK_EN));
    ESP_ERROR_CHECK(i2c_master_read(cmd, buf, len, I2C_MASTER_LAST_NACK));
    ESP_ERROR_CHECK(i2c_master_stop(cmd));
    esp_err_t ret = i2c_master_cmd_begin(ZMOD_I2C_PORT, cmd, 1000 / portTICK_PERIOD_MS);
    assert(ret == ESP_OK);
    i2c_cmd_link_delete(cmd);
    return ZMOD4XXX_OK;
}

/**
 * @brief Write a register over I2C using protocol described in Renesas App Note \n
 *        ZMOD4xxx functional description.
 * @param [in] i2c_addr 7-bit I2C slave address of the ZMOD4xxx
 * @param [in] reg_addr address of internal register to write
 * @param [in] buf source buffer; must have at least a size of len*uint8_t
 * @param [in] len number of bytes to write
 * @return error code
 */
static int8_t esp_i2c_write(uint8_t i2c_addr, uint8_t reg_addr, uint8_t *buf, uint8_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    ESP_ERROR_CHECK(i2c_master_start(cmd));
    ESP_ERROR_CHECK(i2c_master_write_byte(
                        cmd, (i2c_addr << 1) | I2C_MASTER_WRITE, ACK_EN));
    ESP_ERROR_CHECK(i2c_master_write_byte(cmd, reg_addr, ACK_EN));
    ESP_ERROR_CHECK(i2c_master_write(cmd, buf, len, I2C_MASTER_ACK));
    ESP_ERROR_CHECK(i2c_master_stop(cmd));
    esp_err_t ret = i2c_master_cmd_begin(ZMOD_I2C_PORT, cmd, 1000 / portTICK_PERIOD_MS);
    assert(ret == ESP_OK);
    i2c_cmd_link_delete(cmd);
    return ZMOD4XXX_OK;

}
void air_intr_task(void *arg)
{
    uint32_t io_num;
    for (;;) {
        if (xQueueReceive(gpio_zmod_irq_queue, &io_num, portMAX_DELAY)) {
            ESP_LOGI(TAG, "GPIO[%d] intr, val: %d", io_num, gpio_get_level(io_num));
        }
        // vTaskDelete(NULL);
    }
}

void IRAM_ATTR air_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_zmod_irq_queue, &gpio_num, NULL);
}

void irq_init(void)
{
    gpio_zmod_irq_queue = xQueueCreate(20, sizeof(uint32_t));
    gpio_isr_handler_add(AIR_INT, air_isr_handler, NULL);
    xTaskCreate(air_intr_task, "airCallback", 5048, NULL, 0, NULL);
}

// static void irq_callback(void *args)
// {
//     is_key = 1;
// }

// static void esp_i2c_scan_addr(void)
// {
//     uint8_t addr_list[] = ZMOD_ESP_HAL_SCAN_I2C_ADDRS;
//     for (int i = 0; i < sizeof(addr_list); ++i) {
//         uint8_t addr = addr_list[i];
//         i2c_cmd_handle_t cmd = i2c_cmd_link_create();
//         ESP_ERROR_CHECK(i2c_master_start(cmd));
//         ESP_ERROR_CHECK(
//             i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true));
//         ESP_ERROR_CHECK(i2c_master_stop(cmd));
//         esp_err_t esp_err = i2c_master_cmd_begin(
//                                 ZMOD_I2C_PORT, cmd, pdMS_TO_TICKS(300));
//         i2c_cmd_link_delete(cmd);
//         if (esp_err == ESP_OK) {
//             ESP_LOGW(TAG, "FOUND ADRESS : %d %s %d\n", __LINE__, __func__, addr);
//         }
//     }
// }


/**
 * @brief   Initialize the target hardware
 * @param   [in] dev pointer to the device
 * @return  error code
 * @retval  0 success
 * @retval  "!= 0" error
 */
int8_t init_hardware(zmod4xxx_dev_t *dev)
{
#ifdef CONFIG_BOSH_SINGLE
    i2c_config_t i2c = {.mode = I2C_MODE_MASTER,
                        .sda_io_num = ZMOD_I2C_GPIO_NUM_DATA,
                        .scl_io_num = ZMOD_I2C_GPIO_NUM_CLOCK,
                        // .sda_pullup_en = GPIO_PULLUP_ENABLE,
                        // .scl_pullup_en = GPIO_PULLUP_ENABLE,
                        // .slave.addr_10bit_en = 0,
                        .master.clk_speed = ZMOD_I2C_FREQ
                       };

    ESP_LOGD(TAG, ":%d %s \n", __LINE__, __func__);
    ESP_ERROR_CHECK(i2c_param_config(ZMOD_I2C_PORT, &i2c));
    ESP_ERROR_CHECK(i2c_driver_install(ZMOD_I2C_PORT, I2C_MODE_MASTER, 0, 0,
                                       ESP_INTR_FLAG_IRAM));
#endif
    dev->read = esp_i2c_read;
    dev->write = esp_i2c_write;
    dev->delay_ms = esp_sleep;
    /* init */
    return ZMOD4XXX_OK;
}

/**
 * @brief   Check if any key is pressed
 * @retval  1 pressed
 * @retval  0 not pressed
 */
int8_t is_key_pressed(void)
{
    if (is_key) {
        is_key = 0;
        return 1;
    }
    return 0;
}

/**
 * @brief   deinitialize target hardware
 * @return  error code
 * @retval  0 success
 * @retval  "!= 0" error
 */
int8_t deinit_hardware(void)
{
    return ZMOD4XXX_OK;
}
