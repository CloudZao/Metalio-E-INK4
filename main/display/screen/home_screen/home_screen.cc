#include "home_screen.h"

#include "home_screen/home_hero.h"
#include "assistant_screen/assistant_screen.h"
#include "assets/lang_config.h"
#include "book_screen/book_screen.h"
#include "cloud_screen/cloud_screen.h"
#include "haptic_feedback.h"
#include "lv_adapter_display.h"
#include "screen_common.h"
#include "settings.h"
#include "settings_screen/settings_screen.h"
#include "task_screen/task_screen.h"
#include "vk_key_handler.h"
#include "vk_page_repeat.h"
#include "wallpaper_screen/wallpaper_screen.h"

#include <cstdio>
#include <cstring>

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "fontpack_lvgl.h"


// ---- 首页布局（逻辑分辨率约 480x800，旋转 270°）----
#define HOME_STATUS_BAR_H 45          // 状态栏高度 (px)
#define HOME_APPS_PAD_TOP 16          // 应用区顶部内边距 (px)；贴底时仅作内容间距
#define HOME_APPS_PAD_BOT 16          // 应用区底部内边距 (px)
#define HOME_APPS_LIFT 48             // 宫格整体相对屏底上移 (px)
#define HOME_APP_CARD_W 116           // 应用图标卡片宽 (px)
#define HOME_APP_CARD_H 136           // 应用图标卡片高 (px)
#define HOME_APP_CARD_MARGIN_H 38     // 每行应用卡片距左右边距 (px)
#define HOME_APP_CARD_GAP_COL 28      // 同行相邻应用卡片间距 (px)
#define HOME_APP_CARD_GAP_ROW 41      // 上下两行应用卡片间距 (px)
#define HOME_APP_CARD_ICON_PAD_TOP 10 // 卡片内图标距卡片顶部 (px)，水平居中
#define HOME_APP_CARD_NAME_PAD_BOT 10 // 卡片内名称距卡片底部 (px)，水平居中
#define HOME_APP_CARD_BORDER_W 4      // 描边样式时黑色描边宽度 (px)
#define HOME_PAGE_INDICATOR_H 36      // 多页时底部页码高度 (px)
#define HOME_APPS_VISIBLE_ROWS 2      // 贴底宫格按两行算高（当前 6 app）
#define HOME_SLASH_ROWS 3             // 斜切主题：3 行 × 2 列
#define HOME_SLASH_SIDE_PAD 16        // 宫格左右边距（防右卡图标贴屏边）
#define HOME_SLASH_ROW_GAP 10         // 行间距；左右斜边平行缝同此宽度
#define HOME_SLASH_BOTTOM_PAD HOME_SLASH_ROW_GAP // 底边距与行间距一致
#define HOME_SLASH_SLANT 22           // 内侧斜切水平偏移
#define HOME_SLASH_COL_GAP HOME_SLASH_ROW_GAP // 左右斜边间隙（与行间距一致）
#define HOME_SLASH_ICON 70            // 与经典宫格同资源边长，勿依赖 transform 缩放
#define HOME_SLASH_PAD_OUTER 18       // 卡内外侧边距（给图标完整空间）
#define HOME_SLASH_PAD_V 8            // 卡内上下边距
#define HOME_SLASH_PAD_SLANT 10       // 卡内靠斜边在 SLANT 之外再内收
#define HOME_SLASH_PAD_COL 14         // 图标与文字间距
#define HOME_SLASH_NAME_NUDGE_R 12    // 右卡文字相对图标再右移一点
#define HOME_SLASH_BORDER_W 1         // 白卡梯形描边（含斜边）
// NVS 无记录时的默认卡片样式：0=白底，1=网点灰底，2=白底+黑色描边，3=斜切棋盘
#define HOME_APP_CARD_STYLE_DEFAULT HomeScreen::kCardStyleBorder

namespace
{

    constexpr const char *TAG = "HomeScreen";
    constexpr const char *kScreenId = "home";
    constexpr lv_coord_t kIconSize = 70;
    constexpr lv_coord_t kCardRadius = 20;
    constexpr int kColsPerRow = 3;
    constexpr int kRowsPerPage = 4;
    constexpr int kAppsPerPage = kColsPerRow * kRowsPerPage; // 每页 12 个

