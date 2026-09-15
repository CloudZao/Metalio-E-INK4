# Define source files
set(SOURCES "audio/audio_codec.cc"
            "audio/audio_service.cc"
            "audio/codecs/no_audio_codec.cc"
            "audio/codecs/box_audio_codec.cc"
            "audio/codecs/es8311_audio_codec.cc"
            "audio/codecs/es8374_audio_codec.cc"
            "audio/codecs/es8388_audio_codec.cc"
            "audio/codecs/es8389_audio_codec.cc"
            "audio/codecs/dummy_audio_codec.cc"
            "audio/processors/audio_debugger.cc"
            "led/single_led.cc"
            "led/circular_strip.cc"
            "led/gpio_led.cc"
            "display/display.cc"
            "display/lcd_display.cc"
            "display/oled_display.cc"
            "display/lv_adapter_display.cc"
            "display/a2ui/a2ui.c"
            "display/a2ui/a2ui_render.c"
            "display/a2ui/a2ui_math.c"
            "display/a2ui/a2ui_image.cc"
            "display/a2ui/a2ui_img_cache.cc"
            "display/font/font_misans_light_50_2.c"
            "display/font/font_misans_regular_160_2.c"
            "display/font/epdfont.cc"
            "display/font/ttf_convert.cc"
            "display/font/font_loader.c"
            "display/font/fontpack_lvgl.cc"
            "display/screen/screen_common.cc"
            "display/screen/vk_key_handler.cc"
            "display/screen/vk_page_repeat.cc"
            "display/screen/boot_key_handler.cc"
            "display/screen/home_screen/home_screen.cc"
            "display/screen/home_screen/home_hero.cc"
            "display/screen/checklist_cache.cc"
            "display/screen/standby_screen/standby_screen.cc"
            "display/screen/standby_screen/standby_classic.cc"
            "display/screen/standby_screen/standby_wallpaper.cc"
            "display/screen/ota_upgrade_screen/ota_upgrade_screen.cc"
            "display/screen/ota_confirm_dialog/ota_confirm_dialog.cc"
            "display/screen/book_screen/book_screen.cc"
            "display/screen/book_screen/book_reader_prefs.cc"
            "display/screen/book_screen/book_cover_loader.cc"
            "display/screen/book_screen/book_cloud_library.cc"
            "display/screen/cloud_screen/cloud_screen.cc"
            "display/screen/cloud_screen/push_resources_library.cc"
            "display/screen/cloud_screen/wallpaper_cloud_library.cc"
            "display/screen/wallpaper_screen/wallpaper_screen.cc"
            "display/screen/wallpaper_screen/wallpaper_active.cc"
            "reader/zip_reader.cc"
            "reader/html_content.cc"
            "reader/book_library.cc"
            "reader/book_home_snapshot.cc"
            "reader/book_progress_cache.cc"
            "reader/book_library_warmup.cc"
            "reader/book_progress_sync.cc"
            "device_wifi_location.cc"
            "reader/image_util.cc"
            "reader/image_stream.cc"
            "reader/text_encoding.cc"
            "reader/epub_document.cc"
            "reader/ebook_document.cc"
            "reader/book_cover_sidecar.cc"
            "reader/http_download_file.cc"
            "reader/book_session.cc"
            "reader/txt_chapter.cc"
            "display/screen/task_screen/task_screen.cc"
            "display/screen/assistant_screen/assistant_screen.cc"
            "display/screen/assistant_screen/assistant_chat_store.cc"
            "display/screen/assistant_screen/assistant_boot_photo.cc"
            "display/screen/settings_screen/settings_screen.cc"
            "display/screen/settings_screen/settings_common.cc"
            "display/screen/settings_screen/settings_network_tab.cc"
            "display/screen/network_screen/network_screen.cc"
            "display/screen/settings_screen/settings_theme_tab.cc"
            "display/screen/settings_screen/settings_language_tab.cc"
            "display/screen/settings_screen/settings_haptic_tab.cc"
            "display/screen/settings_screen/settings_power_tab.cc"
            "display/screen/settings_screen/settings_conversation_tab.cc"
            "display/screen/settings_screen/settings_storage_tab.cc"
            "display/screen/settings_screen/settings_bluetooth_tab.cc"
            "display/screen/settings_screen/settings_test/settings_test_tab.cc"
            "display/screen/settings_screen/settings_test/settings_test_auto_screen.cc"
            "display/screen/settings_screen/settings_test/settings_test_common.cc"
            "display/screen/settings_screen/settings_test/settings_test_sensors.cc"
            "display/screen/settings_screen/settings_test/settings_test_wifi.cc"
            "display/screen/settings_screen/settings_test/settings_test_audio.cc"
            "display/screen/settings_screen/settings_test/settings_test_cell.cc"
            "display/screen/settings_screen/settings_test/settings_test_camera.cc"
            "display/screen/settings_screen/settings_test/settings_test_touch_screen.cc"
            "display/screen/settings_screen/settings_test/settings_test_battery_screen.cc"
            "display/screen/settings_screen/settings_test/settings_test_aging_screen.cc"
            "display/screen/settings_screen/settings_about_tab.cc"
            "display/screen/bluetooth_screen/bluetooth_screen.cc"
            "display/screen/touch_missing_screen/touch_missing_screen.cc"
            "display/lvgl_display/lvgl_display.cc"
            "display/emote_display.cc"
            "display/lvgl_display/emoji_collection.cc"
            "display/lvgl_display/lvgl_theme.cc"
            "display/lvgl_display/lvgl_font.cc"
            "display/lvgl_display/lvgl_image.cc"
            "display/lvgl_display/gif/lvgl_gif.cc"
            "display/lvgl_display/gif/gifdec.c"
            "display/lvgl_display/jpg/image_to_jpeg.cpp"
            "display/lvgl_display/jpg/jpeg_to_image.c"
            "protocols/protocol.cc"
            "protocols/mqtt_protocol.cc"
            "protocols/websocket_protocol.cc"
            "mcp_server.cc"
            "system_info.cc"
            "application.cc"
            "cloudzao_endpoints.c"
            "ota.cc"
            "ota_ignore.cc"
            "settings.cc"
            "haptic_feedback.cc"
            "device_state_event.cc"
            "assets.cc"
            "esp_debug_cloud.cc"
            "main.cc"
            )

