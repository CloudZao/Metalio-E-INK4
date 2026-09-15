/*
 * CX25601N charger driver — ESP-IDF port (C++).
 * Register map aligned with example/cx2560x.c (MediaTek reference).
 */

#include "cx25601n.h"

#include "bq27220_gauge.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

namespace {

constexpr const char *TAG = "cx25601n";

/* --- Register map (CX25601N) --- */
constexpr uint8_t REG_ICHG_LO    = 0x02;
constexpr uint8_t REG_ICHG_HI    = 0x03;
constexpr uint8_t REG_VREG_LO    = 0x04;
constexpr uint8_t REG_VREG_HI    = 0x05;
constexpr uint8_t REG_IINDPM_LO  = 0x06;
constexpr uint8_t REG_IINDPM_HI  = 0x07;
constexpr uint8_t REG_IOTG_LO    = 0x0A; /* IOTG[3:0] @ [7:4] */
constexpr uint8_t REG_IOTG_HI    = 0x0B; /* IOTG[7:4] @ [3:0] */
constexpr uint8_t REG_VOTG_LO    = 0x0C; /* VOTG[1:0] @ [7:6] */
constexpr uint8_t REG_VOTG_HI    = 0x0D; /* VOTG[6:2] @ [4:0] */
constexpr uint8_t REG_IPRECHG_LO = 0x10;
constexpr uint8_t REG_IPRECHG_HI = 0x11;
constexpr uint8_t REG_ITERM_LO   = 0x12;
constexpr uint8_t REG_ITERM_HI   = 0x13;
constexpr uint8_t REG_CHG_CTRL0  = 0x14;
constexpr uint8_t REG_CHG_TMR    = 0x15;
constexpr uint8_t REG_CHG_CTRL1  = 0x16; /* EN_HIZ bit4, EN_CHG bit5, WDT[1:0] */
constexpr uint8_t REG_CHG_CTRL3  = 0x18; /* EN_OTG bit6, BATFET_DLY bit2 */
constexpr uint8_t REG_PART_INFO  = 0x38;
constexpr uint8_t REG_STATUS1    = 0x1E;
constexpr uint8_t REG_UNLOCK     = 0x70;

constexpr int I2C_TIMEOUT_MS = 100;
constexpr int OTG_ENTRY_DELAY_MS = 30;

i2c_master_dev_handle_t s_dev = nullptr;
SemaphoreHandle_t s_lock = nullptr;
bool s_ready = false;
TaskHandle_t s_vreg_task = nullptr;
uint32_t s_vreg_vol_mv = 0; // 截止电压目标（固定写入芯片，如 4350）
uint32_t s_vreg_now_mv = 0; // 当前写入芯片的 VREG

esp_err_t read_byte(uint8_t reg, uint8_t *val)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, val, 1, I2C_TIMEOUT_MS);
}

esp_err_t write_byte(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    return i2c_master_transmit(s_dev, buf, 2, I2C_TIMEOUT_MS);
}

esp_err_t update_bits(uint8_t reg, uint8_t mask, uint8_t shift, uint8_t field)
{
    uint8_t cur = 0;
    esp_err_t err = read_byte(reg, &cur);
    if (err != ESP_OK) {
        return err;
    }
    uint8_t m = static_cast<uint8_t>(mask << shift);
    cur = static_cast<uint8_t>((cur & static_cast<uint8_t>(~m)) |
                               ((field << shift) & m));
    return write_byte(reg, cur);
}

esp_err_t read_bits(uint8_t reg, uint8_t mask, uint8_t shift, uint8_t *field)
{
    uint8_t cur = 0;
    esp_err_t err = read_byte(reg, &cur);
    if (err != ESP_OK) {
        return err;
    }
    *field = static_cast<uint8_t>((cur >> shift) & mask);
    return ESP_OK;
}