    // I1 无真灰阶：#AAAAAA 会量化成白。共用 8x8 L8 平铺（64B），稀疏网点近似浅灰。
    constexpr int kDitherW = 8;
    constexpr int kDitherH = 8;
    // 4x4 Bayer：黑点越少越淡；阈值 2 → 2/16=12.5% 黑
    constexpr uint8_t kBayer4[4][4] = {
        {0, 8, 2, 10},
        {12, 4, 14, 6},
        {3, 11, 1, 9},
        {15, 7, 13, 5},
    };
    constexpr int kBayerThreshold = 2; // 0..15，<2 为黑 → 2/16=12.5%
    uint8_t s_gray_dither_l8[kDitherW * kDitherH];
    lv_image_dsc_t s_gray_dither_img;
    bool s_gray_dither_ready = false;

    const lv_image_dsc_t *GrayDitherImg()
    {
        if (!s_gray_dither_ready)
        {
            for (int y = 0; y < kDitherH; ++y)
            {
                for (int x = 0; x < kDitherW; ++x)
                {
                    const bool black = kBayer4[y & 3][x & 3] < kBayerThreshold;
                    s_gray_dither_l8[y * kDitherW + x] = black ? 0x00 : 0xFF;
                }
            }
            s_gray_dither_img.header.magic = LV_IMAGE_HEADER_MAGIC;
            s_gray_dither_img.header.cf = LV_COLOR_FORMAT_L8;
            s_gray_dither_img.header.flags = 0;
            s_gray_dither_img.header.w = kDitherW;
            s_gray_dither_img.header.h = kDitherH;
            s_gray_dither_img.header.stride = kDitherW;
            s_gray_dither_img.data_size = sizeof(s_gray_dither_l8);
            s_gray_dither_img.data = s_gray_dither_l8;
            s_gray_dither_ready = true;
        }
        return &s_gray_dither_img;
    }

    void ApplyCardStyle(lv_obj_t *cell, int style)
    {
        if (style == HomeScreen::kCardStyleGray)
        {
            lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, 0);
            lv_obj_set_style_bg_image_src(cell, GrayDitherImg(), 0);
            lv_obj_set_style_bg_image_tiled(cell, true, 0);
            lv_obj_set_style_border_width(cell, 0, 0);
            return;
        }

        lv_obj_set_style_bg_color(cell, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_image_src(cell, nullptr, 0);
        if (style == HomeScreen::kCardStyleBorder)
        {
            lv_obj_set_style_border_width(cell, HOME_APP_CARD_BORDER_W, 0);
            lv_obj_set_style_border_color(cell, lv_color_black(), 0);
            lv_obj_set_style_border_opa(cell, LV_OPA_COVER, 0);
        }
        else
        {
            lv_obj_set_style_border_width(cell, 0, 0);
        }
    }

    struct AppEntry
    {
        const char *icon_path;
        const char *(*name)();
        lv_obj_t *(*create)();
    };

    const char *AppNameTask() { return Lang::Strings::HOME_APP_TASK; }
    const char *AppNameAssistant() { return Lang::Strings::HOME_APP_ASSISTANT; }
    const char *AppNameBook() { return Lang::Strings::HOME_APP_BOOK; }
    const char *AppNameWallpaper() { return Lang::Strings::HOME_APP_WALLPAPER; }
    const char *AppNameCloud() { return Lang::Strings::HOME_APP_CLOUD; }
    const char *AppNameSettings() { return Lang::Strings::HOME_APP_SETTINGS; }

    constexpr AppEntry kApps[] = {
        {"A:ic_app_home_task.spng", AppNameTask, TaskScreen::Create},
        {"A:ic_app_home_teacher.spng", AppNameAssistant, AssistantScreen::Create},
        {"A:ic_app_home_book.spng", AppNameBook, BookScreen::Create},
        {"A:ic_app_home_wallpaper.spng", AppNameWallpaper, WallpaperScreen::Create},
        {"A:ic_app_home_cloud.spng", AppNameCloud, CloudScreen::Create},
        {"A:ic_app_home_setting.spng", AppNameSettings, SettingsScreen::Create},
    };

