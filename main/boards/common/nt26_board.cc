#include "nt26_board.h"
#include "display.h"
#include "application.h"
#include "audio_codec.h"
#include "assets/lang_config.h"
#include "power_hw.h"
#include "board.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <font_awesome.h>
#include <cJSON.h>

#define TAG "Nt26Board"

namespace {

constexpr uint32_t kWaitNetworkConnected = (1 << 0);
constexpr uint32_t kWaitNetworkFailed = (1 << 1);
constexpr uint32_t kWaitNetworkInFlight = (1 << 2);
constexpr uint32_t kWaitNetworkAll =
    kWaitNetworkConnected | kWaitNetworkFailed | kWaitNetworkInFlight;
constexpr uint32_t kNetworkWaitTimeoutMs = 60 * 1000;
/** MAIN_PWR 上升沿后模组冷启动 settle（再 AT） */
constexpr uint32_t kMainRailCellularSettleMs = 1000;
constexpr uint32_t kMainRailCellularRecoverStack = 6144;
constexpr int kRecoverWaitMs = 45000;

}  // namespace

Nt26Board::Nt26Board(gpio_num_t tx_pin, gpio_num_t rx_pin, gpio_num_t dtr_pin, gpio_num_t ri_pin, gpio_num_t reset_pin)
    : tx_pin_(tx_pin), rx_pin_(rx_pin), dtr_pin_(dtr_pin), ri_pin_(ri_pin), reset_pin_(reset_pin) {

    // 勿用 ESP_INTR_FLAG_IRAM：OTA/NVS 写 Flash 时 Cache 关闭，若 GPIO ISR 仍跑
    // 会踩 Flash 映射区（Cache error）。无 IRAM 标志时驱动会在 Cache off 窗口屏蔽本 ISR。
    gpio_install_isr_service(0);
    esp_event_loop_create_default();
    esp_netif_init();
    
    esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "nt26_cpu", &pm_lock_cpu_max_);
    
    esp_timer_create_args_t timer_args = {
        .callback = OnNetworkReadyTimeout,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "nt26_net_timer",
        .skip_unhandled_events = true
    };
    esp_timer_create(&timer_args, &network_ready_timer_);
}

Nt26Board::~Nt26Board() {
    TearDownModem();

    if (network_ready_timer_) {
        esp_timer_stop(network_ready_timer_);
        esp_timer_delete(network_ready_timer_);
        network_ready_timer_ = nullptr;
    }
    
    if (pm_lock_cpu_max_) {
        esp_pm_lock_delete(pm_lock_cpu_max_);
        pm_lock_cpu_max_ = nullptr;
    }
}

std::string Nt26Board::GetBoardType() {
    return "nt26";
}

void Nt26Board::OnNetworkEvent(NetworkEvent event, const std::string& data) {
    if (event == NetworkEvent::Connected) {
        network_ready_.store(true, std::memory_order_release);
    } else if (event == NetworkEvent::Disconnected ||
               event == NetworkEvent::ModemErrorTimeout ||
               event == NetworkEvent::ModemErrorNoSim ||
               event == NetworkEvent::ModemErrorRegDenied ||
               event == NetworkEvent::ModemErrorInitFailed) {
        network_ready_.store(false, std::memory_order_release);
    }
    if (network_event_callback_) {
        network_event_callback_(event, data);
    }
}

void Nt26Board::SignalWaitBits(uint32_t bits) {
    // 勿在 modem 任务里抢 lifecycle_mu_：TearDown 可能正持锁外等 Stop 汇合
    EventGroupHandle_t eg = nullptr;
    {
        std::lock_guard<std::mutex> lock(lifecycle_mu_);
        eg = network_wait_event_;
    }
    if (eg != nullptr) {
        xEventGroupSetBits(eg, bits);
    }
}