set(INCLUDE_DIRS "." "display" "display/LUT" "display/a2ui" "display/font" "display/screen" "display/screen/settings_screen" "display/screen/settings_screen/settings_test" "display/lvgl_display" "display/lvgl_display/jpg" "audio" "protocols" "reader")

# Add board common files（GLOB 缓存：新增 boards/common/*.cc 后需 idf.py reconfigure）
file(GLOB BOARD_COMMON_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/boards/common/*.cc)
list(APPEND SOURCES ${BOARD_COMMON_SOURCES})
# 显式纳入，避免仅依赖 GLOB 缓存漏编
list(APPEND SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/boards/common/uvc_still_camera.cc)
list(REMOVE_DUPLICATES SOURCES)
list(APPEND INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/boards/common)
list(APPEND INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/boards/common/power_policy)
list(APPEND SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/boards/common/power_policy/power_policy.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/boards/common/power_policy/power_policy_standby_lp.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/boards/common/power_policy/power_hw.cc
)

idf_build_get_property(build_components BUILD_COMPONENTS)
# Function to find component dynamically by pattern
function(find_component_by_pattern PATTERN COMPONENT_VAR PATH_VAR)
    foreach(COMPONENT ${build_components})
        if(COMPONENT MATCHES "${PATTERN}")
            set(${COMPONENT_VAR} ${COMPONENT} PARENT_SCOPE)
            idf_component_get_property(COMPONENT_PATH ${COMPONENT} COMPONENT_DIR)
            set(${PATH_VAR} "${COMPONENT_PATH}" PARENT_SCOPE)
            break()
        endif()
    endforeach()
endfunction()

# Set default BUILTIN_TEXT_FONT and BUILTIN_ICON_FONT
set(BUILTIN_TEXT_FONT font_puhui_14_1)
set(BUILTIN_ICON_FONT font_awesome_14_1)

# Add board files according to BOARD_TYPE
if(CONFIG_BOARD_TYPE_METALIO_E_INK_4)
    set(BOARD_TYPE "metalio-e-ink-4")
    set(BUILTIN_TEXT_FONT font_puhui_basic_14_1)
    set(BUILTIN_ICON_FONT font_awesome_14_1)
endif()

file(GLOB BOARD_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_TYPE}/*.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_TYPE}/*.c
)
list(APPEND SOURCES ${BOARD_SOURCES})
list(APPEND INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_TYPE})

# Select audio processor according to Kconfig
if(CONFIG_USE_AUDIO_PROCESSOR)
    list(APPEND SOURCES "audio/processors/afe_audio_processor.cc")
else()
    list(APPEND SOURCES "audio/processors/no_audio_processor.cc")
endif()
if(CONFIG_IDF_TARGET_ESP32S3 OR CONFIG_IDF_TARGET_ESP32P4)
    list(APPEND SOURCES "audio/wake_words/afe_wake_word.cc")
    list(APPEND SOURCES "audio/wake_words/custom_wake_word.cc")
else()
    list(APPEND SOURCES "audio/wake_words/esp_wake_word.cc")
endif()

# Select language directory according to Kconfig
if(CONFIG_LANGUAGE_ZH_CN)
    set(LANG_DIR "zh-CN")
elseif(CONFIG_LANGUAGE_ZH_TW)
    set(LANG_DIR "zh-TW")
elseif(CONFIG_LANGUAGE_EN_US)
    set(LANG_DIR "en-US")
elseif(CONFIG_LANGUAGE_JA_JP)
    set(LANG_DIR "ja-JP")
elseif(CONFIG_LANGUAGE_KO_KR)
    set(LANG_DIR "ko-KR")
elseif(CONFIG_LANGUAGE_VI_VN)
    set(LANG_DIR "vi-VN")
elseif(CONFIG_LANGUAGE_TH_TH)
    set(LANG_DIR "th-TH")
elseif(CONFIG_LANGUAGE_DE_DE)
    set(LANG_DIR "de-DE")
elseif(CONFIG_LANGUAGE_FR_FR)
    set(LANG_DIR "fr-FR")
elseif(CONFIG_LANGUAGE_ES_ES)
    set(LANG_DIR "es-ES")
elseif(CONFIG_LANGUAGE_IT_IT)
    set(LANG_DIR "it-IT")
elseif(CONFIG_LANGUAGE_RU_RU)
    set(LANG_DIR "ru-RU")
elseif(CONFIG_LANGUAGE_AR_SA)
    set(LANG_DIR "ar-SA")
elseif(CONFIG_LANGUAGE_HI_IN)
    set(LANG_DIR "hi-IN")
elseif(CONFIG_LANGUAGE_PT_PT)
    set(LANG_DIR "pt-PT")
elseif(CONFIG_LANGUAGE_PL_PL)
    set(LANG_DIR "pl-PL")
elseif(CONFIG_LANGUAGE_CS_CZ)
    set(LANG_DIR "cs-CZ")
elseif(CONFIG_LANGUAGE_FI_FI)
    set(LANG_DIR "fi-FI")
elseif(CONFIG_LANGUAGE_TR_TR)
    set(LANG_DIR "tr-TR")
elseif(CONFIG_LANGUAGE_ID_ID)
    set(LANG_DIR "id-ID")
elseif(CONFIG_LANGUAGE_UK_UA)
    set(LANG_DIR "uk-UA")
elseif(CONFIG_LANGUAGE_RO_RO)
    set(LANG_DIR "ro-RO")
elseif(CONFIG_LANGUAGE_BG_BG)
    set(LANG_DIR "bg-BG")
elseif(CONFIG_LANGUAGE_CA_ES)
    set(LANG_DIR "ca-ES")
elseif(CONFIG_LANGUAGE_DA_DK)
    set(LANG_DIR "da-DK")
elseif(CONFIG_LANGUAGE_EL_GR)
    set(LANG_DIR "el-GR")
elseif(CONFIG_LANGUAGE_FA_IR)
    set(LANG_DIR "fa-IR")
elseif(CONFIG_LANGUAGE_FIL_PH)
    set(LANG_DIR "fil-PH")
elseif(CONFIG_LANGUAGE_HE_IL)
    set(LANG_DIR "he-IL")
elseif(CONFIG_LANGUAGE_HR_HR)
    set(LANG_DIR "hr-HR")
elseif(CONFIG_LANGUAGE_HU_HU)
    set(LANG_DIR "hu-HU")
elseif(CONFIG_LANGUAGE_MS_MY)
    set(LANG_DIR "ms-MY")
elseif(CONFIG_LANGUAGE_NB_NO)
    set(LANG_DIR "nb-NO")
elseif(CONFIG_LANGUAGE_NL_NL)
    set(LANG_DIR "nl-NL")
elseif(CONFIG_LANGUAGE_SK_SK)
    set(LANG_DIR "sk-SK")
elseif(CONFIG_LANGUAGE_SL_SI)
    set(LANG_DIR "sl-SI")
elseif(CONFIG_LANGUAGE_SV_SE)
    set(LANG_DIR "sv-SE")
elseif(CONFIG_LANGUAGE_SR_RS)
    set(LANG_DIR "sr-RS")
endif()

# Define generation path（UI 字表固定嵌入 zh-CN+en-US；音效仍跟 LANG_DIR）
set(LANG_JSON_ZH "${CMAKE_CURRENT_SOURCE_DIR}/assets/locales/zh-CN/language.json")
set(LANG_JSON_EN "${CMAKE_CURRENT_SOURCE_DIR}/assets/locales/en-US/language.json")
set(LANG_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/assets/lang_config.h")
set(LANG_TABLES_CC "${CMAKE_CURRENT_SOURCE_DIR}/assets/lang_tables.cc")
list(APPEND SOURCES "assets/lang_tables.cc")

# Collect current language audio files
file(GLOB LANG_SOUNDS ${CMAKE_CURRENT_SOURCE_DIR}/assets/locales/${LANG_DIR}/*.ogg)

# If not en-US, collect en-US audio files as fallback for missing files
if(NOT LANG_DIR STREQUAL "en-US")
    file(GLOB EN_US_SOUNDS ${CMAKE_CURRENT_SOURCE_DIR}/assets/locales/en-US/*.ogg)
    
    # Extract filenames (without path) from current language
    set(EXISTING_NAMES "")
    foreach(SOUND_FILE ${LANG_SOUNDS})
        get_filename_component(FILENAME ${SOUND_FILE} NAME)
        list(APPEND EXISTING_NAMES ${FILENAME})
    endforeach()
    
    # Only add en-US audio files that are missing in current language
    foreach(EN_SOUND ${EN_US_SOUNDS})
        get_filename_component(FILENAME ${EN_SOUND} NAME)
        if(NOT ${FILENAME} IN_LIST EXISTING_NAMES)
            list(APPEND LANG_SOUNDS ${EN_SOUND})
            message(STATUS "Using en-US fallback for missing audio: ${FILENAME}")
        endif()
    endforeach()
endif()

file(GLOB COMMON_SOUNDS ${CMAKE_CURRENT_SOURCE_DIR}/assets/common/*.ogg)
# 产测/老化等：与 Lang 音效同路径 EMBED，符号为 _binary_<basename>_ogg_{start,end}
file(GLOB FACTORY_TEST_SOUNDS ${CMAKE_CURRENT_SOURCE_DIR}/factory-test-assets/*.ogg)
if(FACTORY_TEST_SOUNDS)
    list(LENGTH FACTORY_TEST_SOUNDS _factory_test_sound_count)
    message(STATUS "Embedding ${_factory_test_sound_count} factory-test sound(s) from factory-test-assets/")
endif()

# 固件内嵌 UI 图（随 app OTA，不进 resources/assets 分区 bin）
# 符号：_binary_<basename 去点换下划线>_{start,end}
set(ASSISTANT_HINT_EN_A2I1
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/embedded/ic_s_assistant_hint_en.a2i1")

# If target chip is ESP32, exclude specific files to avoid build errors
if(CONFIG_IDF_TARGET_ESP32)
    list(REMOVE_ITEM SOURCES "audio/codecs/box_audio_codec.cc"
                             "audio/codecs/es8388_audio_codec.cc"
                             "audio/codecs/es8389_audio_codec.cc"
                             "led/gpio_led.cc"
                             "${CMAKE_CURRENT_SOURCE_DIR}/boards/common/esp32_camera.cc"
                             "display/lvgl_display/jpg/image_to_jpeg.cpp"
                             "display/lvgl_display/jpg/jpeg_to_image.c"
                             )
endif()

idf_component_register(SRCS ${SOURCES}
                    EMBED_FILES ${LANG_SOUNDS} ${COMMON_SOUNDS} ${FACTORY_TEST_SOUNDS}
                                ${ASSISTANT_HINT_EN_A2I1}
                    INCLUDE_DIRS ${INCLUDE_DIRS}
                    WHOLE_ARCHIVE
                    )

# xtensa-g++ ICE（cfgcleanup try_forward_edges）：book_screen.cc 过大时 -O2 偶发，降到 -O1
set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/display/screen/book_screen/book_screen.cc"
    PROPERTIES COMPILE_OPTIONS "-O1"
)

# SDMMC / FATFS（SdCardManager）；不放进 PRIV_REQUIRES，避免搅动 managed_components 依赖可见性
# zlib：reader/ZipReader 解压 EPUB；EbookDocument 分块 inflate .ebook
# esp-ml307 / esp-wifi-connect：本地 vendored 组件（components/），不再走 Component Manager
# esp_tinyusb：设置页模拟 U 盘（MSC）；仅 S3 拉取，链接 usb PHY
target_link_libraries(${COMPONENT_LIB} PRIVATE idf::fatfs idf::sdmmc idf::esp_driver_sdmmc idf::espressif__zlib idf::esp_partition idf::espcoredump idf::esp-ml307 idf::esp-wifi-connect idf::espressif__freetype)
if(TARGET idf::espressif__esp_tinyusb)
    target_link_libraries(${COMPONENT_LIB} PRIVATE idf::espressif__esp_tinyusb idf::usb)
endif()
# USB UVC 单帧静拍（UsbUvcStill）；与 TinyUSB 同依赖 idf::usb
if(TARGET idf::espressif__usb_host_uvc)
    target_link_libraries(${COMPONENT_LIB} PRIVATE idf::espressif__usb_host_uvc idf::usb)
endif()

# I1 字形变细：lv_draw_sw_blend_to_i1.c 需要找到 main/display/epd_i1_glyph_thin.h
idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
if(TARGET ${lvgl_lib})
    target_include_directories(${lvgl_lib} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/display")
endif()

# Use target_compile_definitions to define BOARD_TYPE, BOARD_NAME
# If BOARD_NAME is empty, use BOARD_TYPE
if(NOT BOARD_NAME)
    set(BOARD_NAME ${BOARD_TYPE})
endif()
target_compile_definitions(${COMPONENT_LIB}
                    PRIVATE BOARD_TYPE=\"${BOARD_TYPE}\" BOARD_NAME=\"${BOARD_NAME}\"
                    PRIVATE BUILTIN_TEXT_FONT=${BUILTIN_TEXT_FONT} BUILTIN_ICON_FONT=${BUILTIN_ICON_FONT}
                    )

# Add generation rules
add_custom_command(
    OUTPUT ${LANG_HEADER} ${LANG_TABLES_CC}
    COMMAND python ${PROJECT_DIR}/scripts/gen_lang.py
            --language "${LANG_DIR}"
            --output "${LANG_HEADER}"
            --cc-output "${LANG_TABLES_CC}"
    DEPENDS
        ${LANG_JSON_ZH}
        ${LANG_JSON_EN}
        ${PROJECT_DIR}/scripts/gen_lang.py
    COMMENT "Generating runtime zh-CN/en-US language tables (sounds: ${LANG_DIR})"
)

# Force build generation dependencies
add_custom_target(lang_header ALL
    DEPENDS ${LANG_HEADER} ${LANG_TABLES_CC}
)
add_dependencies(${COMPONENT_LIB} lang_header)

# Find ESP-SR component dynamically
find_component_by_pattern("espressif__esp-sr" ESP_SR_COMPONENT ESP_SR_COMPONENT_PATH)
if(ESP_SR_COMPONENT_PATH)
    set(ESP_SR_MODEL_PATH "${ESP_SR_COMPONENT_PATH}/model")
endif()

# Find xiaozhi-fonts component dynamically
find_component_by_pattern("xiaozhi-fonts" XIAOZHI_FONTS_COMPONENT XIAOZHI_FONTS_COMPONENT_PATH)
if(XIAOZHI_FONTS_COMPONENT_PATH)
    set(XIAOZHI_FONTS_PATH "${XIAOZHI_FONTS_COMPONENT_PATH}")
endif()

# Function to build default assets based on configuration
function(build_default_assets_bin)
    # Set output path for generated assets.bin
    set(GENERATED_ASSETS_BIN "${CMAKE_BINARY_DIR}/generated_assets.bin")
    
    # Prepare arguments for build script
    set(BUILD_ARGS
        "--sdkconfig" "${SDKCONFIG}"
        "--output" "${GENERATED_ASSETS_BIN}"
    )
    
    # Add builtin text font if defined
    if(BUILTIN_TEXT_FONT)
        list(APPEND BUILD_ARGS "--builtin_text_font" "${BUILTIN_TEXT_FONT}")
    endif()
    
    # Add default emoji collection if defined
    if(DEFAULT_EMOJI_COLLECTION)
        list(APPEND BUILD_ARGS "--emoji_collection" "${DEFAULT_EMOJI_COLLECTION}")
    endif()
    
    # Add default assets extra files if defined
    if(DEFAULT_ASSETS_EXTRA_FILES)
        list(APPEND BUILD_ARGS "--extra_files" "${DEFAULT_ASSETS_EXTRA_FILES}")
    endif()
    
    list(APPEND BUILD_ARGS "--esp_sr_model_path" "${ESP_SR_MODEL_PATH}")
    list(APPEND BUILD_ARGS "--xiaozhi_fonts_path" "${XIAOZHI_FONTS_PATH}")
    
    # Create custom command to build assets
    add_custom_command(
        OUTPUT ${GENERATED_ASSETS_BIN}
        COMMAND python ${PROJECT_DIR}/scripts/build_default_assets.py ${BUILD_ARGS}
        DEPENDS
            ${SDKCONFIG}
            ${PROJECT_DIR}/scripts/build_default_assets.py
        COMMENT "Building default assets.bin based on configuration"
        VERBATIM
    )
    
    # Create target for generated assets
    add_custom_target(generated_default_assets ALL
        DEPENDS ${GENERATED_ASSETS_BIN}
    )
    
    # Set the generated file path in parent scope
    set(GENERATED_ASSETS_LOCAL_FILE ${GENERATED_ASSETS_BIN} PARENT_SCOPE)
    
    message(STATUS "Default assets build configured: ${GENERATED_ASSETS_BIN}")
endfunction()


# Function to get local assets file path (handles both URL and local file)
function(get_assets_local_file assets_source assets_local_file_var)
    # Check if it's a URL (starts with http:// or https://)
    if(assets_source MATCHES "^https?://")
        # It's a URL, download it
        get_filename_component(ASSETS_FILENAME "${assets_source}" NAME)
        set(ASSETS_LOCAL_FILE "${CMAKE_BINARY_DIR}/${ASSETS_FILENAME}")
        set(ASSETS_TEMP_FILE "${CMAKE_BINARY_DIR}/${ASSETS_FILENAME}.tmp")
        
        # Check if local file exists
        if(EXISTS ${ASSETS_LOCAL_FILE})
            message(STATUS "Assets file ${ASSETS_FILENAME} already exists, skipping download")
        else()
            message(STATUS "Downloading ${ASSETS_FILENAME}")
            
            # Clean up any existing temp file
            if(EXISTS ${ASSETS_TEMP_FILE})
                file(REMOVE ${ASSETS_TEMP_FILE})
            endif()
            
            # Download to temporary file first
            file(DOWNLOAD ${assets_source} ${ASSETS_TEMP_FILE}
                 STATUS DOWNLOAD_STATUS)
            list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
            if(NOT STATUS_CODE EQUAL 0)
                # Clean up temp file on failure
                if(EXISTS ${ASSETS_TEMP_FILE})
                    file(REMOVE ${ASSETS_TEMP_FILE})
                endif()
                message(FATAL_ERROR "Failed to download ${ASSETS_FILENAME} from ${assets_source}")
            endif()
            
            # Move temp file to final location (atomic operation)
            file(RENAME ${ASSETS_TEMP_FILE} ${ASSETS_LOCAL_FILE})
            message(STATUS "Successfully downloaded ${ASSETS_FILENAME}")
        endif()
    else()
        # It's a local file path
        if(IS_ABSOLUTE "${assets_source}")
            set(ASSETS_LOCAL_FILE "${assets_source}")
        else()
            set(ASSETS_LOCAL_FILE "${CMAKE_CURRENT_SOURCE_DIR}/${assets_source}")
        endif()
        
        # Check if local file exists
        if(NOT EXISTS ${ASSETS_LOCAL_FILE})
            message(FATAL_ERROR "Assets file not found: ${ASSETS_LOCAL_FILE}")
        endif()
        
        message(STATUS "Using assets file: ${ASSETS_LOCAL_FILE}")
    endif()
    
    set(${assets_local_file_var} ${ASSETS_LOCAL_FILE} PARENT_SCOPE)
endfunction()


partition_table_get_partition_info(size "--partition-name assets" "size")
partition_table_get_partition_info(offset "--partition-name assets" "offset")
if ("${size}" AND "${offset}")
    # Flash assets based on configuration
    if(CONFIG_FLASH_DEFAULT_ASSETS)
        # Build default assets based on configuration
        build_default_assets_bin()
        esptool_py_flash_to_partition(flash "assets" "${GENERATED_ASSETS_LOCAL_FILE}")
        message(STATUS "Generated default assets flash configured: ${GENERATED_ASSETS_LOCAL_FILE} -> assets partition")
    elseif(CONFIG_FLASH_CUSTOM_ASSETS)
        # Flash custom assets
        get_assets_local_file("${CONFIG_CUSTOM_ASSETS_FILE}" ASSETS_LOCAL_FILE)
        esptool_py_flash_to_partition(flash "assets" "${ASSETS_LOCAL_FILE}")
        message(STATUS "Custom assets flash configured: ${ASSETS_LOCAL_FILE} -> assets partition")
    elseif(CONFIG_FLASH_NONE_ASSETS)
        message(STATUS "Assets flashing disabled (FLASH_NONE_ASSETS)")
    endif()
else()
    message(STATUS "Assets partition not found, using v1 partition table")
endif()

# Flash fontpack into font_data so idf.py flash / flasher_args.json include it
partition_table_get_partition_info(font_size "--partition-name font_data" "size")
partition_table_get_partition_info(font_offset "--partition-name font_data" "offset")
if("${font_size}" AND "${font_offset}")
    set(FONTPACK_FILE "${PROJECT_DIR}/use_font/font.fontpack")
    if(EXISTS "${FONTPACK_FILE}")
        esptool_py_flash_to_partition(flash "font_data" "${FONTPACK_FILE}")
        message(STATUS "Fontpack flash configured: ${FONTPACK_FILE} -> font_data @ ${font_offset}")
    else()
        message(WARNING "fontpack missing: ${FONTPACK_FILE} (font_data will not be flashed)")
    endif()
else()
    message(STATUS "font_data partition not found, skip fontpack flash")
endif()

spiffs_create_partition_assets(
    resources
    "./xingzhi-assets"
    FLASH_IN_PROJECT
    MMAP_FILE_SUPPORT_FORMAT ".jpg,.png,.eaf,.a2i1"
    MMAP_SUPPORT_SJPG
    MMAP_SUPPORT_SPNG
    MMAP_SPLIT_HEIGHT 16
) 