esp_err_t set_vreg_hw_mv(uint32_t mv)
{
    if (!s_dev) {
        return ESP_ERR_INVALID_STATE;
    }
    if (mv < CX25601N_VREG_MIN_MV) {
        mv = CX25601N_VREG_MIN_MV;
    }
    if (mv > CX25601N_VREG_MAX_MV) {
        mv = CX25601N_VREG_MAX_MV;
    }
    uint32_t code = mv / CX25601N_VREG_STEP_MV;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    esp_err_t err = update_bits(REG_VREG_LO, 0x1F, 3, static_cast<uint8_t>(code & 0x1F));
    if (err == ESP_OK) {
        err = update_bits(REG_VREG_HI, 0x0F, 0, static_cast<uint8_t>((code >> 5) & 0x0F));
    }
    xSemaphoreGive(s_lock);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "set VREG=%lu mV (code=%lu)",
                 static_cast<unsigned long>(code * CX25601N_VREG_STEP_MV),
                 static_cast<unsigned long>(code));
    }
    return err;
}

/** ibat：battery_get_bat_current()/10，单位约 µA 标度（1mA→1000） */
int32_t ibat_to_ma(int32_t ibat_scaled)
{
    int32_t ma = ibat_scaled / 1000;
    return ma > 0 ? ma : 0;
}

void apply_vreg_fixed(uint32_t mv)
{
    if (mv < CX25601N_VREG_MIN_MV) {
        mv = CX25601N_VREG_MIN_MV;
    }
    if (mv > CX25601N_VREG_MAX_MV) {
        mv = CX25601N_VREG_MAX_MV;
    }
    if (mv == s_vreg_now_mv) {
        return;
    }
    if (set_vreg_hw_mv(mv) == ESP_OK) {
        s_vreg_now_mv = mv;
    }
}

void set_en_term(bool enable)
{
    // EN_TERM @ REG0x14 bit2（厂商 cx2560x.c）
    update_bits(REG_CHG_CTRL0, 0x01, 2, enable ? 1 : 0);
}

/** 低于截止电压且已终止时：关 EN_TERM、toggle EN_CHG 再开一轮 */
void kick_recharge(int32_t vbat, uint32_t vreg_target)
{
    ESP_LOGI(TAG, "recharge kick: vbat=%ldmV < VREG=%lumV",
             static_cast<long>(vbat), static_cast<unsigned long>(vreg_target));

    apply_vreg_fixed(vreg_target);

    xSemaphoreTake(s_lock, portMAX_DELAY);
    set_en_term(false);
    update_bits(REG_CHG_CTRL1, 0x01, 5, 0); /* EN_CHG off */
    update_bits(REG_CHG_CTRL1, 0x01, 4, 0); /* clear HIZ */
    xSemaphoreGive(s_lock);

    vTaskDelay(pdMS_TO_TICKS(200));

    xSemaphoreTake(s_lock, portMAX_DELAY);
    update_bits(REG_CHG_CTRL1, 0x01, 4, 0);
    update_bits(REG_CHG_CTRL1, 0x01, 5, 1); /* EN_CHG on */
    xSemaphoreGive(s_lock);

    vTaskDelay(pdMS_TO_TICKS(500));

    uint8_t stat = 0;
    const bool ok = (cx25601n_get_chrg_stat(&stat) == ESP_OK);
    ESP_LOGI(TAG, "recharge after: stat=%u (%s)",
             static_cast<unsigned>(stat),
             ok ? cx25601n_chrg_stat_str(stat) : "read fail");
}

