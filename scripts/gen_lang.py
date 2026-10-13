#!/usr/bin/env python3
"""Generate runtime zh-CN/en-US string tables."""
import argparse
import json
import os

RUNTIME_LOCALES = ("en-US", "zh-CN")


def load_language_json(path):
    with open(path, "r", encoding="utf-8") as f:
        data = json.load(f)
    if "language" not in data or "strings" not in data:
        raise ValueError(f"Invalid JSON structure: {path}")
    return data


def escape_c_string(value):
    return value.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n")


def locale_var(code):
    return code.replace("-", "_").lower()


def merge_strings(assets_dir):
    """Load en-US + zh-CN; union keys with en-US as fallback per locale."""
    tables = {}
    for code in RUNTIME_LOCALES:
        path = os.path.join(assets_dir, "locales", code, "language.json")
        if not os.path.exists(path):
            raise FileNotFoundError(f"Language file not found: {path}")
        tables[code] = load_language_json(path).get("strings", {})

    base = dict(tables["en-US"])
    all_keys = sorted(set(base) | set(tables["zh-CN"]), key=lambda k: k.upper())
    merged = {}
    for code in RUNTIME_LOCALES:
        merged[code] = {key: tables[code].get(key, base.get(key, "")) for key in all_keys}
        missing = [k for k in all_keys if k not in tables[code]]
        print(f"Locale {code}: {len(tables[code])} keys, {len(missing)} fallback from en-US")
    return all_keys, merged


def generate(sound_lang, header_path, cc_path):
    assets_dir = os.path.dirname(header_path)
    if os.path.basename(assets_dir) != "assets":
        raise ValueError(f"Expected header under assets/: {header_path}")

    keys, merged = merge_strings(assets_dir)
    key_macros = [k.upper() for k in keys]
    orig_by_upper = {k.upper(): k for k in keys}

    decl_lines = [f"        extern const char* {k};" for k in key_macros]
    font_guard = locale_var(sound_lang)

    header = f"""// Auto-generated language config (runtime zh-CN/en-US strings)
// Default language: {sound_lang}
#pragma once

#ifndef {font_guard}
    #define {font_guard}  // 默认字体侧语言
#endif

#ifndef LANG_DEFAULT_CODE
#define LANG_DEFAULT_CODE "{sound_lang}"
#endif

namespace Lang {{
    /** 当前 UI 语言码（zh-CN / en-US），可热切换 */
    extern const char* CODE;

    /**
     * @brief 切换 UI 语言并重绑 Strings 指针
     * @param code "zh-CN" 或 "en-US"；其它值回退 en-US
     * @param persist true 时写入 NVS ui/language
     * @return 语言实际发生变化时为 true
     */
    bool SetLanguage(const char* code, bool persist = true);

    /** @brief 当前语言码，同 CODE */
    const char* Current();

    /** @brief 从 NVS 加载语言；无记录则用 LANG_DEFAULT_CODE */
    void InitFromNvs();

    // 字符串资源（指针随 SetLanguage 重绑）
    namespace Strings {{
{chr(10).join(decl_lines)}
    }}
}}
"""

    table_structs = []
    for code in RUNTIME_LOCALES:
        var = locale_var(code)
        fields = []
        for ku in key_macros:
            orig = orig_by_upper[ku]
            value = escape_c_string(merged[code][orig])
            fields.append(f'        "{value}",  /* {ku} */')
        table_structs.append(
            f"static const char* const kLiterals_{var}[] = {{\n"
            + "\n".join(fields)
            + "\n};\n"
        )

    apply_assigns = "\n".join(
        f"    Strings::{ku} = literals[static_cast<size_t>(LangStringId::{ku})];"
        for ku in key_macros
    )
    enum_entries = ",\n".join(f"    {ku}" for ku in key_macros)
    ptr_defs = "\n".join(f'const char* Strings::{ku} = "";' for ku in key_macros)

    cc = f"""// Auto-generated runtime language tables — do not edit
#include "assets/lang_config.h"
#include "settings.h"

#include <cstring>
#include <string>

namespace Lang {{
namespace {{

enum class LangStringId : size_t {{
{enum_entries},
    kCount
}};

{"".join(table_structs)}
void ApplyLiterals(const char* const* literals) {{
{apply_assigns}
}}

const char* const* LiteralsFor(const char* code) {{
    if (code != nullptr && std::strcmp(code, "zh-CN") == 0) {{
        return kLiterals_zh_cn;
    }}
    return kLiterals_en_us;
}}

const char* NormalizeCode(const char* code) {{
    if (code != nullptr && std::strcmp(code, "zh-CN") == 0) {{
        return "zh-CN";
    }}
    return "en-US";
}}

}}  // namespace

const char* CODE = LANG_DEFAULT_CODE;

{ptr_defs}

const char* Current() {{
    return CODE;
}}

bool SetLanguage(const char* code, bool persist) {{
    const char* normalized = NormalizeCode(code);
    const bool changed = (CODE == nullptr) || (std::strcmp(CODE, normalized) != 0);
    ApplyLiterals(LiteralsFor(normalized));
    CODE = normalized;
    if (persist) {{
        Settings settings("ui", true);
        settings.SetString("language", normalized);
    }}
    return changed;
}}

void InitFromNvs() {{
    Settings settings("ui", false);
    const std::string saved = settings.GetString("language", "");
    if (saved.empty()) {{
        SetLanguage(LANG_DEFAULT_CODE, false);
    }} else {{
        SetLanguage(saved.c_str(), false);
    }}
}}

}}  // namespace Lang

namespace {{
struct LangStaticInit {{
    LangStaticInit() {{
        Lang::SetLanguage(LANG_DEFAULT_CODE, false);
    }}
}};
LangStaticInit g_lang_static_init;
}}  // namespace
"""

    os.makedirs(os.path.dirname(header_path), exist_ok=True)
    with open(header_path, "w", encoding="utf-8") as f:
        f.write(header)
    with open(cc_path, "w", encoding="utf-8") as f:
        f.write(cc)
    print(f"Wrote {header_path}")
    print(f"Wrote {cc_path}")
    print(f"Keys: {len(key_macros)}; default lang: {sound_lang}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Generate runtime zh-CN/en-US language tables"
    )
    parser.add_argument(
        "--language",
        required=True,
        help="Default language code (e.g. zh-CN)",
    )
    parser.add_argument("--output", required=True, help="Output header path (lang_config.h)")
    parser.add_argument(
        "--cc-output",
        default="",
        help="Output .cc path (default: same dir as header, lang_tables.cc)",
    )
    args = parser.parse_args()
    cc_out = args.cc_output or os.path.join(os.path.dirname(args.output), "lang_tables.cc")
    try:
        generate(args.language, args.output, cc_out)
    except Exception as e:
        print(f"Error: {e}")
        raise SystemExit(1)