void Nt26Board::OnNetworkReadyTimeout(void* arg) {
    auto* self = static_cast<Nt26Board*>(arg);
    ESP_LOGW(TAG, "Network ready timeout");
    if (self->network_ready_timer_) {
        esp_timer_stop(self->network_ready_timer_);
    }
    self->SignalWaitBits(kWaitNetworkFailed);
    self->OnNetworkEvent(NetworkEvent::ModemErrorTimeout, "网络连接超时");
}

void Nt26Board::TearDownModem() {
    std::shared_ptr<UartEthModem> doomed;
    {
        std::lock_guard<std::mutex> lock(lifecycle_mu_);
        network_ready_.store(false, std::memory_order_release);
        lifecycle_epoch_.fetch_add(1, std::memory_order_acq_rel);

        if (network_ready_timer_) {
            esp_timer_stop(network_ready_timer_);
        }
        // 唤醒 WaitBits；event group 仍由等待方释放，避免 UAF
        if (network_wait_event_) {
            xEventGroupSetBits(network_wait_event_, kWaitNetworkFailed);
        }
        doomed = std::move(modem_);
    }
    // Stop 在锁外：汇合 modem 任务时回调只 SignalWaitBits，不会与本锁死锁
    if (doomed) {
        ESP_LOGI(TAG, "TearDown modem (MAIN_PWR / restart)");
        doomed->Stop();
    }
}

void Nt26Board::PrepareMainRailOff() {
    ESP_LOGI(TAG, "PrepareMainRailOff: stop cellular before MAIN_PWR down");
    TearDownModem();
    // 立即刷信号图标，避免状态栏仍显示旧 CSQ 直到下一秒 clock
    if (auto* display = Board::GetInstance().GetDisplay()) {
        display->UpdateStatusBar(true);
    }
}

