#ifndef SD_CARD_MANAGER_HPP
#define SD_CARD_MANAGER_HPP

#include <dirent.h>
#include <sys/stat.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <driver/sdmmc_host.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>

#include "config.h"  // SDMMC_*_PIN（板级提供）
#include "sd_paths.h"

// SdCardManager：板级 SDMMC 单例，适用于 ESP32-S3 1-bit 模式。
// 初始化配置与参考工程保持一致：SDMMC_HOST_DEFAULT + SLOT_CONFIG_DEFAULT，CLK/CMD/D0，width=1。
// 挂载点为 /sdcard，应用数据统一放在 /sdcard/metalio/e-ink/…。
class SdCardManager {
public:
    static constexpr const char* kMountPoint = SD_MOUNT_POINT;

    static SdCardManager& GetInstance() {
        static SdCardManager instance;
        return instance;
    }

    SdCardManager(const SdCardManager&) = delete;
    SdCardManager& operator=(const SdCardManager&) = delete;

    bool Mount() {
        if (mounted_) {
            ESP_LOGD(kTag, "SD card already mounted");
            return true;
        }
        ReleaseRawCard();

        // 与 esp32-s3-sdcard 测试工程一致的主机 / 槽位 / 挂载配置
        sdmmc_host_t host = SDMMC_HOST_DEFAULT();
        sdmmc_slot_config_t slot_config = MakeSlotConfig();

        esp_vfs_fat_sdmmc_mount_config_t mount_config = {
            .format_if_mount_failed = false,
            .max_files = 12,
            .allocation_unit_size = 16 * 1024,
        };

        ESP_LOGI(kTag, "正在挂载 SD 卡 (SDMMC 1-bit CLK=%d CMD=%d D0=%d)...",
                 SDMMC_CLK_PIN, SDMMC_CMD_PIN, SDMMC_D0_PIN);
        esp_err_t ret =
            esp_vfs_fat_sdmmc_mount(kMountPoint, &host, &slot_config, &mount_config, &card_);
        if (ret != ESP_OK) {
            if (ret == ESP_FAIL) {
                ESP_LOGE(kTag,
                         "SD卡挂载失败，请检查：1) 接线是否正确 2) 是否接有上拉电阻 "
                         "3) SD卡格式是否为 FAT32");
            } else {
                ESP_LOGE(kTag, "SD卡初始化错误: %s", esp_err_to_name(ret));
            }
            card_ = nullptr;
            return false;
        }

        raw_owned_ = false;
        mounted_ = true;
        sdmmc_card_print_info(stdout, card_);
        ESP_LOGI(kTag, "SD卡挂载成功!");
        if (!SdEnsureAppLayout()) {
            ESP_LOGW(kTag, "app layout under %s incomplete", SD_APP_ROOT);
        }
        LogRootListing();
        return true;
    }

    void Unmount() {
        if (!mounted_ || card_ == nullptr) {
            ReleaseRawCard();
            return;
        }
        ESP_LOGI(kTag, "Unmounting SD card ...");
        esp_err_t err = esp_vfs_fat_sdcard_unmount(kMountPoint, card_);
        if (err != ESP_OK) {
            ESP_LOGW(kTag, "sdcard_unmount: %s", esp_err_to_name(err));
        }
        card_ = nullptr;
        mounted_ = false;
        raw_owned_ = false;
        ESP_LOGI(kTag, "SD card unmounted");
    }

    // 卸掉 VFS（若有），重新 sdmmc_card_init，供后续 MSC 等 raw 访问。
    sdmmc_card_t* InitRawCardForMsc() {
        if (mounted_) {
            Unmount();
        } else {
            ReleaseRawCard();
        }

        sdmmc_host_t host = SDMMC_HOST_DEFAULT();
        sdmmc_slot_config_t slot_config = MakeSlotConfig();

        sdmmc_card_t* card = static_cast<sdmmc_card_t*>(calloc(1, sizeof(sdmmc_card_t)));
        if (card == nullptr) {
            ESP_LOGE(kTag, "calloc sdmmc_card_t failed");
            return nullptr;
        }

        esp_err_t ret = (*host.init)();
        if (ret == ESP_ERR_INVALID_STATE) {
            ESP_LOGW(kTag, "host already inited, force deinit then retry");
            CallHostDeinit(&host);
            ret = (*host.init)();
        }
        if (ret != ESP_OK) {
            ESP_LOGE(kTag, "SDMMC host init failed: %s", esp_err_to_name(ret));
            free(card);
            return nullptr;
        }

        ret = sdmmc_host_init_slot(host.slot, &slot_config);
        if (ret != ESP_OK) {
            ESP_LOGE(kTag, "SDMMC slot init failed: %s", esp_err_to_name(ret));
            CallHostDeinit(&host);
            free(card);
            return nullptr;
        }

        ret = sdmmc_card_init(&host, card);
        if (ret != ESP_OK) {
            ESP_LOGE(kTag, "sdmmc_card_init failed: %s", esp_err_to_name(ret));
            CallHostDeinit(&host);
            free(card);
            return nullptr;
        }

        ret = sdmmc_get_status(card);
        if (ret != ESP_OK) {
            ESP_LOGE(kTag, "sdmmc_get_status failed: %s", esp_err_to_name(ret));
            CallHostDeinit(&card->host);
            free(card);
            return nullptr;
        }

        card_ = card;
        raw_owned_ = true;
        mounted_ = false;
        ESP_LOGI(kTag, "Raw SDMMC card ready for MSC");
        return card_;
    }

