/**
 * @file epd_i1_glyph_thin.h
 * @brief I1 字形落墨：对齐 CrossPoint Reader 的 BW「非白即黑」
 *
 * CrossPoint（GfxRenderer::renderCharImpl）2bpp 阅读字：
 *   raw: 0白 1浅灰 2深灰 3黑
 *   bmpVal = 3 - raw
 *   BW 模式：bmpVal < 3 → 全部画黑（即 raw 1/2/3 全实心）
 * 随后灰度 AA 波形把浅/深灰从实黑「削」成灰边，观感才细且清。
 *
 * 本项目保持 I1 + OTP 局刷、不做 4 灰二次刷时，应复刻 BW 这一步：
 * 任意覆盖度 > 0 都实心落墨 → 笔画实、清楚。勿再提高阈值或 Bayer 挖空。
 */

#ifndef EPD_I1_GLYPH_THIN_H
#define EPD_I1_GLYPH_THIN_H

#include <stdint.h>

/**
 * A2 mask：0 / 85 / 170 / 255  ↔ CrossPoint raw 0 / 1 / 2 / 3
 * @return 1 落墨，0 不画
 */
static inline int epd_i1_glyph_mask_hit(uint8_t mask_val, int32_t abs_x, int32_t abs_y)
{
    (void)abs_x;
    (void)abs_y;
    /* CrossPoint BW：非白即黑 */
    return mask_val > 0;
}

#endif /* EPD_I1_GLYPH_THIN_H */