bool Nt26Board::StartOrRestartModem(int timeout_ms, bool wait_for_ready) {
    // 整段单飞：EnsureNetworkReady 与 MAIN_PWR 上升沿恢复不得并行 TearDown 彼此
    std::lock_guard<std::mutex> start_lock(start_mu_);

    if (IsNetworkReady()) {
        return true;
    }

    TearDownModem();

    EventGroupHandle_t wait_eg = nullptr;
    uint32_t my_epoch = 0;
    std::shared_ptr<UartEthModem> started;

    {
        std::lock_guard<std::mutex> lock(lifecycle_mu_);

        if (network_ready_.load(std::memory_order_acquire) && modem_ &&
            modem_->IsInitialized()) {
            return true;
        }

        my_epoch = lifecycle_epoch_.load(std::memory_order_acquire);
        OnNetworkEvent(NetworkEvent::ModemDetecting);

        wait_eg = xEventGroupCreate();
        if (!wait_eg) {
            ESP_LOGE(TAG, "Failed to create network wait event group");
            OnNetworkEvent(NetworkEvent::ModemErrorInitFailed);
            return false;
        }
        network_wait_event_ = wait_eg;

        UartEthModem::Config config = {
            .uart_num = UART_NUM_1,
            .baud_rate = 2000000,
            .tx_pin = tx_pin_,
            .rx_pin = rx_pin_,
            .mrdy_pin = dtr_pin_,
            .srdy_pin = ri_pin_
        };

        modem_ = std::make_shared<UartEthModem>(config);
        modem_->SetDebug(false);
        modem_->SetNetworkEventCallback([this](UartEthModem::UartEthModemEvent event,
                                                const std::string& detail) {
            if (detail.empty()) {
                ESP_LOGI(TAG, "Modem event: %s", UartEthModem::GetNetworkEventName(event));
            } else {
                ESP_LOGI(TAG, "Modem event: %s (%s)",
                         UartEthModem::GetNetworkEventName(event), detail.c_str());
            }
            switch (event) {
                case UartEthModem::UartEthModemEvent::Connected:
                    if (network_ready_timer_) {
                        esp_timer_stop(network_ready_timer_);
                    }
                    SignalWaitBits(kWaitNetworkConnected);
                    OnNetworkEvent(NetworkEvent::Connected);
                    break;
                case UartEthModem::UartEthModemEvent::Disconnected:
                    OnNetworkEvent(NetworkEvent::Disconnected);
                    break;
                case UartEthModem::UartEthModemEvent::ErrorNoSim:
                    if (network_ready_timer_) {
                        esp_timer_stop(network_ready_timer_);
                    }
                    SignalWaitBits(kWaitNetworkFailed);
                    OnNetworkEvent(NetworkEvent::ModemErrorNoSim);
                    break;
                case UartEthModem::UartEthModemEvent::ErrorRegistrationDenied:
                    if (network_ready_timer_) {
                        esp_timer_stop(network_ready_timer_);
                    }
                    SignalWaitBits(kWaitNetworkFailed);
                    OnNetworkEvent(NetworkEvent::ModemErrorRegDenied);
                    break;
                case UartEthModem::UartEthModemEvent::Connecting:
                    OnNetworkEvent(NetworkEvent::Connecting);
                    break;
                case UartEthModem::UartEthModemEvent::ErrorInitFailed:
                case UartEthModem::UartEthModemEvent::ErrorNoCarrier:
                    if (network_ready_timer_) {
                        esp_timer_stop(network_ready_timer_);
                    }
                    SignalWaitBits(kWaitNetworkFailed);
                    OnNetworkEvent(NetworkEvent::ModemErrorInitFailed, detail);
                    break;
                case UartEthModem::UartEthModemEvent::InFlightMode:
                    ESP_LOGW(TAG, "Modem in flight mode");
                    SignalWaitBits(kWaitNetworkInFlight);
                    break;
                case UartEthModem::UartEthModemEvent::RfTestReady:
                    ESP_LOGI(TAG, "Modem RF test mode ready");
                    break;
                case UartEthModem::UartEthModemEvent::RequestingPdpContext:
                    break;
            }
        });
        started = modem_;
    }

    if (started->Start() != ESP_OK) {
        {
            std::lock_guard<std::mutex> lock(lifecycle_mu_);
            if (network_wait_event_ == wait_eg) {
                network_wait_event_ = nullptr;
            }
            if (modem_ == started) {
                modem_.reset();
            }
        }
        vEventGroupDelete(wait_eg);
        OnNetworkEvent(NetworkEvent::ModemErrorInitFailed);
        return false;
    }

    if (network_ready_timer_ && wait_for_ready && timeout_ms > 0) {
        const int timer_ms = timeout_ms > 30000 ? 30000 : timeout_ms;
        esp_timer_start_once(network_ready_timer_,
                             static_cast<uint64_t>(timer_ms) * 1000ULL);
    }

    OnNetworkEvent(NetworkEvent::Connecting);
    if (auto* display = Board::GetInstance().GetDisplay()) {
        display->SetStatus(Lang::Strings::REGISTERING_NETWORK);
    }

    if (!wait_for_ready) {
        return true;
    }

    ESP_LOGI(TAG, "Waiting for network ready (timeout=%d ms)...", timeout_ms);
    const int wait_ms = timeout_ms > 0 ? timeout_ms : static_cast<int>(kNetworkWaitTimeoutMs);
    EventBits_t bits = xEventGroupWaitBits(wait_eg, kWaitNetworkAll, pdFALSE, pdFALSE,
                                           pdMS_TO_TICKS(wait_ms));

    {
        std::lock_guard<std::mutex> lock(lifecycle_mu_);
        if (network_wait_event_ == wait_eg) {
            network_wait_event_ = nullptr;
        }
        if (network_ready_timer_) {
            esp_timer_stop(network_ready_timer_);
        }
    }
    vEventGroupDelete(wait_eg);

    if (my_epoch != lifecycle_epoch_.load(std::memory_order_acquire)) {
        ESP_LOGW(TAG, "StartOrRestart aborted (MAIN_PWR / newer lifecycle)");
        return false;
    }

    if (bits & kWaitNetworkConnected) {
        ESP_LOGI(TAG, "Network ready");
        return true;
    }
    if (bits & kWaitNetworkInFlight) {
        ESP_LOGW(TAG, "Network unavailable (flight mode / no SIM)");
        return false;
    }

    ESP_LOGW(TAG, "Network wait failed or timed out (bits=0x%lx)", (unsigned long)bits);
    return false;
}

