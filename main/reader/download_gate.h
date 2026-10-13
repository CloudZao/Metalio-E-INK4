#pragma once

#include "http.h"

#include <atomic>
#include <memory>
#include <mutex>

/**
 * @brief HTTP 落盘下载可取消门闩
 * @note UI 先 RequestCancel() 置位；AbortBoundHttp() 才能打断阻塞 Read。
 * @note AbortBoundHttp / RequestStop 勿在持 LVGL 锁时调用：Http::Close→Disconnect 会 join 接收任务，
 *       易在 lv_timer_handler 内把定时器链踩坏（InstructionFetchError）。
 * @note 真实所有权在 gate（shared_ptr）；AbortBoundHttp 先 pin 再 Close，与 worker CloseHttp 并发时
 *       对象活到 join/回调结束，避免析构后 OnTcpDisconnected 踩 mutex（LoadProhibited / queue assert）。
 */
struct DownloadGate {
    std::atomic<bool> cancel{false};
    std::mutex mu;
    std::shared_ptr<Http> http;

    void Reset() {
        cancel.store(false, std::memory_order_release);
        std::lock_guard<std::mutex> lock(mu);
        http.reset();
    }

    /** 仅置取消位；可在 LVGL 线程调用 */
    void RequestCancel() {
        cancel.store(true, std::memory_order_release);
    }

    /** Close 当前绑定 Http 以打断阻塞；pin 住对象直至 Close/join 返回 */
    void AbortBoundHttp() {
        std::shared_ptr<Http> pin;
        {
            std::lock_guard<std::mutex> lock(mu);
            pin = http;
        }
        if (pin != nullptr) {
            pin->Close();
        }
    }

    /** 置位并 Close；仅可在非 LVGL 路径调用 */
    void RequestStop() {
        RequestCancel();
        AbortBoundHttp();
    }

    /**
     * @brief 绑定；若已取消则 Close 并返回 false
     */
    bool BindHttp(std::unique_ptr<Http> h) {
        if (h == nullptr) {
            return false;
        }
        if (cancel.load(std::memory_order_acquire)) {
            h->Close();
            return false;
        }
        std::shared_ptr<Http> owned = std::move(h);
        std::lock_guard<std::mutex> lock(mu);
        if (cancel.load(std::memory_order_acquire)) {
            owned->Close();
            return false;
        }
        http = std::move(owned);
        return true;
    }

    void UnbindHttp() {
        std::lock_guard<std::mutex> lock(mu);
        http.reset();
    }

    /**
     * @brief 下载任务收尾：移出绑定并 Close；Abort 仍 pin 时对象延后析构
     */
    void CloseHttp() {
        std::shared_ptr<Http> bound;
        {
            std::lock_guard<std::mutex> lock(mu);
            bound = std::move(http);
        }
        if (bound != nullptr) {
            bound->Close();
        }
    }

    bool IsCancelled() const {
        return cancel.load(std::memory_order_acquire);
    }
};