    // 仅对外展示用户可见的应用，保持首页与设计稿一致。
    constexpr AppEntry kSlashApps[] = {
        {"A:ic_app_home_book.spng", AppNameBook, BookScreen::Create},
        {"A:ic_app_home_wallpaper.spng", AppNameWallpaper, WallpaperScreen::Create},
        {"A:ic_app_home_teacher.spng", AppNameAssistant, AssistantScreen::Create},
        {"A:ic_app_home_task.spng", AppNameTask, TaskScreen::Create},
        {"A:ic_app_home_cloud.spng", AppNameCloud, CloudScreen::Create},
        {"A:ic_app_home_setting.spng", AppNameSettings, SettingsScreen::Create},
    };

    // 随 kApps 自动变长；页数 = ceil(总数 / 每页 12)
    constexpr int kTotalApps = static_cast<int>(sizeof(kApps) / sizeof(kApps[0]));
    constexpr int kSlashAppCount = static_cast<int>(sizeof(kSlashApps) / sizeof(kSlashApps[0]));

    lv_obj_t *s_apps = nullptr;
    lv_obj_t *s_page_lbl = nullptr;
    lv_obj_t *s_home_scr = nullptr;
    int s_home_page = 0; // 离开子页再回首页时保持页码
    bool s_slash_layout = false;
    lv_coord_t s_slash_apps_h = 0;

    lv_obj_t *CreateAppCell(lv_obj_t *parent, const AppEntry &entry, int card_style);
    void CreateSlashAppCell(lv_obj_t *row, const AppEntry &entry, int row_i, int col,
                            lv_coord_t row_h, lv_coord_t cell_w);

    int PageCount()
    {
        if (s_slash_layout)
        {
            return 1;
        }
        return (kTotalApps + kAppsPerPage - 1) / kAppsPerPage;
    }