void Nt26Board::StartNetwork() {
    StartOrRestartModem(static_cast<int>(kNetworkWaitTimeoutMs), true);
}

bool Nt26Board::IsNetworkReady() {
    // 仅看 network_ready_：TearDown 先清标志再 reset modem_，避免无锁读 modem_ UAF
    return network_ready_.load(std::memory_order_acquire);
}

bool Nt26Board::EnsureNetworkReady(int timeout_ms) {
    const char* task = pcTaskGetName(nullptr);
    const UBaseType_t stack_before = uxTaskGetStackHighWaterMark(nullptr);
    ESP_LOGI(TAG, "EnsureNetworkReady: enter task=%s core=%d timeout_ms=%d stack_hwm=%u ready=%d",
             task != nullptr ? task : "?", xPortGetCoreID(), timeout_ms,
             static_cast<unsigned>(stack_before), IsNetworkReady() ? 1 : 0);
    if (IsNetworkReady()) {
        return true;
    }
    // 与 WiFi 路径同序：先保证 MAIN_PWR，再启模组
    (void)power_hw_main_rail_set(true);
    if (IsNetworkReady()) {
        return true;
    }
    ESP_LOGI(TAG, "EnsureNetworkReady: restart cellular, wait %d ms", timeout_ms);
    const bool ok = StartOrRestartModem(timeout_ms, true);
    ESP_LOGI(TAG, "EnsureNetworkReady: leave ok=%d task=%s stack_hwm=%u->%u", ok ? 1 : 0,
             task != nullptr ? task : "?", static_cast<unsigned>(stack_before),
             static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
    return ok;
}

void Nt26Board::RefreshNetworkWaitDeadline(int timeout_ms) {
    // 蜂窝等网已在 StartOrRestartModem 用 start_mu_ 单飞；暂不延长模组等待窗
    (void)timeout_ms;
}

void Nt26Board::PauseNetworkIfNotReady() {
    // 蜂窝无 STA Pause；失败路径由模组 TearDown/空闲策略处理
}

void Nt26Board::MainRailCellularRecoverTask(void* arg) {
    auto* self = static_cast<Nt26Board*>(arg);
    vTaskDelay(pdMS_TO_TICKS(kMainRailCellularSettleMs));

    if (!power_hw_main_rail_is_on()) {
        ESP_LOGI(TAG, "MAIN_PWR cellular recover skipped (rail off again)");
        self->recover_busy_.store(false, std::memory_order_release);
        vTaskDelete(nullptr);
        return;
    }

    if (self->IsNetworkReady()) {
        ESP_LOGI(TAG, "MAIN_PWR cellular recover skipped (already ready)");
        self->recover_busy_.store(false, std::memory_order_release);
        vTaskDelete(nullptr);
        return;
    }

    ESP_LOGI(TAG, "MAIN_PWR rising: re-init 4G modem");
    const bool ok = self->EnsureNetworkReady(kRecoverWaitMs);
    ESP_LOGI(TAG, "MAIN_PWR cellular recover %s", ok ? "ok" : "failed");

    if (auto* display = Board::GetInstance().GetDisplay()) {
        display->UpdateStatusBar(true);
    }

    self->recover_busy_.store(false, std::memory_order_release);
    vTaskDelete(nullptr);
}

void Nt26Board::RecoverAfterMainRailOnAsync() {
    bool expected = false;
    if (!recover_busy_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        ESP_LOGW(TAG, "MAIN_PWR cellular recover already pending");
        return;
    }
    if (IsNetworkReady()) {
        recover_busy_.store(false, std::memory_order_release);
        return;
    }
    if (xTaskCreate(&Nt26Board::MainRailCellularRecoverTask, "main_rail_cell",
                    kMainRailCellularRecoverStack, this, 5, nullptr) != pdPASS) {
        recover_busy_.store(false, std::memory_order_release);
        ESP_LOGW(TAG, "MAIN_PWR cellular recover task create failed");
    }
}

void Nt26Board::ScheduleAsyncStop() {
    Application::GetInstance().Schedule([this]() { TearDownModem(); });
}

void Nt26Board::SetNetworkEventCallback(NetworkEventCallback callback) {
    network_event_callback_ = std::move(callback);
}

NetworkInterface* Nt26Board::GetNetwork() {
    static EspNetwork network;
    return &network;
}

const char* Nt26Board::GetNetworkStateIcon() {
    // 短持锁：与 TearDown 并发时避免读已 reset 的 modem_
    bool inited = false;
    int csq = 99;
    {
        std::lock_guard<std::mutex> lock(lifecycle_mu_);
        if (modem_ != nullptr && modem_->IsInitialized()) {
            inited = true;
            csq = modem_->GetSignalStrength();
        }
    }
    if (!inited) {
        return FONT_AWESOME_SIGNAL_OFF;
    }
    if (csq == 99 || csq == -1) {
        return FONT_AWESOME_SIGNAL_OFF;
    } else if (csq >= 0 && csq <= 9) {
        return FONT_AWESOME_SIGNAL_WEAK;
    } else if (csq >= 10 && csq <= 14) {
        return FONT_AWESOME_SIGNAL_FAIR;
    } else if (csq >= 15 && csq <= 19) {
        return FONT_AWESOME_SIGNAL_GOOD;
    } else if (csq >= 20 && csq <= 31) {
        return FONT_AWESOME_SIGNAL_STRONG;
    }
    return FONT_AWESOME_SIGNAL_OFF;
}

void Nt26Board::SetPowerSaveMode(bool enabled) {
    (void)enabled;
}

std::string Nt26Board::GetBoardJson() {
    std::shared_ptr<UartEthModem> modem;
    {
        std::lock_guard<std::mutex> lock(lifecycle_mu_);
        modem = modem_;
    }
    std::string board_json = std::string("{\"type\":\"" BOARD_TYPE "\",");
    board_json += "\"name\":\"" BOARD_NAME "\",";
    if (modem) {
        board_json += "\"revision\":\"" + modem->GetModuleRevision() + "\",";
        board_json += "\"carrier\":\"" + modem->GetCarrierName() + "\",";
        board_json += "\"csq\":\"" + std::to_string(modem->GetSignalStrength()) + "\",";
        board_json += "\"imei\":\"" + modem->GetImei() + "\",";
        board_json += "\"iccid\":\"" + modem->GetIccid() + "\",";
        board_json += "\"cereg\":" + GetRegistrationState().ToString() + "}";
    } else {
        board_json += "\"status\":\"offline\"}";
    }
    return board_json;
}

Nt26CeregState Nt26Board::GetRegistrationState() {
    Nt26CeregState state;
    std::shared_ptr<UartEthModem> modem;
    {
        std::lock_guard<std::mutex> lock(lifecycle_mu_);
        modem = modem_;
    }
    if (modem) {
        auto cell_info = modem->GetCellInfo();
        state.stat = cell_info.stat;
        state.tac = cell_info.tac;
        state.ci = cell_info.ci;
        state.AcT = cell_info.act;
    }
    return state;
}

int Nt26Board::GetSignalStrength() {
    std::shared_ptr<UartEthModem> modem;
    {
        std::lock_guard<std::mutex> lock(lifecycle_mu_);
        modem = modem_;
    }
    if (!modem) {
        return 99;
    }
    return modem->GetSignalStrength();
}

esp_err_t Nt26Board::SendAtCommand(const std::string& cmd, std::string& response,
                                   uint32_t timeout_ms,
                                   bool bypass_init_check) {
    std::shared_ptr<UartEthModem> modem;
    {
        std::lock_guard<std::mutex> lock(lifecycle_mu_);
        modem = modem_;
        if (!modem) {
            ESP_LOGW(TAG, "SendAtCommand: modem 未实例化（WiFi 模式或 MAIN_PWR 已掉电）");
            return ESP_ERR_INVALID_STATE;
        }
        if (!bypass_init_check && !modem->IsInitialized()) {
            ESP_LOGW(TAG, "SendAtCommand: modem 尚未完成初始化");
            return ESP_ERR_INVALID_STATE;
        }
    }
    return modem->SendAt(cmd, response, timeout_ms);
}

esp_err_t Nt26Board::SendAtCommandCollectUntil(const std::string& cmd,
                                               std::string& response,
                                               uint32_t timeout_ms,
                                               const char* done_marker,
                                               bool bypass_init_check) {
    std::shared_ptr<UartEthModem> modem;
    {
        std::lock_guard<std::mutex> lock(lifecycle_mu_);
        modem = modem_;
        if (!modem) {
            ESP_LOGW(TAG, "SendAtCommandCollectUntil: modem 未实例化");
            return ESP_ERR_INVALID_STATE;
        }
        if (!bypass_init_check && !modem->IsInitialized()) {
            ESP_LOGW(TAG, "SendAtCommandCollectUntil: modem 尚未完成初始化");
            return ESP_ERR_INVALID_STATE;
        }
    }
    return modem->SendAtCollectUntil(cmd, response, timeout_ms, done_marker);
}

std::string Nt26Board::GetDeviceStatusJson() {
    auto& board = Board::GetInstance();
    auto root = cJSON_CreateObject();

    auto audio_speaker = cJSON_CreateObject();
    auto audio_codec = board.GetAudioCodec();
    if (audio_codec) {
        cJSON_AddNumberToObject(audio_speaker, "volume", audio_codec->output_volume());
    }
    cJSON_AddItemToObject(root, "audio_speaker", audio_speaker);

    auto backlight = board.GetBacklight();
    auto screen = cJSON_CreateObject();
    if (backlight) {
        cJSON_AddNumberToObject(screen, "brightness", backlight->brightness());
    }
    auto display = board.GetDisplay();
    if (display && display->height() > 64) {
        auto theme = display->GetTheme();
        if (theme != nullptr) {
            cJSON_AddStringToObject(screen, "theme", theme->name().c_str());
        }
    }
    cJSON_AddItemToObject(root, "screen", screen);

    int battery_level = 0;
    bool charging = false, discharging = false;
    if (board.GetBatteryLevel(battery_level, charging, discharging)) {
        auto battery = cJSON_CreateObject();
        cJSON_AddNumberToObject(battery, "level", battery_level);
        cJSON_AddBoolToObject(battery, "charging", charging);
        cJSON_AddItemToObject(root, "battery", battery);
    }

    auto network = cJSON_CreateObject();
    cJSON_AddStringToObject(network, "type", "cellular");
    std::shared_ptr<UartEthModem> modem;
    {
        std::lock_guard<std::mutex> lock(lifecycle_mu_);
        modem = modem_;
    }
    if (modem) {
        cJSON_AddStringToObject(network, "carrier", modem->GetCarrierName().c_str());
        int csq = modem->GetSignalStrength();
        if (csq == 99 || csq == -1) {
            cJSON_AddStringToObject(network, "signal", "unknown");
        } else if (csq >= 0 && csq <= 14) {
            cJSON_AddStringToObject(network, "signal", "weak");
        } else if (csq >= 15 && csq <= 24) {
            cJSON_AddStringToObject(network, "signal", "medium");
        } else if (csq >= 25 && csq <= 31) {
            cJSON_AddStringToObject(network, "signal", "strong");
        }
    }
    cJSON_AddItemToObject(root, "network", network);

    auto json_str = cJSON_PrintUnformatted(root);
    std::string json(json_str);
    cJSON_free(json_str);
    cJSON_Delete(root);
    return json;
}