/** 固定 VREG；仅负责 EN_TERM 与低于截止时的再充 kick */
void charger_vreg_task(void *arg)
{
    (void)arg;

    while (true) {
        const uint32_t vreg_target = s_vreg_vol_mv;
        const int32_t vbat_peek = battery_get_bat_voltage();
        const bool below_cutoff =
            (vreg_target >= CX25601N_VREG_MIN_MV && vreg_target <= CX25601N_VREG_MAX_MV &&
             vbat_peek >= 2500 && static_cast<uint32_t>(vbat_peek) < vreg_target);
        vTaskDelay(pdMS_TO_TICKS(below_cutoff ? 2000 : 10000));

        if (vreg_target < CX25601N_VREG_MIN_MV || vreg_target > CX25601N_VREG_MAX_MV) {
            continue;
        }

        apply_vreg_fixed(vreg_target);

        const int32_t vbat = battery_get_bat_voltage();
        const int32_t ima = ibat_to_ma(battery_get_bat_current() / 10);
        uint8_t stat = 0;
        const bool have_stat = (cx25601n_get_chrg_stat(&stat) == ESP_OK);
        const bool idle = have_stat && (stat == CX25601N_CHG_STAT_NOT);
        const bool vbat_below =
            (vbat >= 2500 && static_cast<uint32_t>(vbat) < vreg_target);

        ESP_LOGI(TAG, "vbat=%ldmV ima=%ldmA VREG=%lumV stat=%u",
                 static_cast<long>(vbat), static_cast<long>(ima),
                 static_cast<unsigned long>(vreg_target), static_cast<unsigned>(stat));

        // 未到截止：关 EN_TERM；到了才允许按 ITERM 停充
        xSemaphoreTake(s_lock, portMAX_DELAY);
        set_en_term(!vbat_below);
        xSemaphoreGive(s_lock);

        if (idle && ima < 5 && vbat_below) {
            kick_recharge(vbat, vreg_target);
        }
    }
}

void unlock_private(bool enable)
{
    if (!enable) {
        write_byte(REG_UNLOCK, 0x00);
        return;
    }
    for (int i = 0; i < 10; i++) {
        write_byte(REG_UNLOCK, 0x00);
        write_byte(REG_UNLOCK, 0x50);
        write_byte(REG_UNLOCK, 0x57);
        write_byte(REG_UNLOCK, 0x44);
        uint8_t v = 0;
        if (read_byte(REG_UNLOCK, &v) == ESP_OK && v == 0x03) {
            return;
        }
    }
    ESP_LOGW(TAG, "private register unlock failed");
}

esp_err_t set_dis_dpdm(bool disable)
{
    /* REG0x15 bit6 EN_AUTO_INDET: disable→0, enable→1（对齐 cx25601_set_dis_dpdm） */
    return update_bits(REG_CHG_TMR, 0x01, 6, disable ? 0 : 1);
}

esp_err_t set_iindpm_ma_nolock(uint32_t ma)
{
    if (ma < CX25601N_IINDPM_MIN_MA) {
        ma = CX25601N_IINDPM_MIN_MA;
    }
    if (ma > CX25601N_IINDPM_MAX_MA) {
        ma = CX25601N_IINDPM_MAX_MA;
    }
    uint32_t code = ma / CX25601N_IINDPM_STEP_MA;
    if (code < 5) {
        code = 5;
    }
    if (code > 150) {
        code = 150;
    }

    /* IINDPM[3:0] @ 0x06[7:4], IINDPM[7:4] @ 0x07[3:0] */
    esp_err_t err = update_bits(REG_IINDPM_LO, 0x0F, 4, static_cast<uint8_t>(code & 0x0F));
    if (err == ESP_OK) {
        err = update_bits(REG_IINDPM_HI, 0x0F, 0, static_cast<uint8_t>((code >> 4) & 0x0F));
    }
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "set IINDPM=%lu mA (code=%lu)",
                 static_cast<unsigned long>(code * CX25601N_IINDPM_STEP_MA),
                 static_cast<unsigned long>(code));
    }
    return err;
}