    void FillSlashAppsPage()
    {
        if (s_apps == nullptr || s_slash_apps_h <= 0)
        {
            ESP_LOGW(TAG, "slash apps skip h=%d", static_cast<int>(s_slash_apps_h));
            return;
        }
        lv_obj_clean(s_apps);
        const lv_coord_t inner_h =
            s_slash_apps_h - HOME_SLASH_ROW_GAP * (HOME_SLASH_ROWS - 1);
        const lv_coord_t row_h = inner_h / HOME_SLASH_ROWS;
        // 左右斜边平行，水平间隙 = COL_GAP（与行间距同）；bbox 仍重叠 SLANT-COL_GAP
        const lv_coord_t cell_w =
            (LV_HOR_RES - 2 * HOME_SLASH_SIDE_PAD + HOME_SLASH_SLANT - HOME_SLASH_COL_GAP) / 2;
        for (int r = 0; r < HOME_SLASH_ROWS; ++r)
        {
            lv_obj_t *row = lv_obj_create(s_apps);
            lv_obj_remove_style_all(row);
            lv_obj_set_size(row, LV_HOR_RES, row_h);
            lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
            lv_obj_set_style_layout(row, LV_LAYOUT_NONE, 0);
            lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);

            const int base = r * 2;
            if (base < kSlashAppCount)
            {
                CreateSlashAppCell(row, kSlashApps[base], r, 0, row_h, cell_w);
            }
            if (base + 1 < kSlashAppCount)
            {
                CreateSlashAppCell(row, kSlashApps[base + 1], r, 1, row_h, cell_w);
            }
        }
        if (s_page_lbl != nullptr)
        {
            lv_obj_add_flag(s_page_lbl, LV_OBJ_FLAG_HIDDEN);
        }
        ESP_LOGI(TAG, "slash apps %d row_h=%d cell_w=%d", kSlashAppCount, static_cast<int>(row_h),
                 static_cast<int>(cell_w));
    }

    void FillAppsPage()
    {
        if (s_apps == nullptr)
        {
            return;
        }
        if (s_slash_layout)
        {
            FillSlashAppsPage();
            return;
        }
        const int pages = PageCount();
        if (pages < 1)
        {
            return;
        }
        if (s_home_page < 0)
        {
            s_home_page = 0;
        }
        if (s_home_page >= pages)
        {
            s_home_page = pages - 1;
        }

        lv_obj_clean(s_apps);
        const int card_style = HomeScreen::LoadCardStyle();
        const int start = s_home_page * kAppsPerPage;
        int end = start + kAppsPerPage;
        if (end > kTotalApps)
        {
            end = kTotalApps;
        }
        for (int i = start; i < end; ++i)
        {
            CreateAppCell(s_apps, kApps[i], card_style);
        }

        if (s_page_lbl != nullptr)
        {
            if (pages > 1)
            {
                char buf[16];
                std::snprintf(buf, sizeof(buf), "%d / %d", s_home_page + 1, pages);
                lv_label_set_text(s_page_lbl, buf);
                lv_obj_clear_flag(s_page_lbl, LV_OBJ_FLAG_HIDDEN);
            }
            else
            {
                lv_obj_add_flag(s_page_lbl, LV_OBJ_FLAG_HIDDEN);
            }
        }
        ESP_LOGI(TAG, "home page %d/%d apps %d..%d of %d", s_home_page + 1, pages, start, end - 1,
                 kTotalApps);
    }

    ScreenPaintCoalesce s_home_paint{};

    void RequestFillAppsPage() {
        if (s_home_paint.paint == nullptr) {
            s_home_paint.paint = FillAppsPage;
        }
        ScreenPaintCoalesceRequest(&s_home_paint);
    }

    bool OnVkKey(const char *key)
    {
        if (key == nullptr)
        {
            return false;
        }
        // vk_home 走默认（已在首页 no-op）
        if (std::strcmp(key, "vk_prev") == 0)
        {
            if (s_home_page > 0)
            {
                --s_home_page;
                RequestFillAppsPage();
            }
            return true;
        }
        if (std::strcmp(key, "vk_next") == 0)
        {
            if (s_home_page + 1 < PageCount())
            {
                ++s_home_page;
                RequestFillAppsPage();
            }
            return true;
        }
        return false;
    }

    bool HomePageRepeatStep(int page_delta)
    {
        if (page_delta == 0)
        {
            return false;
        }
        const int last = PageCount() > 0 ? PageCount() - 1 : 0;
        int next = s_home_page + page_delta;
        if (next < 0)
        {
            next = 0;
        }
        else if (next > last)
        {
            next = last;
        }
        if (next == s_home_page)
        {
            return false;
        }
        s_home_page = next;
        RequestFillAppsPage();
        return page_delta < 0 ? s_home_page > 0 : s_home_page < last;
    }

    bool OnVkKeyLongPress(const char *key)
    {
        return VkPageRepeatTryStart(key, HomePageRepeatStep);
    }

    bool OnVkKeyPressUp(const char *key)
    {
        return VkPageRepeatOnPressUp(key);
    }

    // BOOT 长按快捷进百问（仍按住则 hold-through 聆听）；短按进待机改走电源键
    bool OnBootLongPress()
    {
        ESP_LOGI(TAG, "boot long -> assistant (hold-through PTT if still pressed)");
        AssistantScreen::RequestOpen();
        return true;
    }

    void OnHomeDeleted(lv_event_t *e)
    {
        // ScreenLoadReplace 会 async 删旧屏；勿清掉新首页的指针
        if (lv_event_get_target(e) != s_home_scr)
        {
            return;
        }
        home_hero::Teardown();
        s_home_scr = nullptr;
        s_apps = nullptr;
        s_page_lbl = nullptr;
        s_slash_layout = false;
        s_slash_apps_h = 0;
    }

    void LaunchAppAsync(void *user_data)
    {
        const auto *entry = static_cast<const AppEntry *>(user_data);
        if (entry == nullptr || entry->create == nullptr)
        {
            return;
        }
        ESP_LOGI(TAG, "launch %s", entry->name());
        ScreenNavigateTo(entry->create);
    }

    void OnAppClicked(lv_event_t *e)
    {
        lv_obj_t *obj = static_cast<lv_obj_t *>(lv_event_get_target(e));
        const AppEntry *entry = nullptr;
        while (obj != nullptr)
        {
            entry = static_cast<const AppEntry *>(lv_obj_get_user_data(obj));
            if (entry != nullptr)
            {
                break;
            }
            obj = lv_obj_get_parent(obj);
        }
        if (entry == nullptr)
        {
            entry = static_cast<const AppEntry *>(lv_event_get_user_data(e));
        }
        if (entry == nullptr || entry->create == nullptr)
        {
            return;
        }
        // 事件回调里删当前屏不安全，延后一拍再切（与返回键一致）
        lv_async_call(LaunchAppAsync, const_cast<AppEntry *>(entry));
    }

    lv_obj_t *CreateAppCell(lv_obj_t *parent, const AppEntry &entry, int card_style)
    {
        // 整卡为唯一点击热区；样式来自设置→主题（NVS）。
        lv_obj_t *cell = lv_obj_create(parent);
        lv_obj_remove_style_all(cell);
        lv_obj_set_size(cell, HOME_APP_CARD_W, HOME_APP_CARD_H);
        lv_obj_set_style_pad_all(cell, 0, 0);
        ApplyCardStyle(cell, card_style);
        lv_obj_set_style_radius(cell, kCardRadius, 0);
        lv_obj_set_style_clip_corner(cell, true, 0);
        lv_obj_set_style_layout(cell, LV_LAYOUT_NONE, 0);
        lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE);
        HapticAttachClick(cell);
        lv_obj_set_user_data(cell, const_cast<AppEntry *>(&entry));
        lv_obj_add_event_cb(cell, OnAppClicked, LV_EVENT_CLICKED, const_cast<AppEntry *>(&entry));
        lv_obj_t *icon = lv_image_create(cell);
        lv_image_set_src(icon, entry.icon_path);
        lv_obj_set_size(icon, kIconSize, kIconSize);
        lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, HOME_APP_CARD_ICON_PAD_TOP);
        lv_obj_clear_flag(icon, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t *name = lv_label_create(cell);
        lv_label_set_text(name, entry.name());
        lv_obj_set_width(name, HOME_APP_CARD_W);
        lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_font(name, fontpack_lv_font_ui(), 0);
        lv_obj_set_style_text_color(name, lv_color_black(), 0);
        lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(name, LV_ALIGN_BOTTOM_MID, 0, -HOME_APP_CARD_NAME_PAD_BOT);
        lv_obj_clear_flag(name, LV_OBJ_FLAG_CLICKABLE);

        return cell;
    }

    // 图二：透明底 + DRAW_MAIN 只填梯形；白卡描梯形轮廓，勿用矩形 border（否则像方框里斜切）。
    void OnSlashCellDraw(lv_event_t *e)
    {
        if (lv_event_get_code(e) != LV_EVENT_DRAW_MAIN)
        {
            return;
        }
        lv_obj_t *obj = static_cast<lv_obj_t *>(lv_event_get_target(e));
        lv_layer_t *layer = lv_event_get_layer(e);
        if (obj == nullptr || layer == nullptr)
        {
            return;
        }
        const intptr_t flags = reinterpret_cast<intptr_t>(lv_event_get_user_data(e));
        const bool left = (flags & 1) != 0;
        const bool black = (flags & 2) != 0;
        // 用整格坐标画梯形；content_coords 会扣掉 pad，中缝会被垫宽
        lv_area_t a;
        lv_obj_get_coords(obj, &a);
        const lv_coord_t slant = HOME_SLASH_SLANT;

        lv_point_precise_t p[4];
        if (left)
        {
            // 左卡：右缘为反斜线
            p[0].x = a.x1;
            p[0].y = a.y1;
            p[1].x = a.x2;
            p[1].y = a.y1;
            p[2].x = a.x2 - slant;
            p[2].y = a.y2;
            p[3].x = a.x1;
            p[3].y = a.y2;
        }
        else
        {
            // 右卡：左缘为反斜线
            p[0].x = a.x1 + slant;
            p[0].y = a.y1;
            p[1].x = a.x2;
            p[1].y = a.y1;
            p[2].x = a.x2;
            p[2].y = a.y2;
            p[3].x = a.x1;
            p[3].y = a.y2;
        }

        lv_draw_triangle_dsc_t tri;
        lv_draw_triangle_dsc_init(&tri);
        tri.color = black ? lv_color_black() : lv_color_white();
        tri.opa = LV_OPA_COVER;
        tri.p[0] = p[0];
        tri.p[1] = p[1];
        tri.p[2] = p[2];
        lv_draw_triangle(layer, &tri);
        tri.p[0] = p[0];
        tri.p[1] = p[2];
        tri.p[2] = p[3];
        lv_draw_triangle(layer, &tri);

        if (!black)
        {
            // 白卡四边同宽描边（含斜边）
            lv_draw_line_dsc_t line;
            lv_draw_line_dsc_init(&line);
            line.color = lv_color_black();
            line.opa = LV_OPA_COVER;
            line.width = HOME_SLASH_BORDER_W;
            line.round_start = 0;
            line.round_end = 0;
            for (int i = 0; i < 4; ++i)
            {
                line.p1 = p[i];
                line.p2 = p[(i + 1) % 4];
                lv_draw_line(layer, &line);
            }
        }
    }

    void CreateSlashAppCell(lv_obj_t *row, const AppEntry &entry, int row_i, int col,
                            lv_coord_t row_h, lv_coord_t cell_w)
    {
        const bool left = (col == 0);
        const bool black = ((row_i + col) % 2) == 0;
        const intptr_t flags = (left ? 1 : 0) | (black ? 2 : 0);
        const lv_coord_t x =
            left ? HOME_SLASH_SIDE_PAD
                 : (HOME_SLASH_SIDE_PAD + cell_w - HOME_SLASH_SLANT + HOME_SLASH_COL_GAP);

        lv_obj_t *cell = lv_obj_create(row);
        lv_obj_remove_style_all(cell);
        lv_obj_set_size(cell, cell_w, row_h);
        lv_obj_set_pos(cell, x, 0);
        // 不用矩形 bg/border，外形完全由 OnSlashCellDraw 梯形决定
        lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, 0);
        lv_obj_set_style_pad_all(cell, 0, 0);
        lv_obj_set_flex_flow(cell, LV_FLEX_FLOW_ROW);
        // 图标+文字成组靠外侧，勿 SPACE_BETWEEN 把字顶到中缝
        if (left)
        {
            lv_obj_set_flex_align(cell, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                                  LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_left(cell, HOME_SLASH_PAD_OUTER, 0);
            lv_obj_set_style_pad_right(cell, HOME_SLASH_SLANT + HOME_SLASH_PAD_SLANT, 0);
            lv_obj_set_style_pad_column(cell, HOME_SLASH_PAD_COL, 0);
        }
        else
        {
            lv_obj_set_flex_align(cell, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER,
                                  LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_left(cell, HOME_SLASH_SLANT + HOME_SLASH_PAD_SLANT, 0);
            // 右卡文字略贴图标、整体更靠右（如「每日清单」）
            lv_obj_set_style_pad_right(cell, HOME_SLASH_PAD_OUTER - 6, 0);
            lv_obj_set_style_pad_column(cell, HOME_SLASH_PAD_COL - HOME_SLASH_NAME_NUDGE_R / 2, 0);
        }
        lv_obj_set_style_pad_top(cell, HOME_SLASH_PAD_V, 0);
        lv_obj_set_style_pad_bottom(cell, HOME_SLASH_PAD_V, 0);
        lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE);
        HapticAttachClick(cell);
        lv_obj_set_user_data(cell, const_cast<AppEntry *>(&entry));
        lv_obj_add_event_cb(cell, OnAppClicked, LV_EVENT_CLICKED, const_cast<AppEntry *>(&entry));
        lv_obj_add_event_cb(cell, OnSlashCellDraw, LV_EVENT_DRAW_MAIN,
                            reinterpret_cast<void *>(flags));

        const lv_color_t fg = black ? lv_color_white() : lv_color_black();

        auto make_icon = [&]() {
            lv_obj_t *icon = lv_image_create(cell);
            lv_image_set_src(icon, entry.icon_path);
            lv_obj_set_size(icon, HOME_SLASH_ICON, HOME_SLASH_ICON);
            // 与经典卡相同：按资源原尺寸画，避免 CONTAIN/STRETCH 在墨水屏无效导致裁切
            if (black)
            {
                lv_obj_set_style_image_recolor(icon, lv_color_white(), 0);
                lv_obj_set_style_image_recolor_opa(icon, LV_OPA_COVER, 0);
            }
            lv_obj_clear_flag(icon, LV_OBJ_FLAG_CLICKABLE);
        };
        auto make_name = [&]() {
            lv_obj_t *name = lv_label_create(cell);
            lv_label_set_text(name, entry.name());
            lv_label_set_long_mode(name, LV_LABEL_LONG_CLIP);
            const lv_font_t *name_font = fontpack_lv_font_get(30, 2);
            if (name_font == nullptr)
            {
                name_font = fontpack_lv_font_ui();
            }
            lv_obj_set_style_text_font(name, name_font, 0);
            lv_obj_set_style_text_color(name, fg, 0);
            lv_obj_clear_flag(name, LV_OBJ_FLAG_CLICKABLE);
        };

        if (left)
        {
            make_icon();
            make_name();
        }
        else
        {
            make_name();
            make_icon();
        }
    }

    // LVGL worker 栈在 PSRAM：不可在该任务里直接 NVS 写 flash（会关 cache 触发断言）。
    int s_card_style = HOME_APP_CARD_STYLE_DEFAULT;
    bool s_card_style_ready = false;

    int NormalizeCardStyle(int style)
    {
        if (style == HomeScreen::kCardStyleWhite || style == HomeScreen::kCardStyleGray ||
            style == HomeScreen::kCardStyleBorder || style == HomeScreen::kCardStyleSlash)
        {
            return style;
        }
        return HOME_APP_CARD_STYLE_DEFAULT;
    }

    void PersistCardStyleTask(void *arg)
    {
        const int style = static_cast<int>(reinterpret_cast<intptr_t>(arg));
        Settings settings("display", true);
        settings.SetInt("card_style", style);
        ESP_LOGI(TAG, "nvs card_style=%d", style);
        vTaskDelete(nullptr);
    }

    void LoadCardStyleFromNvs()
    {
        Settings settings("display", false);
        s_card_style =
            NormalizeCardStyle(static_cast<int>(settings.GetInt("card_style", HOME_APP_CARD_STYLE_DEFAULT)));
        s_card_style_ready = true;
    }

} // namespace