    void ReleaseRawCard() {
        if (!raw_owned_ || card_ == nullptr) {
            if (!mounted_) {
                card_ = nullptr;
            }
            raw_owned_ = false;
            return;
        }
        ESP_LOGI(kTag, "Releasing raw SDMMC card");
        CallHostDeinit(&card_->host);
        free(card_);
        card_ = nullptr;
        raw_owned_ = false;
        mounted_ = false;
    }

    void NotifyExternalAppMount(sdmmc_card_t* card) {
        card_ = card;
        mounted_ = (card != nullptr);
    }

    void NotifyExportedToHost() {
        mounted_ = false;
    }

    bool RemountVfsFromCard() {
        if (mounted_ && !raw_owned_) {
            return true;
        }
        ReleaseRawCard();
        return Mount();
    }

    bool IsMounted() const { return mounted_; }

    sdmmc_card_t* GetCard() const { return card_; }

    bool HasCard() const { return card_ != nullptr; }

    const char* GetMountPoint() const { return kMountPoint; }

private:
    SdCardManager() = default;

    static constexpr const char* kTag = "SdCardManager";

    static void CallHostDeinit(const sdmmc_host_t* host) {
        if (host == nullptr) {
            return;
        }
        if (host->flags & SDMMC_HOST_FLAG_DEINIT_ARG) {
            if (host->deinit_p) {
                host->deinit_p(host->slot);
            }
        } else if (host->deinit) {
            host->deinit();
        }
    }

    // 对齐 esp32-s3-sdcard：仅 CLK/CMD/D0，d1~d3=NC，1-bit，内部上拉，不用 CD
    static sdmmc_slot_config_t MakeSlotConfig() {
        sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
        slot_config.clk = SDMMC_CLK_PIN;
        slot_config.cmd = SDMMC_CMD_PIN;
        slot_config.d0 = SDMMC_D0_PIN;
        slot_config.d1 = GPIO_NUM_NC;
        slot_config.d2 = GPIO_NUM_NC;
        slot_config.d3 = GPIO_NUM_NC;
        slot_config.width = 1;
        slot_config.flags = SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
        return slot_config;
    }

    void LogRootListing() {
        DIR* dir = opendir(kMountPoint);
        if (dir == nullptr) {
            ESP_LOGW(kTag, "opendir(%s) failed, skip listing", kMountPoint);
            return;
        }

        struct Entry {
            std::string name;
            bool is_dir;
        };
        std::vector<Entry> entries;
        entries.reserve(16);

        struct dirent* ent;
        while ((ent = readdir(dir)) != nullptr) {
            if (ent->d_name[0] == '.' &&
                (ent->d_name[1] == '\0' ||
                 (ent->d_name[1] == '.' && ent->d_name[2] == '\0'))) {
                continue;
            }
            entries.push_back({std::string(ent->d_name), ent->d_type == DT_DIR});
        }
        closedir(dir);

        ESP_LOGI(kTag, "Root directory of %s:", kMountPoint);
        for (const auto& e : entries) {
            if (e.is_dir) {
                ESP_LOGI(kTag, "  [DIR]  %s", e.name.c_str());
                continue;
            }
            char full_path[300];
            snprintf(full_path, sizeof(full_path), "%s/%s", kMountPoint, e.name.c_str());
            struct stat st;
            if (stat(full_path, &st) == 0) {
                ESP_LOGI(kTag, "  %10lu  %s", (unsigned long)st.st_size, e.name.c_str());
            } else {
                ESP_LOGI(kTag, "  [???]  %s", e.name.c_str());
            }
        }
        ESP_LOGI(kTag, "Total %u entries", (unsigned)entries.size());
    }

    sdmmc_card_t* card_ = nullptr;
    bool mounted_ = false;
    bool raw_owned_ = false;
};

#endif  // SD_CARD_MANAGER_HPP
