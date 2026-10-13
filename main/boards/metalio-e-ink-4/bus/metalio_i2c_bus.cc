#include "metalio_i2c_bus.h"

#include "config.h"

#include <driver/gpio.h>
#include <esp_check.h>
#include <esp_rom_sys.h>

static void MetalioI2cBus_RecoverPins() {
    const gpio_config_t conf = {
        .pin_bit_mask = (1ULL << I2C_SDA_PIN) | (1ULL << I2C_SCL_PIN),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&conf);
    gpio_set_level(I2C_SDA_PIN, 1);
    gpio_set_level(I2C_SCL_PIN, 1);
    esp_rom_delay_us(10);

    for (int i = 0; i < 9 && gpio_get_level(I2C_SDA_PIN) == 0; ++i) {
        gpio_set_level(I2C_SCL_PIN, 0);
        esp_rom_delay_us(5);
        gpio_set_level(I2C_SCL_PIN, 1);
        esp_rom_delay_us(5);
    }
    // STOP
    gpio_set_level(I2C_SDA_PIN, 0);
    esp_rom_delay_us(5);
    gpio_set_level(I2C_SCL_PIN, 1);
    esp_rom_delay_us(5);
    gpio_set_level(I2C_SDA_PIN, 1);
    esp_rom_delay_us(5);
}

void MetalioI2cBus_Create(i2c_master_bus_handle_t* bus) {
    if (bus == nullptr) {
        return;
    }
    if (*bus != nullptr) {
        i2c_del_master_bus(*bus);
        *bus = nullptr;
    }
    MetalioI2cBus_RecoverPins();
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags =
            {
                .enable_internal_pullup = 1,
            },
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, bus));
}
