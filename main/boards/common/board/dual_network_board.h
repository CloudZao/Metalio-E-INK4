#ifndef DUAL_NETWORK_BOARD_H
#define DUAL_NETWORK_BOARD_H

#include "board.h"
#include "wifi_board.h"
#include "ml307_board.h"
#include <memory>
#include "nt26_board.h"

//enum NetworkType
enum class NetworkType {
    WIFI,
    ML307
};

// 双网络板卡类，可以在WiFi和ML307之间切换
class DualNetworkBoard : public Board {
private:
    // 使用基类指针存储当前活动的板卡
    std::unique_ptr<Board> current_board_;
    NetworkType network_type_ = NetworkType::ML307;  // Default to ML307

    // ML307的引脚配置
    gpio_num_t ml307_tx_pin_;
    gpio_num_t ml307_rx_pin_;
    gpio_num_t ml307_dtr_pin_;
    
    // Cellular(NT26) pin configuration
    gpio_num_t cellular_tx_pin_;
    gpio_num_t cellular_rx_pin_;
    gpio_num_t cellular_dtr_pin_;
    gpio_num_t cellular_ri_pin_;

    // 保存网络类型到Settings
    void SaveNetworkTypeToSettings(NetworkType type);

    // 初始化当前网络类型对应的板卡
    void InitializeCurrentBoard();
 
public:
    DualNetworkBoard(gpio_num_t ml307_tx_pin, gpio_num_t ml307_rx_pin, gpio_num_t ml307_dtr_pin = GPIO_NUM_NC, int32_t default_net_type = 1);
    
    // Explicit constructor with SRDY/RI pin.
    DualNetworkBoard(gpio_num_t cellular_tx_pin,
        gpio_num_t cellular_rx_pin,
        gpio_num_t cellular_dtr_pin,
        gpio_num_t cellular_ri_pin,
        int32_t default_net_type);
    
    virtual ~DualNetworkBoard() = default;
 
    // 从 Settings 加载网络类型（namespace "network", key "type"：0=WiFi, 1=4G）
    static NetworkType LoadNetworkTypeFromSettings(int32_t default_net_type);

    // 切换网络类型（在 WiFi / 4G 之间对调，写 NVS 后重启）
    void SwitchNetworkType();

    // 切换到指定网络类型；已是目标类型则 no-op
    void SwitchToNetworkType(NetworkType type);
    
    // 获取当前网络类型
    NetworkType GetNetworkType() const { return network_type_; }
    
    // 获取当前活动的板卡引用
    Board& GetCurrentBoard() const { return *current_board_; }
    
    // 重写Board接口
    virtual std::string GetBoardType() override;
    virtual void StartNetwork() override;
    virtual bool EnsureNetworkReady(int timeout_ms = 30000) override;
    virtual void RefreshNetworkWaitDeadline(int timeout_ms = 30000) override;
    virtual void PauseNetworkIfNotReady() override;
    virtual bool IsNetworkReady() override;
    virtual bool IsWifiConfigMode() override;
    virtual NetworkInterface* GetNetwork() override;
    virtual const char* GetNetworkStateIcon() override;
    virtual void SetPowerSaveMode(bool enabled) override;
    virtual std::string GetBoardJson() override;
    virtual std::string GetDeviceStatusJson() override;
};

#endif // DUAL_NETWORK_BOARD_H 