esp_err_t set_votg_mv_nolock(uint32_t mv)
{
    if (mv < CX25601N_VOTG_MIN_MV) {
        mv = CX25601N_VOTG_MIN_MV;
    }
    if (mv > CX25601N_VOTG_MAX_MV) {
        mv = CX25601N_VOTG_MAX_MV;
    }
    uint32_t code = mv / CX25601N_VOTG_STEP_MV;
    if (code < 0x30) {
        code = 0x30;
    }
    if (code > 0x42) {
        code = 0x42;
    }

    /* VOTG[1:0] @ 0x0C[7:6], VOTG[6:2] @ 0x0D[4:0] */
    esp_err_t err = update_bits(REG_VOTG_LO, 0x03, 6, static_cast<uint8_t>(code & 0x03));
    if (err == ESP_OK) {
        err = update_bits(REG_VOTG_HI, 0x1F, 0, static_cast<uint8_t>((code >> 2) & 0x1F));
    }
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "set VOTG=%lu mV (code=0x%02lX)",
                 static_cast<unsigned long>(code * CX25601N_VOTG_STEP_MV),
                 static_cast<unsigned long>(code));
    }
    return err;
}

esp_err_t set_iotg_ma_nolock(uint32_t ma)
{
    if (ma < CX25601N_IOTG_MIN_MA) {
        ma = CX25601N_IOTG_MIN_MA;
    }
    if (ma > CX25601N_IOTG_MAX_MA) {
        ma = CX25601N_IOTG_MAX_MA;
    }
    uint32_t code = ma / CX25601N_IOTG_STEP_MA;
    if (code < 5) {
        code = 5;
    }
    if (code > 60) {
        code = 60;
    }

    /* IOTG[3:0] @ 0x0A[7:4], IOTG[7:4] @ 0x0B[3:0] */
    esp_err_t err = update_bits(REG_IOTG_LO, 0x0F, 4, static_cast<uint8_t>(code & 0x0F));
    if (err == ESP_OK) {
        err = update_bits(REG_IOTG_HI, 0x0F, 0, static_cast<uint8_t>((code >> 4) & 0x0F));
    }
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "set IOTG=%lu mA (code=0x%02lX)",
                 static_cast<unsigned long>(code * CX25601N_IOTG_STEP_MA),
                 static_cast<unsigned long>(code));
    }
    return err;
}

esp_err_t hw_init_defaults(void)
{
    /* 上电关闭 D+/D- 自动识别，避免被判定为 SDP 后限流 500mA */
    ESP_RETURN_ON_ERROR(set_dis_dpdm(true), TAG, "dis_dpdm");

    /* Disable HIZ & watchdog; clear EN_CHG initially (UI will enable). */
    ESP_RETURN_ON_ERROR(update_bits(REG_CHG_CTRL1, 0x01, 4, 0), TAG, "HIZ");
    ESP_RETURN_ON_ERROR(update_bits(REG_CHG_CTRL1, 0x03, 0, 0), TAG, "WDT");

    /* IPRECHG = 12 * 20mA = 240mA */
    ESP_RETURN_ON_ERROR(update_bits(REG_IPRECHG_LO, 0x0F, 4, 0x0C), TAG, "iprechg lo");
    ESP_RETURN_ON_ERROR(update_bits(REG_IPRECHG_HI, 0x01, 0, 0x00), TAG, "iprechg hi");

    /* ITERM = 6 * 10mA = 60mA（原 180mA 过高：系统占流后 Ibat 易 < ITERM，刚开充就终止） */
    ESP_RETURN_ON_ERROR(update_bits(REG_ITERM_LO, 0x1F, 3, 0x06), TAG, "iterm lo");
    ESP_RETURN_ON_ERROR(update_bits(REG_ITERM_HI, 0x01, 0, 0x00), TAG, "iterm hi");

    /* 默认充电电压 VREG = 4350mV（恒压截止） */
    ESP_RETURN_ON_ERROR(cx25601n_set_vreg_mv(4350), TAG, "default vreg");

    /* 默认充电电流 ICHG = 500mA；写 ICHG 时同步更新 IINDPM(0x06/0x07) */
    ESP_RETURN_ON_ERROR(cx25601n_set_ichg_ma(500), TAG, "default ichg");

    /* Vendor private init sequence from reference */
    unlock_private(true);
    write_byte(0x86, 0x06);
    write_byte(0x3A, 0x10);
    write_byte(0x46, 0x20);
    unlock_private(false);

    update_bits(REG_CHG_CTRL0, 0x01, 0, 1);
    update_bits(REG_CHG_CTRL3, 0x01, 2, 0); /* BATFET_DLY=0；EN_OTG 保持默认关闭 */
    update_bits(0x1A, 0x01, 7, 1);
    update_bits(0x23, 0x07, 2, 0x07);
    update_bits(0x24, 0x01, 3, 1);

    /* 上电默认打开充电使能 */
    ESP_RETURN_ON_ERROR(update_bits(REG_CHG_CTRL1, 0x01, 5, 1), TAG, "EN_CHG");

    ESP_LOGI(TAG, "hw defaults applied");
    return ESP_OK;
}

}  // namespace