int HomeScreen::LoadCardStyle()
{
    if (!s_card_style_ready)
    {
        // 首次读取：只读 NVS；之后一律走内存缓存，避免 LVGL 任务反复碰 flash。
        LoadCardStyleFromNvs();
    }
    return s_card_style;
}

void HomeScreen::SaveCardStyle(int style)
{
    style = NormalizeCardStyle(style);
    s_card_style = style;
    s_card_style_ready = true;
    // 立刻更新内存；写 NVS 放到内部 RAM 栈任务（与网络切换同理）。
    if (xTaskCreate(PersistCardStyleTask, "card_style", 4096,
                    reinterpret_cast<void *>(static_cast<intptr_t>(style)), 5, nullptr) != pdPASS)
    {
        ESP_LOGE("HomeScreen", "xTaskCreate(card_style) failed");
    }
}

lv_obj_t *HomeScreen::Create()
{
    lv_obj_t *scr = lv_obj_create(nullptr);
    s_home_scr = scr;
    lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(scr, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(scr, lv_color_black(), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    EpdStatusBar status = ScreenCreateStatusBar(scr);
    status.height = HOME_STATUS_BAR_H;
    if (status.bar != nullptr)
    {
        lv_obj_set_size(status.bar, LV_HOR_RES, HOME_STATUS_BAR_H);
    }
    if (status.overlay != nullptr)
    {
        lv_obj_set_size(status.overlay, LV_HOR_RES, HOME_STATUS_BAR_H);
    }

    const int card_style = LoadCardStyle();
    s_slash_layout = (card_style == kCardStyleSlash);
    const int pages = PageCount();
    ESP_LOGI(TAG, "layout apps=%d pages=%d per_page=%d style=%d slash=%d", kTotalApps, pages,
             kAppsPerPage, card_style, s_slash_layout ? 1 : 0);

    if (s_slash_layout)
    {
        home_hero::MountSlash(scr, status.height);
        home_hero::Start();

        // 下半约一半高度留给 3 行斜切宫格（勿用 get_height，创建当下可能仍为 0）
        const lv_coord_t content_h = LV_VER_RES - status.height;
        s_slash_apps_h = content_h / 2;
        if (s_slash_apps_h < HOME_SLASH_ROWS * 110)
        {
            s_slash_apps_h = HOME_SLASH_ROWS * 110;
        }
        s_apps = lv_obj_create(scr);
        lv_obj_remove_style_all(s_apps);
        lv_obj_set_size(s_apps, LV_HOR_RES, s_slash_apps_h);
        lv_obj_align(s_apps, LV_ALIGN_BOTTOM_MID, 0, -HOME_SLASH_BOTTOM_PAD);
        lv_obj_set_style_bg_color(s_apps, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(s_apps, LV_OPA_COVER, 0);
        lv_obj_set_flex_flow(s_apps, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(s_apps, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_row(s_apps, HOME_SLASH_ROW_GAP, 0);
        lv_obj_add_flag(s_apps, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
        lv_obj_clear_flag(s_apps, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(s_apps, LV_OBJ_FLAG_CLICKABLE);

        s_page_lbl = nullptr;
        FillAppsPage();

        lv_obj_add_event_cb(scr, OnHomeDeleted, LV_EVENT_DELETE, nullptr);
        ScreenSetIsHome(true);
        VkKey_AttachScreen(scr, kScreenId,
                           VkKeyScreenDesc{HomeScreen::Create, OnVkKey, nullptr, OnBootLongPress,
                                           nullptr, nullptr, OnVkKeyLongPress, OnVkKeyPressUp});
        return scr;
    }

    // 上半：时分 + 日期天气（历史经典待机上半区；不含待办）
    home_hero::Mount(scr, status.height);
    home_hero::Start();

    const int indicator_h = pages > 1 ? HOME_PAGE_INDICATOR_H : 0;
    const lv_coord_t apps_content_h =
        HOME_APPS_PAD_TOP + HOME_APPS_VISIBLE_ROWS * HOME_APP_CARD_H +
        (HOME_APPS_VISIBLE_ROWS > 1 ? (HOME_APPS_VISIBLE_ROWS - 1) * HOME_APP_CARD_GAP_ROW : 0) +
        HOME_APPS_PAD_BOT;
    const lv_coord_t apps_bottom_inset =
        HOME_APPS_LIFT + (indicator_h > 0 ? indicator_h : 0);

    s_apps = lv_obj_create(scr);
    lv_obj_remove_style_all(s_apps);
    lv_obj_set_size(s_apps, LV_HOR_RES, apps_content_h);
    lv_obj_align(s_apps, LV_ALIGN_BOTTOM_MID, 0, -apps_bottom_inset);
    lv_obj_set_style_bg_color(s_apps, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(s_apps, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_top(s_apps, HOME_APPS_PAD_TOP, 0);
    lv_obj_set_style_pad_bottom(s_apps, HOME_APPS_PAD_BOT, 0);
    lv_obj_set_style_pad_left(s_apps, HOME_APP_CARD_MARGIN_H, 0);
    lv_obj_set_style_pad_right(s_apps, HOME_APP_CARD_MARGIN_H, 0);
    lv_obj_set_style_pad_column(s_apps, HOME_APP_CARD_GAP_COL, 0);
    lv_obj_set_style_pad_row(s_apps, HOME_APP_CARD_GAP_ROW, 0);
    lv_obj_set_flex_flow(s_apps, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(s_apps, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_clear_flag(s_apps, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(s_apps, LV_OBJ_FLAG_CLICKABLE);

    s_page_lbl = lv_label_create(scr);
    lv_label_set_text(s_page_lbl, "");
    lv_obj_set_style_text_font(s_page_lbl, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(s_page_lbl, lv_color_black(), 0);
    lv_obj_align(s_page_lbl, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_obj_clear_flag(s_page_lbl, LV_OBJ_FLAG_CLICKABLE);
    if (pages <= 1)
    {
        lv_obj_add_flag(s_page_lbl, LV_OBJ_FLAG_HIDDEN);
    }

    FillAppsPage();

    lv_obj_add_event_cb(scr, OnHomeDeleted, LV_EVENT_DELETE, nullptr);
    ScreenSetIsHome(true);
    VkKey_AttachScreen(scr, kScreenId,
                       VkKeyScreenDesc{HomeScreen::Create, OnVkKey, nullptr, OnBootLongPress,
                                       nullptr, nullptr, OnVkKeyLongPress, OnVkKeyPressUp});
    return scr;
}