esp_err_t cx25601n_init(i2c_master_bus_handle_t bus)
{
    if (s_ready) {
        return ESP_OK;
    }
    if (!bus) {
        return ESP_ERR_INVALID_ARG;
    }

    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) {
        return ESP_ERR_NO_MEM;
    }

    i2c_device_config_t dev_cfg = {};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address = CX25601N_I2C_ADDR;
    dev_cfg.scl_speed_hz = 400000;  // Fast Mode（与手册上限一致）

    esp_err_t err = i2c_master_bus_add_device(bus, &dev_cfg, &s_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "add device failed: %s", esp_err_to_name(err));
        return err;
    }

    uint8_t part = 0;
    err = read_byte(REG_PART_INFO, &part);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "probe PART_INFO(0x38) failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "PART_INFO=0x%02X @0x%02X", part, CX25601N_I2C_ADDR);

    err = hw_init_defaults();
    if (err != ESP_OK) {
        return err;
    }

    s_ready = true;
    if (xTaskCreate(charger_vreg_task, "cx25601n_vreg", 3072, nullptr, 5, &s_vreg_task) != pdPASS) {
        s_ready = false;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

bool cx25601n_is_ready(void)
{
    return s_ready;
}

esp_err_t cx25601n_read_reg(uint8_t reg, uint8_t *val)
{
    if (!s_ready || !val) {
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    esp_err_t err = read_byte(reg, val);
    xSemaphoreGive(s_lock);
    return err;
}

esp_err_t cx25601n_enable_charge(bool enable)
{
    if (!s_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    esp_err_t err = ESP_OK;
    if (enable) {
        err = update_bits(REG_CHG_CTRL1, 0x01, 4, 0); /* clear HIZ */
        if (err == ESP_OK) {
            err = update_bits(REG_CHG_CTRL1, 0x01, 5, 1); /* EN_CHG */
        }
    } else {
        err = update_bits(REG_CHG_CTRL1, 0x01, 5, 0);
    }
    xSemaphoreGive(s_lock);
    ESP_LOGI(TAG, "charge %s", enable ? "ON" : "OFF");
    return err;
}

esp_err_t cx25601n_is_charge_enabled(bool *enabled)
{
    if (!s_ready || !enabled) {
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t bit = 0;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    esp_err_t err = read_bits(REG_CHG_CTRL1, 0x01, 5, &bit);
    xSemaphoreGive(s_lock);
    if (err == ESP_OK) {
        *enabled = bit != 0;
    }
    return err;
}

esp_err_t cx25601n_enable_otg(bool enable)
{
    if (!s_ready) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    esp_err_t err = ESP_OK;

    if (enable) {
        /* 对齐 cx2560x：先关充电，再配 5V/1A，最后开 EN_OTG */
        err = update_bits(REG_CHG_CTRL1, 0x01, 5, 0); /* EN_CHG=0 */
        if (err == ESP_OK) {
            err = update_bits(REG_CHG_CTRL1, 0x01, 4, 0); /* EN_HIZ=0 */
        }
        if (err == ESP_OK) {
            err = set_votg_mv_nolock(CX25601N_OTG_DEFAULT_MV);
        }
        if (err == ESP_OK) {
            err = set_iotg_ma_nolock(CX25601N_OTG_DEFAULT_MA);
        }
        if (err == ESP_OK) {
            err = update_bits(REG_CHG_CTRL3, 0x01, 6, 1); /* EN_OTG=1 */
        }
        xSemaphoreGive(s_lock);

        if (err == ESP_OK) {
            /* 手册：EN_OTG=1 后至少 30ms 才进入 Boost */
            vTaskDelay(pdMS_TO_TICKS(OTG_ENTRY_DELAY_MS));
            ESP_LOGI(TAG, "OTG ON (target %dmV/%dmA)",
                     CX25601N_OTG_DEFAULT_MV, CX25601N_OTG_DEFAULT_MA);
        } else {
            ESP_LOGE(TAG, "OTG enable failed: %s", esp_err_to_name(err));
        }
        return err;
    }

    err = update_bits(REG_CHG_CTRL3, 0x01, 6, 0); /* EN_OTG=0 */
    if (err == ESP_OK) {
        err = update_bits(REG_CHG_CTRL1, 0x01, 5, 1); /* 恢复充电 */
    }
    xSemaphoreGive(s_lock);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "OTG OFF (charge restored)");
    } else {
        ESP_LOGE(TAG, "OTG disable failed: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t cx25601n_is_otg_enabled(bool *enabled)
{
    if (!s_ready || !enabled) {
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t bit = 0;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    esp_err_t err = read_bits(REG_CHG_CTRL3, 0x01, 6, &bit);
    xSemaphoreGive(s_lock);
    if (err == ESP_OK) {
        *enabled = bit != 0;
    }
    return err;
}

esp_err_t cx25601n_set_iindpm_ma(uint32_t ma)
{
    if (!s_dev) {
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    esp_err_t err = set_iindpm_ma_nolock(ma);
    xSemaphoreGive(s_lock);
    return err;
}

esp_err_t cx25601n_get_iindpm_ma(uint32_t *ma)
{
    if (!s_ready || !ma) {
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t lo = 0, hi = 0;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    esp_err_t err = read_bits(REG_IINDPM_LO, 0x0F, 4, &lo);
    if (err == ESP_OK) {
        err = read_bits(REG_IINDPM_HI, 0x0F, 0, &hi);
    }
    xSemaphoreGive(s_lock);
    if (err != ESP_OK) {
        return err;
    }
    uint32_t code = static_cast<uint32_t>(lo) | (static_cast<uint32_t>(hi) << 4);
    *ma = code * CX25601N_IINDPM_STEP_MA;
    return ESP_OK;
}

esp_err_t cx25601n_set_ichg_ma(uint32_t ma)
{
    if (!s_dev) {
        return ESP_ERR_INVALID_STATE;
    }
    if (ma < CX25601N_ICHG_MIN_MA) {
        ma = CX25601N_ICHG_MIN_MA;
    }
    if (ma > CX25601N_ICHG_MAX_MA) {
        ma = CX25601N_ICHG_MAX_MA;
    }
    uint32_t code = ma / CX25601N_ICHG_STEP_MA;
    if (code < 1) {
        code = 1;
    }
    if (code > 38) {
        code = 38;
    }
    uint32_t applied_ma = code * CX25601N_ICHG_STEP_MA;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    /* ICHG[1:0] @ 0x02[7:6], ICHG[5:2] @ 0x03[3:0] */
    esp_err_t err = update_bits(REG_ICHG_LO, 0x03, 6, static_cast<uint8_t>(code & 0x03));
    if (err == ESP_OK) {
        err = update_bits(REG_ICHG_HI, 0x0F, 0, static_cast<uint8_t>((code >> 2) & 0x0F));
    }
    /* 应用电流时同步写输入限流 IINDPM（0x06/0x07），避免被 SDP 等默认限流卡住 */
    if (err == ESP_OK) {
        err = set_iindpm_ma_nolock(applied_ma);
    }
    xSemaphoreGive(s_lock);

    ESP_LOGI(TAG, "set ICHG=%lu mA (code=%lu)", static_cast<unsigned long>(applied_ma),
             static_cast<unsigned long>(code));
    return err;
}

esp_err_t cx25601n_get_ichg_ma(uint32_t *ma)
{
    if (!s_ready || !ma) {
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t lo = 0, hi = 0;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    esp_err_t err = read_bits(REG_ICHG_LO, 0x03, 6, &lo);
    if (err == ESP_OK) {
        err = read_bits(REG_ICHG_HI, 0x0F, 0, &hi);
    }
    xSemaphoreGive(s_lock);
    if (err != ESP_OK) {
        return err;
    }
    uint32_t code = static_cast<uint32_t>(lo) | (static_cast<uint32_t>(hi) << 2);
    *ma = code * CX25601N_ICHG_STEP_MA;
    return ESP_OK;
}

esp_err_t cx25601n_set_vreg_mv(uint32_t mv)
{
    if (!s_dev) {
        return ESP_ERR_INVALID_STATE;
    }
    if (mv < CX25601N_VREG_MIN_MV) {
        mv = CX25601N_VREG_MIN_MV;
    }
    if (mv > CX25601N_VREG_MAX_MV) {
        mv = CX25601N_VREG_MAX_MV;
    }

    if (s_vreg_vol_mv == mv && s_vreg_now_mv == mv) {
        return ESP_OK;
    }

    esp_err_t err = set_vreg_hw_mv(mv);
    if (err == ESP_OK) {
        s_vreg_vol_mv = mv;
        s_vreg_now_mv = mv;
    }
    return err;
}

esp_err_t cx25601n_get_vreg_mv(uint32_t *mv)
{
    if (!s_ready || !mv) {
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t lo = 0, hi = 0;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    esp_err_t err = read_bits(REG_VREG_LO, 0x1F, 3, &lo);
    if (err == ESP_OK) {
        err = read_bits(REG_VREG_HI, 0x0F, 0, &hi);
    }
    xSemaphoreGive(s_lock);
    if (err != ESP_OK) {
        return err;
    }
    uint32_t code = static_cast<uint32_t>(lo) | (static_cast<uint32_t>(hi) << 5);
    *mv = code * CX25601N_VREG_STEP_MV;
    return ESP_OK;
}

esp_err_t cx25601n_get_chrg_stat(uint8_t *stat)
{
    if (!s_ready || !stat) {
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    esp_err_t err = read_bits(REG_STATUS1, 0x03, 3, stat);
    xSemaphoreGive(s_lock);
    return err;
}

esp_err_t cx25601n_get_vbus_stat(uint8_t *stat)
{
    if (!s_ready || !stat) {
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    esp_err_t err = read_bits(REG_STATUS1, 0x07, 0, stat);
    xSemaphoreGive(s_lock);
    return err;
}

const char *cx25601n_chrg_stat_str(uint8_t stat)
{
    switch (stat) {
    case CX25601N_CHG_STAT_NOT:
        return "未充电/已满";
    case CX25601N_CHG_STAT_CC:
        return "涓流/预充/CC";
    case CX25601N_CHG_STAT_CV:
        return "恒压降流";
    case CX25601N_CHG_STAT_TOPOFF:
        return "Top-off";
    default:
        return "未知";
    }
}

const char *cx25601n_vbus_stat_str(uint8_t stat)
{
    switch (stat) {
    case 0:
        return "无输入";
    case 1:
        return "USB SDP";
    case 2:
        return "USB CDP";
    case 3:
        return "USB DCP";
    case 4:
        return "未知适配器";
    case 5:
        return "非标适配器";
    case 7:
        return "OTG";
    default:
        return "适配器";
    }
}
