// Auto-generated runtime language tables — do not edit
#include "assets/lang_config.h"
#include "settings.h"

#include <cstring>
#include <string>

namespace Lang {
namespace {

enum class LangStringId : size_t {
    ACCESS_VIA_BROWSER,
    ACTIVATION,
    ACTIVATION_CODE_FMT,
    BATTERY_CHARGING,
    BATTERY_FULL,
    BATTERY_LOW,
    BATTERY_NEED_CHARGE,
    BOOK_BAD_ARG,
    BOOK_BLANK,
    BOOK_BODY,
    BOOK_CANCELLED,
    BOOK_CHAPTER_FMT,
    BOOK_CHAPTER_NO_TEXT,
    BOOK_CHAPTER_PROGRESS,
    BOOK_CHECKSUM_FAIL,
    BOOK_CONN_FAIL,
    BOOK_CONTINUE,
    BOOK_COUNT_FMT,
    BOOK_COVER_TOO_LARGE,
    BOOK_DELETE_FAILED,
    BOOK_DETAIL,
    BOOK_DOWNLOAD_FAIL,
    BOOK_DURATION_HM_FMT,
    BOOK_DURATION_H_FMT,
    BOOK_DURATION_LT1MIN_FMT,
    BOOK_DURATION_M_FMT,
    BOOK_EMPTY_COVER,
    BOOK_EMPTY_FILE,
    BOOK_EMPTY_HOME_FMT,
    BOOK_EMPTY_SHELF_FMT,
    BOOK_FILE_TOO_LARGE,
    BOOK_FINISHED,
    BOOK_FONT_EMPTY,
    BOOK_FONT_NAME_INVALID,
    BOOK_FONT_TITLE,
    BOOK_IMAGE_PLACEHOLDER,
    BOOK_LAYOUT_BUSY,
    BOOK_LAYOUT_DONE,
    BOOK_LINE_GAP,
    BOOK_MARGIN,
    BOOK_MARGIN_NARROW,
    BOOK_MARGIN_STANDARD,
    BOOK_MARGIN_VERY_WIDE,
    BOOK_MARGIN_WIDE,
    BOOK_NET_NOT_READY,
    BOOK_NO_CONTINUE,
    BOOK_NO_COVER_URL,
    BOOK_NO_DOWNLOAD_URL,
    BOOK_NO_LOCAL_FILE,
    BOOK_NO_NETWORK,
    BOOK_NO_RECENT,
    BOOK_NO_SD,
    BOOK_NO_SD_SHORT,
    BOOK_OPENING,
    BOOK_OPEN_FAILED,
    BOOK_OPEN_FAILED_CHAPTER,
    BOOK_OPEN_FAILED_EPUB,
    BOOK_OPEN_FAILED_TASK,
    BOOK_OPEN_FAILED_TXT,
    BOOK_OUT_OF_MEMORY,
    BOOK_PAGE_LOAD_FAILED,
    BOOK_PATH_INVALID,
    BOOK_READ_DURATION,
    BOOK_READ_FAIL,
    BOOK_READ_FILE_FAIL,
    BOOK_READ_PCT_FMT,
    BOOK_READ_PROGRESS,
    BOOK_RECENT,
    BOOK_SAVE_FAIL,
    BOOK_SELECTED_FMT,
    BOOK_SHELF_TITLE,
    BOOK_SPACING_COMPACT,
    BOOK_SPACING_RELAXED,
    BOOK_SPACING_STANDARD,
    BOOK_SPACING_VERY_RELAXED,
    BOOK_START,
    BOOK_STORAGE_UNAVAIL,
    BOOK_SYNC_EMPTY_BODY,
    BOOK_TOC,
    BOOK_TOC_EMPTY,
    BOOK_TOC_PAGE_FMT,
    BOOK_TODAY_DURATION,
    BOOK_UNDERLINE,
    BOOK_UNDERLINE_DASHED,
    BOOK_UNDERLINE_SOLID,
    BOOK_WRITE_FAIL,
    BT_CALL_BTN,
    BT_CALL_MODE_SCO,
    BT_CONNECTING,
    BT_CONNECTING_FMT,
    BT_CONNECT_OK,
    BT_CONNECT_TIMEOUT,
    BT_DESC,
    BT_FOUND_FMT,
    BT_MODE1,
    BT_MODE1_ACTIVE,
    BT_MODE1_SET,
    BT_MODE2,
    BT_MODE2_SET,
    BT_MODE3,
    BT_MODE3_SET,
    BT_MUSIC_BTN,
    BT_MUSIC_MODE_SCO,
    BT_NEED_CONNECT,
    BT_NEED_MODE2,
    BT_PWR_RESET_OK,
    BT_PWR_RESET_UNSUP,
    BT_RESET_BTN,
    BT_RESET_HINT,
    BT_SCANNING,
    BT_SCAN_BTN,
    BT_SCAN_DONE_FMT,
    BT_SCAN_START,
    BT_SELECT_MODE,
    BT_SWITCH_CALL,
    BT_SWITCH_MODE1,
    BT_SWITCH_MODE2,
    BT_SWITCH_MODE3,
    BT_SWITCH_MUSIC,
    BT_TITLE,
    BT_UART_NOT_INIT,
    CHECKING_NEW_VERSION,
    CHECK_NEW_VERSION_FAILED,
    CLOUD_API_ERROR,
    CLOUD_CANCELLED,
    CLOUD_CANCELLING,
    CLOUD_CONFIRM_DL_FMT,
    CLOUD_CONNECTING_NET,
    CLOUD_CONNECT_NET,
    CLOUD_CONN_FAIL,
    CLOUD_COVER_FAIL,
    CLOUD_DATA_FORMAT_ERR,
    CLOUD_DECODE_FAIL,
    CLOUD_DELETE_FAIL,
    CLOUD_DOWNLOAD,
    CLOUD_DOWNLOADING,
    CLOUD_DOWNLOAD_FAIL,
    CLOUD_EMPTY_ALL,
    CLOUD_EMPTY_BOOK,
    CLOUD_EMPTY_FONT,
    CLOUD_EMPTY_WALLPAPER,
    CLOUD_FETCHING_LIST,
    CLOUD_FILE_INVALID,
    CLOUD_IN_QUEUE,
    CLOUD_JSON_CREATE_FAIL,
    CLOUD_JSON_PARSE_FAIL,
    CLOUD_JSON_SERIALIZE_FAIL,
    CLOUD_LIST_TOO_LARGE,
    CLOUD_LOADING,
    CLOUD_LOAD_COVER,
    CLOUD_LOAD_FAIL,
    CLOUD_MISSING_TASK_ID,
    CLOUD_NEED_WIFI_CFG,
    CLOUD_NET_NOT_READY,
    CLOUD_NO_COVER,
    CLOUD_NO_DOWNLOAD_URL,
    CLOUD_NO_LOCAL_FILE,
    CLOUD_NO_NETWORK,
    CLOUD_NO_SD,
    CLOUD_PATH_INVALID,
    CLOUD_PENDING,
    CLOUD_PLEASE_WAIT,
    CLOUD_PREVIEW_FAIL,
    CLOUD_PREVIEW_START_FAIL,
    CLOUD_PREVIEW_URL_LONG,
    CLOUD_PUSH_BOOK,
    CLOUD_PUSH_FONT,
    CLOUD_PUSH_WALLPAPER,
    CLOUD_READ_TIMEOUT,
    CLOUD_REFRESH,
    CLOUD_REFRESHED,
    CLOUD_REFRESHING,
    CLOUD_REQUEST_FAIL,
    CLOUD_SAVE,
    CLOUD_SAVED_0_1,
    CLOUD_SAVED_FMT,
    CLOUD_SAVED_SYNC_FAIL,
    CLOUD_SAVE_FAIL,
    CLOUD_SAVE_LOCAL,
    CLOUD_SAVING,
    CLOUD_SAVING_NAME_FMT,
    CLOUD_SELECTED_FMT,
    CLOUD_SELECT_FIRST,
    CLOUD_STORAGE_UNAVAIL,
    CLOUD_SYNC_FAIL,
    CLOUD_TAB_ALL,
    CLOUD_TAB_BOOK,
    CLOUD_TAB_FONT,
    CLOUD_TAB_WALLPAPER,
    CLOUD_TASK_FAIL,
    CLOUD_TYPE_BOOK,
    CLOUD_TYPE_FONT,
    CLOUD_TYPE_WALLPAPER,
    CLOUD_UNKNOWN_TYPE,
    CLOUD_WAIT_DOWNLOAD,
    COMMON_CANCEL,
    COMMON_DELETE,
    COMMON_EMPTY,
    COMMON_FAILED,
    COMMON_LOADING,
    COMMON_OFF,
    COMMON_OK,
    COMMON_ON,
    COMMON_REMOVE,
    COMMON_RETRY,
    COMMON_SELECT_ALL,
    COMMON_SUCCESS,
    COMMON_UNKNOWN,
    CONNECTED_TO,
    CONNECTING,
    CONNECTION_SUCCESSFUL,
    CONNECT_TO,
    CONNECT_TO_HOTSPOT,
    DETECTING_MODULE,
    DOWNLOAD_ASSETS_FAILED,
    ENTERING_WIFI_CONFIG_MODE,
    ERROR,
    FOUND_NEW_ASSETS,
    HELLO_MY_FRIEND,
    HOME_APP_ASSISTANT,
    HOME_APP_BOOK,
    HOME_APP_CLOUD,
    HOME_APP_SETTINGS,
    HOME_APP_TASK,
    HOME_APP_WALLPAPER,
    HOME_DATE_FMT,
    HOME_DATE_PLACEHOLDER,
    HOME_DATE_SLASH_FMT,
    HOME_DATE_SLASH_PLACEHOLDER,
    HOME_WDAY_FRI,
    HOME_WDAY_MON,
    HOME_WDAY_SAT,
    HOME_WDAY_SUN,
    HOME_WDAY_THU,
    HOME_WDAY_TUE,
    HOME_WDAY_WED,
    INFO,
    INITIALIZING,
    LISTENING,
    LOADING_ASSETS,
    LOADING_PROTOCOL,
    MAX_VOLUME,
    MUTED,
    NEED_WIFI_CFG,
    NETWORK_AUTH_OPEN,
    NETWORK_AUTH_SECURE,
    NETWORK_BUSY_CONNECT,
    NETWORK_CANCEL,
    NETWORK_CLEARED_OK,
    NETWORK_CLEAR_ALL,
    NETWORK_CONNECT,
    NETWORK_CONNECTED_FMT,
    NETWORK_CONNECTING_FMT,
    NETWORK_CONNECT_FAIL,
    NETWORK_CONNECT_TASK_FAIL,
    NETWORK_CONNECT_TIMEOUT,
    NETWORK_CONNECT_TIMEOUT_HINT,
    NETWORK_CONNECT_TO_FMT,
    NETWORK_DEFAULT_FMT,
    NETWORK_DELETE,
    NETWORK_DELETED_OK,
    NETWORK_ERR_AP_GONE,
    NETWORK_ERR_ASSOC,
    NETWORK_ERR_BAD_PASSWORD,
    NETWORK_ERR_REASON_FMT,
    NETWORK_ERR_WEAK,
    NETWORK_NEARBY_EMPTY,
    NETWORK_PWD_HINT,
    NETWORK_PWD_PLACEHOLDER,
    NETWORK_PWD_TOO_LONG,
    NETWORK_SAVED_EMPTY,
    NETWORK_SCAN,
    NETWORK_SCANNING,
    NETWORK_SCAN_DONE_FMT,
    NETWORK_SCAN_FAIL,
    NETWORK_SCAN_TASK_FAIL,
    NETWORK_SCAN_TIMEOUT,
    NETWORK_SET_DEFAULT,
    NETWORK_SET_DEFAULT_OK,
    NETWORK_SHOW_PWD,
    NETWORK_SSID_INVALID,
    NETWORK_TAB_NEARBY,
    NETWORK_TAB_SAVED,
    NETWORK_TITLE,
    NETWORK_WIFI_INIT,
    NETWORK_WIFI_INIT_FAIL,
    NEW_VERSION,
    OTA_ALREADY_LATEST,
    OTA_CHECK_FAILED,
    OTA_CONFIRM_FMT,
    OTA_CUR_VER_FMT,
    OTA_HINT,
    OTA_IGNORE_VERSION,
    OTA_MANUAL,
    OTA_NEW_VER_FMT,
    OTA_REMIND_LATER,
    OTA_SUCCESS_REBOOT,
    OTA_TITLE,
    OTA_UPGRADE,
    OTA_UPGRADE_NOW,
    PHONE_BUSY,
    PHONE_CHECKING_NET,
    PHONE_CHECK_CELL,
    PHONE_CONFIRM_SIM,
    PHONE_DIAL,
    PHONE_DIAL_FAIL,
    PHONE_HANGUP,
    PHONE_INTERNAL_SIM,
    PHONE_IN_CALL,
    PHONE_NO_4G,
    PHONE_WIFI_BLOCK,
    PIN_ERROR,
    PLEASE_WAIT,
    POWERED_OFF,
    RECORD_ASR_DONE,
    RECORD_ASR_EMPTY,
    RECORD_ASR_FAIL,
    RECORD_ASR_HINT,
    RECORD_ASR_HTTP_FMT,
    RECORD_ASR_NEED_NET,
    RECORD_ASR_PARSE_FAIL,
    RECORD_ASR_PENDING,
    RECORD_ASR_REQ_FAIL,
    RECORD_ASR_START_FAIL,
    RECORD_ASR_UPLOADED_HINT,
    RECORD_AUDIO_BUSY,
    RECORD_AUDIO_NOT_READY,
    RECORD_AUDIO_STARTING,
    RECORD_BTN_ASR,
    RECORD_BTN_PLAY,
    RECORD_BTN_SAVING,
    RECORD_BTN_START,
    RECORD_BTN_STOP,
    RECORD_BTN_STOP_PLAY,
    RECORD_DELETED,
    RECORD_DELETE_FAIL,
    RECORD_DURATION_SEC_FMT,
    RECORD_EMPTY,
    RECORD_FILE_CORRUPT,
    RECORD_FILE_TOO_LARGE,
    RECORD_HINT_START,
    RECORD_INSERT_SD,
    RECORD_MAX_MIN_FMT,
    RECORD_META_FMT,
    RECORD_MKDIR_FAIL,
    RECORD_NET_UNAVAIL,
    RECORD_NO_SD_HINT,
    RECORD_OOM,
    RECORD_OPEN_FAIL,
    RECORD_PLAYING,
    RECORD_PLAY_END,
    RECORD_PLAY_START_FAIL,
    RECORD_PLAY_STOPPED,
    RECORD_PLEASE_WAIT,
    RECORD_READ_FAIL,
    RECORD_RECORDING,
    RECORD_SAVED_SD,
    RECORD_STOP_FIRST,
    RECORD_SUMMARY,
    RECORD_TAB_LIST,
    RECORD_TAB_REC,
    RECORD_TASK_START_FAIL,
    RECORD_TOO_SHORT,
    RECORD_UPLOADING,
    RECORD_WRITE_FAIL,
    REGISTERING_NETWORK,
    REG_ERROR,
    RTC_MODE_OFF,
    RTC_MODE_ON,
    SCANNING_WIFI,
    SERVER_ERROR,
    SERVER_NOT_CONNECTED,
    SERVER_NOT_FOUND,
    SERVER_TIMEOUT,
    SETTINGS_ABOUT_BUILD,
    SETTINGS_ABOUT_CHIP,
    SETTINGS_ABOUT_CORES,
    SETTINGS_ABOUT_CORES_FMT,
    SETTINGS_ABOUT_FLASH,
    SETTINGS_ABOUT_FW,
    SETTINGS_ABOUT_FW_CHECK,
    SETTINGS_ABOUT_FW_VER_FMT,
    SETTINGS_ABOUT_MAC,
    SETTINGS_ABOUT_MODEL,
    SETTINGS_ABOUT_NONE,
    SETTINGS_ABOUT_PSRAM,
    SETTINGS_CONV_BUSY,
    SETTINGS_CONV_CONNECTING,
    SETTINGS_CONV_CONN_FAIL,
    SETTINGS_CONV_CUR_DASH,
    SETTINGS_CONV_CUR_OFF,
    SETTINGS_CONV_CUR_ON,
    SETTINGS_CONV_DISABLING,
    SETTINGS_CONV_ENABLING,
    SETTINGS_CONV_HINT,
    SETTINGS_CONV_MISSING_ENABLED,
    SETTINGS_CONV_NET_NOT_READY,
    SETTINGS_CONV_NO_NETWORK,
    SETTINGS_CONV_PARSE_FAIL,
    SETTINGS_CONV_REQUEST_FAIL,
    SETTINGS_CONV_TITLE,
    SETTINGS_CONV_TTS_OFF,
    SETTINGS_CONV_TTS_ON,
    SETTINGS_HAPTIC_CURRENT_OFF,
    SETTINGS_HAPTIC_CURRENT_ON,
    SETTINGS_HAPTIC_HINT,
    SETTINGS_HAPTIC_OFF,
    SETTINGS_HAPTIC_ON,
    SETTINGS_HAPTIC_TITLE,
    SETTINGS_LANG_CURRENT_EN,
    SETTINGS_LANG_CURRENT_ZH,
    SETTINGS_LANG_EN_US,
    SETTINGS_LANG_HINT,
    SETTINGS_LANG_TITLE,
    SETTINGS_LANG_ZH_CN,
    SETTINGS_NET_AP_TASK_FAIL,
    SETTINGS_NET_BOARD_NO_AP,
    SETTINGS_NET_CFUN0,
    SETTINGS_NET_CFUN0_FAIL,
    SETTINGS_NET_CFUN1,
    SETTINGS_NET_CURRENT_FMT,
    SETTINGS_NET_CUR_4G,
    SETTINGS_NET_CUR_DASH,
    SETTINGS_NET_CUR_WIFI,
    SETTINGS_NET_ECSIMCFG_FAIL,
    SETTINGS_NET_ENTER_AP,
    SETTINGS_NET_ENTER_CFG,
    SETTINGS_NET_HOTSPOT_NAME,
    SETTINGS_NET_MODE_4G,
    SETTINGS_NET_MODE_TITLE,
    SETTINGS_NET_MODE_WIFI,
    SETTINGS_NET_NOT_4G,
    SETTINGS_NET_NO_4G,
    SETTINGS_NET_REBOOTING,
    SETTINGS_NET_REBOOT_COUNT_FMT,
    SETTINGS_NET_REBOOT_HINT,
    SETTINGS_NET_SIM_EXT,
    SETTINGS_NET_SIM_INT,
    SETTINGS_NET_SIM_TASK_FAIL,
    SETTINGS_NET_SIM_TITLE,
    SETTINGS_NET_SWITCHED_FMT,
    SETTINGS_NET_SWITCH_4G,
    SETTINGS_NET_SWITCH_SIM_CFUN0_FMT,
    SETTINGS_NET_SWITCH_SIM_FMT,
    SETTINGS_NET_SWITCH_WIFI,
    SETTINGS_NET_WIFI_CFG_HINT,
    SETTINGS_NET_WIFI_CFG_TITLE,
    SETTINGS_POWER_10_MIN,
    SETTINGS_POWER_120_SEC,
    SETTINGS_POWER_30_MIN,
    SETTINGS_POWER_30_SEC,
    SETTINGS_POWER_3_MIN,
    SETTINGS_POWER_60_SEC,
    SETTINGS_POWER_IDLE_MHZ_HINT,
    SETTINGS_POWER_IDLE_MHZ_TITLE,
    SETTINGS_POWER_NET_GRACE_HINT,
    SETTINGS_POWER_NET_GRACE_TITLE,
    SETTINGS_POWER_OFF_HINT,
    SETTINGS_POWER_OFF_NEVER,
    SETTINGS_POWER_OFF_TITLE,
    SETTINGS_POWER_STANDBY_HINT,
    SETTINGS_POWER_STANDBY_TITLE,
    SETTINGS_STORAGE_CAP_DASH,
    SETTINGS_STORAGE_CAP_EXPORT,
    SETTINGS_STORAGE_CAP_FAIL,
    SETTINGS_STORAGE_CAP_FMT,
    SETTINGS_STORAGE_INSERTED,
    SETTINGS_STORAGE_MISSING,
    SETTINGS_STORAGE_PC_BUSY,
    SETTINGS_STORAGE_TITLE,
    SETTINGS_STORAGE_USB_DISABLED,
    SETTINGS_STORAGE_USB_HINT_DISABLED,
    SETTINGS_STORAGE_USB_HINT_DISABLE_FAIL,
    SETTINGS_STORAGE_USB_HINT_DISABLING,
    SETTINGS_STORAGE_USB_HINT_ENABLE_FAIL,
    SETTINGS_STORAGE_USB_HINT_ENABLING,
    SETTINGS_STORAGE_USB_HINT_FORMAT,
    SETTINGS_STORAGE_USB_HINT_HOST,
    SETTINGS_STORAGE_USB_HINT_HOST_BUSY,
    SETTINGS_STORAGE_USB_HINT_IDLE,
    SETTINGS_STORAGE_USB_HINT_LOCAL,
    SETTINGS_STORAGE_USB_HINT_NO_SD,
    SETTINGS_STORAGE_USB_HINT_SWITCHING,
    SETTINGS_STORAGE_USB_OFF,
    SETTINGS_STORAGE_USB_ON,
    SETTINGS_TAB_ABOUT,
    SETTINGS_TAB_BLUETOOTH,
    SETTINGS_TAB_CONVERSATION,
    SETTINGS_TAB_HAPTIC,
    SETTINGS_TAB_LANGUAGE,
    SETTINGS_TAB_NETWORK,
    SETTINGS_TAB_POWER,
    SETTINGS_TAB_STORAGE,
    SETTINGS_TAB_TEST,
    SETTINGS_TAB_THEME,
    SETTINGS_TEST_AGING,
    SETTINGS_TEST_AGING_RUNNING,
    SETTINGS_TEST_AUDIO_CONFIRM,
    SETTINGS_TEST_AUDIO_HOLD,
    SETTINGS_TEST_AUDIO_RECORDING,
    SETTINGS_TEST_AUDIO_TITLE,
    SETTINGS_TEST_AUTO,
    SETTINGS_TEST_BATTERY,
    SETTINGS_TEST_BATTERY_CHARGING,
    SETTINGS_TEST_BATTERY_CHG_CC,
    SETTINGS_TEST_BATTERY_CHG_CV,
    SETTINGS_TEST_BATTERY_CHG_EN,
    SETTINGS_TEST_BATTERY_CHG_NOT,
    SETTINGS_TEST_BATTERY_CHG_TOPOFF,
    SETTINGS_TEST_BATTERY_CHIP_CHG,
    SETTINGS_TEST_BATTERY_CURR,
    SETTINGS_TEST_BATTERY_DISCHARGING,
    SETTINGS_TEST_BATTERY_EN_OFF,
    SETTINGS_TEST_BATTERY_EN_ON,
    SETTINGS_TEST_BATTERY_GAUGE,
    SETTINGS_TEST_BATTERY_GAUGE_STAT,
    SETTINGS_TEST_BATTERY_HINT,
    SETTINGS_TEST_BATTERY_ICHG,
    SETTINGS_TEST_BATTERY_IDLE_LOAD,
    SETTINGS_TEST_BATTERY_MATCH,
    SETTINGS_TEST_BATTERY_MATCH_DPDM,
    SETTINGS_TEST_BATTERY_MATCH_OK,
    SETTINGS_TEST_BATTERY_MISMATCH_CHIP,
    SETTINGS_TEST_BATTERY_MISMATCH_GAUGE,
    SETTINGS_TEST_BATTERY_MISMATCH_NO_CHG,
    SETTINGS_TEST_BATTERY_MISMATCH_NO_IN,
    SETTINGS_TEST_BATTERY_SOC_RAW,
    SETTINGS_TEST_BATTERY_SOC_SMOOTH,
    SETTINGS_TEST_BATTERY_VBUS,
    SETTINGS_TEST_BATTERY_VBUS_ADP,
    SETTINGS_TEST_BATTERY_VBUS_CDP,
    SETTINGS_TEST_BATTERY_VBUS_DCP,
    SETTINGS_TEST_BATTERY_VBUS_NONE,
    SETTINGS_TEST_BATTERY_VBUS_NONSTD,
    SETTINGS_TEST_BATTERY_VBUS_OTG,
    SETTINGS_TEST_BATTERY_VBUS_SDP,
    SETTINGS_TEST_BATTERY_VBUS_UNKNOWN_IN,
    SETTINGS_TEST_BATTERY_VBUS_UNK_ADP,
    SETTINGS_TEST_BATTERY_VOLT,
    SETTINGS_TEST_BATTERY_VOLT_RANGE,
    SETTINGS_TEST_BATTERY_VREG,
    SETTINGS_TEST_BUSY,
    SETTINGS_TEST_CAMERA,
    SETTINGS_TEST_CAMERA_FAIL,
    SETTINGS_TEST_CAMERA_OK,
    SETTINGS_TEST_CAMERA_RESULT,
    SETTINGS_TEST_CAMERA_SHOT,
    SETTINGS_TEST_CAMERA_TIMEOUT,
    SETTINGS_TEST_CAMERA_USB_BUSY,
    SETTINGS_TEST_CAMERA_USB_IRQ,
    SETTINGS_TEST_CAPTURING,
    SETTINGS_TEST_CELL_CANCELLED,
    SETTINGS_TEST_CELL_CFUN0_FAIL,
    SETTINGS_TEST_CELL_CFUN1_FAIL,
    SETTINGS_TEST_CELL_EXT,
    SETTINGS_TEST_CELL_INT,
    SETTINGS_TEST_CELL_LOSS_FMT,
    SETTINGS_TEST_CELL_MODEM_NOT_READY,
    SETTINGS_TEST_CELL_MODEM_NO_RESP,
    SETTINGS_TEST_CELL_NEED_4G,
    SETTINGS_TEST_CELL_NO_RESOURCE,
    SETTINGS_TEST_CELL_NO_SIM,
    SETTINGS_TEST_CELL_PING_CMD_FAIL,
    SETTINGS_TEST_CELL_PING_FAIL,
    SETTINGS_TEST_CELL_PING_OK,
    SETTINGS_TEST_CELL_PING_WAIT_TIMEOUT,
    SETTINGS_TEST_CELL_REG_REJECT,
    SETTINGS_TEST_CELL_SEARCH_TIMEOUT,
    SETTINGS_TEST_CELL_SIM_SWITCH_FAIL,
    SETTINGS_TEST_CELL_SLOT_FAIL,
    SETTINGS_TEST_CELL_TESTING,
    SETTINGS_TEST_CELL_TEST_FAIL,
    SETTINGS_TEST_CELL_WAIT_RESOURCE,
    SETTINGS_TEST_DETECTING,
    SETTINGS_TEST_EXIT_REBOOT,
    SETTINGS_TEST_FAIL,
    SETTINGS_TEST_NO,
    SETTINGS_TEST_NOT_DETECTED,
    SETTINGS_TEST_NOT_PASS,
    SETTINGS_TEST_OK,
    SETTINGS_TEST_PASS,
    SETTINGS_TEST_REBOOTING,
    SETTINGS_TEST_SCANNING,
    SETTINGS_TEST_SDCARD,
    SETTINGS_TEST_SD_MOUNT_FAIL,
    SETTINGS_TEST_SIGNAL_BOARD_UNSUP,
    SETTINGS_TEST_SIGNAL_CELL_CSQ_FMT,
    SETTINGS_TEST_SIGNAL_CELL_CSQ_UNKNOWN,
    SETTINGS_TEST_SIGNAL_CELL_UNAVAIL,
    SETTINGS_TEST_SIGNAL_READING,
    SETTINGS_TEST_SIGNAL_TITLE,
    SETTINGS_TEST_SIGNAL_WIFI_DISCONNECTED,
    SETTINGS_TEST_SIGNAL_WIFI_READ_FAIL,
    SETTINGS_TEST_SIGNAL_WIFI_RSSI_FMT,
    SETTINGS_TEST_TASK_CREATE_FAIL,
    SETTINGS_TEST_TOUCH,
    SETTINGS_TEST_TOUCH_START,
    SETTINGS_TEST_WAITING,
    SETTINGS_TEST_WIFI_CLOSE,
    SETTINGS_TEST_WIFI_COUNT_FMT,
    SETTINGS_TEST_WIFI_FOUND_FMT,
    SETTINGS_TEST_WIFI_NEARBY,
    SETTINGS_TEST_WIFI_NEXT,
    SETTINGS_TEST_WIFI_NONE,
    SETTINGS_TEST_WIFI_PREV,
    SETTINGS_TEST_WIFI_RESCAN,
    SETTINGS_TEST_WIFI_SCANNING,
    SETTINGS_TEST_WIFI_SCAN_FAIL,
    SETTINGS_TEST_YES,
    SETTINGS_THEME_BORDER,
    SETTINGS_THEME_CURRENT_BORDER,
    SETTINGS_THEME_CURRENT_GRAY,
    SETTINGS_THEME_CURRENT_SLASH,
    SETTINGS_THEME_CURRENT_WHITE,
    SETTINGS_THEME_GRAY,
    SETTINGS_THEME_HINT,
    SETTINGS_THEME_SLASH,
    SETTINGS_THEME_TITLE,
    SETTINGS_THEME_WHITE,
    SPEAKING,
    STANDBY,
    STANDBY_CONN_FAIL,
    STANDBY_DATE_FMT,
    STANDBY_DATE_PLACEHOLDER,
    STANDBY_DECODE_FAIL,
    STANDBY_EMPTY_TODO,
    STANDBY_NO_DATA,
    STANDBY_NO_NETWORK,
    STANDBY_NO_TODO_CACHE,
    STANDBY_NO_WEATHER,
    STANDBY_PARSE_FAIL,
    STANDBY_REQUEST_FAIL,
    STANDBY_WEATHER_FAIL,
    STANDBY_WP_NOT_FOUND,
    SWITCH_TO_4G_NETWORK,
    SWITCH_TO_WIFI_NETWORK,
    TASK_API_ERROR,
    TASK_BATCH_COMPLETING,
    TASK_BATCH_FAIL,
    TASK_BATCH_REMOVING,
    TASK_BATCH_RESULT_FMT,
    TASK_BATCH_START_FAIL,
    TASK_CHOOSE_ACTION,
    TASK_COMPLETE,
    TASK_COMPLETED,
    TASK_COMPLETED_N_FMT,
    TASK_COMPLETE_FAIL,
    TASK_COMPLETE_START_FAIL,
    TASK_COMPLETE_TODO,
    TASK_COMPLETING,
    TASK_CONNECTING_NET,
    TASK_CONN_FAIL,
    TASK_DATA_FORMAT_ERR,
    TASK_DELETED,
    TASK_DELETE_FAIL,
    TASK_DELETE_START_FAIL,
    TASK_DELETE_TODO,
    TASK_DELETING,
    TASK_DONE_OK,
    TASK_EMPTY_DONE,
    TASK_EMPTY_TODO,
    TASK_INVALID,
    TASK_LOAD_FAIL,
    TASK_NEED_WIFI_CFG,
    TASK_NET_NOT_READY,
    TASK_NO_COMPLETABLE,
    TASK_NO_NETWORK,
    TASK_PARSE_FAIL,
    TASK_PLEASE_WAIT,
    TASK_REFRESH,
    TASK_REFRESHED,
    TASK_REFRESHING,
    TASK_REFRESH_START_FAIL,
    TASK_REMOVED_N_FMT,
    TASK_REQUEST_FAIL,
    TASK_SELECTED_FMT,
    TASK_SELECT_FIRST,
    TASK_TAB_DONE,
    TASK_TAB_TODO,
    TASK_TITLE_DONE,
    TASK_TITLE_TODO,
    TOUCH_MISSING_HINT,
    TRANSLATE_AUDIO_BUSY,
    TRANSLATE_CANCELLED,
    TRANSLATE_CONNECTING,
    TRANSLATE_CONNECT_FAIL,
    TRANSLATE_CONNECT_NET,
    TRANSLATE_ERROR,
    TRANSLATE_GET_TOKEN,
    TRANSLATE_GET_TOKEN_FAIL,
    TRANSLATE_HINT,
    TRANSLATE_LANG_INVALID,
    TRANSLATE_LANG_SAME,
    TRANSLATE_LIVE,
    TRANSLATE_NEED_WIFI_CFG,
    TRANSLATE_NET_CONNECTING,
    TRANSLATE_NET_NOT_READY,
    TRANSLATE_PICK_FROM,
    TRANSLATE_PICK_LANG,
    TRANSLATE_PICK_TO,
    TRANSLATE_READY_TIMEOUT,
    TRANSLATE_SOURCE_PLACEHOLDER,
    TRANSLATE_SOURCE_TITLE,
    TRANSLATE_START_FAIL,
    TRANSLATE_STOP,
    TRANSLATE_STOPPED,
    TRANSLATE_STOPPING,
    TRANSLATE_TRANSLATING,
    TRANSLATE_TRANS_PLACEHOLDER,
    TRANSLATE_TRANS_TITLE,
    TRANSLATE_TRY_LATER,
    UPGRADE_FAILED,
    UPGRADING,
    VERSION,
    VOICE_STARTING_NET,
    VOLUME,
    WALLPAPER_CHIP_SHUTDOWN,
    WALLPAPER_CHIP_STANDBY,
    WALLPAPER_DECODE_FAIL,
    WALLPAPER_DELETE_BTN,
    WALLPAPER_DELETE_FAIL,
    WALLPAPER_DELETE_START_FAIL,
    WALLPAPER_DELETING,
    WALLPAPER_EMPTY_FMT,
    WALLPAPER_ENABLE_FAIL,
    WALLPAPER_FILE_INVALID,
    WALLPAPER_FILE_MISSING,
    WALLPAPER_LOADING,
    WALLPAPER_NAME_INVALID,
    WALLPAPER_NO_SD,
    WALLPAPER_PATH_INVALID,
    WALLPAPER_PREVIEW_FAIL,
    WALLPAPER_PREVIEW_OOM,
    WALLPAPER_PREVIEW_START_FAIL,
    WALLPAPER_SELECTED_FMT,
    WALLPAPER_SET_SHUTDOWN,
    WALLPAPER_SET_STANDBY,
    WALLPAPER_SHUTDOWN_ON,
    WALLPAPER_STANDBY_ON,
    WARNING,
    WIFI_CONFIG_MODE,
    kCount
};

static const char* const kLiterals_en_us[] = {
        " Config URL: ",  /* ACCESS_VIA_BROWSER */
        "Activation",  /* ACTIVATION */
        "Code:%s",  /* ACTIVATION_CODE_FMT */
        "Charging",  /* BATTERY_CHARGING */
        "Battery full",  /* BATTERY_FULL */
        "Low battery",  /* BATTERY_LOW */
        "Low battery, please charge",  /* BATTERY_NEED_CHARGE */
        "Invalid argument",  /* BOOK_BAD_ARG */
        "(Blank)",  /* BOOK_BLANK */
        "Body",  /* BOOK_BODY */
        "Cancelled",  /* BOOK_CANCELLED */
        "Ch.%d",  /* BOOK_CHAPTER_FMT */
        "(No text in chapter)",  /* BOOK_CHAPTER_NO_TEXT */
        "Chapter progress",  /* BOOK_CHAPTER_PROGRESS */
        "Checksum failed",  /* BOOK_CHECKSUM_FAIL */
        "Connection failed",  /* BOOK_CONN_FAIL */
        "Continue",  /* BOOK_CONTINUE */
        "%d",  /* BOOK_COUNT_FMT */
        "Cover too large",  /* BOOK_COVER_TOO_LARGE */
        "Delete failed",  /* BOOK_DELETE_FAILED */
        "Details",  /* BOOK_DETAIL */
        "Download failed",  /* BOOK_DOWNLOAD_FAIL */
        "%s %uh %umin",  /* BOOK_DURATION_HM_FMT */
        "%s %uh",  /* BOOK_DURATION_H_FMT */
        "%s <1 min",  /* BOOK_DURATION_LT1MIN_FMT */
        "%s %umin",  /* BOOK_DURATION_M_FMT */
        "Empty cover",  /* BOOK_EMPTY_COVER */
        "Empty file",  /* BOOK_EMPTY_FILE */
        "No books\nPut .epub / .txt / .ebook\nin %s",  /* BOOK_EMPTY_HOME_FMT */
        "Bookshelf empty\nPut .epub / .txt / .ebook\nin %s",  /* BOOK_EMPTY_SHELF_FMT */
        "File too large",  /* BOOK_FILE_TOO_LARGE */
        "Finished",  /* BOOK_FINISHED */
        "No fonts",  /* BOOK_FONT_EMPTY */
        "Invalid filename",  /* BOOK_FONT_NAME_INVALID */
        "Font",  /* BOOK_FONT_TITLE */
        "[Image]",  /* BOOK_IMAGE_PLACEHOLDER */
        "Building",  /* BOOK_LAYOUT_BUSY */
        "Done",  /* BOOK_LAYOUT_DONE */
        "Line spacing",  /* BOOK_LINE_GAP */
        "Margin",  /* BOOK_MARGIN */
        "Narrow",  /* BOOK_MARGIN_NARROW */
        "Standard",  /* BOOK_MARGIN_STANDARD */
        "Very wide",  /* BOOK_MARGIN_VERY_WIDE */
        "Wide",  /* BOOK_MARGIN_WIDE */
        "Network not ready",  /* BOOK_NET_NOT_READY */
        "Nothing to continue",  /* BOOK_NO_CONTINUE */
        "No cover URL",  /* BOOK_NO_COVER_URL */
        "Missing download URL",  /* BOOK_NO_DOWNLOAD_URL */
        "No local file",  /* BOOK_NO_LOCAL_FILE */
        "No network",  /* BOOK_NO_NETWORK */
        "No recent books",  /* BOOK_NO_RECENT */
        "No SD card\nInsert and retry",  /* BOOK_NO_SD */
        "No SD card",  /* BOOK_NO_SD_SHORT */
        "Opening",  /* BOOK_OPENING */
        "Open failed",  /* BOOK_OPEN_FAILED */
        "Open failed\nChapter too large or out of memory",  /* BOOK_OPEN_FAILED_CHAPTER */
        "Open failed\nCheck EPUB integrity",  /* BOOK_OPEN_FAILED_EPUB */
        "Open failed\nCannot start parse task",  /* BOOK_OPEN_FAILED_TASK */
        "Open failed\nTXT too large or out of memory",  /* BOOK_OPEN_FAILED_TXT */
        "Out of memory",  /* BOOK_OUT_OF_MEMORY */
        "Cannot load page",  /* BOOK_PAGE_LOAD_FAILED */
        "Invalid path",  /* BOOK_PATH_INVALID */
        "Total time",  /* BOOK_READ_DURATION */
        "Read failed",  /* BOOK_READ_FAIL */
        "(Read failed)",  /* BOOK_READ_FILE_FAIL */
        "Read %d.%d%%",  /* BOOK_READ_PCT_FMT */
        "Reading progress",  /* BOOK_READ_PROGRESS */
        "Recent",  /* BOOK_RECENT */
        "Save failed",  /* BOOK_SAVE_FAIL */
        "Selected %d",  /* BOOK_SELECTED_FMT */
        "My bookshelf",  /* BOOK_SHELF_TITLE */
        "Compact",  /* BOOK_SPACING_COMPACT */
        "Relaxed",  /* BOOK_SPACING_RELAXED */
        "Standard",  /* BOOK_SPACING_STANDARD */
        "Very relaxed",  /* BOOK_SPACING_VERY_RELAXED */
        "Start reading",  /* BOOK_START */
        "Storage unavailable",  /* BOOK_STORAGE_UNAVAIL */
        "Empty request body",  /* BOOK_SYNC_EMPTY_BODY */
        "Contents",  /* BOOK_TOC */
        "No contents",  /* BOOK_TOC_EMPTY */
        "Contents %d/%d",  /* BOOK_TOC_PAGE_FMT */
        "Today",  /* BOOK_TODAY_DURATION */
        "Underline",  /* BOOK_UNDERLINE */
        "Dashed",  /* BOOK_UNDERLINE_DASHED */
        "Solid",  /* BOOK_UNDERLINE_SOLID */
        "Write failed",  /* BOOK_WRITE_FAIL */
        "Call",  /* BT_CALL_BTN */
        "Call mode (SCO up)",  /* BT_CALL_MODE_SCO */
        "Connecting...",  /* BT_CONNECTING */
        "Connecting: %s...",  /* BT_CONNECTING_FMT */
        "Connected",  /* BT_CONNECT_OK */
        "Connect failed (timeout)",  /* BT_CONNECT_TIMEOUT */
        "External BT audio chip settings (not ESP32 BLE)",  /* BT_DESC */
        "Found: %s",  /* BT_FOUND_FMT */
        "Mode 1",  /* BT_MODE1 */
        "Mode 1 active\n(AT+RX=2 / AT+MODE=1)",  /* BT_MODE1_ACTIVE */
        "Mode 1 set",  /* BT_MODE1_SET */
        "Mode 2",  /* BT_MODE2 */
        "Mode 2 set, ready to scan",  /* BT_MODE2_SET */
        "Mode 3",  /* BT_MODE3 */
        "Mode 3 set",  /* BT_MODE3_SET */
        "Music",  /* BT_MUSIC_BTN */
        "Music mode (SCO down)",  /* BT_MUSIC_MODE_SCO */
        "Connect a Bluetooth device first",  /* BT_NEED_CONNECT */
        "Switch to mode 2 first",  /* BT_NEED_MODE2 */
        "BT power reset",  /* BT_PWR_RESET_OK */
        "BT power reset not supported",  /* BT_PWR_RESET_UNSUP */
        "Reset BT",  /* BT_RESET_BTN */
        "Use when flashing BT firmware",  /* BT_RESET_HINT */
        "Scanning...",  /* BT_SCANNING */
        "Scan",  /* BT_SCAN_BTN */
        "Scan done, %d device(s)",  /* BT_SCAN_DONE_FMT */
        "Starting scan...",  /* BT_SCAN_START */
        "Select a Bluetooth mode",  /* BT_SELECT_MODE */
        "Switching to call mode...",  /* BT_SWITCH_CALL */
        "Switching to mode 1...",  /* BT_SWITCH_MODE1 */
        "Switching to mode 2...",  /* BT_SWITCH_MODE2 */
        "Switching to mode 3...",  /* BT_SWITCH_MODE3 */
        "Switching to music mode...",  /* BT_SWITCH_MUSIC */
        "Bluetooth",  /* BT_TITLE */
        "UART not initialized",  /* BT_UART_NOT_INIT */
        "Checking for new version...",  /* CHECKING_NEW_VERSION */
        "Check for new version failed, will retry in %d seconds: %s",  /* CHECK_NEW_VERSION_FAILED */
        "API error",  /* CLOUD_API_ERROR */
        "Cancelled",  /* CLOUD_CANCELLED */
        "Cancelling…",  /* CLOUD_CANCELLING */
        "Download “%s”?\nType %s · Size %s",  /* CLOUD_CONFIRM_DL_FMT */
        "Connecting…",  /* CLOUD_CONNECTING_NET */
        "Connecting…",  /* CLOUD_CONNECT_NET */
        "Connection failed",  /* CLOUD_CONN_FAIL */
        "Cover load failed",  /* CLOUD_COVER_FAIL */
        "Bad data format",  /* CLOUD_DATA_FORMAT_ERR */
        "Decode failed",  /* CLOUD_DECODE_FAIL */
        "Delete failed",  /* CLOUD_DELETE_FAIL */
        "Download",  /* CLOUD_DOWNLOAD */
        "Downloading",  /* CLOUD_DOWNLOADING */
        "Download failed",  /* CLOUD_DOWNLOAD_FAIL */
        "Nothing to transfer",  /* CLOUD_EMPTY_ALL */
        "No books",  /* CLOUD_EMPTY_BOOK */
        "No fonts",  /* CLOUD_EMPTY_FONT */
        "No Walls",  /* CLOUD_EMPTY_WALLPAPER */
        "Fetching list…",  /* CLOUD_FETCHING_LIST */
        "Invalid file",  /* CLOUD_FILE_INVALID */
        "Already queued",  /* CLOUD_IN_QUEUE */
        "JSON create failed",  /* CLOUD_JSON_CREATE_FAIL */
        "JSON parse failed",  /* CLOUD_JSON_PARSE_FAIL */
        "JSON serialize failed",  /* CLOUD_JSON_SERIALIZE_FAIL */
        "List too large",  /* CLOUD_LIST_TOO_LARGE */
        "Loading…",  /* CLOUD_LOADING */
        "Loading covers…",  /* CLOUD_LOAD_COVER */
        "Load failed",  /* CLOUD_LOAD_FAIL */
        "Missing taskId",  /* CLOUD_MISSING_TASK_ID */
        "Set up Wi-Fi first",  /* CLOUD_NEED_WIFI_CFG */
        "Network not ready",  /* CLOUD_NET_NOT_READY */
        "No cover",  /* CLOUD_NO_COVER */
        "Missing download URL",  /* CLOUD_NO_DOWNLOAD_URL */
        "No local file",  /* CLOUD_NO_LOCAL_FILE */
        "No network",  /* CLOUD_NO_NETWORK */
        "No SD card",  /* CLOUD_NO_SD */
        "Invalid path",  /* CLOUD_PATH_INVALID */
        "Incoming",  /* CLOUD_PENDING */
        "Please wait",  /* CLOUD_PLEASE_WAIT */
        "Cannot preview",  /* CLOUD_PREVIEW_FAIL */
        "Failed to start preview",  /* CLOUD_PREVIEW_START_FAIL */
        "Preview URL too long",  /* CLOUD_PREVIEW_URL_LONG */
        "Book push",  /* CLOUD_PUSH_BOOK */
        "Font push",  /* CLOUD_PUSH_FONT */
        "Walls push",  /* CLOUD_PUSH_WALLPAPER */
        "Read timeout or interrupted",  /* CLOUD_READ_TIMEOUT */
        "Refresh",  /* CLOUD_REFRESH */
        "Refreshed",  /* CLOUD_REFRESHED */
        "Refreshing…",  /* CLOUD_REFRESHING */
        "Request failed",  /* CLOUD_REQUEST_FAIL */
        "Save",  /* CLOUD_SAVE */
        "Saved 0 / 1",  /* CLOUD_SAVED_0_1 */
        "Saved %d / %d",  /* CLOUD_SAVED_FMT */
        "Saved (sync failed)",  /* CLOUD_SAVED_SYNC_FAIL */
        "Save failed",  /* CLOUD_SAVE_FAIL */
        "Save to device",  /* CLOUD_SAVE_LOCAL */
        "Saving",  /* CLOUD_SAVING */
        "Saving\n%s",  /* CLOUD_SAVING_NAME_FMT */
        "Selected %d",  /* CLOUD_SELECTED_FMT */
        "Select items first",  /* CLOUD_SELECT_FIRST */
        "Storage unavailable",  /* CLOUD_STORAGE_UNAVAIL */
        "Sync failed",  /* CLOUD_SYNC_FAIL */
        "All",  /* CLOUD_TAB_ALL */
        "Books",  /* CLOUD_TAB_BOOK */
        "Fonts",  /* CLOUD_TAB_FONT */
        "Walls",  /* CLOUD_TAB_WALLPAPER */
        "Task failed",  /* CLOUD_TASK_FAIL */
        "Book",  /* CLOUD_TYPE_BOOK */
        "Font",  /* CLOUD_TYPE_FONT */
        "Walls",  /* CLOUD_TYPE_WALLPAPER */
        "Unknown type",  /* CLOUD_UNKNOWN_TYPE */
        "Wait for download to finish",  /* CLOUD_WAIT_DOWNLOAD */
        "Cancel",  /* COMMON_CANCEL */
        "Delete",  /* COMMON_DELETE */
        "Empty",  /* COMMON_EMPTY */
        "Failed",  /* COMMON_FAILED */
        "Loading...",  /* COMMON_LOADING */
        "Off",  /* COMMON_OFF */
        "OK",  /* COMMON_OK */
        "On",  /* COMMON_ON */
        "Remove",  /* COMMON_REMOVE */
        "Retry",  /* COMMON_RETRY */
        "Select all",  /* COMMON_SELECT_ALL */
        "Success",  /* COMMON_SUCCESS */
        "Unknown",  /* COMMON_UNKNOWN */
        "Connected to ",  /* CONNECTED_TO */
        "Connecting...",  /* CONNECTING */
        "Connection Successful",  /* CONNECTION_SUCCESSFUL */
        "Connect to ",  /* CONNECT_TO */
        "Hotspot: ",  /* CONNECT_TO_HOTSPOT */
        "Detecting module...",  /* DETECTING_MODULE */
        "Failed to download assets",  /* DOWNLOAD_ASSETS_FAILED */
        "Entering Wi-Fi setup…",  /* ENTERING_WIFI_CONFIG_MODE */
        "Error",  /* ERROR */
        "Found new assets: %s",  /* FOUND_NEW_ASSETS */
        "Hello, my friend!",  /* HELLO_MY_FRIEND */
        "Ask AI",  /* HOME_APP_ASSISTANT */
        "Books",  /* HOME_APP_BOOK */
        "Transfer",  /* HOME_APP_CLOUD */
        "Settings",  /* HOME_APP_SETTINGS */
        "Tasks",  /* HOME_APP_TASK */
        "Walls",  /* HOME_APP_WALLPAPER */
        "%02d/%02d",  /* HOME_DATE_FMT */
        "--/--",  /* HOME_DATE_PLACEHOLDER */
        "%d.%d.%d",  /* HOME_DATE_SLASH_FMT */
        "----.-.--",  /* HOME_DATE_SLASH_PLACEHOLDER */
        "Fri",  /* HOME_WDAY_FRI */
        "Mon",  /* HOME_WDAY_MON */
        "Sat",  /* HOME_WDAY_SAT */
        "Sun",  /* HOME_WDAY_SUN */
        "Thu",  /* HOME_WDAY_THU */
        "Tue",  /* HOME_WDAY_TUE */
        "Wed",  /* HOME_WDAY_WED */
        "Information",  /* INFO */
        "Initializing...",  /* INITIALIZING */
        "Listening...",  /* LISTENING */
        "Loading assets...",  /* LOADING_ASSETS */
        "Logging in...",  /* LOADING_PROTOCOL */
        "Max volume",  /* MAX_VOLUME */
        "Muted",  /* MUTED */
        "Set up Wi-Fi first",  /* NEED_WIFI_CFG */
        "[Open]",  /* NETWORK_AUTH_OPEN */
        "[Secured]",  /* NETWORK_AUTH_SECURE */
        "Connecting; scan later",  /* NETWORK_BUSY_CONNECT */
        "Cancel",  /* NETWORK_CANCEL */
        "Cleared saved networks",  /* NETWORK_CLEARED_OK */
        "Clear",  /* NETWORK_CLEAR_ALL */
        "Connect",  /* NETWORK_CONNECT */
        "Connected to %s",  /* NETWORK_CONNECTED_FMT */
        "Connecting to %s …",  /* NETWORK_CONNECTING_FMT */
        "Connection failed",  /* NETWORK_CONNECT_FAIL */
        "Cannot start connect task",  /* NETWORK_CONNECT_TASK_FAIL */
        "Connection timed out",  /* NETWORK_CONNECT_TIMEOUT */
        "Could not connect within 15s; retry",  /* NETWORK_CONNECT_TIMEOUT_HINT */
        "Connect to: %s",  /* NETWORK_CONNECT_TO_FMT */
        "%s (default)",  /* NETWORK_DEFAULT_FMT */
        "Delete",  /* NETWORK_DELETE */
        "Network deleted",  /* NETWORK_DELETED_OK */
        "Wi-Fi not found (signal lost)",  /* NETWORK_ERR_AP_GONE */
        "Association failed; AP rejected",  /* NETWORK_ERR_ASSOC */
        "Wrong password; try again",  /* NETWORK_ERR_BAD_PASSWORD */
        "Rejected (reason=%u)",  /* NETWORK_ERR_REASON_FMT */
        "Signal too weak; timed out",  /* NETWORK_ERR_WEAK */
        "No networks. Tap Refresh to retry",  /* NETWORK_NEARBY_EMPTY */
        "Enter Wi-Fi password (8–63 chars)",  /* NETWORK_PWD_HINT */
        "Wi-Fi password",  /* NETWORK_PWD_PLACEHOLDER */
        "Password too long",  /* NETWORK_PWD_TOO_LONG */
        "No saved Wi-Fi yet",  /* NETWORK_SAVED_EMPTY */
        "Refresh",  /* NETWORK_SCAN */
        "Scanning nearby Wi-Fi…",  /* NETWORK_SCANNING */
        "Scan done, %d networks",  /* NETWORK_SCAN_DONE_FMT */
        "Failed to start scan",  /* NETWORK_SCAN_FAIL */
        "Cannot start scan task",  /* NETWORK_SCAN_TASK_FAIL */
        "Scan timed out",  /* NETWORK_SCAN_TIMEOUT */
        "Default",  /* NETWORK_SET_DEFAULT */
        "Set as default network",  /* NETWORK_SET_DEFAULT_OK */
        "Show password",  /* NETWORK_SHOW_PWD */
        "Invalid SSID",  /* NETWORK_SSID_INVALID */
        "Nearby",  /* NETWORK_TAB_NEARBY */
        "Saved",  /* NETWORK_TAB_SAVED */
        "Configure Wi-Fi",  /* NETWORK_TITLE */
        "Initializing Wi-Fi…",  /* NETWORK_WIFI_INIT */
        "Wi-Fi init failed",  /* NETWORK_WIFI_INIT_FAIL */
        "New version ",  /* NEW_VERSION */
        "Already up to date",  /* OTA_ALREADY_LATEST */
        "Update check failed",  /* OTA_CHECK_FAILED */
        "New firmware available\nCurrent version %s\nLatest version %s",  /* OTA_CONFIRM_FMT */
        "Current  %s",  /* OTA_CUR_VER_FMT */
        "Do not power off or exit",  /* OTA_HINT */
        "Ignore this update",  /* OTA_IGNORE_VERSION */
        "Manual upgrade",  /* OTA_MANUAL */
        "Latest  %s",  /* OTA_NEW_VER_FMT */
        "Remind me next time",  /* OTA_REMIND_LATER */
        "Upgrade OK, rebooting",  /* OTA_SUCCESS_REBOOT */
        "Upgrading system",  /* OTA_TITLE */
        "OTA Upgrade",  /* OTA_UPGRADE */
        "Upgrade now",  /* OTA_UPGRADE_NOW */
        "System busy",  /* PHONE_BUSY */
        "Checking network...",  /* PHONE_CHECKING_NET */
        "Check mobile network",  /* PHONE_CHECK_CELL */
        "Confirming SIM…",  /* PHONE_CONFIRM_SIM */
        "Call",  /* PHONE_DIAL */
        "Dial failed",  /* PHONE_DIAL_FAIL */
        "Hang up",  /* PHONE_HANGUP */
        "Internal SIM cannot dial\nSwitch to external SIM in Settings → Network",  /* PHONE_INTERNAL_SIM */
        "In call",  /* PHONE_IN_CALL */
        "No 4G module",  /* PHONE_NO_4G */
        "Wi-Fi mode cannot dial\nSwitch to 4G in Settings → Network",  /* PHONE_WIFI_BLOCK */
        "Please insert SIM card",  /* PIN_ERROR */
        "Please wait...",  /* PLEASE_WAIT */
        "Powered off",  /* POWERED_OFF */
        "Transcription done",  /* RECORD_ASR_DONE */
        "Transcribed, but nothing to show",  /* RECORD_ASR_EMPTY */
        "Transcription failed",  /* RECORD_ASR_FAIL */
        "Tap “Transcribe”, then reopen later for results",  /* RECORD_ASR_HINT */
        "Transcription failed (HTTP %d)",  /* RECORD_ASR_HTTP_FMT */
        "Connect to network for transcription",  /* RECORD_ASR_NEED_NET */
        "Failed to parse transcription",  /* RECORD_ASR_PARSE_FAIL */
        "Transcription in progress",  /* RECORD_ASR_PENDING */
        "Transcription request failed",  /* RECORD_ASR_REQ_FAIL */
        "Failed to start transcription",  /* RECORD_ASR_START_FAIL */
        "Uploaded. Check back later; do not re-upload",  /* RECORD_ASR_UPLOADED_HINT */
        "Audio busy, try later",  /* RECORD_AUDIO_BUSY */
        "Audio not ready",  /* RECORD_AUDIO_NOT_READY */
        "Audio starting, please wait",  /* RECORD_AUDIO_STARTING */
        "Transcribe",  /* RECORD_BTN_ASR */
        "Play",  /* RECORD_BTN_PLAY */
        "Saving…",  /* RECORD_BTN_SAVING */
        "Start",  /* RECORD_BTN_START */
        "Stop",  /* RECORD_BTN_STOP */
        "Stop",  /* RECORD_BTN_STOP_PLAY */
        "Deleted",  /* RECORD_DELETED */
        "Delete failed",  /* RECORD_DELETE_FAIL */
        "Duration: %.1f s",  /* RECORD_DURATION_SEC_FMT */
        "No recordings",  /* RECORD_EMPTY */
        "Recording file corrupt",  /* RECORD_FILE_CORRUPT */
        "Recording too large or invalid",  /* RECORD_FILE_TOO_LARGE */
        "Tap below to start recording",  /* RECORD_HINT_START */
        "Insert SD card",  /* RECORD_INSERT_SD */
        "Max %d min reached, auto-saved",  /* RECORD_MAX_MIN_FMT */
        "%s · %s",  /* RECORD_META_FMT */
        "Cannot create record folder",  /* RECORD_MKDIR_FAIL */
        "Network unavailable",  /* RECORD_NET_UNAVAIL */
        "No SD card\n\nInsert an SD card to use recording",  /* RECORD_NO_SD_HINT */
        "Out of memory",  /* RECORD_OOM */
        "Cannot open recording",  /* RECORD_OPEN_FAIL */
        "Playing…",  /* RECORD_PLAYING */
        "Playback ended",  /* RECORD_PLAY_END */
        "Failed to start playback",  /* RECORD_PLAY_START_FAIL */
        "Playback stopped",  /* RECORD_PLAY_STOPPED */
        "Please wait",  /* RECORD_PLEASE_WAIT */
        "Failed to read recording",  /* RECORD_READ_FAIL */
        "Recording…",  /* RECORD_RECORDING */
        "Saved to SD card",  /* RECORD_SAVED_SD */
        "Stop recording first",  /* RECORD_STOP_FIRST */
        "Summary",  /* RECORD_SUMMARY */
        "List",  /* RECORD_TAB_LIST */
        "Record",  /* RECORD_TAB_REC */
        "Failed to start recording",  /* RECORD_TASK_START_FAIL */
        "Too short, try again",  /* RECORD_TOO_SHORT */
        "Uploading…",  /* RECORD_UPLOADING */
        "Write failed, not saved",  /* RECORD_WRITE_FAIL */
        "Waiting for network...",  /* REGISTERING_NETWORK */
        "Unable to access network, please check SIM card status",  /* REG_ERROR */
        "AEC Off",  /* RTC_MODE_OFF */
        "AEC On",  /* RTC_MODE_ON */
        "Scanning Wi-Fi...",  /* SCANNING_WIFI */
        "Sending failed, please check the network",  /* SERVER_ERROR */
        "Unable to connect to service, please try again later",  /* SERVER_NOT_CONNECTED */
        "Looking for available service",  /* SERVER_NOT_FOUND */
        "Waiting for response timeout",  /* SERVER_TIMEOUT */
        "Build time",  /* SETTINGS_ABOUT_BUILD */
        "Chip",  /* SETTINGS_ABOUT_CHIP */
        "CPU cores",  /* SETTINGS_ABOUT_CORES */
        "%u cores",  /* SETTINGS_ABOUT_CORES_FMT */
        "Flash",  /* SETTINGS_ABOUT_FLASH */
        "Firmware",  /* SETTINGS_ABOUT_FW */
        "Tap to check for updates",  /* SETTINGS_ABOUT_FW_CHECK */
        "Firmware:%s",  /* SETTINGS_ABOUT_FW_VER_FMT */
        "MAC",  /* SETTINGS_ABOUT_MAC */
        "Device model",  /* SETTINGS_ABOUT_MODEL */
        "None",  /* SETTINGS_ABOUT_NONE */
        "PSRAM",  /* SETTINGS_ABOUT_PSRAM */
        "Busy, please wait",  /* SETTINGS_CONV_BUSY */
        "Connecting…",  /* SETTINGS_CONV_CONNECTING */
        "Connection failed",  /* SETTINGS_CONV_CONN_FAIL */
        "Current: —",  /* SETTINGS_CONV_CUR_DASH */
        "Current: TTS off",  /* SETTINGS_CONV_CUR_OFF */
        "Current: TTS on",  /* SETTINGS_CONV_CUR_ON */
        "Disabling…",  /* SETTINGS_CONV_DISABLING */
        "Enabling…",  /* SETTINGS_CONV_ENABLING */
        "Whether Ask AI speaks aloud\nOff = text only",  /* SETTINGS_CONV_HINT */
        "Missing enabled",  /* SETTINGS_CONV_MISSING_ENABLED */
        "Network not ready",  /* SETTINGS_CONV_NET_NOT_READY */
        "No network",  /* SETTINGS_CONV_NO_NETWORK */
        "Response parse failed",  /* SETTINGS_CONV_PARSE_FAIL */
        "Request failed",  /* SETTINGS_CONV_REQUEST_FAIL */
        "Voice playback",  /* SETTINGS_CONV_TITLE */
        "TTS off",  /* SETTINGS_CONV_TTS_OFF */
        "TTS on",  /* SETTINGS_CONV_TTS_ON */
        "Current: Off",  /* SETTINGS_HAPTIC_CURRENT_OFF */
        "Current: On",  /* SETTINGS_HAPTIC_CURRENT_ON */
        "UI buttons and cover keys",  /* SETTINGS_HAPTIC_HINT */
        "Off",  /* SETTINGS_HAPTIC_OFF */
        "On",  /* SETTINGS_HAPTIC_ON */
        "Button haptic",  /* SETTINGS_HAPTIC_TITLE */
        "Current: English",  /* SETTINGS_LANG_CURRENT_EN */
        "Current: 简体中文",  /* SETTINGS_LANG_CURRENT_ZH */
        "English",  /* SETTINGS_LANG_EN_US */
        "Takes effect immediately",  /* SETTINGS_LANG_HINT */
        "Language",  /* SETTINGS_LANG_TITLE */
        "简体中文",  /* SETTINGS_LANG_ZH_CN */
        "Cannot open Wi-Fi setup",  /* SETTINGS_NET_AP_TASK_FAIL */
        "This board has no Wi-Fi setup",  /* SETTINGS_NET_BOARD_NO_AP */
        "RF off…\nAT+CFUN=0",  /* SETTINGS_NET_CFUN0 */
        "AT+CFUN=0 failed",  /* SETTINGS_NET_CFUN0_FAIL */
        "Searching network…\nAT+CFUN=1",  /* SETTINGS_NET_CFUN1 */
        "Current: %s",  /* SETTINGS_NET_CURRENT_FMT */
        "Current: Cellular",  /* SETTINGS_NET_CUR_4G */
        "Current: --",  /* SETTINGS_NET_CUR_DASH */
        "Current: Wi-Fi",  /* SETTINGS_NET_CUR_WIFI */
        "AT+ECSIMCFG failed",  /* SETTINGS_NET_ECSIMCFG_FAIL */
        "Opening Wi-Fi setup…",  /* SETTINGS_NET_ENTER_AP */
        "Configure Wi-Fi",  /* SETTINGS_NET_ENTER_CFG */
        "Hotspot name:",  /* SETTINGS_NET_HOTSPOT_NAME */
        "Cellular",  /* SETTINGS_NET_MODE_4G */
        "Network mode",  /* SETTINGS_NET_MODE_TITLE */
        "Wi-Fi",  /* SETTINGS_NET_MODE_WIFI */
        "Not in 4G mode, cannot switch",  /* SETTINGS_NET_NOT_4G */
        "No 4G module",  /* SETTINGS_NET_NO_4G */
        "Rebooting…",  /* SETTINGS_NET_REBOOTING */
        "%s\nRebooting in %d s…",  /* SETTINGS_NET_REBOOT_COUNT_FMT */
        "Device reboots after switching",  /* SETTINGS_NET_REBOOT_HINT */
        "External SIM",  /* SETTINGS_NET_SIM_EXT */
        "Internal SIM",  /* SETTINGS_NET_SIM_INT */
        "Cannot start SIM switch task",  /* SETTINGS_NET_SIM_TASK_FAIL */
        "SIM",  /* SETTINGS_NET_SIM_TITLE */
        "Switched to %s",  /* SETTINGS_NET_SWITCHED_FMT */
        "Switching to 4G…",  /* SETTINGS_NET_SWITCH_4G */
        "Switching to %s…\nAT+CFUN=0",  /* SETTINGS_NET_SWITCH_SIM_CFUN0_FMT */
        "Switching to %s…\n%s",  /* SETTINGS_NET_SWITCH_SIM_FMT */
        "Switching to Wi-Fi…",  /* SETTINGS_NET_SWITCH_WIFI */
        "Scan nearby Wi-Fi and enter the password",  /* SETTINGS_NET_WIFI_CFG_HINT */
        "Wi-Fi network",  /* SETTINGS_NET_WIFI_CFG_TITLE */
        "10 min",  /* SETTINGS_POWER_10_MIN */
        "120 s",  /* SETTINGS_POWER_120_SEC */
        "30 min",  /* SETTINGS_POWER_30_MIN */
        "30 s",  /* SETTINGS_POWER_30_SEC */
        "3 min",  /* SETTINGS_POWER_3_MIN */
        "60 s",  /* SETTINGS_POWER_60_SEC */
        "CPU clock when idle",  /* SETTINGS_POWER_IDLE_MHZ_HINT */
        "Idle CPU (def 240 MHz)",  /* SETTINGS_POWER_IDLE_MHZ_TITLE */
        "How long to stay online after last need",  /* SETTINGS_POWER_NET_GRACE_HINT */
        "Keep-alive (def 30 s)",  /* SETTINGS_POWER_NET_GRACE_TITLE */
        "Light-sleep time before power-off",  /* SETTINGS_POWER_OFF_HINT */
        "Never",  /* SETTINGS_POWER_OFF_NEVER */
        "Auto power-off (def 3 min)",  /* SETTINGS_POWER_OFF_TITLE */
        "Idle time before light sleep",  /* SETTINGS_POWER_STANDBY_HINT */
        "Light sleep (def 3 min)",  /* SETTINGS_POWER_STANDBY_TITLE */
        "Capacity: —",  /* SETTINGS_STORAGE_CAP_DASH */
        "Capacity: exporting; disable to view",  /* SETTINGS_STORAGE_CAP_EXPORT */
        "Capacity: read failed",  /* SETTINGS_STORAGE_CAP_FAIL */
        "%s free / %s total",  /* SETTINGS_STORAGE_CAP_FMT */
        "SD: inserted",  /* SETTINGS_STORAGE_INSERTED */
        "SD: not detected",  /* SETTINGS_STORAGE_MISSING */
        "SD: in use by PC",  /* SETTINGS_STORAGE_PC_BUSY */
        "Storage",  /* SETTINGS_STORAGE_TITLE */
        "USB disk not enabled in this firmware",  /* SETTINGS_STORAGE_USB_DISABLED */
        "Uses USB when on; disable restores debug port",  /* SETTINGS_STORAGE_USB_HINT_DISABLED */
        "Disable failed — eject the disk on PC first",  /* SETTINGS_STORAGE_USB_HINT_DISABLE_FAIL */
        "Disabling USB disk…",  /* SETTINGS_STORAGE_USB_HINT_DISABLING */
        "Enable failed — check USB cable and retry",  /* SETTINGS_STORAGE_USB_HINT_ENABLE_FAIL */
        "Enabling USB disk…",  /* SETTINGS_STORAGE_USB_HINT_ENABLING */
        "SD card needs formatting (FAT32)",  /* SETTINGS_STORAGE_USB_HINT_FORMAT */
        "Enabled — access from PC; eject before disable",  /* SETTINGS_STORAGE_USB_HINT_HOST */
        "Failed — eject the disk on PC first",  /* SETTINGS_STORAGE_USB_HINT_HOST_BUSY */
        "When enabled, PC sees this device as a USB disk",  /* SETTINGS_STORAGE_USB_HINT_IDLE */
        "Enabled — host idle; disable to restore USB debug",  /* SETTINGS_STORAGE_USB_HINT_LOCAL */
        "No SD card detected",  /* SETTINGS_STORAGE_USB_HINT_NO_SD */
        "Switching, please wait…",  /* SETTINGS_STORAGE_USB_HINT_SWITCHING */
        "Disable USB disk",  /* SETTINGS_STORAGE_USB_OFF */
        "Enable USB disk",  /* SETTINGS_STORAGE_USB_ON */
        "About",  /* SETTINGS_TAB_ABOUT */
        "Bluetooth",  /* SETTINGS_TAB_BLUETOOTH */
        "Talk",  /* SETTINGS_TAB_CONVERSATION */
        "Haptic",  /* SETTINGS_TAB_HAPTIC */
        "Language",  /* SETTINGS_TAB_LANGUAGE */
        "Network",  /* SETTINGS_TAB_NETWORK */
        "Power",  /* SETTINGS_TAB_POWER */
        "Storage",  /* SETTINGS_TAB_STORAGE */
        "Test",  /* SETTINGS_TAB_TEST */
        "Theme",  /* SETTINGS_TAB_THEME */
        "Aging test",  /* SETTINGS_TEST_AGING */
        "Aging in progress",  /* SETTINGS_TEST_AGING_RUNNING */
        "Mic and speaker OK?",  /* SETTINGS_TEST_AUDIO_CONFIRM */
        "Hold to talk",  /* SETTINGS_TEST_AUDIO_HOLD */
        "Recording…",  /* SETTINGS_TEST_AUDIO_RECORDING */
        "Audio",  /* SETTINGS_TEST_AUDIO_TITLE */
        "Auto test",  /* SETTINGS_TEST_AUTO */
        "Battery test",  /* SETTINGS_TEST_BATTERY */
        "Charging",  /* SETTINGS_TEST_BATTERY_CHARGING */
        "Trickle/pre/CC",  /* SETTINGS_TEST_BATTERY_CHG_CC */
        "CV taper",  /* SETTINGS_TEST_BATTERY_CHG_CV */
        "Charge enable",  /* SETTINGS_TEST_BATTERY_CHG_EN */
        "Not charging / full",  /* SETTINGS_TEST_BATTERY_CHG_NOT */
        "Top-off",  /* SETTINGS_TEST_BATTERY_CHG_TOPOFF */
        "Chip charge",  /* SETTINGS_TEST_BATTERY_CHIP_CHG */
        "Current",  /* SETTINGS_TEST_BATTERY_CURR */
        "Discharging",  /* SETTINGS_TEST_BATTERY_DISCHARGING */
        "Off",  /* SETTINGS_TEST_BATTERY_EN_OFF */
        "On",  /* SETTINGS_TEST_BATTERY_EN_ON */
        "Fuel gauge",  /* SETTINGS_TEST_BATTERY_GAUGE */
        "Gauge state",  /* SETTINGS_TEST_BATTERY_GAUGE_STAT */
        "SOC: 3.30~4.28V; ≥4.28 hold 30s→100%",  /* SETTINGS_TEST_BATTERY_HINT */
        "Charge current",  /* SETTINGS_TEST_BATTERY_ICHG */
        "Idle",  /* SETTINGS_TEST_BATTERY_IDLE_LOAD */
        "Consistency",  /* SETTINGS_TEST_BATTERY_MATCH */
        "Match (DPDM off)",  /* SETTINGS_TEST_BATTERY_MATCH_DPDM */
        "Match",  /* SETTINGS_TEST_BATTERY_MATCH_OK */
        "Chip chg / gauge no",  /* SETTINGS_TEST_BATTERY_MISMATCH_CHIP */
        "Gauge chg / chip no",  /* SETTINGS_TEST_BATTERY_MISMATCH_GAUGE */
        "Input, not charging",  /* SETTINGS_TEST_BATTERY_MISMATCH_NO_CHG */
        "No input but charging?",  /* SETTINGS_TEST_BATTERY_MISMATCH_NO_IN */
        "SOC (instant)",  /* SETTINGS_TEST_BATTERY_SOC_RAW */
        "SOC (smooth)",  /* SETTINGS_TEST_BATTERY_SOC_SMOOTH */
        "VBUS",  /* SETTINGS_TEST_BATTERY_VBUS */
        "Adapter",  /* SETTINGS_TEST_BATTERY_VBUS_ADP */
        "USB CDP",  /* SETTINGS_TEST_BATTERY_VBUS_CDP */
        "USB DCP",  /* SETTINGS_TEST_BATTERY_VBUS_DCP */
        "No input",  /* SETTINGS_TEST_BATTERY_VBUS_NONE */
        "Non-std adapter",  /* SETTINGS_TEST_BATTERY_VBUS_NONSTD */
        "OTG",  /* SETTINGS_TEST_BATTERY_VBUS_OTG */
        "USB SDP",  /* SETTINGS_TEST_BATTERY_VBUS_SDP */
        "Input (unknown)",  /* SETTINGS_TEST_BATTERY_VBUS_UNKNOWN_IN */
        "Unknown adapter",  /* SETTINGS_TEST_BATTERY_VBUS_UNK_ADP */
        "Voltage",  /* SETTINGS_TEST_BATTERY_VOLT */
        "Voltage swing",  /* SETTINGS_TEST_BATTERY_VOLT_RANGE */
        "Charge voltage",  /* SETTINGS_TEST_BATTERY_VREG */
        "Busy…",  /* SETTINGS_TEST_BUSY */
        "Camera",  /* SETTINGS_TEST_CAMERA */
        "Capture failed",  /* SETTINGS_TEST_CAMERA_FAIL */
        "OK",  /* SETTINGS_TEST_CAMERA_OK */
        "Capture result",  /* SETTINGS_TEST_CAMERA_RESULT */
        "Capture",  /* SETTINGS_TEST_CAMERA_SHOT */
        "Timeout / no device",  /* SETTINGS_TEST_CAMERA_TIMEOUT */
        "USB in use",  /* SETTINGS_TEST_CAMERA_USB_BUSY */
        "USB IRQ resource short",  /* SETTINGS_TEST_CAMERA_USB_IRQ */
        "Capturing…",  /* SETTINGS_TEST_CAPTURING */
        "Test cancelled",  /* SETTINGS_TEST_CELL_CANCELLED */
        "CFUN=0 failed",  /* SETTINGS_TEST_CELL_CFUN0_FAIL */
        "CFUN=1 failed",  /* SETTINGS_TEST_CELL_CFUN1_FAIL */
        "4G·External",  /* SETTINGS_TEST_CELL_EXT */
        "4G·Internal",  /* SETTINGS_TEST_CELL_INT */
        "Loss %d/%d",  /* SETTINGS_TEST_CELL_LOSS_FMT */
        "Modem not ready",  /* SETTINGS_TEST_CELL_MODEM_NOT_READY */
        "Modem no response",  /* SETTINGS_TEST_CELL_MODEM_NO_RESP */
        "Switch to 4G mode",  /* SETTINGS_TEST_CELL_NEED_4G */
        "Out of resources",  /* SETTINGS_TEST_CELL_NO_RESOURCE */
        "No SIM detected",  /* SETTINGS_TEST_CELL_NO_SIM */
        "PING command failed",  /* SETTINGS_TEST_CELL_PING_CMD_FAIL */
        "PING failed",  /* SETTINGS_TEST_CELL_PING_FAIL */
        "PING OK",  /* SETTINGS_TEST_CELL_PING_OK */
        "PING wait timeout",  /* SETTINGS_TEST_CELL_PING_WAIT_TIMEOUT */
        "Registration rejected",  /* SETTINGS_TEST_CELL_REG_REJECT */
        "Network search timeout",  /* SETTINGS_TEST_CELL_SEARCH_TIMEOUT */
        "SIM switch failed",  /* SETTINGS_TEST_CELL_SIM_SWITCH_FAIL */
        "SIM slot not applied",  /* SETTINGS_TEST_CELL_SLOT_FAIL */
        "Testing…",  /* SETTINGS_TEST_CELL_TESTING */
        "Test failed",  /* SETTINGS_TEST_CELL_TEST_FAIL */
        "Waiting for resources…",  /* SETTINGS_TEST_CELL_WAIT_RESOURCE */
        "Detecting…",  /* SETTINGS_TEST_DETECTING */
        "Leaving this page will reboot",  /* SETTINGS_TEST_EXIT_REBOOT */
        "Failed",  /* SETTINGS_TEST_FAIL */
        "No",  /* SETTINGS_TEST_NO */
        "Not found",  /* SETTINGS_TEST_NOT_DETECTED */
        "Not passed",  /* SETTINGS_TEST_NOT_PASS */
        "OK",  /* SETTINGS_TEST_OK */
        "Pass",  /* SETTINGS_TEST_PASS */
        "Rebooting…",  /* SETTINGS_TEST_REBOOTING */
        "Scanning…",  /* SETTINGS_TEST_SCANNING */
        "SD card",  /* SETTINGS_TEST_SDCARD */
        "No card / mount fail",  /* SETTINGS_TEST_SD_MOUNT_FAIL */
        "Board unsupported",  /* SETTINGS_TEST_SIGNAL_BOARD_UNSUP */
        "4G CSQ %2d",  /* SETTINGS_TEST_SIGNAL_CELL_CSQ_FMT */
        "4G CSQ unknown",  /* SETTINGS_TEST_SIGNAL_CELL_CSQ_UNKNOWN */
        "4G unavailable",  /* SETTINGS_TEST_SIGNAL_CELL_UNAVAIL */
        "Reading…",  /* SETTINGS_TEST_SIGNAL_READING */
        "Signal",  /* SETTINGS_TEST_SIGNAL_TITLE */
        "Wi-Fi disconnected",  /* SETTINGS_TEST_SIGNAL_WIFI_DISCONNECTED */
        "Wi-Fi read failed",  /* SETTINGS_TEST_SIGNAL_WIFI_READ_FAIL */
        "Wi-Fi %d dBm",  /* SETTINGS_TEST_SIGNAL_WIFI_RSSI_FMT */
        "Failed to create task",  /* SETTINGS_TEST_TASK_CREATE_FAIL */
        "Touch test",  /* SETTINGS_TEST_TOUCH */
        "Tap screen to start",  /* SETTINGS_TEST_TOUCH_START */
        "Waiting…",  /* SETTINGS_TEST_WAITING */
        "Close",  /* SETTINGS_TEST_WIFI_CLOSE */
        "%d network(s)",  /* SETTINGS_TEST_WIFI_COUNT_FMT */
        "Found %d",  /* SETTINGS_TEST_WIFI_FOUND_FMT */
        "Nearby Wi-Fi",  /* SETTINGS_TEST_WIFI_NEARBY */
        "Next",  /* SETTINGS_TEST_WIFI_NEXT */
        "No networks",  /* SETTINGS_TEST_WIFI_NONE */
        "Prev",  /* SETTINGS_TEST_WIFI_PREV */
        "Rescan",  /* SETTINGS_TEST_WIFI_RESCAN */
        "Scanning…",  /* SETTINGS_TEST_WIFI_SCANNING */
        "Scan failed",  /* SETTINGS_TEST_WIFI_SCAN_FAIL */
        "Yes",  /* SETTINGS_TEST_YES */
        "Black border",  /* SETTINGS_THEME_BORDER */
        "Current: Black border",  /* SETTINGS_THEME_CURRENT_BORDER */
        "Current: Dotted gray",  /* SETTINGS_THEME_CURRENT_GRAY */
        "Current: Slash grid",  /* SETTINGS_THEME_CURRENT_SLASH */
        "Current: White",  /* SETTINGS_THEME_CURRENT_WHITE */
        "Dotted gray",  /* SETTINGS_THEME_GRAY */
        "Applies when you return home",  /* SETTINGS_THEME_HINT */
        "Slash grid",  /* SETTINGS_THEME_SLASH */
        "Home theme",  /* SETTINGS_THEME_TITLE */
        "White",  /* SETTINGS_THEME_WHITE */
        "Speaking...",  /* SPEAKING */
        "Standby",  /* STANDBY */
        "Connection failed",  /* STANDBY_CONN_FAIL */
        "%02d/%02d",  /* STANDBY_DATE_FMT */
        "--/--",  /* STANDBY_DATE_PLACEHOLDER */
        "Decode failed",  /* STANDBY_DECODE_FAIL */
        "No todos",  /* STANDBY_EMPTY_TODO */
        "No data",  /* STANDBY_NO_DATA */
        "No network",  /* STANDBY_NO_NETWORK */
        "No task cache",  /* STANDBY_NO_TODO_CACHE */
        "No weather",  /* STANDBY_NO_WEATHER */
        "Parse failed",  /* STANDBY_PARSE_FAIL */
        "Request failed",  /* STANDBY_REQUEST_FAIL */
        "Weather failed",  /* STANDBY_WEATHER_FAIL */
        "Standby wallpaper not found",  /* STANDBY_WP_NOT_FOUND */
        "Switching to 4G...",  /* SWITCH_TO_4G_NETWORK */
        "Switching to Wi-Fi...",  /* SWITCH_TO_WIFI_NETWORK */
        "API error",  /* TASK_API_ERROR */
        "Batch completing...",  /* TASK_BATCH_COMPLETING */
        "Batch failed",  /* TASK_BATCH_FAIL */
        "Batch removing...",  /* TASK_BATCH_REMOVING */
        "%d ok, %d failed",  /* TASK_BATCH_RESULT_FMT */
        "Failed to start batch",  /* TASK_BATCH_START_FAIL */
        "Choose action",  /* TASK_CHOOSE_ACTION */
        "Done",  /* TASK_COMPLETE */
        "Completed",  /* TASK_COMPLETED */
        "Completed %d",  /* TASK_COMPLETED_N_FMT */
        "Complete failed",  /* TASK_COMPLETE_FAIL */
        "Failed to start complete",  /* TASK_COMPLETE_START_FAIL */
        "Complete",  /* TASK_COMPLETE_TODO */
        "Completing...",  /* TASK_COMPLETING */
        "Connecting…",  /* TASK_CONNECTING_NET */
        "Connection failed",  /* TASK_CONN_FAIL */
        "Bad data format",  /* TASK_DATA_FORMAT_ERR */
        "Deleted",  /* TASK_DELETED */
        "Delete failed",  /* TASK_DELETE_FAIL */
        "Failed to start delete",  /* TASK_DELETE_START_FAIL */
        "Delete",  /* TASK_DELETE_TODO */
        "Deleting...",  /* TASK_DELETING */
        "Task completed",  /* TASK_DONE_OK */
        "No completed tasks",  /* TASK_EMPTY_DONE */
        "No todos",  /* TASK_EMPTY_TODO */
        "Invalid todo",  /* TASK_INVALID */
        "Load failed",  /* TASK_LOAD_FAIL */
        "Set up Wi-Fi first",  /* TASK_NEED_WIFI_CFG */
        "Network not ready",  /* TASK_NET_NOT_READY */
        "Nothing to complete",  /* TASK_NO_COMPLETABLE */
        "No network",  /* TASK_NO_NETWORK */
        "Response parse failed",  /* TASK_PARSE_FAIL */
        "Please wait...",  /* TASK_PLEASE_WAIT */
        "Refresh",  /* TASK_REFRESH */
        "Refreshed",  /* TASK_REFRESHED */
        "Refreshing...",  /* TASK_REFRESHING */
        "Failed to start refresh",  /* TASK_REFRESH_START_FAIL */
        "Removed %d",  /* TASK_REMOVED_N_FMT */
        "Request failed",  /* TASK_REQUEST_FAIL */
        "Selected %d",  /* TASK_SELECTED_FMT */
        "Select items first",  /* TASK_SELECT_FIRST */
        "Done",  /* TASK_TAB_DONE */
        "Todo",  /* TASK_TAB_TODO */
        "Done",  /* TASK_TITLE_DONE */
        "Schedule",  /* TASK_TITLE_TODO */
        "Touch not detected",  /* TOUCH_MISSING_HINT */
        "Audio busy, try later",  /* TRANSLATE_AUDIO_BUSY */
        "Cancelled",  /* TRANSLATE_CANCELLED */
        "Connecting…",  /* TRANSLATE_CONNECTING */
        "Connection failed",  /* TRANSLATE_CONNECT_FAIL */
        "Connecting…",  /* TRANSLATE_CONNECT_NET */
        "Translation error",  /* TRANSLATE_ERROR */
        "Getting token…",  /* TRANSLATE_GET_TOKEN */
        "Token failed",  /* TRANSLATE_GET_TOKEN_FAIL */
        "Pick languages, then tap Live translate",  /* TRANSLATE_HINT */
        "Invalid language selection",  /* TRANSLATE_LANG_INVALID */
        "Source and target must differ",  /* TRANSLATE_LANG_SAME */
        "Live translate",  /* TRANSLATE_LIVE */
        "Complete Wi-Fi setup first",  /* TRANSLATE_NEED_WIFI_CFG */
        "Connecting to network…",  /* TRANSLATE_NET_CONNECTING */
        "Network not ready",  /* TRANSLATE_NET_NOT_READY */
        "Select source language",  /* TRANSLATE_PICK_FROM */
        "Select language",  /* TRANSLATE_PICK_LANG */
        "Select target language",  /* TRANSLATE_PICK_TO */
        "Ready timeout",  /* TRANSLATE_READY_TIMEOUT */
        "Recognized text will appear here",  /* TRANSLATE_SOURCE_PLACEHOLDER */
        "Source",  /* TRANSLATE_SOURCE_TITLE */
        "Start failed",  /* TRANSLATE_START_FAIL */
        "Stop",  /* TRANSLATE_STOP */
        "Stopped",  /* TRANSLATE_STOPPED */
        "Stopping…",  /* TRANSLATE_STOPPING */
        "Translating",  /* TRANSLATE_TRANSLATING */
        "Translation will appear here",  /* TRANSLATE_TRANS_PLACEHOLDER */
        "Translation",  /* TRANSLATE_TRANS_TITLE */
        "Try again later",  /* TRANSLATE_TRY_LATER */
        "Upgrade failed",  /* UPGRADE_FAILED */
        "System is upgrading...",  /* UPGRADING */
        "Ver ",  /* VERSION */
        "Connecting… will start automatically",  /* VOICE_STARTING_NET */
        "Volume ",  /* VOLUME */
        "Power-off",  /* WALLPAPER_CHIP_SHUTDOWN */
        "Standby",  /* WALLPAPER_CHIP_STANDBY */
        "Decode failed",  /* WALLPAPER_DECODE_FAIL */
        "Delete wallpaper",  /* WALLPAPER_DELETE_BTN */
        "Delete failed",  /* WALLPAPER_DELETE_FAIL */
        "Failed to start delete",  /* WALLPAPER_DELETE_START_FAIL */
        "Deleting…",  /* WALLPAPER_DELETING */
        "No wallpapers\nPlace in %s",  /* WALLPAPER_EMPTY_FMT */
        "Enable failed",  /* WALLPAPER_ENABLE_FAIL */
        "Invalid file",  /* WALLPAPER_FILE_INVALID */
        "File not found",  /* WALLPAPER_FILE_MISSING */
        "Loading…",  /* WALLPAPER_LOADING */
        "Invalid filename",  /* WALLPAPER_NAME_INVALID */
        "No SD card",  /* WALLPAPER_NO_SD */
        "Invalid path",  /* WALLPAPER_PATH_INVALID */
        "Cannot preview",  /* WALLPAPER_PREVIEW_FAIL */
        "Out of memory, preview failed",  /* WALLPAPER_PREVIEW_OOM */
        "Failed to start preview",  /* WALLPAPER_PREVIEW_START_FAIL */
        "Selected %d",  /* WALLPAPER_SELECTED_FMT */
        "Set as power-off",  /* WALLPAPER_SET_SHUTDOWN */
        "Set as standby",  /* WALLPAPER_SET_STANDBY */
        "Power-off on",  /* WALLPAPER_SHUTDOWN_ON */
        "Standby on",  /* WALLPAPER_STANDBY_ON */
        "Warning",  /* WARNING */
        "Wi-Fi Setup",  /* WIFI_CONFIG_MODE */
};
static const char* const kLiterals_zh_cn[] = {
        "，浏览器访问 ",  /* ACCESS_VIA_BROWSER */
        "激活设备",  /* ACTIVATION */
        "验证码:%s",  /* ACTIVATION_CODE_FMT */
        "正在充电",  /* BATTERY_CHARGING */
        "电量已满",  /* BATTERY_FULL */
        "电量不足",  /* BATTERY_LOW */
        "电量低，请充电",  /* BATTERY_NEED_CHARGE */
        "参数错误",  /* BOOK_BAD_ARG */
        "（空白）",  /* BOOK_BLANK */
        "正文",  /* BOOK_BODY */
        "已取消",  /* BOOK_CANCELLED */
        "第%d章",  /* BOOK_CHAPTER_FMT */
        "（本章无文本）",  /* BOOK_CHAPTER_NO_TEXT */
        "章节进度",  /* BOOK_CHAPTER_PROGRESS */
        "校验失败",  /* BOOK_CHECKSUM_FAIL */
        "创建连接失败",  /* BOOK_CONN_FAIL */
        "继续阅读",  /* BOOK_CONTINUE */
        "%d本",  /* BOOK_COUNT_FMT */
        "封面过大",  /* BOOK_COVER_TOO_LARGE */
        "删除失败",  /* BOOK_DELETE_FAILED */
        "详情",  /* BOOK_DETAIL */
        "下载失败",  /* BOOK_DOWNLOAD_FAIL */
        "%s %u小时%u分钟",  /* BOOK_DURATION_HM_FMT */
        "%s %u小时",  /* BOOK_DURATION_H_FMT */
        "%s 不足1分钟",  /* BOOK_DURATION_LT1MIN_FMT */
        "%s %u分钟",  /* BOOK_DURATION_M_FMT */
        "空封面",  /* BOOK_EMPTY_COVER */
        "空文件",  /* BOOK_EMPTY_FILE */
        "暂无书籍\n请将 .epub / .txt / .ebook\n放到 %s",  /* BOOK_EMPTY_HOME_FMT */
        "书架为空\n请将 .epub / .txt / .ebook\n放到 %s",  /* BOOK_EMPTY_SHELF_FMT */
        "文件过大",  /* BOOK_FILE_TOO_LARGE */
        "已看完",  /* BOOK_FINISHED */
        "无可用字体",  /* BOOK_FONT_EMPTY */
        "文件名无效",  /* BOOK_FONT_NAME_INVALID */
        "字体",  /* BOOK_FONT_TITLE */
        "[图片]",  /* BOOK_IMAGE_PLACEHOLDER */
        "编排中",  /* BOOK_LAYOUT_BUSY */
        "完成",  /* BOOK_LAYOUT_DONE */
        "行距",  /* BOOK_LINE_GAP */
        "边距",  /* BOOK_MARGIN */
        "窄",  /* BOOK_MARGIN_NARROW */
        "标准",  /* BOOK_MARGIN_STANDARD */
        "很宽",  /* BOOK_MARGIN_VERY_WIDE */
        "宽",  /* BOOK_MARGIN_WIDE */
        "网络未就绪",  /* BOOK_NET_NOT_READY */
        "暂无可续",  /* BOOK_NO_CONTINUE */
        "无封面地址",  /* BOOK_NO_COVER_URL */
        "缺少下载地址",  /* BOOK_NO_DOWNLOAD_URL */
        "无本地文件",  /* BOOK_NO_LOCAL_FILE */
        "无网络",  /* BOOK_NO_NETWORK */
        "暂无最近阅读",  /* BOOK_NO_RECENT */
        "未检测到 SD 卡\n请插入后重试",  /* BOOK_NO_SD */
        "未检测到 SD 卡",  /* BOOK_NO_SD_SHORT */
        "正在打开",  /* BOOK_OPENING */
        "打开失败",  /* BOOK_OPEN_FAILED */
        "打开失败\n章节过大或内存不足",  /* BOOK_OPEN_FAILED_CHAPTER */
        "打开失败\n请检查 EPUB 完整性",  /* BOOK_OPEN_FAILED_EPUB */
        "打开失败\n无法启动解析任务",  /* BOOK_OPEN_FAILED_TASK */
        "打开失败\nTXT 过大或内存不足",  /* BOOK_OPEN_FAILED_TXT */
        "内存不足",  /* BOOK_OUT_OF_MEMORY */
        "无法加载页面",  /* BOOK_PAGE_LOAD_FAILED */
        "路径无效",  /* BOOK_PATH_INVALID */
        "阅读时长",  /* BOOK_READ_DURATION */
        "读取失败",  /* BOOK_READ_FAIL */
        "（读文件失败）",  /* BOOK_READ_FILE_FAIL */
        "已阅读%d.%d%%",  /* BOOK_READ_PCT_FMT */
        "阅读进度",  /* BOOK_READ_PROGRESS */
        "最近阅读",  /* BOOK_RECENT */
        "保存失败",  /* BOOK_SAVE_FAIL */
        "已选 %d",  /* BOOK_SELECTED_FMT */
        "我的书架",  /* BOOK_SHELF_TITLE */
        "紧凑",  /* BOOK_SPACING_COMPACT */
        "宽松",  /* BOOK_SPACING_RELAXED */
        "标准",  /* BOOK_SPACING_STANDARD */
        "很宽松",  /* BOOK_SPACING_VERY_RELAXED */
        "开始阅读",  /* BOOK_START */
        "存储目录不可用",  /* BOOK_STORAGE_UNAVAIL */
        "空请求体",  /* BOOK_SYNC_EMPTY_BODY */
        "目录",  /* BOOK_TOC */
        "暂无目录",  /* BOOK_TOC_EMPTY */
        "目录 %d/%d",  /* BOOK_TOC_PAGE_FMT */
        "今日时长",  /* BOOK_TODAY_DURATION */
        "下划线",  /* BOOK_UNDERLINE */
        "虚线",  /* BOOK_UNDERLINE_DASHED */
        "实线",  /* BOOK_UNDERLINE_SOLID */
        "写文件失败",  /* BOOK_WRITE_FAIL */
        "通话模式",  /* BT_CALL_BTN */
        "通话模式 (SCO 已建立)",  /* BT_CALL_MODE_SCO */
        "正在连接...",  /* BT_CONNECTING */
        "连接中: %s...",  /* BT_CONNECTING_FMT */
        "连接成功",  /* BT_CONNECT_OK */
        "连接失败 (超时)",  /* BT_CONNECT_TIMEOUT */
        "外置蓝牙音频解码芯片设置（非 ESP32 内置蓝牙）",  /* BT_DESC */
        "发现设备: %s",  /* BT_FOUND_FMT */
        "模式1",  /* BT_MODE1 */
        "模式1 已激活\n(AT+RX=2 / AT+MODE=1)",  /* BT_MODE1_ACTIVE */
        "模式1 已设置",  /* BT_MODE1_SET */
        "模式2",  /* BT_MODE2 */
        "模式2 已设置，可扫描设备",  /* BT_MODE2_SET */
        "模式3",  /* BT_MODE3 */
        "模式3 已设置",  /* BT_MODE3_SET */
        "音乐模式",  /* BT_MUSIC_BTN */
        "音乐模式 (SCO 已断开)",  /* BT_MUSIC_MODE_SCO */
        "请先连接蓝牙设备",  /* BT_NEED_CONNECT */
        "请先切换到模式2",  /* BT_NEED_MODE2 */
        "蓝牙电源已复位",  /* BT_PWR_RESET_OK */
        "当前硬件不支持蓝牙电源复位",  /* BT_PWR_RESET_UNSUP */
        "复位蓝牙",  /* BT_RESET_BTN */
        "烧录蓝牙固件时使用",  /* BT_RESET_HINT */
        "正在扫描...",  /* BT_SCANNING */
        "扫描设备",  /* BT_SCAN_BTN */
        "扫描完成，共 %d 个设备",  /* BT_SCAN_DONE_FMT */
        "开始扫描...",  /* BT_SCAN_START */
        "请选择蓝牙模式",  /* BT_SELECT_MODE */
        "切换通话模式...",  /* BT_SWITCH_CALL */
        "切换模式1...",  /* BT_SWITCH_MODE1 */
        "切换模式2...",  /* BT_SWITCH_MODE2 */
        "切换模式3...",  /* BT_SWITCH_MODE3 */
        "切换音乐模式...",  /* BT_SWITCH_MUSIC */
        "蓝牙",  /* BT_TITLE */
        "UART 未初始化",  /* BT_UART_NOT_INIT */
        "检查新版本...",  /* CHECKING_NEW_VERSION */
        "检查新版本失败，将在 %d 秒后重试：%s",  /* CHECK_NEW_VERSION_FAILED */
        "接口返回错误",  /* CLOUD_API_ERROR */
        "已取消",  /* CLOUD_CANCELLED */
        "正在取消…",  /* CLOUD_CANCELLING */
        "是否下载「%s」？\n类型 %s · 大小 %s",  /* CLOUD_CONFIRM_DL_FMT */
        "正在联网…",  /* CLOUD_CONNECTING_NET */
        "连接网络…",  /* CLOUD_CONNECT_NET */
        "创建连接失败",  /* CLOUD_CONN_FAIL */
        "封面加载失败",  /* CLOUD_COVER_FAIL */
        "数据格式错误",  /* CLOUD_DATA_FORMAT_ERR */
        "解码失败",  /* CLOUD_DECODE_FAIL */
        "删除失败",  /* CLOUD_DELETE_FAIL */
        "下载",  /* CLOUD_DOWNLOAD */
        "正在下载",  /* CLOUD_DOWNLOADING */
        "下载失败",  /* CLOUD_DOWNLOAD_FAIL */
        "暂无待传输资源",  /* CLOUD_EMPTY_ALL */
        "暂无书籍",  /* CLOUD_EMPTY_BOOK */
        "暂无字体",  /* CLOUD_EMPTY_FONT */
        "暂无壁纸",  /* CLOUD_EMPTY_WALLPAPER */
        "正在拉取列表…",  /* CLOUD_FETCHING_LIST */
        "文件无效",  /* CLOUD_FILE_INVALID */
        "已在队列中",  /* CLOUD_IN_QUEUE */
        "JSON 创建失败",  /* CLOUD_JSON_CREATE_FAIL */
        "JSON 解析失败",  /* CLOUD_JSON_PARSE_FAIL */
        "JSON 序列化失败",  /* CLOUD_JSON_SERIALIZE_FAIL */
        "列表过大",  /* CLOUD_LIST_TOO_LARGE */
        "加载中…",  /* CLOUD_LOADING */
        "加载封面…",  /* CLOUD_LOAD_COVER */
        "加载失败",  /* CLOUD_LOAD_FAIL */
        "缺少 taskId",  /* CLOUD_MISSING_TASK_ID */
        "请先完成配网",  /* CLOUD_NEED_WIFI_CFG */
        "网络未就绪",  /* CLOUD_NET_NOT_READY */
        "暂无封面",  /* CLOUD_NO_COVER */
        "缺少下载地址",  /* CLOUD_NO_DOWNLOAD_URL */
        "无本地文件",  /* CLOUD_NO_LOCAL_FILE */
        "无网络",  /* CLOUD_NO_NETWORK */
        "未检测到 SD 卡",  /* CLOUD_NO_SD */
        "路径无效",  /* CLOUD_PATH_INVALID */
        "待接收",  /* CLOUD_PENDING */
        "请稍候",  /* CLOUD_PLEASE_WAIT */
        "无法预览",  /* CLOUD_PREVIEW_FAIL */
        "预览启动失败",  /* CLOUD_PREVIEW_START_FAIL */
        "预览地址过长",  /* CLOUD_PREVIEW_URL_LONG */
        "书籍推送",  /* CLOUD_PUSH_BOOK */
        "字体推送",  /* CLOUD_PUSH_FONT */
        "壁纸推送",  /* CLOUD_PUSH_WALLPAPER */
        "读取超时或中断",  /* CLOUD_READ_TIMEOUT */
        "刷新",  /* CLOUD_REFRESH */
        "已刷新",  /* CLOUD_REFRESHED */
        "刷新中…",  /* CLOUD_REFRESHING */
        "请求失败",  /* CLOUD_REQUEST_FAIL */
        "保存",  /* CLOUD_SAVE */
        "已保存 0 / 共 1",  /* CLOUD_SAVED_0_1 */
        "已保存 %d / 共 %d",  /* CLOUD_SAVED_FMT */
        "已保存(同步失败)",  /* CLOUD_SAVED_SYNC_FAIL */
        "保存失败",  /* CLOUD_SAVE_FAIL */
        "保存到本地",  /* CLOUD_SAVE_LOCAL */
        "正在保存",  /* CLOUD_SAVING */
        "正在保存\n%s",  /* CLOUD_SAVING_NAME_FMT */
        "已选 %d",  /* CLOUD_SELECTED_FMT */
        "请先选择",  /* CLOUD_SELECT_FIRST */
        "存储目录不可用",  /* CLOUD_STORAGE_UNAVAIL */
        "同步失败",  /* CLOUD_SYNC_FAIL */
        "全部",  /* CLOUD_TAB_ALL */
        "书籍",  /* CLOUD_TAB_BOOK */
        "字体",  /* CLOUD_TAB_FONT */
        "壁纸",  /* CLOUD_TAB_WALLPAPER */
        "任务失败",  /* CLOUD_TASK_FAIL */
        "书籍",  /* CLOUD_TYPE_BOOK */
        "字体",  /* CLOUD_TYPE_FONT */
        "壁纸",  /* CLOUD_TYPE_WALLPAPER */
        "未知类型",  /* CLOUD_UNKNOWN_TYPE */
        "请等待下载任务结束",  /* CLOUD_WAIT_DOWNLOAD */
        "取消",  /* COMMON_CANCEL */
        "删除",  /* COMMON_DELETE */
        "暂无内容",  /* COMMON_EMPTY */
        "失败",  /* COMMON_FAILED */
        "加载中...",  /* COMMON_LOADING */
        "关闭",  /* COMMON_OFF */
        "确定",  /* COMMON_OK */
        "开启",  /* COMMON_ON */
        "移除",  /* COMMON_REMOVE */
        "重试",  /* COMMON_RETRY */
        "全选",  /* COMMON_SELECT_ALL */
        "成功",  /* COMMON_SUCCESS */
        "未知",  /* COMMON_UNKNOWN */
        "已连接 ",  /* CONNECTED_TO */
        "连接中...",  /* CONNECTING */
        "Connection Successful",  /* CONNECTION_SUCCESSFUL */
        "连接 ",  /* CONNECT_TO */
        "手机连接热点 ",  /* CONNECT_TO_HOTSPOT */
        "检测模组...",  /* DETECTING_MODULE */
        "下载资源失败",  /* DOWNLOAD_ASSETS_FAILED */
        "进入配网模式...",  /* ENTERING_WIFI_CONFIG_MODE */
        "错误",  /* ERROR */
        "发现新资源: %s",  /* FOUND_NEW_ASSETS */
        "你好，我的朋友！",  /* HELLO_MY_FRIEND */
        "百问AI",  /* HOME_APP_ASSISTANT */
        "阅读",  /* HOME_APP_BOOK */
        "传输",  /* HOME_APP_CLOUD */
        "设置",  /* HOME_APP_SETTINGS */
        "每日清单",  /* HOME_APP_TASK */
        "壁纸",  /* HOME_APP_WALLPAPER */
        "%02d月%02d日",  /* HOME_DATE_FMT */
        "--月--日",  /* HOME_DATE_PLACEHOLDER */
        "%d.%d.%d",  /* HOME_DATE_SLASH_FMT */
        "----.-.--",  /* HOME_DATE_SLASH_PLACEHOLDER */
        "周五",  /* HOME_WDAY_FRI */
        "周一",  /* HOME_WDAY_MON */
        "周六",  /* HOME_WDAY_SAT */
        "周日",  /* HOME_WDAY_SUN */
        "周四",  /* HOME_WDAY_THU */
        "周二",  /* HOME_WDAY_TUE */
        "周三",  /* HOME_WDAY_WED */
        "信息",  /* INFO */
        "正在初始化...",  /* INITIALIZING */
        "聆听中...",  /* LISTENING */
        "加载资源...",  /* LOADING_ASSETS */
        "登录服务器...",  /* LOADING_PROTOCOL */
        "最大音量",  /* MAX_VOLUME */
        "已静音",  /* MUTED */
        "请先完成配网",  /* NEED_WIFI_CFG */
        "[开放]",  /* NETWORK_AUTH_OPEN */
        "[加密]",  /* NETWORK_AUTH_SECURE */
        "正在连接，请稍后再扫描",  /* NETWORK_BUSY_CONNECT */
        "取消",  /* NETWORK_CANCEL */
        "已清空已保存网络",  /* NETWORK_CLEARED_OK */
        "清空",  /* NETWORK_CLEAR_ALL */
        "连接",  /* NETWORK_CONNECT */
        "已连接 %s",  /* NETWORK_CONNECTED_FMT */
        "正在连接 %s …",  /* NETWORK_CONNECTING_FMT */
        "连接失败",  /* NETWORK_CONNECT_FAIL */
        "无法启动连接任务",  /* NETWORK_CONNECT_TASK_FAIL */
        "连接超时",  /* NETWORK_CONNECT_TIMEOUT */
        "未能在 15 秒内完成连接，请重试",  /* NETWORK_CONNECT_TIMEOUT_HINT */
        "连接到：%s",  /* NETWORK_CONNECT_TO_FMT */
        "%s（默认）",  /* NETWORK_DEFAULT_FMT */
        "删除",  /* NETWORK_DELETE */
        "已删除该网络",  /* NETWORK_DELETED_OK */
        "未找到该 WiFi（信号丢失）",  /* NETWORK_ERR_AP_GONE */
        "关联失败，路由器拒绝连接",  /* NETWORK_ERR_ASSOC */
        "密码错误，请重新输入",  /* NETWORK_ERR_BAD_PASSWORD */
        "连接被拒绝 (reason=%u)",  /* NETWORK_ERR_REASON_FMT */
        "信号太弱，连接超时",  /* NETWORK_ERR_WEAK */
        "未发现网络，点「刷新」重试",  /* NETWORK_NEARBY_EMPTY */
        "请输入 WiFi 密码（8~63 字符）",  /* NETWORK_PWD_HINT */
        "WiFi 密码",  /* NETWORK_PWD_PLACEHOLDER */
        "密码超长",  /* NETWORK_PWD_TOO_LONG */
        "暂无已保存的 WiFi",  /* NETWORK_SAVED_EMPTY */
        "刷新",  /* NETWORK_SCAN */
        "正在扫描附近 WiFi…",  /* NETWORK_SCANNING */
        "扫描完成，共 %d 个网络",  /* NETWORK_SCAN_DONE_FMT */
        "启动扫描失败",  /* NETWORK_SCAN_FAIL */
        "无法启动扫描任务",  /* NETWORK_SCAN_TASK_FAIL */
        "扫描超时",  /* NETWORK_SCAN_TIMEOUT */
        "置顶",  /* NETWORK_SET_DEFAULT */
        "已设为默认网络",  /* NETWORK_SET_DEFAULT_OK */
        "显示密码",  /* NETWORK_SHOW_PWD */
        "SSID 不合法",  /* NETWORK_SSID_INVALID */
        "附近",  /* NETWORK_TAB_NEARBY */
        "已保存",  /* NETWORK_TAB_SAVED */
        "配置 WIFI",  /* NETWORK_TITLE */
        "正在初始化 WiFi…",  /* NETWORK_WIFI_INIT */
        "WiFi 初始化失败",  /* NETWORK_WIFI_INIT_FAIL */
        "新版本 ",  /* NEW_VERSION */
        "已是最新版本",  /* OTA_ALREADY_LATEST */
        "检查更新失败",  /* OTA_CHECK_FAILED */
        "发现新固件\n当前版本 %s\n最新版本 %s",  /* OTA_CONFIRM_FMT */
        "当前版本  %s",  /* OTA_CUR_VER_FMT */
        "请勿断电或退出",  /* OTA_HINT */
        "忽略本次升级",  /* OTA_IGNORE_VERSION */
        "手动升级",  /* OTA_MANUAL */
        "最新版本  %s",  /* OTA_NEW_VER_FMT */
        "下次提醒我",  /* OTA_REMIND_LATER */
        "升级成功，即将重启",  /* OTA_SUCCESS_REBOOT */
        "正在升级系统",  /* OTA_TITLE */
        "OTA 升级",  /* OTA_UPGRADE */
        "立刻升级",  /* OTA_UPGRADE_NOW */
        "系统忙",  /* PHONE_BUSY */
        "正在检查网络...",  /* PHONE_CHECKING_NET */
        "请检查移动网络",  /* PHONE_CHECK_CELL */
        "正在确认 SIM…",  /* PHONE_CONFIRM_SIM */
        "拨打",  /* PHONE_DIAL */
        "拨号失败",  /* PHONE_DIAL_FAIL */
        "挂断",  /* PHONE_HANGUP */
        "内置卡无法拨打电话\n请到设置→网络中切换到外置卡",  /* PHONE_INTERNAL_SIM */
        "通话中",  /* PHONE_IN_CALL */
        "无 4G 模块",  /* PHONE_NO_4G */
        "WiFi 模式无法拨打电话\n请到设置→网络中切换到 4G",  /* PHONE_WIFI_BLOCK */
        "请插入 SIM 卡",  /* PIN_ERROR */
        "请稍候...",  /* PLEASE_WAIT */
        "已关机",  /* POWERED_OFF */
        "转写完成",  /* RECORD_ASR_DONE */
        "转写成功，但没有可显示的内容",  /* RECORD_ASR_EMPTY */
        "转写失败",  /* RECORD_ASR_FAIL */
        "点击「转写」上传，稍后再进详情查看结果",  /* RECORD_ASR_HINT */
        "转写失败（HTTP %d）",  /* RECORD_ASR_HTTP_FMT */
        "请先连接网络后再使用转写",  /* RECORD_ASR_NEED_NET */
        "转写结果解析失败",  /* RECORD_ASR_PARSE_FAIL */
        "转写处理中，请稍后",  /* RECORD_ASR_PENDING */
        "转写请求失败",  /* RECORD_ASR_REQ_FAIL */
        "转写任务启动失败",  /* RECORD_ASR_START_FAIL */
        "上传成功，请稍后来查看转写结果，勿反复上传",  /* RECORD_ASR_UPLOADED_HINT */
        "音频忙，稍后再试",  /* RECORD_AUDIO_BUSY */
        "音频未就绪",  /* RECORD_AUDIO_NOT_READY */
        "音频启动中，请稍候",  /* RECORD_AUDIO_STARTING */
        "转写",  /* RECORD_BTN_ASR */
        "播放",  /* RECORD_BTN_PLAY */
        "保存中…",  /* RECORD_BTN_SAVING */
        "开始录音",  /* RECORD_BTN_START */
        "结束录音",  /* RECORD_BTN_STOP */
        "停止播放",  /* RECORD_BTN_STOP_PLAY */
        "已删除",  /* RECORD_DELETED */
        "删除失败",  /* RECORD_DELETE_FAIL */
        "音频时长：%.1f 秒",  /* RECORD_DURATION_SEC_FMT */
        "暂无录音",  /* RECORD_EMPTY */
        "录音文件损坏",  /* RECORD_FILE_CORRUPT */
        "录音文件过大或无效",  /* RECORD_FILE_TOO_LARGE */
        "点击下方按钮开始录音",  /* RECORD_HINT_START */
        "请插入 SD 卡",  /* RECORD_INSERT_SD */
        "已达最长 %d 分钟，已自动保存",  /* RECORD_MAX_MIN_FMT */
        "时长 %s · %s",  /* RECORD_META_FMT */
        "无法创建录音目录",  /* RECORD_MKDIR_FAIL */
        "网络不可用",  /* RECORD_NET_UNAVAIL */
        "未检测到 SD 卡\n\n请插入 SD 卡后再使用录音功能",  /* RECORD_NO_SD_HINT */
        "内存不足",  /* RECORD_OOM */
        "无法打开录音文件",  /* RECORD_OPEN_FAIL */
        "播放中…",  /* RECORD_PLAYING */
        "播放结束",  /* RECORD_PLAY_END */
        "播放任务启动失败",  /* RECORD_PLAY_START_FAIL */
        "已停止播放",  /* RECORD_PLAY_STOPPED */
        "请稍候",  /* RECORD_PLEASE_WAIT */
        "读取录音失败",  /* RECORD_READ_FAIL */
        "正在录音…",  /* RECORD_RECORDING */
        "已保存到 SD 卡",  /* RECORD_SAVED_SD */
        "请先结束录音",  /* RECORD_STOP_FIRST */
        "摘要",  /* RECORD_SUMMARY */
        "列表",  /* RECORD_TAB_LIST */
        "录音",  /* RECORD_TAB_REC */
        "录音任务启动失败",  /* RECORD_TASK_START_FAIL */
        "录音太短，再试一次",  /* RECORD_TOO_SHORT */
        "正在上传…",  /* RECORD_UPLOADING */
        "写入失败，未保存",  /* RECORD_WRITE_FAIL */
        "等待网络...",  /* REGISTERING_NETWORK */
        "无法接入网络，请检查流量卡状态",  /* REG_ERROR */
        "AEC 关闭",  /* RTC_MODE_OFF */
        "AEC 开启",  /* RTC_MODE_ON */
        "扫描 Wi-Fi...",  /* SCANNING_WIFI */
        "发送失败，请检查网络",  /* SERVER_ERROR */
        "无法连接服务，请稍后再试",  /* SERVER_NOT_CONNECTED */
        "正在寻找可用服务",  /* SERVER_NOT_FOUND */
        "等待响应超时",  /* SERVER_TIMEOUT */
        "编译时间",  /* SETTINGS_ABOUT_BUILD */
        "芯片型号",  /* SETTINGS_ABOUT_CHIP */
        "CPU 核心",  /* SETTINGS_ABOUT_CORES */
        "%u 核",  /* SETTINGS_ABOUT_CORES_FMT */
        "Flash 容量",  /* SETTINGS_ABOUT_FLASH */
        "固件版本",  /* SETTINGS_ABOUT_FW */
        "点击检测更新",  /* SETTINGS_ABOUT_FW_CHECK */
        "固件版本:%s",  /* SETTINGS_ABOUT_FW_VER_FMT */
        "MAC 地址",  /* SETTINGS_ABOUT_MAC */
        "设备型号",  /* SETTINGS_ABOUT_MODEL */
        "无",  /* SETTINGS_ABOUT_NONE */
        "PSRAM 总大小",  /* SETTINGS_ABOUT_PSRAM */
        "忙，请稍候",  /* SETTINGS_CONV_BUSY */
        "连接网络…",  /* SETTINGS_CONV_CONNECTING */
        "创建连接失败",  /* SETTINGS_CONV_CONN_FAIL */
        "当前：—",  /* SETTINGS_CONV_CUR_DASH */
        "当前：关闭 TTS",  /* SETTINGS_CONV_CUR_OFF */
        "当前：开启 TTS",  /* SETTINGS_CONV_CUR_ON */
        "正在关闭…",  /* SETTINGS_CONV_DISABLING */
        "正在开启…",  /* SETTINGS_CONV_ENABLING */
        "控制百问AI是否播报语音\n关闭后仅显示文字",  /* SETTINGS_CONV_HINT */
        "缺少 enabled",  /* SETTINGS_CONV_MISSING_ENABLED */
        "网络未就绪",  /* SETTINGS_CONV_NET_NOT_READY */
        "无网络",  /* SETTINGS_CONV_NO_NETWORK */
        "响应解析失败",  /* SETTINGS_CONV_PARSE_FAIL */
        "请求失败",  /* SETTINGS_CONV_REQUEST_FAIL */
        "语音播报",  /* SETTINGS_CONV_TITLE */
        "关闭 TTS",  /* SETTINGS_CONV_TTS_OFF */
        "开启 TTS",  /* SETTINGS_CONV_TTS_ON */
        "当前：关闭",  /* SETTINGS_HAPTIC_CURRENT_OFF */
        "当前：开启",  /* SETTINGS_HAPTIC_CURRENT_ON */
        "界面按钮与盖板虚拟键",  /* SETTINGS_HAPTIC_HINT */
        "关闭",  /* SETTINGS_HAPTIC_OFF */
        "开启",  /* SETTINGS_HAPTIC_ON */
        "按键震动",  /* SETTINGS_HAPTIC_TITLE */
        "当前：English",  /* SETTINGS_LANG_CURRENT_EN */
        "当前：简体中文",  /* SETTINGS_LANG_CURRENT_ZH */
        "English",  /* SETTINGS_LANG_EN_US */
        "切换后立即生效",  /* SETTINGS_LANG_HINT */
        "界面语言",  /* SETTINGS_LANG_TITLE */
        "简体中文",  /* SETTINGS_LANG_ZH_CN */
        "无法打开 WiFi 配置页",  /* SETTINGS_NET_AP_TASK_FAIL */
        "当前板型不支持 WiFi 配置",  /* SETTINGS_NET_BOARD_NO_AP */
        "正在关闭射频…\nAT+CFUN=0",  /* SETTINGS_NET_CFUN0 */
        "AT+CFUN=0 执行失败",  /* SETTINGS_NET_CFUN0_FAIL */
        "正在重新搜网…\nAT+CFUN=1",  /* SETTINGS_NET_CFUN1 */
        "当前：%s",  /* SETTINGS_NET_CURRENT_FMT */
        "当前：4G 上网",  /* SETTINGS_NET_CUR_4G */
        "当前：--",  /* SETTINGS_NET_CUR_DASH */
        "当前：WiFi 上网",  /* SETTINGS_NET_CUR_WIFI */
        "AT+ECSIMCFG 执行失败",  /* SETTINGS_NET_ECSIMCFG_FAIL */
        "打开 WiFi 配置…",  /* SETTINGS_NET_ENTER_AP */
        "配置 WIFI 网络",  /* SETTINGS_NET_ENTER_CFG */
        "热点名称：",  /* SETTINGS_NET_HOTSPOT_NAME */
        "4G 上网",  /* SETTINGS_NET_MODE_4G */
        "上网方式",  /* SETTINGS_NET_MODE_TITLE */
        "WiFi 上网",  /* SETTINGS_NET_MODE_WIFI */
        "当前不在 4G 模式，无法切换",  /* SETTINGS_NET_NOT_4G */
        "未检测到 4G 模块",  /* SETTINGS_NET_NO_4G */
        "正在重启…",  /* SETTINGS_NET_REBOOTING */
        "%s\n%d 秒后重启…",  /* SETTINGS_NET_REBOOT_COUNT_FMT */
        "切换后设备将自动重启",  /* SETTINGS_NET_REBOOT_HINT */
        "外置卡",  /* SETTINGS_NET_SIM_EXT */
        "内置卡",  /* SETTINGS_NET_SIM_INT */
        "无法启动 SIM 切换任务",  /* SETTINGS_NET_SIM_TASK_FAIL */
        "SIM 卡",  /* SETTINGS_NET_SIM_TITLE */
        "已切换到%s",  /* SETTINGS_NET_SWITCHED_FMT */
        "正在切换到 4G…",  /* SETTINGS_NET_SWITCH_4G */
        "正在切换到%s…\nAT+CFUN=0",  /* SETTINGS_NET_SWITCH_SIM_CFUN0_FMT */
        "正在切换到%s…\n%s",  /* SETTINGS_NET_SWITCH_SIM_FMT */
        "正在切换到 WiFi…",  /* SETTINGS_NET_SWITCH_WIFI */
        "扫描附近 WiFi，选择后输入密码",  /* SETTINGS_NET_WIFI_CFG_HINT */
        "WiFi 网络",  /* SETTINGS_NET_WIFI_CFG_TITLE */
        "10 分",  /* SETTINGS_POWER_10_MIN */
        "120 秒",  /* SETTINGS_POWER_120_SEC */
        "30 分",  /* SETTINGS_POWER_30_MIN */
        "30 秒",  /* SETTINGS_POWER_30_SEC */
        "3 分",  /* SETTINGS_POWER_3_MIN */
        "60 秒",  /* SETTINGS_POWER_60_SEC */
        "无操作空闲时的 CPU 频率",  /* SETTINGS_POWER_IDLE_MHZ_HINT */
        "空闲降频（默认240Mhz）",  /* SETTINGS_POWER_IDLE_MHZ_TITLE */
        "无保网需求后，仍保持联网的时长",  /* SETTINGS_POWER_NET_GRACE_HINT */
        "保网时长（默认 30 秒）",  /* SETTINGS_POWER_NET_GRACE_TITLE */
        "浅睡累计多久自动关机",  /* SETTINGS_POWER_OFF_HINT */
        "永不",  /* SETTINGS_POWER_OFF_NEVER */
        "自动关机（默认 3 分钟）",  /* SETTINGS_POWER_OFF_TITLE */
        "无操作多久进入浅睡待机",  /* SETTINGS_POWER_STANDBY_HINT */
        "浅睡待机（默认 3 分钟）",  /* SETTINGS_POWER_STANDBY_TITLE */
        "容量：—",  /* SETTINGS_STORAGE_CAP_DASH */
        "容量：导出中，停用后可查看",  /* SETTINGS_STORAGE_CAP_EXPORT */
        "容量：读取失败",  /* SETTINGS_STORAGE_CAP_FAIL */
        "剩余 %s / 总容量 %s",  /* SETTINGS_STORAGE_CAP_FMT */
        "内存卡：已插入",  /* SETTINGS_STORAGE_INSERTED */
        "内存卡：未检测到",  /* SETTINGS_STORAGE_MISSING */
        "内存卡：电脑占用中",  /* SETTINGS_STORAGE_PC_BUSY */
        "内存卡",  /* SETTINGS_STORAGE_TITLE */
        "当前固件未启用模拟 U 盘",  /* SETTINGS_STORAGE_USB_DISABLED */
        "启用后占用 USB；停用后恢复 USB 调试口",  /* SETTINGS_STORAGE_USB_HINT_DISABLED */
        "停用失败，请先在电脑上弹出 U 盘",  /* SETTINGS_STORAGE_USB_HINT_DISABLE_FAIL */
        "正在停用模拟 U 盘…",  /* SETTINGS_STORAGE_USB_HINT_DISABLING */
        "启用失败，请检查 USB 线并重试",  /* SETTINGS_STORAGE_USB_HINT_ENABLE_FAIL */
        "正在启用模拟 U 盘…",  /* SETTINGS_STORAGE_USB_HINT_ENABLING */
        "内存卡需要格式化 (FAT32)",  /* SETTINGS_STORAGE_USB_HINT_FORMAT */
        "已启用，请在电脑访问；停用前请先弹出",  /* SETTINGS_STORAGE_USB_HINT_HOST */
        "操作失败，请先在电脑上弹出 U 盘",  /* SETTINGS_STORAGE_USB_HINT_HOST_BUSY */
        "启用后电脑可将本机识别为 U 盘",  /* SETTINGS_STORAGE_USB_HINT_IDLE */
        "已启用，主机未占用；点停用恢复 USB 调试",  /* SETTINGS_STORAGE_USB_HINT_LOCAL */
        "未检测到内存卡",  /* SETTINGS_STORAGE_USB_HINT_NO_SD */
        "正在切换，请稍候…",  /* SETTINGS_STORAGE_USB_HINT_SWITCHING */
        "停用模拟 U 盘",  /* SETTINGS_STORAGE_USB_OFF */
        "启用模拟 U 盘",  /* SETTINGS_STORAGE_USB_ON */
        "关于",  /* SETTINGS_TAB_ABOUT */
        "蓝牙",  /* SETTINGS_TAB_BLUETOOTH */
        "对话",  /* SETTINGS_TAB_CONVERSATION */
        "震动",  /* SETTINGS_TAB_HAPTIC */
        "语言",  /* SETTINGS_TAB_LANGUAGE */
        "网络",  /* SETTINGS_TAB_NETWORK */
        "功耗",  /* SETTINGS_TAB_POWER */
        "存储",  /* SETTINGS_TAB_STORAGE */
        "测试",  /* SETTINGS_TAB_TEST */
        "主题",  /* SETTINGS_TAB_THEME */
        "老化测试",  /* SETTINGS_TEST_AGING */
        "老化测试中",  /* SETTINGS_TEST_AGING_RUNNING */
        "录音、喇叭是否正常？",  /* SETTINGS_TEST_AUDIO_CONFIRM */
        "按住说话",  /* SETTINGS_TEST_AUDIO_HOLD */
        "录音中…",  /* SETTINGS_TEST_AUDIO_RECORDING */
        "音频",  /* SETTINGS_TEST_AUDIO_TITLE */
        "自动测试",  /* SETTINGS_TEST_AUTO */
        "电池测试",  /* SETTINGS_TEST_BATTERY */
        "充电中",  /* SETTINGS_TEST_BATTERY_CHARGING */
        "涓流/预充/CC",  /* SETTINGS_TEST_BATTERY_CHG_CC */
        "恒压降流",  /* SETTINGS_TEST_BATTERY_CHG_CV */
        "充电使能",  /* SETTINGS_TEST_BATTERY_CHG_EN */
        "未充电/已满",  /* SETTINGS_TEST_BATTERY_CHG_NOT */
        "Top-off",  /* SETTINGS_TEST_BATTERY_CHG_TOPOFF */
        "芯片充电",  /* SETTINGS_TEST_BATTERY_CHIP_CHG */
        "电流",  /* SETTINGS_TEST_BATTERY_CURR */
        "放电中",  /* SETTINGS_TEST_BATTERY_DISCHARGING */
        "关",  /* SETTINGS_TEST_BATTERY_EN_OFF */
        "开",  /* SETTINGS_TEST_BATTERY_EN_ON */
        "电量计",  /* SETTINGS_TEST_BATTERY_GAUGE */
        "表计状态",  /* SETTINGS_TEST_BATTERY_GAUGE_STAT */
        "SOC: 3.30~4.28V; ≥4.28需稳30s→100%",  /* SETTINGS_TEST_BATTERY_HINT */
        "设定电流",  /* SETTINGS_TEST_BATTERY_ICHG */
        "空载",  /* SETTINGS_TEST_BATTERY_IDLE_LOAD */
        "一致性",  /* SETTINGS_TEST_BATTERY_MATCH */
        "一致(DPDM关)",  /* SETTINGS_TEST_BATTERY_MATCH_DPDM */
        "一致",  /* SETTINGS_TEST_BATTERY_MATCH_OK */
        "芯片在充/表计否",  /* SETTINGS_TEST_BATTERY_MISMATCH_CHIP */
        "表计在充/芯片否",  /* SETTINGS_TEST_BATTERY_MISMATCH_GAUGE */
        "有输入未在充",  /* SETTINGS_TEST_BATTERY_MISMATCH_NO_CHG */
        "无输入却在充?",  /* SETTINGS_TEST_BATTERY_MISMATCH_NO_IN */
        "电量(瞬时)",  /* SETTINGS_TEST_BATTERY_SOC_RAW */
        "电量(平滑)",  /* SETTINGS_TEST_BATTERY_SOC_SMOOTH */
        "VBUS",  /* SETTINGS_TEST_BATTERY_VBUS */
        "适配器",  /* SETTINGS_TEST_BATTERY_VBUS_ADP */
        "USB CDP",  /* SETTINGS_TEST_BATTERY_VBUS_CDP */
        "USB DCP",  /* SETTINGS_TEST_BATTERY_VBUS_DCP */
        "无输入",  /* SETTINGS_TEST_BATTERY_VBUS_NONE */
        "非标适配器",  /* SETTINGS_TEST_BATTERY_VBUS_NONSTD */
        "OTG",  /* SETTINGS_TEST_BATTERY_VBUS_OTG */
        "USB SDP",  /* SETTINGS_TEST_BATTERY_VBUS_SDP */
        "有输入(未识别)",  /* SETTINGS_TEST_BATTERY_VBUS_UNKNOWN_IN */
        "未知适配器",  /* SETTINGS_TEST_BATTERY_VBUS_UNK_ADP */
        "电压",  /* SETTINGS_TEST_BATTERY_VOLT */
        "电压波动",  /* SETTINGS_TEST_BATTERY_VOLT_RANGE */
        "截止电压",  /* SETTINGS_TEST_BATTERY_VREG */
        "忙碌中…",  /* SETTINGS_TEST_BUSY */
        "摄像头",  /* SETTINGS_TEST_CAMERA */
        "拍照失败",  /* SETTINGS_TEST_CAMERA_FAIL */
        "成功",  /* SETTINGS_TEST_CAMERA_OK */
        "拍照结果",  /* SETTINGS_TEST_CAMERA_RESULT */
        "拍照",  /* SETTINGS_TEST_CAMERA_SHOT */
        "超时无设备",  /* SETTINGS_TEST_CAMERA_TIMEOUT */
        "USB占用中",  /* SETTINGS_TEST_CAMERA_USB_BUSY */
        "USB中断资源不足",  /* SETTINGS_TEST_CAMERA_USB_IRQ */
        "拍照中…",  /* SETTINGS_TEST_CAPTURING */
        "测试已取消",  /* SETTINGS_TEST_CELL_CANCELLED */
        "CFUN=0失败",  /* SETTINGS_TEST_CELL_CFUN0_FAIL */
        "CFUN=1失败",  /* SETTINGS_TEST_CELL_CFUN1_FAIL */
        "4G·外置",  /* SETTINGS_TEST_CELL_EXT */
        "4G·内置",  /* SETTINGS_TEST_CELL_INT */
        "丢包 %d/%d",  /* SETTINGS_TEST_CELL_LOSS_FMT */
        "模组未就绪",  /* SETTINGS_TEST_CELL_MODEM_NOT_READY */
        "模组无响应",  /* SETTINGS_TEST_CELL_MODEM_NO_RESP */
        "需切到4G模式",  /* SETTINGS_TEST_CELL_NEED_4G */
        "资源不足",  /* SETTINGS_TEST_CELL_NO_RESOURCE */
        "未检测到SIM卡",  /* SETTINGS_TEST_CELL_NO_SIM */
        "PING命令失败",  /* SETTINGS_TEST_CELL_PING_CMD_FAIL */
        "PING失败",  /* SETTINGS_TEST_CELL_PING_FAIL */
        "PING成功",  /* SETTINGS_TEST_CELL_PING_OK */
        "PING等待超时",  /* SETTINGS_TEST_CELL_PING_WAIT_TIMEOUT */
        "网络注册被拒绝",  /* SETTINGS_TEST_CELL_REG_REJECT */
        "搜网超时",  /* SETTINGS_TEST_CELL_SEARCH_TIMEOUT */
        "切换SIM失败",  /* SETTINGS_TEST_CELL_SIM_SWITCH_FAIL */
        "SIM槽位未生效",  /* SETTINGS_TEST_CELL_SLOT_FAIL */
        "测试中…",  /* SETTINGS_TEST_CELL_TESTING */
        "测试失败",  /* SETTINGS_TEST_CELL_TEST_FAIL */
        "等待资源…",  /* SETTINGS_TEST_CELL_WAIT_RESOURCE */
        "检测中…",  /* SETTINGS_TEST_DETECTING */
        "退出本页将自动重启",  /* SETTINGS_TEST_EXIT_REBOOT */
        "失败",  /* SETTINGS_TEST_FAIL */
        "否",  /* SETTINGS_TEST_NO */
        "未检测到",  /* SETTINGS_TEST_NOT_DETECTED */
        "未通过",  /* SETTINGS_TEST_NOT_PASS */
        "正常",  /* SETTINGS_TEST_OK */
        "通过",  /* SETTINGS_TEST_PASS */
        "正在重启…",  /* SETTINGS_TEST_REBOOTING */
        "扫描中…",  /* SETTINGS_TEST_SCANNING */
        "SD卡",  /* SETTINGS_TEST_SDCARD */
        "未插卡/挂载失败",  /* SETTINGS_TEST_SD_MOUNT_FAIL */
        "板型不支持",  /* SETTINGS_TEST_SIGNAL_BOARD_UNSUP */
        "4G CSQ %2d",  /* SETTINGS_TEST_SIGNAL_CELL_CSQ_FMT */
        "4G CSQ 未知",  /* SETTINGS_TEST_SIGNAL_CELL_CSQ_UNKNOWN */
        "4G 不可用",  /* SETTINGS_TEST_SIGNAL_CELL_UNAVAIL */
        "读取中…",  /* SETTINGS_TEST_SIGNAL_READING */
        "网络信号",  /* SETTINGS_TEST_SIGNAL_TITLE */
        "WiFi 未连接",  /* SETTINGS_TEST_SIGNAL_WIFI_DISCONNECTED */
        "WiFi 读取失败",  /* SETTINGS_TEST_SIGNAL_WIFI_READ_FAIL */
        "WiFi %d dBm",  /* SETTINGS_TEST_SIGNAL_WIFI_RSSI_FMT */
        "任务创建失败",  /* SETTINGS_TEST_TASK_CREATE_FAIL */
        "触摸测试",  /* SETTINGS_TEST_TOUCH */
        "点击屏幕，开始测试",  /* SETTINGS_TEST_TOUCH_START */
        "等待测试…",  /* SETTINGS_TEST_WAITING */
        "关闭",  /* SETTINGS_TEST_WIFI_CLOSE */
        "共 %d 个网络",  /* SETTINGS_TEST_WIFI_COUNT_FMT */
        "发现 %d 个",  /* SETTINGS_TEST_WIFI_FOUND_FMT */
        "附近 WiFi",  /* SETTINGS_TEST_WIFI_NEARBY */
        "下页",  /* SETTINGS_TEST_WIFI_NEXT */
        "未发现网络",  /* SETTINGS_TEST_WIFI_NONE */
        "上页",  /* SETTINGS_TEST_WIFI_PREV */
        "重扫",  /* SETTINGS_TEST_WIFI_RESCAN */
        "正在扫描…",  /* SETTINGS_TEST_WIFI_SCANNING */
        "扫描失败",  /* SETTINGS_TEST_WIFI_SCAN_FAIL */
        "是",  /* SETTINGS_TEST_YES */
        "黑色描边",  /* SETTINGS_THEME_BORDER */
        "当前：黑色描边",  /* SETTINGS_THEME_CURRENT_BORDER */
        "当前：网点灰底",  /* SETTINGS_THEME_CURRENT_GRAY */
        "当前：斜切棋盘",  /* SETTINGS_THEME_CURRENT_SLASH */
        "当前：白底",  /* SETTINGS_THEME_CURRENT_WHITE */
        "网点灰底",  /* SETTINGS_THEME_GRAY */
        "返回首页后立即生效",  /* SETTINGS_THEME_HINT */
        "斜切棋盘",  /* SETTINGS_THEME_SLASH */
        "首页主题",  /* SETTINGS_THEME_TITLE */
        "白底",  /* SETTINGS_THEME_WHITE */
        "回答中...",  /* SPEAKING */
        "待命",  /* STANDBY */
        "创建连接失败",  /* STANDBY_CONN_FAIL */
        "%02d月%02d日",  /* STANDBY_DATE_FMT */
        "--月--日",  /* STANDBY_DATE_PLACEHOLDER */
        "解码失败",  /* STANDBY_DECODE_FAIL */
        "暂无待办",  /* STANDBY_EMPTY_TODO */
        "无数据",  /* STANDBY_NO_DATA */
        "无网络",  /* STANDBY_NO_NETWORK */
        "暂无清单缓存",  /* STANDBY_NO_TODO_CACHE */
        "无天气",  /* STANDBY_NO_WEATHER */
        "解析失败",  /* STANDBY_PARSE_FAIL */
        "请求失败",  /* STANDBY_REQUEST_FAIL */
        "天气失败",  /* STANDBY_WEATHER_FAIL */
        "未找到待机壁纸",  /* STANDBY_WP_NOT_FOUND */
        "切换到 4G...",  /* SWITCH_TO_4G_NETWORK */
        "切换到 Wi-Fi...",  /* SWITCH_TO_WIFI_NETWORK */
        "接口返回错误",  /* TASK_API_ERROR */
        "批量完成中...",  /* TASK_BATCH_COMPLETING */
        "批量操作失败",  /* TASK_BATCH_FAIL */
        "批量移除中...",  /* TASK_BATCH_REMOVING */
        "成功 %d，失败 %d",  /* TASK_BATCH_RESULT_FMT */
        "批量任务启动失败",  /* TASK_BATCH_START_FAIL */
        "选择操作",  /* TASK_CHOOSE_ACTION */
        "完成",  /* TASK_COMPLETE */
        "已完成",  /* TASK_COMPLETED */
        "已完成 %d 项",  /* TASK_COMPLETED_N_FMT */
        "完成失败",  /* TASK_COMPLETE_FAIL */
        "完成任务启动失败",  /* TASK_COMPLETE_START_FAIL */
        "完成待办",  /* TASK_COMPLETE_TODO */
        "完成中...",  /* TASK_COMPLETING */
        "正在联网…",  /* TASK_CONNECTING_NET */
        "创建连接失败",  /* TASK_CONN_FAIL */
        "数据格式错误",  /* TASK_DATA_FORMAT_ERR */
        "已删除",  /* TASK_DELETED */
        "删除失败",  /* TASK_DELETE_FAIL */
        "删除任务启动失败",  /* TASK_DELETE_START_FAIL */
        "删除待办",  /* TASK_DELETE_TODO */
        "删除中...",  /* TASK_DELETING */
        "任务已完成",  /* TASK_DONE_OK */
        "暂无已完成任务",  /* TASK_EMPTY_DONE */
        "暂无待办",  /* TASK_EMPTY_TODO */
        "无效待办",  /* TASK_INVALID */
        "加载失败",  /* TASK_LOAD_FAIL */
        "请先完成配网",  /* TASK_NEED_WIFI_CFG */
        "网络未就绪",  /* TASK_NET_NOT_READY */
        "没有可完成的待办",  /* TASK_NO_COMPLETABLE */
        "无网络",  /* TASK_NO_NETWORK */
        "响应解析失败",  /* TASK_PARSE_FAIL */
        "请稍候...",  /* TASK_PLEASE_WAIT */
        "刷新",  /* TASK_REFRESH */
        "已刷新",  /* TASK_REFRESHED */
        "刷新中...",  /* TASK_REFRESHING */
        "刷新启动失败",  /* TASK_REFRESH_START_FAIL */
        "已移除 %d 项",  /* TASK_REMOVED_N_FMT */
        "请求失败",  /* TASK_REQUEST_FAIL */
        "已选 %d",  /* TASK_SELECTED_FMT */
        "请先选择",  /* TASK_SELECT_FIRST */
        "已完成",  /* TASK_TAB_DONE */
        "待办",  /* TASK_TAB_TODO */
        "已完成",  /* TASK_TITLE_DONE */
        "日程待办",  /* TASK_TITLE_TODO */
        "检测不到触摸",  /* TOUCH_MISSING_HINT */
        "音频忙，稍后再试",  /* TRANSLATE_AUDIO_BUSY */
        "已取消",  /* TRANSLATE_CANCELLED */
        "连接中…",  /* TRANSLATE_CONNECTING */
        "连接失败",  /* TRANSLATE_CONNECT_FAIL */
        "连接网络…",  /* TRANSLATE_CONNECT_NET */
        "翻译出错",  /* TRANSLATE_ERROR */
        "获取令牌…",  /* TRANSLATE_GET_TOKEN */
        "获取令牌失败",  /* TRANSLATE_GET_TOKEN_FAIL */
        "选择语言后点击实时翻译",  /* TRANSLATE_HINT */
        "语言选择无效",  /* TRANSLATE_LANG_INVALID */
        "源语言与目标语言不能相同",  /* TRANSLATE_LANG_SAME */
        "实时翻译",  /* TRANSLATE_LIVE */
        "请先完成配网",  /* TRANSLATE_NEED_WIFI_CFG */
        "网络连接中…",  /* TRANSLATE_NET_CONNECTING */
        "网络未就绪",  /* TRANSLATE_NET_NOT_READY */
        "选择源语言",  /* TRANSLATE_PICK_FROM */
        "选择语言",  /* TRANSLATE_PICK_LANG */
        "选择目标语言",  /* TRANSLATE_PICK_TO */
        "等待就绪超时",  /* TRANSLATE_READY_TIMEOUT */
        "识别原文将显示在这里",  /* TRANSLATE_SOURCE_PLACEHOLDER */
        "识别原文",  /* TRANSLATE_SOURCE_TITLE */
        "启动失败",  /* TRANSLATE_START_FAIL */
        "停止翻译",  /* TRANSLATE_STOP */
        "已停止",  /* TRANSLATE_STOPPED */
        "停止中…",  /* TRANSLATE_STOPPING */
        "翻译中",  /* TRANSLATE_TRANSLATING */
        "译文将显示在这里",  /* TRANSLATE_TRANS_PLACEHOLDER */
        "译文",  /* TRANSLATE_TRANS_TITLE */
        "请稍后再试",  /* TRANSLATE_TRY_LATER */
        "升级失败",  /* UPGRADE_FAILED */
        "正在升级系统...",  /* UPGRADING */
        "版本 ",  /* VERSION */
        "正在联网启动，完成后自动开始…",  /* VOICE_STARTING_NET */
        "音量 ",  /* VOLUME */
        "关机",  /* WALLPAPER_CHIP_SHUTDOWN */
        "待机",  /* WALLPAPER_CHIP_STANDBY */
        "解码失败",  /* WALLPAPER_DECODE_FAIL */
        "删除壁纸",  /* WALLPAPER_DELETE_BTN */
        "删除失败",  /* WALLPAPER_DELETE_FAIL */
        "删除启动失败",  /* WALLPAPER_DELETE_START_FAIL */
        "正在删除…",  /* WALLPAPER_DELETING */
        "暂无壁纸\n请放到 %s",  /* WALLPAPER_EMPTY_FMT */
        "启用失败",  /* WALLPAPER_ENABLE_FAIL */
        "文件无效",  /* WALLPAPER_FILE_INVALID */
        "文件不存在",  /* WALLPAPER_FILE_MISSING */
        "加载中…",  /* WALLPAPER_LOADING */
        "文件名无效",  /* WALLPAPER_NAME_INVALID */
        "未检测到 SD 卡",  /* WALLPAPER_NO_SD */
        "路径无效",  /* WALLPAPER_PATH_INVALID */
        "无法预览",  /* WALLPAPER_PREVIEW_FAIL */
        "内存不足，预览失败",  /* WALLPAPER_PREVIEW_OOM */
        "预览启动失败",  /* WALLPAPER_PREVIEW_START_FAIL */
        "已选 %d",  /* WALLPAPER_SELECTED_FMT */
        "设为关机",  /* WALLPAPER_SET_SHUTDOWN */
        "设为待机",  /* WALLPAPER_SET_STANDBY */
        "关机中",  /* WALLPAPER_SHUTDOWN_ON */
        "待机中",  /* WALLPAPER_STANDBY_ON */
        "警告",  /* WARNING */
        "配网模式",  /* WIFI_CONFIG_MODE */
};

void ApplyLiterals(const char* const* literals) {
    Strings::ACCESS_VIA_BROWSER = literals[static_cast<size_t>(LangStringId::ACCESS_VIA_BROWSER)];
    Strings::ACTIVATION = literals[static_cast<size_t>(LangStringId::ACTIVATION)];
    Strings::ACTIVATION_CODE_FMT = literals[static_cast<size_t>(LangStringId::ACTIVATION_CODE_FMT)];
    Strings::BATTERY_CHARGING = literals[static_cast<size_t>(LangStringId::BATTERY_CHARGING)];
    Strings::BATTERY_FULL = literals[static_cast<size_t>(LangStringId::BATTERY_FULL)];
    Strings::BATTERY_LOW = literals[static_cast<size_t>(LangStringId::BATTERY_LOW)];
    Strings::BATTERY_NEED_CHARGE = literals[static_cast<size_t>(LangStringId::BATTERY_NEED_CHARGE)];
    Strings::BOOK_BAD_ARG = literals[static_cast<size_t>(LangStringId::BOOK_BAD_ARG)];
    Strings::BOOK_BLANK = literals[static_cast<size_t>(LangStringId::BOOK_BLANK)];
    Strings::BOOK_BODY = literals[static_cast<size_t>(LangStringId::BOOK_BODY)];
    Strings::BOOK_CANCELLED = literals[static_cast<size_t>(LangStringId::BOOK_CANCELLED)];
    Strings::BOOK_CHAPTER_FMT = literals[static_cast<size_t>(LangStringId::BOOK_CHAPTER_FMT)];
    Strings::BOOK_CHAPTER_NO_TEXT = literals[static_cast<size_t>(LangStringId::BOOK_CHAPTER_NO_TEXT)];
    Strings::BOOK_CHAPTER_PROGRESS = literals[static_cast<size_t>(LangStringId::BOOK_CHAPTER_PROGRESS)];
    Strings::BOOK_CHECKSUM_FAIL = literals[static_cast<size_t>(LangStringId::BOOK_CHECKSUM_FAIL)];
    Strings::BOOK_CONN_FAIL = literals[static_cast<size_t>(LangStringId::BOOK_CONN_FAIL)];
    Strings::BOOK_CONTINUE = literals[static_cast<size_t>(LangStringId::BOOK_CONTINUE)];
    Strings::BOOK_COUNT_FMT = literals[static_cast<size_t>(LangStringId::BOOK_COUNT_FMT)];
    Strings::BOOK_COVER_TOO_LARGE = literals[static_cast<size_t>(LangStringId::BOOK_COVER_TOO_LARGE)];
    Strings::BOOK_DELETE_FAILED = literals[static_cast<size_t>(LangStringId::BOOK_DELETE_FAILED)];
    Strings::BOOK_DETAIL = literals[static_cast<size_t>(LangStringId::BOOK_DETAIL)];
    Strings::BOOK_DOWNLOAD_FAIL = literals[static_cast<size_t>(LangStringId::BOOK_DOWNLOAD_FAIL)];
    Strings::BOOK_DURATION_HM_FMT = literals[static_cast<size_t>(LangStringId::BOOK_DURATION_HM_FMT)];
    Strings::BOOK_DURATION_H_FMT = literals[static_cast<size_t>(LangStringId::BOOK_DURATION_H_FMT)];
    Strings::BOOK_DURATION_LT1MIN_FMT = literals[static_cast<size_t>(LangStringId::BOOK_DURATION_LT1MIN_FMT)];
    Strings::BOOK_DURATION_M_FMT = literals[static_cast<size_t>(LangStringId::BOOK_DURATION_M_FMT)];
    Strings::BOOK_EMPTY_COVER = literals[static_cast<size_t>(LangStringId::BOOK_EMPTY_COVER)];
    Strings::BOOK_EMPTY_FILE = literals[static_cast<size_t>(LangStringId::BOOK_EMPTY_FILE)];
    Strings::BOOK_EMPTY_HOME_FMT = literals[static_cast<size_t>(LangStringId::BOOK_EMPTY_HOME_FMT)];
    Strings::BOOK_EMPTY_SHELF_FMT = literals[static_cast<size_t>(LangStringId::BOOK_EMPTY_SHELF_FMT)];
    Strings::BOOK_FILE_TOO_LARGE = literals[static_cast<size_t>(LangStringId::BOOK_FILE_TOO_LARGE)];
    Strings::BOOK_FINISHED = literals[static_cast<size_t>(LangStringId::BOOK_FINISHED)];
    Strings::BOOK_FONT_EMPTY = literals[static_cast<size_t>(LangStringId::BOOK_FONT_EMPTY)];
    Strings::BOOK_FONT_NAME_INVALID = literals[static_cast<size_t>(LangStringId::BOOK_FONT_NAME_INVALID)];
    Strings::BOOK_FONT_TITLE = literals[static_cast<size_t>(LangStringId::BOOK_FONT_TITLE)];
    Strings::BOOK_IMAGE_PLACEHOLDER = literals[static_cast<size_t>(LangStringId::BOOK_IMAGE_PLACEHOLDER)];
    Strings::BOOK_LAYOUT_BUSY = literals[static_cast<size_t>(LangStringId::BOOK_LAYOUT_BUSY)];
    Strings::BOOK_LAYOUT_DONE = literals[static_cast<size_t>(LangStringId::BOOK_LAYOUT_DONE)];
    Strings::BOOK_LINE_GAP = literals[static_cast<size_t>(LangStringId::BOOK_LINE_GAP)];
    Strings::BOOK_MARGIN = literals[static_cast<size_t>(LangStringId::BOOK_MARGIN)];
    Strings::BOOK_MARGIN_NARROW = literals[static_cast<size_t>(LangStringId::BOOK_MARGIN_NARROW)];
    Strings::BOOK_MARGIN_STANDARD = literals[static_cast<size_t>(LangStringId::BOOK_MARGIN_STANDARD)];
    Strings::BOOK_MARGIN_VERY_WIDE = literals[static_cast<size_t>(LangStringId::BOOK_MARGIN_VERY_WIDE)];
    Strings::BOOK_MARGIN_WIDE = literals[static_cast<size_t>(LangStringId::BOOK_MARGIN_WIDE)];
    Strings::BOOK_NET_NOT_READY = literals[static_cast<size_t>(LangStringId::BOOK_NET_NOT_READY)];
    Strings::BOOK_NO_CONTINUE = literals[static_cast<size_t>(LangStringId::BOOK_NO_CONTINUE)];
    Strings::BOOK_NO_COVER_URL = literals[static_cast<size_t>(LangStringId::BOOK_NO_COVER_URL)];
    Strings::BOOK_NO_DOWNLOAD_URL = literals[static_cast<size_t>(LangStringId::BOOK_NO_DOWNLOAD_URL)];
    Strings::BOOK_NO_LOCAL_FILE = literals[static_cast<size_t>(LangStringId::BOOK_NO_LOCAL_FILE)];
    Strings::BOOK_NO_NETWORK = literals[static_cast<size_t>(LangStringId::BOOK_NO_NETWORK)];
    Strings::BOOK_NO_RECENT = literals[static_cast<size_t>(LangStringId::BOOK_NO_RECENT)];
    Strings::BOOK_NO_SD = literals[static_cast<size_t>(LangStringId::BOOK_NO_SD)];
    Strings::BOOK_NO_SD_SHORT = literals[static_cast<size_t>(LangStringId::BOOK_NO_SD_SHORT)];
    Strings::BOOK_OPENING = literals[static_cast<size_t>(LangStringId::BOOK_OPENING)];
    Strings::BOOK_OPEN_FAILED = literals[static_cast<size_t>(LangStringId::BOOK_OPEN_FAILED)];
    Strings::BOOK_OPEN_FAILED_CHAPTER = literals[static_cast<size_t>(LangStringId::BOOK_OPEN_FAILED_CHAPTER)];
    Strings::BOOK_OPEN_FAILED_EPUB = literals[static_cast<size_t>(LangStringId::BOOK_OPEN_FAILED_EPUB)];
    Strings::BOOK_OPEN_FAILED_TASK = literals[static_cast<size_t>(LangStringId::BOOK_OPEN_FAILED_TASK)];
    Strings::BOOK_OPEN_FAILED_TXT = literals[static_cast<size_t>(LangStringId::BOOK_OPEN_FAILED_TXT)];
    Strings::BOOK_OUT_OF_MEMORY = literals[static_cast<size_t>(LangStringId::BOOK_OUT_OF_MEMORY)];
    Strings::BOOK_PAGE_LOAD_FAILED = literals[static_cast<size_t>(LangStringId::BOOK_PAGE_LOAD_FAILED)];
    Strings::BOOK_PATH_INVALID = literals[static_cast<size_t>(LangStringId::BOOK_PATH_INVALID)];
    Strings::BOOK_READ_DURATION = literals[static_cast<size_t>(LangStringId::BOOK_READ_DURATION)];
    Strings::BOOK_READ_FAIL = literals[static_cast<size_t>(LangStringId::BOOK_READ_FAIL)];
    Strings::BOOK_READ_FILE_FAIL = literals[static_cast<size_t>(LangStringId::BOOK_READ_FILE_FAIL)];
    Strings::BOOK_READ_PCT_FMT = literals[static_cast<size_t>(LangStringId::BOOK_READ_PCT_FMT)];
    Strings::BOOK_READ_PROGRESS = literals[static_cast<size_t>(LangStringId::BOOK_READ_PROGRESS)];
    Strings::BOOK_RECENT = literals[static_cast<size_t>(LangStringId::BOOK_RECENT)];
    Strings::BOOK_SAVE_FAIL = literals[static_cast<size_t>(LangStringId::BOOK_SAVE_FAIL)];
    Strings::BOOK_SELECTED_FMT = literals[static_cast<size_t>(LangStringId::BOOK_SELECTED_FMT)];
    Strings::BOOK_SHELF_TITLE = literals[static_cast<size_t>(LangStringId::BOOK_SHELF_TITLE)];
    Strings::BOOK_SPACING_COMPACT = literals[static_cast<size_t>(LangStringId::BOOK_SPACING_COMPACT)];
    Strings::BOOK_SPACING_RELAXED = literals[static_cast<size_t>(LangStringId::BOOK_SPACING_RELAXED)];
    Strings::BOOK_SPACING_STANDARD = literals[static_cast<size_t>(LangStringId::BOOK_SPACING_STANDARD)];
    Strings::BOOK_SPACING_VERY_RELAXED = literals[static_cast<size_t>(LangStringId::BOOK_SPACING_VERY_RELAXED)];
    Strings::BOOK_START = literals[static_cast<size_t>(LangStringId::BOOK_START)];
    Strings::BOOK_STORAGE_UNAVAIL = literals[static_cast<size_t>(LangStringId::BOOK_STORAGE_UNAVAIL)];
    Strings::BOOK_SYNC_EMPTY_BODY = literals[static_cast<size_t>(LangStringId::BOOK_SYNC_EMPTY_BODY)];
    Strings::BOOK_TOC = literals[static_cast<size_t>(LangStringId::BOOK_TOC)];
    Strings::BOOK_TOC_EMPTY = literals[static_cast<size_t>(LangStringId::BOOK_TOC_EMPTY)];
    Strings::BOOK_TOC_PAGE_FMT = literals[static_cast<size_t>(LangStringId::BOOK_TOC_PAGE_FMT)];
    Strings::BOOK_TODAY_DURATION = literals[static_cast<size_t>(LangStringId::BOOK_TODAY_DURATION)];
    Strings::BOOK_UNDERLINE = literals[static_cast<size_t>(LangStringId::BOOK_UNDERLINE)];
    Strings::BOOK_UNDERLINE_DASHED = literals[static_cast<size_t>(LangStringId::BOOK_UNDERLINE_DASHED)];
    Strings::BOOK_UNDERLINE_SOLID = literals[static_cast<size_t>(LangStringId::BOOK_UNDERLINE_SOLID)];
    Strings::BOOK_WRITE_FAIL = literals[static_cast<size_t>(LangStringId::BOOK_WRITE_FAIL)];
    Strings::BT_CALL_BTN = literals[static_cast<size_t>(LangStringId::BT_CALL_BTN)];
    Strings::BT_CALL_MODE_SCO = literals[static_cast<size_t>(LangStringId::BT_CALL_MODE_SCO)];
    Strings::BT_CONNECTING = literals[static_cast<size_t>(LangStringId::BT_CONNECTING)];
    Strings::BT_CONNECTING_FMT = literals[static_cast<size_t>(LangStringId::BT_CONNECTING_FMT)];
    Strings::BT_CONNECT_OK = literals[static_cast<size_t>(LangStringId::BT_CONNECT_OK)];
    Strings::BT_CONNECT_TIMEOUT = literals[static_cast<size_t>(LangStringId::BT_CONNECT_TIMEOUT)];
    Strings::BT_DESC = literals[static_cast<size_t>(LangStringId::BT_DESC)];
    Strings::BT_FOUND_FMT = literals[static_cast<size_t>(LangStringId::BT_FOUND_FMT)];
    Strings::BT_MODE1 = literals[static_cast<size_t>(LangStringId::BT_MODE1)];
    Strings::BT_MODE1_ACTIVE = literals[static_cast<size_t>(LangStringId::BT_MODE1_ACTIVE)];
    Strings::BT_MODE1_SET = literals[static_cast<size_t>(LangStringId::BT_MODE1_SET)];
    Strings::BT_MODE2 = literals[static_cast<size_t>(LangStringId::BT_MODE2)];
    Strings::BT_MODE2_SET = literals[static_cast<size_t>(LangStringId::BT_MODE2_SET)];
    Strings::BT_MODE3 = literals[static_cast<size_t>(LangStringId::BT_MODE3)];
    Strings::BT_MODE3_SET = literals[static_cast<size_t>(LangStringId::BT_MODE3_SET)];
    Strings::BT_MUSIC_BTN = literals[static_cast<size_t>(LangStringId::BT_MUSIC_BTN)];
    Strings::BT_MUSIC_MODE_SCO = literals[static_cast<size_t>(LangStringId::BT_MUSIC_MODE_SCO)];
    Strings::BT_NEED_CONNECT = literals[static_cast<size_t>(LangStringId::BT_NEED_CONNECT)];
    Strings::BT_NEED_MODE2 = literals[static_cast<size_t>(LangStringId::BT_NEED_MODE2)];
    Strings::BT_PWR_RESET_OK = literals[static_cast<size_t>(LangStringId::BT_PWR_RESET_OK)];
    Strings::BT_PWR_RESET_UNSUP = literals[static_cast<size_t>(LangStringId::BT_PWR_RESET_UNSUP)];
    Strings::BT_RESET_BTN = literals[static_cast<size_t>(LangStringId::BT_RESET_BTN)];
    Strings::BT_RESET_HINT = literals[static_cast<size_t>(LangStringId::BT_RESET_HINT)];
    Strings::BT_SCANNING = literals[static_cast<size_t>(LangStringId::BT_SCANNING)];
    Strings::BT_SCAN_BTN = literals[static_cast<size_t>(LangStringId::BT_SCAN_BTN)];
    Strings::BT_SCAN_DONE_FMT = literals[static_cast<size_t>(LangStringId::BT_SCAN_DONE_FMT)];
    Strings::BT_SCAN_START = literals[static_cast<size_t>(LangStringId::BT_SCAN_START)];
    Strings::BT_SELECT_MODE = literals[static_cast<size_t>(LangStringId::BT_SELECT_MODE)];
    Strings::BT_SWITCH_CALL = literals[static_cast<size_t>(LangStringId::BT_SWITCH_CALL)];
    Strings::BT_SWITCH_MODE1 = literals[static_cast<size_t>(LangStringId::BT_SWITCH_MODE1)];
    Strings::BT_SWITCH_MODE2 = literals[static_cast<size_t>(LangStringId::BT_SWITCH_MODE2)];
    Strings::BT_SWITCH_MODE3 = literals[static_cast<size_t>(LangStringId::BT_SWITCH_MODE3)];
    Strings::BT_SWITCH_MUSIC = literals[static_cast<size_t>(LangStringId::BT_SWITCH_MUSIC)];
    Strings::BT_TITLE = literals[static_cast<size_t>(LangStringId::BT_TITLE)];
    Strings::BT_UART_NOT_INIT = literals[static_cast<size_t>(LangStringId::BT_UART_NOT_INIT)];
    Strings::CHECKING_NEW_VERSION = literals[static_cast<size_t>(LangStringId::CHECKING_NEW_VERSION)];
    Strings::CHECK_NEW_VERSION_FAILED = literals[static_cast<size_t>(LangStringId::CHECK_NEW_VERSION_FAILED)];
    Strings::CLOUD_API_ERROR = literals[static_cast<size_t>(LangStringId::CLOUD_API_ERROR)];
    Strings::CLOUD_CANCELLED = literals[static_cast<size_t>(LangStringId::CLOUD_CANCELLED)];
    Strings::CLOUD_CANCELLING = literals[static_cast<size_t>(LangStringId::CLOUD_CANCELLING)];
    Strings::CLOUD_CONFIRM_DL_FMT = literals[static_cast<size_t>(LangStringId::CLOUD_CONFIRM_DL_FMT)];
    Strings::CLOUD_CONNECTING_NET = literals[static_cast<size_t>(LangStringId::CLOUD_CONNECTING_NET)];
    Strings::CLOUD_CONNECT_NET = literals[static_cast<size_t>(LangStringId::CLOUD_CONNECT_NET)];
    Strings::CLOUD_CONN_FAIL = literals[static_cast<size_t>(LangStringId::CLOUD_CONN_FAIL)];
    Strings::CLOUD_COVER_FAIL = literals[static_cast<size_t>(LangStringId::CLOUD_COVER_FAIL)];
    Strings::CLOUD_DATA_FORMAT_ERR = literals[static_cast<size_t>(LangStringId::CLOUD_DATA_FORMAT_ERR)];
    Strings::CLOUD_DECODE_FAIL = literals[static_cast<size_t>(LangStringId::CLOUD_DECODE_FAIL)];
    Strings::CLOUD_DELETE_FAIL = literals[static_cast<size_t>(LangStringId::CLOUD_DELETE_FAIL)];
    Strings::CLOUD_DOWNLOAD = literals[static_cast<size_t>(LangStringId::CLOUD_DOWNLOAD)];
    Strings::CLOUD_DOWNLOADING = literals[static_cast<size_t>(LangStringId::CLOUD_DOWNLOADING)];
    Strings::CLOUD_DOWNLOAD_FAIL = literals[static_cast<size_t>(LangStringId::CLOUD_DOWNLOAD_FAIL)];
    Strings::CLOUD_EMPTY_ALL = literals[static_cast<size_t>(LangStringId::CLOUD_EMPTY_ALL)];
    Strings::CLOUD_EMPTY_BOOK = literals[static_cast<size_t>(LangStringId::CLOUD_EMPTY_BOOK)];
    Strings::CLOUD_EMPTY_FONT = literals[static_cast<size_t>(LangStringId::CLOUD_EMPTY_FONT)];
    Strings::CLOUD_EMPTY_WALLPAPER = literals[static_cast<size_t>(LangStringId::CLOUD_EMPTY_WALLPAPER)];
    Strings::CLOUD_FETCHING_LIST = literals[static_cast<size_t>(LangStringId::CLOUD_FETCHING_LIST)];
    Strings::CLOUD_FILE_INVALID = literals[static_cast<size_t>(LangStringId::CLOUD_FILE_INVALID)];
    Strings::CLOUD_IN_QUEUE = literals[static_cast<size_t>(LangStringId::CLOUD_IN_QUEUE)];
    Strings::CLOUD_JSON_CREATE_FAIL = literals[static_cast<size_t>(LangStringId::CLOUD_JSON_CREATE_FAIL)];
    Strings::CLOUD_JSON_PARSE_FAIL = literals[static_cast<size_t>(LangStringId::CLOUD_JSON_PARSE_FAIL)];
    Strings::CLOUD_JSON_SERIALIZE_FAIL = literals[static_cast<size_t>(LangStringId::CLOUD_JSON_SERIALIZE_FAIL)];
    Strings::CLOUD_LIST_TOO_LARGE = literals[static_cast<size_t>(LangStringId::CLOUD_LIST_TOO_LARGE)];
    Strings::CLOUD_LOADING = literals[static_cast<size_t>(LangStringId::CLOUD_LOADING)];
    Strings::CLOUD_LOAD_COVER = literals[static_cast<size_t>(LangStringId::CLOUD_LOAD_COVER)];
    Strings::CLOUD_LOAD_FAIL = literals[static_cast<size_t>(LangStringId::CLOUD_LOAD_FAIL)];
    Strings::CLOUD_MISSING_TASK_ID = literals[static_cast<size_t>(LangStringId::CLOUD_MISSING_TASK_ID)];
    Strings::CLOUD_NEED_WIFI_CFG = literals[static_cast<size_t>(LangStringId::CLOUD_NEED_WIFI_CFG)];
    Strings::CLOUD_NET_NOT_READY = literals[static_cast<size_t>(LangStringId::CLOUD_NET_NOT_READY)];
    Strings::CLOUD_NO_COVER = literals[static_cast<size_t>(LangStringId::CLOUD_NO_COVER)];
    Strings::CLOUD_NO_DOWNLOAD_URL = literals[static_cast<size_t>(LangStringId::CLOUD_NO_DOWNLOAD_URL)];
    Strings::CLOUD_NO_LOCAL_FILE = literals[static_cast<size_t>(LangStringId::CLOUD_NO_LOCAL_FILE)];
    Strings::CLOUD_NO_NETWORK = literals[static_cast<size_t>(LangStringId::CLOUD_NO_NETWORK)];
    Strings::CLOUD_NO_SD = literals[static_cast<size_t>(LangStringId::CLOUD_NO_SD)];
    Strings::CLOUD_PATH_INVALID = literals[static_cast<size_t>(LangStringId::CLOUD_PATH_INVALID)];
    Strings::CLOUD_PENDING = literals[static_cast<size_t>(LangStringId::CLOUD_PENDING)];
    Strings::CLOUD_PLEASE_WAIT = literals[static_cast<size_t>(LangStringId::CLOUD_PLEASE_WAIT)];
    Strings::CLOUD_PREVIEW_FAIL = literals[static_cast<size_t>(LangStringId::CLOUD_PREVIEW_FAIL)];
    Strings::CLOUD_PREVIEW_START_FAIL = literals[static_cast<size_t>(LangStringId::CLOUD_PREVIEW_START_FAIL)];
    Strings::CLOUD_PREVIEW_URL_LONG = literals[static_cast<size_t>(LangStringId::CLOUD_PREVIEW_URL_LONG)];
    Strings::CLOUD_PUSH_BOOK = literals[static_cast<size_t>(LangStringId::CLOUD_PUSH_BOOK)];
    Strings::CLOUD_PUSH_FONT = literals[static_cast<size_t>(LangStringId::CLOUD_PUSH_FONT)];
    Strings::CLOUD_PUSH_WALLPAPER = literals[static_cast<size_t>(LangStringId::CLOUD_PUSH_WALLPAPER)];
    Strings::CLOUD_READ_TIMEOUT = literals[static_cast<size_t>(LangStringId::CLOUD_READ_TIMEOUT)];
    Strings::CLOUD_REFRESH = literals[static_cast<size_t>(LangStringId::CLOUD_REFRESH)];
    Strings::CLOUD_REFRESHED = literals[static_cast<size_t>(LangStringId::CLOUD_REFRESHED)];
    Strings::CLOUD_REFRESHING = literals[static_cast<size_t>(LangStringId::CLOUD_REFRESHING)];
    Strings::CLOUD_REQUEST_FAIL = literals[static_cast<size_t>(LangStringId::CLOUD_REQUEST_FAIL)];
    Strings::CLOUD_SAVE = literals[static_cast<size_t>(LangStringId::CLOUD_SAVE)];
    Strings::CLOUD_SAVED_0_1 = literals[static_cast<size_t>(LangStringId::CLOUD_SAVED_0_1)];
    Strings::CLOUD_SAVED_FMT = literals[static_cast<size_t>(LangStringId::CLOUD_SAVED_FMT)];
    Strings::CLOUD_SAVED_SYNC_FAIL = literals[static_cast<size_t>(LangStringId::CLOUD_SAVED_SYNC_FAIL)];
    Strings::CLOUD_SAVE_FAIL = literals[static_cast<size_t>(LangStringId::CLOUD_SAVE_FAIL)];
    Strings::CLOUD_SAVE_LOCAL = literals[static_cast<size_t>(LangStringId::CLOUD_SAVE_LOCAL)];
    Strings::CLOUD_SAVING = literals[static_cast<size_t>(LangStringId::CLOUD_SAVING)];
    Strings::CLOUD_SAVING_NAME_FMT = literals[static_cast<size_t>(LangStringId::CLOUD_SAVING_NAME_FMT)];
    Strings::CLOUD_SELECTED_FMT = literals[static_cast<size_t>(LangStringId::CLOUD_SELECTED_FMT)];
    Strings::CLOUD_SELECT_FIRST = literals[static_cast<size_t>(LangStringId::CLOUD_SELECT_FIRST)];
    Strings::CLOUD_STORAGE_UNAVAIL = literals[static_cast<size_t>(LangStringId::CLOUD_STORAGE_UNAVAIL)];
    Strings::CLOUD_SYNC_FAIL = literals[static_cast<size_t>(LangStringId::CLOUD_SYNC_FAIL)];
    Strings::CLOUD_TAB_ALL = literals[static_cast<size_t>(LangStringId::CLOUD_TAB_ALL)];
    Strings::CLOUD_TAB_BOOK = literals[static_cast<size_t>(LangStringId::CLOUD_TAB_BOOK)];
    Strings::CLOUD_TAB_FONT = literals[static_cast<size_t>(LangStringId::CLOUD_TAB_FONT)];
    Strings::CLOUD_TAB_WALLPAPER = literals[static_cast<size_t>(LangStringId::CLOUD_TAB_WALLPAPER)];
    Strings::CLOUD_TASK_FAIL = literals[static_cast<size_t>(LangStringId::CLOUD_TASK_FAIL)];
    Strings::CLOUD_TYPE_BOOK = literals[static_cast<size_t>(LangStringId::CLOUD_TYPE_BOOK)];
    Strings::CLOUD_TYPE_FONT = literals[static_cast<size_t>(LangStringId::CLOUD_TYPE_FONT)];
    Strings::CLOUD_TYPE_WALLPAPER = literals[static_cast<size_t>(LangStringId::CLOUD_TYPE_WALLPAPER)];
    Strings::CLOUD_UNKNOWN_TYPE = literals[static_cast<size_t>(LangStringId::CLOUD_UNKNOWN_TYPE)];
    Strings::CLOUD_WAIT_DOWNLOAD = literals[static_cast<size_t>(LangStringId::CLOUD_WAIT_DOWNLOAD)];
    Strings::COMMON_CANCEL = literals[static_cast<size_t>(LangStringId::COMMON_CANCEL)];
    Strings::COMMON_DELETE = literals[static_cast<size_t>(LangStringId::COMMON_DELETE)];
    Strings::COMMON_EMPTY = literals[static_cast<size_t>(LangStringId::COMMON_EMPTY)];
    Strings::COMMON_FAILED = literals[static_cast<size_t>(LangStringId::COMMON_FAILED)];
    Strings::COMMON_LOADING = literals[static_cast<size_t>(LangStringId::COMMON_LOADING)];
    Strings::COMMON_OFF = literals[static_cast<size_t>(LangStringId::COMMON_OFF)];
    Strings::COMMON_OK = literals[static_cast<size_t>(LangStringId::COMMON_OK)];
    Strings::COMMON_ON = literals[static_cast<size_t>(LangStringId::COMMON_ON)];
    Strings::COMMON_REMOVE = literals[static_cast<size_t>(LangStringId::COMMON_REMOVE)];
    Strings::COMMON_RETRY = literals[static_cast<size_t>(LangStringId::COMMON_RETRY)];
    Strings::COMMON_SELECT_ALL = literals[static_cast<size_t>(LangStringId::COMMON_SELECT_ALL)];
    Strings::COMMON_SUCCESS = literals[static_cast<size_t>(LangStringId::COMMON_SUCCESS)];
    Strings::COMMON_UNKNOWN = literals[static_cast<size_t>(LangStringId::COMMON_UNKNOWN)];
    Strings::CONNECTED_TO = literals[static_cast<size_t>(LangStringId::CONNECTED_TO)];
    Strings::CONNECTING = literals[static_cast<size_t>(LangStringId::CONNECTING)];
    Strings::CONNECTION_SUCCESSFUL = literals[static_cast<size_t>(LangStringId::CONNECTION_SUCCESSFUL)];
    Strings::CONNECT_TO = literals[static_cast<size_t>(LangStringId::CONNECT_TO)];
    Strings::CONNECT_TO_HOTSPOT = literals[static_cast<size_t>(LangStringId::CONNECT_TO_HOTSPOT)];
    Strings::DETECTING_MODULE = literals[static_cast<size_t>(LangStringId::DETECTING_MODULE)];
    Strings::DOWNLOAD_ASSETS_FAILED = literals[static_cast<size_t>(LangStringId::DOWNLOAD_ASSETS_FAILED)];
    Strings::ENTERING_WIFI_CONFIG_MODE = literals[static_cast<size_t>(LangStringId::ENTERING_WIFI_CONFIG_MODE)];
    Strings::ERROR = literals[static_cast<size_t>(LangStringId::ERROR)];
    Strings::FOUND_NEW_ASSETS = literals[static_cast<size_t>(LangStringId::FOUND_NEW_ASSETS)];
    Strings::HELLO_MY_FRIEND = literals[static_cast<size_t>(LangStringId::HELLO_MY_FRIEND)];
    Strings::HOME_APP_ASSISTANT = literals[static_cast<size_t>(LangStringId::HOME_APP_ASSISTANT)];
    Strings::HOME_APP_BOOK = literals[static_cast<size_t>(LangStringId::HOME_APP_BOOK)];
    Strings::HOME_APP_CLOUD = literals[static_cast<size_t>(LangStringId::HOME_APP_CLOUD)];
    Strings::HOME_APP_SETTINGS = literals[static_cast<size_t>(LangStringId::HOME_APP_SETTINGS)];
    Strings::HOME_APP_TASK = literals[static_cast<size_t>(LangStringId::HOME_APP_TASK)];
    Strings::HOME_APP_WALLPAPER = literals[static_cast<size_t>(LangStringId::HOME_APP_WALLPAPER)];
    Strings::HOME_DATE_FMT = literals[static_cast<size_t>(LangStringId::HOME_DATE_FMT)];
    Strings::HOME_DATE_PLACEHOLDER = literals[static_cast<size_t>(LangStringId::HOME_DATE_PLACEHOLDER)];
    Strings::HOME_DATE_SLASH_FMT = literals[static_cast<size_t>(LangStringId::HOME_DATE_SLASH_FMT)];
    Strings::HOME_DATE_SLASH_PLACEHOLDER = literals[static_cast<size_t>(LangStringId::HOME_DATE_SLASH_PLACEHOLDER)];
    Strings::HOME_WDAY_FRI = literals[static_cast<size_t>(LangStringId::HOME_WDAY_FRI)];
    Strings::HOME_WDAY_MON = literals[static_cast<size_t>(LangStringId::HOME_WDAY_MON)];
    Strings::HOME_WDAY_SAT = literals[static_cast<size_t>(LangStringId::HOME_WDAY_SAT)];
    Strings::HOME_WDAY_SUN = literals[static_cast<size_t>(LangStringId::HOME_WDAY_SUN)];
    Strings::HOME_WDAY_THU = literals[static_cast<size_t>(LangStringId::HOME_WDAY_THU)];
    Strings::HOME_WDAY_TUE = literals[static_cast<size_t>(LangStringId::HOME_WDAY_TUE)];
    Strings::HOME_WDAY_WED = literals[static_cast<size_t>(LangStringId::HOME_WDAY_WED)];
    Strings::INFO = literals[static_cast<size_t>(LangStringId::INFO)];
    Strings::INITIALIZING = literals[static_cast<size_t>(LangStringId::INITIALIZING)];
    Strings::LISTENING = literals[static_cast<size_t>(LangStringId::LISTENING)];
    Strings::LOADING_ASSETS = literals[static_cast<size_t>(LangStringId::LOADING_ASSETS)];
    Strings::LOADING_PROTOCOL = literals[static_cast<size_t>(LangStringId::LOADING_PROTOCOL)];
    Strings::MAX_VOLUME = literals[static_cast<size_t>(LangStringId::MAX_VOLUME)];
    Strings::MUTED = literals[static_cast<size_t>(LangStringId::MUTED)];
    Strings::NEED_WIFI_CFG = literals[static_cast<size_t>(LangStringId::NEED_WIFI_CFG)];
    Strings::NETWORK_AUTH_OPEN = literals[static_cast<size_t>(LangStringId::NETWORK_AUTH_OPEN)];
    Strings::NETWORK_AUTH_SECURE = literals[static_cast<size_t>(LangStringId::NETWORK_AUTH_SECURE)];
    Strings::NETWORK_BUSY_CONNECT = literals[static_cast<size_t>(LangStringId::NETWORK_BUSY_CONNECT)];
    Strings::NETWORK_CANCEL = literals[static_cast<size_t>(LangStringId::NETWORK_CANCEL)];
    Strings::NETWORK_CLEARED_OK = literals[static_cast<size_t>(LangStringId::NETWORK_CLEARED_OK)];
    Strings::NETWORK_CLEAR_ALL = literals[static_cast<size_t>(LangStringId::NETWORK_CLEAR_ALL)];
    Strings::NETWORK_CONNECT = literals[static_cast<size_t>(LangStringId::NETWORK_CONNECT)];
    Strings::NETWORK_CONNECTED_FMT = literals[static_cast<size_t>(LangStringId::NETWORK_CONNECTED_FMT)];
    Strings::NETWORK_CONNECTING_FMT = literals[static_cast<size_t>(LangStringId::NETWORK_CONNECTING_FMT)];
    Strings::NETWORK_CONNECT_FAIL = literals[static_cast<size_t>(LangStringId::NETWORK_CONNECT_FAIL)];
    Strings::NETWORK_CONNECT_TASK_FAIL = literals[static_cast<size_t>(LangStringId::NETWORK_CONNECT_TASK_FAIL)];
    Strings::NETWORK_CONNECT_TIMEOUT = literals[static_cast<size_t>(LangStringId::NETWORK_CONNECT_TIMEOUT)];
    Strings::NETWORK_CONNECT_TIMEOUT_HINT = literals[static_cast<size_t>(LangStringId::NETWORK_CONNECT_TIMEOUT_HINT)];
    Strings::NETWORK_CONNECT_TO_FMT = literals[static_cast<size_t>(LangStringId::NETWORK_CONNECT_TO_FMT)];
    Strings::NETWORK_DEFAULT_FMT = literals[static_cast<size_t>(LangStringId::NETWORK_DEFAULT_FMT)];
    Strings::NETWORK_DELETE = literals[static_cast<size_t>(LangStringId::NETWORK_DELETE)];
    Strings::NETWORK_DELETED_OK = literals[static_cast<size_t>(LangStringId::NETWORK_DELETED_OK)];
    Strings::NETWORK_ERR_AP_GONE = literals[static_cast<size_t>(LangStringId::NETWORK_ERR_AP_GONE)];
    Strings::NETWORK_ERR_ASSOC = literals[static_cast<size_t>(LangStringId::NETWORK_ERR_ASSOC)];
    Strings::NETWORK_ERR_BAD_PASSWORD = literals[static_cast<size_t>(LangStringId::NETWORK_ERR_BAD_PASSWORD)];
    Strings::NETWORK_ERR_REASON_FMT = literals[static_cast<size_t>(LangStringId::NETWORK_ERR_REASON_FMT)];
    Strings::NETWORK_ERR_WEAK = literals[static_cast<size_t>(LangStringId::NETWORK_ERR_WEAK)];
    Strings::NETWORK_NEARBY_EMPTY = literals[static_cast<size_t>(LangStringId::NETWORK_NEARBY_EMPTY)];
    Strings::NETWORK_PWD_HINT = literals[static_cast<size_t>(LangStringId::NETWORK_PWD_HINT)];
    Strings::NETWORK_PWD_PLACEHOLDER = literals[static_cast<size_t>(LangStringId::NETWORK_PWD_PLACEHOLDER)];
    Strings::NETWORK_PWD_TOO_LONG = literals[static_cast<size_t>(LangStringId::NETWORK_PWD_TOO_LONG)];
    Strings::NETWORK_SAVED_EMPTY = literals[static_cast<size_t>(LangStringId::NETWORK_SAVED_EMPTY)];
    Strings::NETWORK_SCAN = literals[static_cast<size_t>(LangStringId::NETWORK_SCAN)];
    Strings::NETWORK_SCANNING = literals[static_cast<size_t>(LangStringId::NETWORK_SCANNING)];
    Strings::NETWORK_SCAN_DONE_FMT = literals[static_cast<size_t>(LangStringId::NETWORK_SCAN_DONE_FMT)];
    Strings::NETWORK_SCAN_FAIL = literals[static_cast<size_t>(LangStringId::NETWORK_SCAN_FAIL)];
    Strings::NETWORK_SCAN_TASK_FAIL = literals[static_cast<size_t>(LangStringId::NETWORK_SCAN_TASK_FAIL)];
    Strings::NETWORK_SCAN_TIMEOUT = literals[static_cast<size_t>(LangStringId::NETWORK_SCAN_TIMEOUT)];
    Strings::NETWORK_SET_DEFAULT = literals[static_cast<size_t>(LangStringId::NETWORK_SET_DEFAULT)];
    Strings::NETWORK_SET_DEFAULT_OK = literals[static_cast<size_t>(LangStringId::NETWORK_SET_DEFAULT_OK)];
    Strings::NETWORK_SHOW_PWD = literals[static_cast<size_t>(LangStringId::NETWORK_SHOW_PWD)];
    Strings::NETWORK_SSID_INVALID = literals[static_cast<size_t>(LangStringId::NETWORK_SSID_INVALID)];
    Strings::NETWORK_TAB_NEARBY = literals[static_cast<size_t>(LangStringId::NETWORK_TAB_NEARBY)];
    Strings::NETWORK_TAB_SAVED = literals[static_cast<size_t>(LangStringId::NETWORK_TAB_SAVED)];
    Strings::NETWORK_TITLE = literals[static_cast<size_t>(LangStringId::NETWORK_TITLE)];
    Strings::NETWORK_WIFI_INIT = literals[static_cast<size_t>(LangStringId::NETWORK_WIFI_INIT)];
    Strings::NETWORK_WIFI_INIT_FAIL = literals[static_cast<size_t>(LangStringId::NETWORK_WIFI_INIT_FAIL)];
    Strings::NEW_VERSION = literals[static_cast<size_t>(LangStringId::NEW_VERSION)];
    Strings::OTA_ALREADY_LATEST = literals[static_cast<size_t>(LangStringId::OTA_ALREADY_LATEST)];
    Strings::OTA_CHECK_FAILED = literals[static_cast<size_t>(LangStringId::OTA_CHECK_FAILED)];
    Strings::OTA_CONFIRM_FMT = literals[static_cast<size_t>(LangStringId::OTA_CONFIRM_FMT)];
    Strings::OTA_CUR_VER_FMT = literals[static_cast<size_t>(LangStringId::OTA_CUR_VER_FMT)];
    Strings::OTA_HINT = literals[static_cast<size_t>(LangStringId::OTA_HINT)];
    Strings::OTA_IGNORE_VERSION = literals[static_cast<size_t>(LangStringId::OTA_IGNORE_VERSION)];
    Strings::OTA_MANUAL = literals[static_cast<size_t>(LangStringId::OTA_MANUAL)];
    Strings::OTA_NEW_VER_FMT = literals[static_cast<size_t>(LangStringId::OTA_NEW_VER_FMT)];
    Strings::OTA_REMIND_LATER = literals[static_cast<size_t>(LangStringId::OTA_REMIND_LATER)];
    Strings::OTA_SUCCESS_REBOOT = literals[static_cast<size_t>(LangStringId::OTA_SUCCESS_REBOOT)];
    Strings::OTA_TITLE = literals[static_cast<size_t>(LangStringId::OTA_TITLE)];
    Strings::OTA_UPGRADE = literals[static_cast<size_t>(LangStringId::OTA_UPGRADE)];
    Strings::OTA_UPGRADE_NOW = literals[static_cast<size_t>(LangStringId::OTA_UPGRADE_NOW)];
    Strings::PHONE_BUSY = literals[static_cast<size_t>(LangStringId::PHONE_BUSY)];
    Strings::PHONE_CHECKING_NET = literals[static_cast<size_t>(LangStringId::PHONE_CHECKING_NET)];
    Strings::PHONE_CHECK_CELL = literals[static_cast<size_t>(LangStringId::PHONE_CHECK_CELL)];
    Strings::PHONE_CONFIRM_SIM = literals[static_cast<size_t>(LangStringId::PHONE_CONFIRM_SIM)];
    Strings::PHONE_DIAL = literals[static_cast<size_t>(LangStringId::PHONE_DIAL)];
    Strings::PHONE_DIAL_FAIL = literals[static_cast<size_t>(LangStringId::PHONE_DIAL_FAIL)];
    Strings::PHONE_HANGUP = literals[static_cast<size_t>(LangStringId::PHONE_HANGUP)];
    Strings::PHONE_INTERNAL_SIM = literals[static_cast<size_t>(LangStringId::PHONE_INTERNAL_SIM)];
    Strings::PHONE_IN_CALL = literals[static_cast<size_t>(LangStringId::PHONE_IN_CALL)];
    Strings::PHONE_NO_4G = literals[static_cast<size_t>(LangStringId::PHONE_NO_4G)];
    Strings::PHONE_WIFI_BLOCK = literals[static_cast<size_t>(LangStringId::PHONE_WIFI_BLOCK)];
    Strings::PIN_ERROR = literals[static_cast<size_t>(LangStringId::PIN_ERROR)];
    Strings::PLEASE_WAIT = literals[static_cast<size_t>(LangStringId::PLEASE_WAIT)];
    Strings::POWERED_OFF = literals[static_cast<size_t>(LangStringId::POWERED_OFF)];
    Strings::RECORD_ASR_DONE = literals[static_cast<size_t>(LangStringId::RECORD_ASR_DONE)];
    Strings::RECORD_ASR_EMPTY = literals[static_cast<size_t>(LangStringId::RECORD_ASR_EMPTY)];
    Strings::RECORD_ASR_FAIL = literals[static_cast<size_t>(LangStringId::RECORD_ASR_FAIL)];
    Strings::RECORD_ASR_HINT = literals[static_cast<size_t>(LangStringId::RECORD_ASR_HINT)];
    Strings::RECORD_ASR_HTTP_FMT = literals[static_cast<size_t>(LangStringId::RECORD_ASR_HTTP_FMT)];
    Strings::RECORD_ASR_NEED_NET = literals[static_cast<size_t>(LangStringId::RECORD_ASR_NEED_NET)];
    Strings::RECORD_ASR_PARSE_FAIL = literals[static_cast<size_t>(LangStringId::RECORD_ASR_PARSE_FAIL)];
    Strings::RECORD_ASR_PENDING = literals[static_cast<size_t>(LangStringId::RECORD_ASR_PENDING)];
    Strings::RECORD_ASR_REQ_FAIL = literals[static_cast<size_t>(LangStringId::RECORD_ASR_REQ_FAIL)];
    Strings::RECORD_ASR_START_FAIL = literals[static_cast<size_t>(LangStringId::RECORD_ASR_START_FAIL)];
    Strings::RECORD_ASR_UPLOADED_HINT = literals[static_cast<size_t>(LangStringId::RECORD_ASR_UPLOADED_HINT)];
    Strings::RECORD_AUDIO_BUSY = literals[static_cast<size_t>(LangStringId::RECORD_AUDIO_BUSY)];
    Strings::RECORD_AUDIO_NOT_READY = literals[static_cast<size_t>(LangStringId::RECORD_AUDIO_NOT_READY)];
    Strings::RECORD_AUDIO_STARTING = literals[static_cast<size_t>(LangStringId::RECORD_AUDIO_STARTING)];
    Strings::RECORD_BTN_ASR = literals[static_cast<size_t>(LangStringId::RECORD_BTN_ASR)];
    Strings::RECORD_BTN_PLAY = literals[static_cast<size_t>(LangStringId::RECORD_BTN_PLAY)];
    Strings::RECORD_BTN_SAVING = literals[static_cast<size_t>(LangStringId::RECORD_BTN_SAVING)];
    Strings::RECORD_BTN_START = literals[static_cast<size_t>(LangStringId::RECORD_BTN_START)];
    Strings::RECORD_BTN_STOP = literals[static_cast<size_t>(LangStringId::RECORD_BTN_STOP)];
    Strings::RECORD_BTN_STOP_PLAY = literals[static_cast<size_t>(LangStringId::RECORD_BTN_STOP_PLAY)];
    Strings::RECORD_DELETED = literals[static_cast<size_t>(LangStringId::RECORD_DELETED)];
    Strings::RECORD_DELETE_FAIL = literals[static_cast<size_t>(LangStringId::RECORD_DELETE_FAIL)];
    Strings::RECORD_DURATION_SEC_FMT = literals[static_cast<size_t>(LangStringId::RECORD_DURATION_SEC_FMT)];
    Strings::RECORD_EMPTY = literals[static_cast<size_t>(LangStringId::RECORD_EMPTY)];
    Strings::RECORD_FILE_CORRUPT = literals[static_cast<size_t>(LangStringId::RECORD_FILE_CORRUPT)];
    Strings::RECORD_FILE_TOO_LARGE = literals[static_cast<size_t>(LangStringId::RECORD_FILE_TOO_LARGE)];
    Strings::RECORD_HINT_START = literals[static_cast<size_t>(LangStringId::RECORD_HINT_START)];
    Strings::RECORD_INSERT_SD = literals[static_cast<size_t>(LangStringId::RECORD_INSERT_SD)];
    Strings::RECORD_MAX_MIN_FMT = literals[static_cast<size_t>(LangStringId::RECORD_MAX_MIN_FMT)];
    Strings::RECORD_META_FMT = literals[static_cast<size_t>(LangStringId::RECORD_META_FMT)];
    Strings::RECORD_MKDIR_FAIL = literals[static_cast<size_t>(LangStringId::RECORD_MKDIR_FAIL)];
    Strings::RECORD_NET_UNAVAIL = literals[static_cast<size_t>(LangStringId::RECORD_NET_UNAVAIL)];
    Strings::RECORD_NO_SD_HINT = literals[static_cast<size_t>(LangStringId::RECORD_NO_SD_HINT)];
    Strings::RECORD_OOM = literals[static_cast<size_t>(LangStringId::RECORD_OOM)];
    Strings::RECORD_OPEN_FAIL = literals[static_cast<size_t>(LangStringId::RECORD_OPEN_FAIL)];
    Strings::RECORD_PLAYING = literals[static_cast<size_t>(LangStringId::RECORD_PLAYING)];
    Strings::RECORD_PLAY_END = literals[static_cast<size_t>(LangStringId::RECORD_PLAY_END)];
    Strings::RECORD_PLAY_START_FAIL = literals[static_cast<size_t>(LangStringId::RECORD_PLAY_START_FAIL)];
    Strings::RECORD_PLAY_STOPPED = literals[static_cast<size_t>(LangStringId::RECORD_PLAY_STOPPED)];
    Strings::RECORD_PLEASE_WAIT = literals[static_cast<size_t>(LangStringId::RECORD_PLEASE_WAIT)];
    Strings::RECORD_READ_FAIL = literals[static_cast<size_t>(LangStringId::RECORD_READ_FAIL)];
    Strings::RECORD_RECORDING = literals[static_cast<size_t>(LangStringId::RECORD_RECORDING)];
    Strings::RECORD_SAVED_SD = literals[static_cast<size_t>(LangStringId::RECORD_SAVED_SD)];
    Strings::RECORD_STOP_FIRST = literals[static_cast<size_t>(LangStringId::RECORD_STOP_FIRST)];
    Strings::RECORD_SUMMARY = literals[static_cast<size_t>(LangStringId::RECORD_SUMMARY)];
    Strings::RECORD_TAB_LIST = literals[static_cast<size_t>(LangStringId::RECORD_TAB_LIST)];
    Strings::RECORD_TAB_REC = literals[static_cast<size_t>(LangStringId::RECORD_TAB_REC)];
    Strings::RECORD_TASK_START_FAIL = literals[static_cast<size_t>(LangStringId::RECORD_TASK_START_FAIL)];
    Strings::RECORD_TOO_SHORT = literals[static_cast<size_t>(LangStringId::RECORD_TOO_SHORT)];
    Strings::RECORD_UPLOADING = literals[static_cast<size_t>(LangStringId::RECORD_UPLOADING)];
    Strings::RECORD_WRITE_FAIL = literals[static_cast<size_t>(LangStringId::RECORD_WRITE_FAIL)];
    Strings::REGISTERING_NETWORK = literals[static_cast<size_t>(LangStringId::REGISTERING_NETWORK)];
    Strings::REG_ERROR = literals[static_cast<size_t>(LangStringId::REG_ERROR)];
    Strings::RTC_MODE_OFF = literals[static_cast<size_t>(LangStringId::RTC_MODE_OFF)];
    Strings::RTC_MODE_ON = literals[static_cast<size_t>(LangStringId::RTC_MODE_ON)];
    Strings::SCANNING_WIFI = literals[static_cast<size_t>(LangStringId::SCANNING_WIFI)];
    Strings::SERVER_ERROR = literals[static_cast<size_t>(LangStringId::SERVER_ERROR)];
    Strings::SERVER_NOT_CONNECTED = literals[static_cast<size_t>(LangStringId::SERVER_NOT_CONNECTED)];
    Strings::SERVER_NOT_FOUND = literals[static_cast<size_t>(LangStringId::SERVER_NOT_FOUND)];
    Strings::SERVER_TIMEOUT = literals[static_cast<size_t>(LangStringId::SERVER_TIMEOUT)];
    Strings::SETTINGS_ABOUT_BUILD = literals[static_cast<size_t>(LangStringId::SETTINGS_ABOUT_BUILD)];
    Strings::SETTINGS_ABOUT_CHIP = literals[static_cast<size_t>(LangStringId::SETTINGS_ABOUT_CHIP)];
    Strings::SETTINGS_ABOUT_CORES = literals[static_cast<size_t>(LangStringId::SETTINGS_ABOUT_CORES)];
    Strings::SETTINGS_ABOUT_CORES_FMT = literals[static_cast<size_t>(LangStringId::SETTINGS_ABOUT_CORES_FMT)];
    Strings::SETTINGS_ABOUT_FLASH = literals[static_cast<size_t>(LangStringId::SETTINGS_ABOUT_FLASH)];
    Strings::SETTINGS_ABOUT_FW = literals[static_cast<size_t>(LangStringId::SETTINGS_ABOUT_FW)];
    Strings::SETTINGS_ABOUT_FW_CHECK = literals[static_cast<size_t>(LangStringId::SETTINGS_ABOUT_FW_CHECK)];
    Strings::SETTINGS_ABOUT_FW_VER_FMT = literals[static_cast<size_t>(LangStringId::SETTINGS_ABOUT_FW_VER_FMT)];
    Strings::SETTINGS_ABOUT_MAC = literals[static_cast<size_t>(LangStringId::SETTINGS_ABOUT_MAC)];
    Strings::SETTINGS_ABOUT_MODEL = literals[static_cast<size_t>(LangStringId::SETTINGS_ABOUT_MODEL)];
    Strings::SETTINGS_ABOUT_NONE = literals[static_cast<size_t>(LangStringId::SETTINGS_ABOUT_NONE)];
    Strings::SETTINGS_ABOUT_PSRAM = literals[static_cast<size_t>(LangStringId::SETTINGS_ABOUT_PSRAM)];
    Strings::SETTINGS_CONV_BUSY = literals[static_cast<size_t>(LangStringId::SETTINGS_CONV_BUSY)];
    Strings::SETTINGS_CONV_CONNECTING = literals[static_cast<size_t>(LangStringId::SETTINGS_CONV_CONNECTING)];
    Strings::SETTINGS_CONV_CONN_FAIL = literals[static_cast<size_t>(LangStringId::SETTINGS_CONV_CONN_FAIL)];
    Strings::SETTINGS_CONV_CUR_DASH = literals[static_cast<size_t>(LangStringId::SETTINGS_CONV_CUR_DASH)];
    Strings::SETTINGS_CONV_CUR_OFF = literals[static_cast<size_t>(LangStringId::SETTINGS_CONV_CUR_OFF)];
    Strings::SETTINGS_CONV_CUR_ON = literals[static_cast<size_t>(LangStringId::SETTINGS_CONV_CUR_ON)];
    Strings::SETTINGS_CONV_DISABLING = literals[static_cast<size_t>(LangStringId::SETTINGS_CONV_DISABLING)];
    Strings::SETTINGS_CONV_ENABLING = literals[static_cast<size_t>(LangStringId::SETTINGS_CONV_ENABLING)];
    Strings::SETTINGS_CONV_HINT = literals[static_cast<size_t>(LangStringId::SETTINGS_CONV_HINT)];
    Strings::SETTINGS_CONV_MISSING_ENABLED = literals[static_cast<size_t>(LangStringId::SETTINGS_CONV_MISSING_ENABLED)];
    Strings::SETTINGS_CONV_NET_NOT_READY = literals[static_cast<size_t>(LangStringId::SETTINGS_CONV_NET_NOT_READY)];
    Strings::SETTINGS_CONV_NO_NETWORK = literals[static_cast<size_t>(LangStringId::SETTINGS_CONV_NO_NETWORK)];
    Strings::SETTINGS_CONV_PARSE_FAIL = literals[static_cast<size_t>(LangStringId::SETTINGS_CONV_PARSE_FAIL)];
    Strings::SETTINGS_CONV_REQUEST_FAIL = literals[static_cast<size_t>(LangStringId::SETTINGS_CONV_REQUEST_FAIL)];
    Strings::SETTINGS_CONV_TITLE = literals[static_cast<size_t>(LangStringId::SETTINGS_CONV_TITLE)];
    Strings::SETTINGS_CONV_TTS_OFF = literals[static_cast<size_t>(LangStringId::SETTINGS_CONV_TTS_OFF)];
    Strings::SETTINGS_CONV_TTS_ON = literals[static_cast<size_t>(LangStringId::SETTINGS_CONV_TTS_ON)];
    Strings::SETTINGS_HAPTIC_CURRENT_OFF = literals[static_cast<size_t>(LangStringId::SETTINGS_HAPTIC_CURRENT_OFF)];
    Strings::SETTINGS_HAPTIC_CURRENT_ON = literals[static_cast<size_t>(LangStringId::SETTINGS_HAPTIC_CURRENT_ON)];
    Strings::SETTINGS_HAPTIC_HINT = literals[static_cast<size_t>(LangStringId::SETTINGS_HAPTIC_HINT)];
    Strings::SETTINGS_HAPTIC_OFF = literals[static_cast<size_t>(LangStringId::SETTINGS_HAPTIC_OFF)];
    Strings::SETTINGS_HAPTIC_ON = literals[static_cast<size_t>(LangStringId::SETTINGS_HAPTIC_ON)];
    Strings::SETTINGS_HAPTIC_TITLE = literals[static_cast<size_t>(LangStringId::SETTINGS_HAPTIC_TITLE)];
    Strings::SETTINGS_LANG_CURRENT_EN = literals[static_cast<size_t>(LangStringId::SETTINGS_LANG_CURRENT_EN)];
    Strings::SETTINGS_LANG_CURRENT_ZH = literals[static_cast<size_t>(LangStringId::SETTINGS_LANG_CURRENT_ZH)];
    Strings::SETTINGS_LANG_EN_US = literals[static_cast<size_t>(LangStringId::SETTINGS_LANG_EN_US)];
    Strings::SETTINGS_LANG_HINT = literals[static_cast<size_t>(LangStringId::SETTINGS_LANG_HINT)];
    Strings::SETTINGS_LANG_TITLE = literals[static_cast<size_t>(LangStringId::SETTINGS_LANG_TITLE)];
    Strings::SETTINGS_LANG_ZH_CN = literals[static_cast<size_t>(LangStringId::SETTINGS_LANG_ZH_CN)];
    Strings::SETTINGS_NET_AP_TASK_FAIL = literals[static_cast<size_t>(LangStringId::SETTINGS_NET_AP_TASK_FAIL)];
    Strings::SETTINGS_NET_BOARD_NO_AP = literals[static_cast<size_t>(LangStringId::SETTINGS_NET_BOARD_NO_AP)];
    Strings::SETTINGS_NET_CFUN0 = literals[static_cast<size_t>(LangStringId::SETTINGS_NET_CFUN0)];
    Strings::SETTINGS_NET_CFUN0_FAIL = literals[static_cast<size_t>(LangStringId::SETTINGS_NET_CFUN0_FAIL)];
    Strings::SETTINGS_NET_CFUN1 = literals[static_cast<size_t>(LangStringId::SETTINGS_NET_CFUN1)];
    Strings::SETTINGS_NET_CURRENT_FMT = literals[static_cast<size_t>(LangStringId::SETTINGS_NET_CURRENT_FMT)];
    Strings::SETTINGS_NET_CUR_4G = literals[static_cast<size_t>(LangStringId::SETTINGS_NET_CUR_4G)];
    Strings::SETTINGS_NET_CUR_DASH = literals[static_cast<size_t>(LangStringId::SETTINGS_NET_CUR_DASH)];
    Strings::SETTINGS_NET_CUR_WIFI = literals[static_cast<size_t>(LangStringId::SETTINGS_NET_CUR_WIFI)];
    Strings::SETTINGS_NET_ECSIMCFG_FAIL = literals[static_cast<size_t>(LangStringId::SETTINGS_NET_ECSIMCFG_FAIL)];
    Strings::SETTINGS_NET_ENTER_AP = literals[static_cast<size_t>(LangStringId::SETTINGS_NET_ENTER_AP)];
    Strings::SETTINGS_NET_ENTER_CFG = literals[static_cast<size_t>(LangStringId::SETTINGS_NET_ENTER_CFG)];
    Strings::SETTINGS_NET_HOTSPOT_NAME = literals[static_cast<size_t>(LangStringId::SETTINGS_NET_HOTSPOT_NAME)];
    Strings::SETTINGS_NET_MODE_4G = literals[static_cast<size_t>(LangStringId::SETTINGS_NET_MODE_4G)];
    Strings::SETTINGS_NET_MODE_TITLE = literals[static_cast<size_t>(LangStringId::SETTINGS_NET_MODE_TITLE)];
    Strings::SETTINGS_NET_MODE_WIFI = literals[static_cast<size_t>(LangStringId::SETTINGS_NET_MODE_WIFI)];
    Strings::SETTINGS_NET_NOT_4G = literals[static_cast<size_t>(LangStringId::SETTINGS_NET_NOT_4G)];
    Strings::SETTINGS_NET_NO_4G = literals[static_cast<size_t>(LangStringId::SETTINGS_NET_NO_4G)];
    Strings::SETTINGS_NET_REBOOTING = literals[static_cast<size_t>(LangStringId::SETTINGS_NET_REBOOTING)];
    Strings::SETTINGS_NET_REBOOT_COUNT_FMT = literals[static_cast<size_t>(LangStringId::SETTINGS_NET_REBOOT_COUNT_FMT)];
    Strings::SETTINGS_NET_REBOOT_HINT = literals[static_cast<size_t>(LangStringId::SETTINGS_NET_REBOOT_HINT)];
    Strings::SETTINGS_NET_SIM_EXT = literals[static_cast<size_t>(LangStringId::SETTINGS_NET_SIM_EXT)];
    Strings::SETTINGS_NET_SIM_INT = literals[static_cast<size_t>(LangStringId::SETTINGS_NET_SIM_INT)];
    Strings::SETTINGS_NET_SIM_TASK_FAIL = literals[static_cast<size_t>(LangStringId::SETTINGS_NET_SIM_TASK_FAIL)];
    Strings::SETTINGS_NET_SIM_TITLE = literals[static_cast<size_t>(LangStringId::SETTINGS_NET_SIM_TITLE)];
    Strings::SETTINGS_NET_SWITCHED_FMT = literals[static_cast<size_t>(LangStringId::SETTINGS_NET_SWITCHED_FMT)];
    Strings::SETTINGS_NET_SWITCH_4G = literals[static_cast<size_t>(LangStringId::SETTINGS_NET_SWITCH_4G)];
    Strings::SETTINGS_NET_SWITCH_SIM_CFUN0_FMT = literals[static_cast<size_t>(LangStringId::SETTINGS_NET_SWITCH_SIM_CFUN0_FMT)];
    Strings::SETTINGS_NET_SWITCH_SIM_FMT = literals[static_cast<size_t>(LangStringId::SETTINGS_NET_SWITCH_SIM_FMT)];
    Strings::SETTINGS_NET_SWITCH_WIFI = literals[static_cast<size_t>(LangStringId::SETTINGS_NET_SWITCH_WIFI)];
    Strings::SETTINGS_NET_WIFI_CFG_HINT = literals[static_cast<size_t>(LangStringId::SETTINGS_NET_WIFI_CFG_HINT)];
    Strings::SETTINGS_NET_WIFI_CFG_TITLE = literals[static_cast<size_t>(LangStringId::SETTINGS_NET_WIFI_CFG_TITLE)];
    Strings::SETTINGS_POWER_10_MIN = literals[static_cast<size_t>(LangStringId::SETTINGS_POWER_10_MIN)];
    Strings::SETTINGS_POWER_120_SEC = literals[static_cast<size_t>(LangStringId::SETTINGS_POWER_120_SEC)];
    Strings::SETTINGS_POWER_30_MIN = literals[static_cast<size_t>(LangStringId::SETTINGS_POWER_30_MIN)];
    Strings::SETTINGS_POWER_30_SEC = literals[static_cast<size_t>(LangStringId::SETTINGS_POWER_30_SEC)];
    Strings::SETTINGS_POWER_3_MIN = literals[static_cast<size_t>(LangStringId::SETTINGS_POWER_3_MIN)];
    Strings::SETTINGS_POWER_60_SEC = literals[static_cast<size_t>(LangStringId::SETTINGS_POWER_60_SEC)];
    Strings::SETTINGS_POWER_IDLE_MHZ_HINT = literals[static_cast<size_t>(LangStringId::SETTINGS_POWER_IDLE_MHZ_HINT)];
    Strings::SETTINGS_POWER_IDLE_MHZ_TITLE = literals[static_cast<size_t>(LangStringId::SETTINGS_POWER_IDLE_MHZ_TITLE)];
    Strings::SETTINGS_POWER_NET_GRACE_HINT = literals[static_cast<size_t>(LangStringId::SETTINGS_POWER_NET_GRACE_HINT)];
    Strings::SETTINGS_POWER_NET_GRACE_TITLE = literals[static_cast<size_t>(LangStringId::SETTINGS_POWER_NET_GRACE_TITLE)];
    Strings::SETTINGS_POWER_OFF_HINT = literals[static_cast<size_t>(LangStringId::SETTINGS_POWER_OFF_HINT)];
    Strings::SETTINGS_POWER_OFF_NEVER = literals[static_cast<size_t>(LangStringId::SETTINGS_POWER_OFF_NEVER)];
    Strings::SETTINGS_POWER_OFF_TITLE = literals[static_cast<size_t>(LangStringId::SETTINGS_POWER_OFF_TITLE)];
    Strings::SETTINGS_POWER_STANDBY_HINT = literals[static_cast<size_t>(LangStringId::SETTINGS_POWER_STANDBY_HINT)];
    Strings::SETTINGS_POWER_STANDBY_TITLE = literals[static_cast<size_t>(LangStringId::SETTINGS_POWER_STANDBY_TITLE)];
    Strings::SETTINGS_STORAGE_CAP_DASH = literals[static_cast<size_t>(LangStringId::SETTINGS_STORAGE_CAP_DASH)];
    Strings::SETTINGS_STORAGE_CAP_EXPORT = literals[static_cast<size_t>(LangStringId::SETTINGS_STORAGE_CAP_EXPORT)];
    Strings::SETTINGS_STORAGE_CAP_FAIL = literals[static_cast<size_t>(LangStringId::SETTINGS_STORAGE_CAP_FAIL)];
    Strings::SETTINGS_STORAGE_CAP_FMT = literals[static_cast<size_t>(LangStringId::SETTINGS_STORAGE_CAP_FMT)];
    Strings::SETTINGS_STORAGE_INSERTED = literals[static_cast<size_t>(LangStringId::SETTINGS_STORAGE_INSERTED)];
    Strings::SETTINGS_STORAGE_MISSING = literals[static_cast<size_t>(LangStringId::SETTINGS_STORAGE_MISSING)];
    Strings::SETTINGS_STORAGE_PC_BUSY = literals[static_cast<size_t>(LangStringId::SETTINGS_STORAGE_PC_BUSY)];
    Strings::SETTINGS_STORAGE_TITLE = literals[static_cast<size_t>(LangStringId::SETTINGS_STORAGE_TITLE)];
    Strings::SETTINGS_STORAGE_USB_DISABLED = literals[static_cast<size_t>(LangStringId::SETTINGS_STORAGE_USB_DISABLED)];
    Strings::SETTINGS_STORAGE_USB_HINT_DISABLED = literals[static_cast<size_t>(LangStringId::SETTINGS_STORAGE_USB_HINT_DISABLED)];
    Strings::SETTINGS_STORAGE_USB_HINT_DISABLE_FAIL = literals[static_cast<size_t>(LangStringId::SETTINGS_STORAGE_USB_HINT_DISABLE_FAIL)];
    Strings::SETTINGS_STORAGE_USB_HINT_DISABLING = literals[static_cast<size_t>(LangStringId::SETTINGS_STORAGE_USB_HINT_DISABLING)];
    Strings::SETTINGS_STORAGE_USB_HINT_ENABLE_FAIL = literals[static_cast<size_t>(LangStringId::SETTINGS_STORAGE_USB_HINT_ENABLE_FAIL)];
    Strings::SETTINGS_STORAGE_USB_HINT_ENABLING = literals[static_cast<size_t>(LangStringId::SETTINGS_STORAGE_USB_HINT_ENABLING)];
    Strings::SETTINGS_STORAGE_USB_HINT_FORMAT = literals[static_cast<size_t>(LangStringId::SETTINGS_STORAGE_USB_HINT_FORMAT)];
    Strings::SETTINGS_STORAGE_USB_HINT_HOST = literals[static_cast<size_t>(LangStringId::SETTINGS_STORAGE_USB_HINT_HOST)];
    Strings::SETTINGS_STORAGE_USB_HINT_HOST_BUSY = literals[static_cast<size_t>(LangStringId::SETTINGS_STORAGE_USB_HINT_HOST_BUSY)];
    Strings::SETTINGS_STORAGE_USB_HINT_IDLE = literals[static_cast<size_t>(LangStringId::SETTINGS_STORAGE_USB_HINT_IDLE)];
    Strings::SETTINGS_STORAGE_USB_HINT_LOCAL = literals[static_cast<size_t>(LangStringId::SETTINGS_STORAGE_USB_HINT_LOCAL)];
    Strings::SETTINGS_STORAGE_USB_HINT_NO_SD = literals[static_cast<size_t>(LangStringId::SETTINGS_STORAGE_USB_HINT_NO_SD)];
    Strings::SETTINGS_STORAGE_USB_HINT_SWITCHING = literals[static_cast<size_t>(LangStringId::SETTINGS_STORAGE_USB_HINT_SWITCHING)];
    Strings::SETTINGS_STORAGE_USB_OFF = literals[static_cast<size_t>(LangStringId::SETTINGS_STORAGE_USB_OFF)];
    Strings::SETTINGS_STORAGE_USB_ON = literals[static_cast<size_t>(LangStringId::SETTINGS_STORAGE_USB_ON)];
    Strings::SETTINGS_TAB_ABOUT = literals[static_cast<size_t>(LangStringId::SETTINGS_TAB_ABOUT)];
    Strings::SETTINGS_TAB_BLUETOOTH = literals[static_cast<size_t>(LangStringId::SETTINGS_TAB_BLUETOOTH)];
    Strings::SETTINGS_TAB_CONVERSATION = literals[static_cast<size_t>(LangStringId::SETTINGS_TAB_CONVERSATION)];
    Strings::SETTINGS_TAB_HAPTIC = literals[static_cast<size_t>(LangStringId::SETTINGS_TAB_HAPTIC)];
    Strings::SETTINGS_TAB_LANGUAGE = literals[static_cast<size_t>(LangStringId::SETTINGS_TAB_LANGUAGE)];
    Strings::SETTINGS_TAB_NETWORK = literals[static_cast<size_t>(LangStringId::SETTINGS_TAB_NETWORK)];
    Strings::SETTINGS_TAB_POWER = literals[static_cast<size_t>(LangStringId::SETTINGS_TAB_POWER)];
    Strings::SETTINGS_TAB_STORAGE = literals[static_cast<size_t>(LangStringId::SETTINGS_TAB_STORAGE)];
    Strings::SETTINGS_TAB_TEST = literals[static_cast<size_t>(LangStringId::SETTINGS_TAB_TEST)];
    Strings::SETTINGS_TAB_THEME = literals[static_cast<size_t>(LangStringId::SETTINGS_TAB_THEME)];
    Strings::SETTINGS_TEST_AGING = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_AGING)];
    Strings::SETTINGS_TEST_AGING_RUNNING = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_AGING_RUNNING)];
    Strings::SETTINGS_TEST_AUDIO_CONFIRM = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_AUDIO_CONFIRM)];
    Strings::SETTINGS_TEST_AUDIO_HOLD = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_AUDIO_HOLD)];
    Strings::SETTINGS_TEST_AUDIO_RECORDING = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_AUDIO_RECORDING)];
    Strings::SETTINGS_TEST_AUDIO_TITLE = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_AUDIO_TITLE)];
    Strings::SETTINGS_TEST_AUTO = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_AUTO)];
    Strings::SETTINGS_TEST_BATTERY = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_BATTERY)];
    Strings::SETTINGS_TEST_BATTERY_CHARGING = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_BATTERY_CHARGING)];
    Strings::SETTINGS_TEST_BATTERY_CHG_CC = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_BATTERY_CHG_CC)];
    Strings::SETTINGS_TEST_BATTERY_CHG_CV = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_BATTERY_CHG_CV)];
    Strings::SETTINGS_TEST_BATTERY_CHG_EN = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_BATTERY_CHG_EN)];
    Strings::SETTINGS_TEST_BATTERY_CHG_NOT = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_BATTERY_CHG_NOT)];
    Strings::SETTINGS_TEST_BATTERY_CHG_TOPOFF = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_BATTERY_CHG_TOPOFF)];
    Strings::SETTINGS_TEST_BATTERY_CHIP_CHG = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_BATTERY_CHIP_CHG)];
    Strings::SETTINGS_TEST_BATTERY_CURR = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_BATTERY_CURR)];
    Strings::SETTINGS_TEST_BATTERY_DISCHARGING = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_BATTERY_DISCHARGING)];
    Strings::SETTINGS_TEST_BATTERY_EN_OFF = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_BATTERY_EN_OFF)];
    Strings::SETTINGS_TEST_BATTERY_EN_ON = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_BATTERY_EN_ON)];
    Strings::SETTINGS_TEST_BATTERY_GAUGE = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_BATTERY_GAUGE)];
    Strings::SETTINGS_TEST_BATTERY_GAUGE_STAT = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_BATTERY_GAUGE_STAT)];
    Strings::SETTINGS_TEST_BATTERY_HINT = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_BATTERY_HINT)];
    Strings::SETTINGS_TEST_BATTERY_ICHG = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_BATTERY_ICHG)];
    Strings::SETTINGS_TEST_BATTERY_IDLE_LOAD = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_BATTERY_IDLE_LOAD)];
    Strings::SETTINGS_TEST_BATTERY_MATCH = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_BATTERY_MATCH)];
    Strings::SETTINGS_TEST_BATTERY_MATCH_DPDM = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_BATTERY_MATCH_DPDM)];
    Strings::SETTINGS_TEST_BATTERY_MATCH_OK = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_BATTERY_MATCH_OK)];
    Strings::SETTINGS_TEST_BATTERY_MISMATCH_CHIP = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_BATTERY_MISMATCH_CHIP)];
    Strings::SETTINGS_TEST_BATTERY_MISMATCH_GAUGE = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_BATTERY_MISMATCH_GAUGE)];
    Strings::SETTINGS_TEST_BATTERY_MISMATCH_NO_CHG = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_BATTERY_MISMATCH_NO_CHG)];
    Strings::SETTINGS_TEST_BATTERY_MISMATCH_NO_IN = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_BATTERY_MISMATCH_NO_IN)];
    Strings::SETTINGS_TEST_BATTERY_SOC_RAW = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_BATTERY_SOC_RAW)];
    Strings::SETTINGS_TEST_BATTERY_SOC_SMOOTH = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_BATTERY_SOC_SMOOTH)];
    Strings::SETTINGS_TEST_BATTERY_VBUS = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_BATTERY_VBUS)];
    Strings::SETTINGS_TEST_BATTERY_VBUS_ADP = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_BATTERY_VBUS_ADP)];
    Strings::SETTINGS_TEST_BATTERY_VBUS_CDP = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_BATTERY_VBUS_CDP)];
    Strings::SETTINGS_TEST_BATTERY_VBUS_DCP = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_BATTERY_VBUS_DCP)];
    Strings::SETTINGS_TEST_BATTERY_VBUS_NONE = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_BATTERY_VBUS_NONE)];
    Strings::SETTINGS_TEST_BATTERY_VBUS_NONSTD = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_BATTERY_VBUS_NONSTD)];
    Strings::SETTINGS_TEST_BATTERY_VBUS_OTG = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_BATTERY_VBUS_OTG)];
    Strings::SETTINGS_TEST_BATTERY_VBUS_SDP = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_BATTERY_VBUS_SDP)];
    Strings::SETTINGS_TEST_BATTERY_VBUS_UNKNOWN_IN = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_BATTERY_VBUS_UNKNOWN_IN)];
    Strings::SETTINGS_TEST_BATTERY_VBUS_UNK_ADP = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_BATTERY_VBUS_UNK_ADP)];
    Strings::SETTINGS_TEST_BATTERY_VOLT = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_BATTERY_VOLT)];
    Strings::SETTINGS_TEST_BATTERY_VOLT_RANGE = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_BATTERY_VOLT_RANGE)];
    Strings::SETTINGS_TEST_BATTERY_VREG = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_BATTERY_VREG)];
    Strings::SETTINGS_TEST_BUSY = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_BUSY)];
    Strings::SETTINGS_TEST_CAMERA = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_CAMERA)];
    Strings::SETTINGS_TEST_CAMERA_FAIL = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_CAMERA_FAIL)];
    Strings::SETTINGS_TEST_CAMERA_OK = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_CAMERA_OK)];
    Strings::SETTINGS_TEST_CAMERA_RESULT = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_CAMERA_RESULT)];
    Strings::SETTINGS_TEST_CAMERA_SHOT = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_CAMERA_SHOT)];
    Strings::SETTINGS_TEST_CAMERA_TIMEOUT = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_CAMERA_TIMEOUT)];
    Strings::SETTINGS_TEST_CAMERA_USB_BUSY = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_CAMERA_USB_BUSY)];
    Strings::SETTINGS_TEST_CAMERA_USB_IRQ = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_CAMERA_USB_IRQ)];
    Strings::SETTINGS_TEST_CAPTURING = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_CAPTURING)];
    Strings::SETTINGS_TEST_CELL_CANCELLED = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_CELL_CANCELLED)];
    Strings::SETTINGS_TEST_CELL_CFUN0_FAIL = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_CELL_CFUN0_FAIL)];
    Strings::SETTINGS_TEST_CELL_CFUN1_FAIL = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_CELL_CFUN1_FAIL)];
    Strings::SETTINGS_TEST_CELL_EXT = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_CELL_EXT)];
    Strings::SETTINGS_TEST_CELL_INT = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_CELL_INT)];
    Strings::SETTINGS_TEST_CELL_LOSS_FMT = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_CELL_LOSS_FMT)];
    Strings::SETTINGS_TEST_CELL_MODEM_NOT_READY = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_CELL_MODEM_NOT_READY)];
    Strings::SETTINGS_TEST_CELL_MODEM_NO_RESP = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_CELL_MODEM_NO_RESP)];
    Strings::SETTINGS_TEST_CELL_NEED_4G = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_CELL_NEED_4G)];
    Strings::SETTINGS_TEST_CELL_NO_RESOURCE = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_CELL_NO_RESOURCE)];
    Strings::SETTINGS_TEST_CELL_NO_SIM = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_CELL_NO_SIM)];
    Strings::SETTINGS_TEST_CELL_PING_CMD_FAIL = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_CELL_PING_CMD_FAIL)];
    Strings::SETTINGS_TEST_CELL_PING_FAIL = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_CELL_PING_FAIL)];
    Strings::SETTINGS_TEST_CELL_PING_OK = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_CELL_PING_OK)];
    Strings::SETTINGS_TEST_CELL_PING_WAIT_TIMEOUT = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_CELL_PING_WAIT_TIMEOUT)];
    Strings::SETTINGS_TEST_CELL_REG_REJECT = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_CELL_REG_REJECT)];
    Strings::SETTINGS_TEST_CELL_SEARCH_TIMEOUT = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_CELL_SEARCH_TIMEOUT)];
    Strings::SETTINGS_TEST_CELL_SIM_SWITCH_FAIL = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_CELL_SIM_SWITCH_FAIL)];
    Strings::SETTINGS_TEST_CELL_SLOT_FAIL = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_CELL_SLOT_FAIL)];
    Strings::SETTINGS_TEST_CELL_TESTING = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_CELL_TESTING)];
    Strings::SETTINGS_TEST_CELL_TEST_FAIL = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_CELL_TEST_FAIL)];
    Strings::SETTINGS_TEST_CELL_WAIT_RESOURCE = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_CELL_WAIT_RESOURCE)];
    Strings::SETTINGS_TEST_DETECTING = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_DETECTING)];
    Strings::SETTINGS_TEST_EXIT_REBOOT = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_EXIT_REBOOT)];
    Strings::SETTINGS_TEST_FAIL = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_FAIL)];
    Strings::SETTINGS_TEST_NO = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_NO)];
    Strings::SETTINGS_TEST_NOT_DETECTED = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_NOT_DETECTED)];
    Strings::SETTINGS_TEST_NOT_PASS = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_NOT_PASS)];
    Strings::SETTINGS_TEST_OK = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_OK)];
    Strings::SETTINGS_TEST_PASS = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_PASS)];
    Strings::SETTINGS_TEST_REBOOTING = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_REBOOTING)];
    Strings::SETTINGS_TEST_SCANNING = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_SCANNING)];
    Strings::SETTINGS_TEST_SDCARD = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_SDCARD)];
    Strings::SETTINGS_TEST_SD_MOUNT_FAIL = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_SD_MOUNT_FAIL)];
    Strings::SETTINGS_TEST_SIGNAL_BOARD_UNSUP = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_SIGNAL_BOARD_UNSUP)];
    Strings::SETTINGS_TEST_SIGNAL_CELL_CSQ_FMT = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_SIGNAL_CELL_CSQ_FMT)];
    Strings::SETTINGS_TEST_SIGNAL_CELL_CSQ_UNKNOWN = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_SIGNAL_CELL_CSQ_UNKNOWN)];
    Strings::SETTINGS_TEST_SIGNAL_CELL_UNAVAIL = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_SIGNAL_CELL_UNAVAIL)];
    Strings::SETTINGS_TEST_SIGNAL_READING = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_SIGNAL_READING)];
    Strings::SETTINGS_TEST_SIGNAL_TITLE = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_SIGNAL_TITLE)];
    Strings::SETTINGS_TEST_SIGNAL_WIFI_DISCONNECTED = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_SIGNAL_WIFI_DISCONNECTED)];
    Strings::SETTINGS_TEST_SIGNAL_WIFI_READ_FAIL = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_SIGNAL_WIFI_READ_FAIL)];
    Strings::SETTINGS_TEST_SIGNAL_WIFI_RSSI_FMT = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_SIGNAL_WIFI_RSSI_FMT)];
    Strings::SETTINGS_TEST_TASK_CREATE_FAIL = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_TASK_CREATE_FAIL)];
    Strings::SETTINGS_TEST_TOUCH = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_TOUCH)];
    Strings::SETTINGS_TEST_TOUCH_START = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_TOUCH_START)];
    Strings::SETTINGS_TEST_WAITING = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_WAITING)];
    Strings::SETTINGS_TEST_WIFI_CLOSE = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_WIFI_CLOSE)];
    Strings::SETTINGS_TEST_WIFI_COUNT_FMT = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_WIFI_COUNT_FMT)];
    Strings::SETTINGS_TEST_WIFI_FOUND_FMT = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_WIFI_FOUND_FMT)];
    Strings::SETTINGS_TEST_WIFI_NEARBY = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_WIFI_NEARBY)];
    Strings::SETTINGS_TEST_WIFI_NEXT = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_WIFI_NEXT)];
    Strings::SETTINGS_TEST_WIFI_NONE = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_WIFI_NONE)];
    Strings::SETTINGS_TEST_WIFI_PREV = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_WIFI_PREV)];
    Strings::SETTINGS_TEST_WIFI_RESCAN = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_WIFI_RESCAN)];
    Strings::SETTINGS_TEST_WIFI_SCANNING = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_WIFI_SCANNING)];
    Strings::SETTINGS_TEST_WIFI_SCAN_FAIL = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_WIFI_SCAN_FAIL)];
    Strings::SETTINGS_TEST_YES = literals[static_cast<size_t>(LangStringId::SETTINGS_TEST_YES)];
    Strings::SETTINGS_THEME_BORDER = literals[static_cast<size_t>(LangStringId::SETTINGS_THEME_BORDER)];
    Strings::SETTINGS_THEME_CURRENT_BORDER = literals[static_cast<size_t>(LangStringId::SETTINGS_THEME_CURRENT_BORDER)];
    Strings::SETTINGS_THEME_CURRENT_GRAY = literals[static_cast<size_t>(LangStringId::SETTINGS_THEME_CURRENT_GRAY)];
    Strings::SETTINGS_THEME_CURRENT_SLASH = literals[static_cast<size_t>(LangStringId::SETTINGS_THEME_CURRENT_SLASH)];
    Strings::SETTINGS_THEME_CURRENT_WHITE = literals[static_cast<size_t>(LangStringId::SETTINGS_THEME_CURRENT_WHITE)];
    Strings::SETTINGS_THEME_GRAY = literals[static_cast<size_t>(LangStringId::SETTINGS_THEME_GRAY)];
    Strings::SETTINGS_THEME_HINT = literals[static_cast<size_t>(LangStringId::SETTINGS_THEME_HINT)];
    Strings::SETTINGS_THEME_SLASH = literals[static_cast<size_t>(LangStringId::SETTINGS_THEME_SLASH)];
    Strings::SETTINGS_THEME_TITLE = literals[static_cast<size_t>(LangStringId::SETTINGS_THEME_TITLE)];
    Strings::SETTINGS_THEME_WHITE = literals[static_cast<size_t>(LangStringId::SETTINGS_THEME_WHITE)];
    Strings::SPEAKING = literals[static_cast<size_t>(LangStringId::SPEAKING)];
    Strings::STANDBY = literals[static_cast<size_t>(LangStringId::STANDBY)];
    Strings::STANDBY_CONN_FAIL = literals[static_cast<size_t>(LangStringId::STANDBY_CONN_FAIL)];
    Strings::STANDBY_DATE_FMT = literals[static_cast<size_t>(LangStringId::STANDBY_DATE_FMT)];
    Strings::STANDBY_DATE_PLACEHOLDER = literals[static_cast<size_t>(LangStringId::STANDBY_DATE_PLACEHOLDER)];
    Strings::STANDBY_DECODE_FAIL = literals[static_cast<size_t>(LangStringId::STANDBY_DECODE_FAIL)];
    Strings::STANDBY_EMPTY_TODO = literals[static_cast<size_t>(LangStringId::STANDBY_EMPTY_TODO)];
    Strings::STANDBY_NO_DATA = literals[static_cast<size_t>(LangStringId::STANDBY_NO_DATA)];
    Strings::STANDBY_NO_NETWORK = literals[static_cast<size_t>(LangStringId::STANDBY_NO_NETWORK)];
    Strings::STANDBY_NO_TODO_CACHE = literals[static_cast<size_t>(LangStringId::STANDBY_NO_TODO_CACHE)];
    Strings::STANDBY_NO_WEATHER = literals[static_cast<size_t>(LangStringId::STANDBY_NO_WEATHER)];
    Strings::STANDBY_PARSE_FAIL = literals[static_cast<size_t>(LangStringId::STANDBY_PARSE_FAIL)];
    Strings::STANDBY_REQUEST_FAIL = literals[static_cast<size_t>(LangStringId::STANDBY_REQUEST_FAIL)];
    Strings::STANDBY_WEATHER_FAIL = literals[static_cast<size_t>(LangStringId::STANDBY_WEATHER_FAIL)];
    Strings::STANDBY_WP_NOT_FOUND = literals[static_cast<size_t>(LangStringId::STANDBY_WP_NOT_FOUND)];
    Strings::SWITCH_TO_4G_NETWORK = literals[static_cast<size_t>(LangStringId::SWITCH_TO_4G_NETWORK)];
    Strings::SWITCH_TO_WIFI_NETWORK = literals[static_cast<size_t>(LangStringId::SWITCH_TO_WIFI_NETWORK)];
    Strings::TASK_API_ERROR = literals[static_cast<size_t>(LangStringId::TASK_API_ERROR)];
    Strings::TASK_BATCH_COMPLETING = literals[static_cast<size_t>(LangStringId::TASK_BATCH_COMPLETING)];
    Strings::TASK_BATCH_FAIL = literals[static_cast<size_t>(LangStringId::TASK_BATCH_FAIL)];
    Strings::TASK_BATCH_REMOVING = literals[static_cast<size_t>(LangStringId::TASK_BATCH_REMOVING)];
    Strings::TASK_BATCH_RESULT_FMT = literals[static_cast<size_t>(LangStringId::TASK_BATCH_RESULT_FMT)];
    Strings::TASK_BATCH_START_FAIL = literals[static_cast<size_t>(LangStringId::TASK_BATCH_START_FAIL)];
    Strings::TASK_CHOOSE_ACTION = literals[static_cast<size_t>(LangStringId::TASK_CHOOSE_ACTION)];
    Strings::TASK_COMPLETE = literals[static_cast<size_t>(LangStringId::TASK_COMPLETE)];
    Strings::TASK_COMPLETED = literals[static_cast<size_t>(LangStringId::TASK_COMPLETED)];
    Strings::TASK_COMPLETED_N_FMT = literals[static_cast<size_t>(LangStringId::TASK_COMPLETED_N_FMT)];
    Strings::TASK_COMPLETE_FAIL = literals[static_cast<size_t>(LangStringId::TASK_COMPLETE_FAIL)];
    Strings::TASK_COMPLETE_START_FAIL = literals[static_cast<size_t>(LangStringId::TASK_COMPLETE_START_FAIL)];
    Strings::TASK_COMPLETE_TODO = literals[static_cast<size_t>(LangStringId::TASK_COMPLETE_TODO)];
    Strings::TASK_COMPLETING = literals[static_cast<size_t>(LangStringId::TASK_COMPLETING)];
    Strings::TASK_CONNECTING_NET = literals[static_cast<size_t>(LangStringId::TASK_CONNECTING_NET)];
    Strings::TASK_CONN_FAIL = literals[static_cast<size_t>(LangStringId::TASK_CONN_FAIL)];
    Strings::TASK_DATA_FORMAT_ERR = literals[static_cast<size_t>(LangStringId::TASK_DATA_FORMAT_ERR)];
    Strings::TASK_DELETED = literals[static_cast<size_t>(LangStringId::TASK_DELETED)];
    Strings::TASK_DELETE_FAIL = literals[static_cast<size_t>(LangStringId::TASK_DELETE_FAIL)];
    Strings::TASK_DELETE_START_FAIL = literals[static_cast<size_t>(LangStringId::TASK_DELETE_START_FAIL)];
    Strings::TASK_DELETE_TODO = literals[static_cast<size_t>(LangStringId::TASK_DELETE_TODO)];
    Strings::TASK_DELETING = literals[static_cast<size_t>(LangStringId::TASK_DELETING)];
    Strings::TASK_DONE_OK = literals[static_cast<size_t>(LangStringId::TASK_DONE_OK)];
    Strings::TASK_EMPTY_DONE = literals[static_cast<size_t>(LangStringId::TASK_EMPTY_DONE)];
    Strings::TASK_EMPTY_TODO = literals[static_cast<size_t>(LangStringId::TASK_EMPTY_TODO)];
    Strings::TASK_INVALID = literals[static_cast<size_t>(LangStringId::TASK_INVALID)];
    Strings::TASK_LOAD_FAIL = literals[static_cast<size_t>(LangStringId::TASK_LOAD_FAIL)];
    Strings::TASK_NEED_WIFI_CFG = literals[static_cast<size_t>(LangStringId::TASK_NEED_WIFI_CFG)];
    Strings::TASK_NET_NOT_READY = literals[static_cast<size_t>(LangStringId::TASK_NET_NOT_READY)];
    Strings::TASK_NO_COMPLETABLE = literals[static_cast<size_t>(LangStringId::TASK_NO_COMPLETABLE)];
    Strings::TASK_NO_NETWORK = literals[static_cast<size_t>(LangStringId::TASK_NO_NETWORK)];
    Strings::TASK_PARSE_FAIL = literals[static_cast<size_t>(LangStringId::TASK_PARSE_FAIL)];
    Strings::TASK_PLEASE_WAIT = literals[static_cast<size_t>(LangStringId::TASK_PLEASE_WAIT)];
    Strings::TASK_REFRESH = literals[static_cast<size_t>(LangStringId::TASK_REFRESH)];
    Strings::TASK_REFRESHED = literals[static_cast<size_t>(LangStringId::TASK_REFRESHED)];
    Strings::TASK_REFRESHING = literals[static_cast<size_t>(LangStringId::TASK_REFRESHING)];
    Strings::TASK_REFRESH_START_FAIL = literals[static_cast<size_t>(LangStringId::TASK_REFRESH_START_FAIL)];
    Strings::TASK_REMOVED_N_FMT = literals[static_cast<size_t>(LangStringId::TASK_REMOVED_N_FMT)];
    Strings::TASK_REQUEST_FAIL = literals[static_cast<size_t>(LangStringId::TASK_REQUEST_FAIL)];
    Strings::TASK_SELECTED_FMT = literals[static_cast<size_t>(LangStringId::TASK_SELECTED_FMT)];
    Strings::TASK_SELECT_FIRST = literals[static_cast<size_t>(LangStringId::TASK_SELECT_FIRST)];
    Strings::TASK_TAB_DONE = literals[static_cast<size_t>(LangStringId::TASK_TAB_DONE)];
    Strings::TASK_TAB_TODO = literals[static_cast<size_t>(LangStringId::TASK_TAB_TODO)];
    Strings::TASK_TITLE_DONE = literals[static_cast<size_t>(LangStringId::TASK_TITLE_DONE)];
    Strings::TASK_TITLE_TODO = literals[static_cast<size_t>(LangStringId::TASK_TITLE_TODO)];
    Strings::TOUCH_MISSING_HINT = literals[static_cast<size_t>(LangStringId::TOUCH_MISSING_HINT)];
    Strings::TRANSLATE_AUDIO_BUSY = literals[static_cast<size_t>(LangStringId::TRANSLATE_AUDIO_BUSY)];
    Strings::TRANSLATE_CANCELLED = literals[static_cast<size_t>(LangStringId::TRANSLATE_CANCELLED)];
    Strings::TRANSLATE_CONNECTING = literals[static_cast<size_t>(LangStringId::TRANSLATE_CONNECTING)];
    Strings::TRANSLATE_CONNECT_FAIL = literals[static_cast<size_t>(LangStringId::TRANSLATE_CONNECT_FAIL)];
    Strings::TRANSLATE_CONNECT_NET = literals[static_cast<size_t>(LangStringId::TRANSLATE_CONNECT_NET)];
    Strings::TRANSLATE_ERROR = literals[static_cast<size_t>(LangStringId::TRANSLATE_ERROR)];
    Strings::TRANSLATE_GET_TOKEN = literals[static_cast<size_t>(LangStringId::TRANSLATE_GET_TOKEN)];
    Strings::TRANSLATE_GET_TOKEN_FAIL = literals[static_cast<size_t>(LangStringId::TRANSLATE_GET_TOKEN_FAIL)];
    Strings::TRANSLATE_HINT = literals[static_cast<size_t>(LangStringId::TRANSLATE_HINT)];
    Strings::TRANSLATE_LANG_INVALID = literals[static_cast<size_t>(LangStringId::TRANSLATE_LANG_INVALID)];
    Strings::TRANSLATE_LANG_SAME = literals[static_cast<size_t>(LangStringId::TRANSLATE_LANG_SAME)];
    Strings::TRANSLATE_LIVE = literals[static_cast<size_t>(LangStringId::TRANSLATE_LIVE)];
    Strings::TRANSLATE_NEED_WIFI_CFG = literals[static_cast<size_t>(LangStringId::TRANSLATE_NEED_WIFI_CFG)];
    Strings::TRANSLATE_NET_CONNECTING = literals[static_cast<size_t>(LangStringId::TRANSLATE_NET_CONNECTING)];
    Strings::TRANSLATE_NET_NOT_READY = literals[static_cast<size_t>(LangStringId::TRANSLATE_NET_NOT_READY)];
    Strings::TRANSLATE_PICK_FROM = literals[static_cast<size_t>(LangStringId::TRANSLATE_PICK_FROM)];
    Strings::TRANSLATE_PICK_LANG = literals[static_cast<size_t>(LangStringId::TRANSLATE_PICK_LANG)];
    Strings::TRANSLATE_PICK_TO = literals[static_cast<size_t>(LangStringId::TRANSLATE_PICK_TO)];
    Strings::TRANSLATE_READY_TIMEOUT = literals[static_cast<size_t>(LangStringId::TRANSLATE_READY_TIMEOUT)];
    Strings::TRANSLATE_SOURCE_PLACEHOLDER = literals[static_cast<size_t>(LangStringId::TRANSLATE_SOURCE_PLACEHOLDER)];
    Strings::TRANSLATE_SOURCE_TITLE = literals[static_cast<size_t>(LangStringId::TRANSLATE_SOURCE_TITLE)];
    Strings::TRANSLATE_START_FAIL = literals[static_cast<size_t>(LangStringId::TRANSLATE_START_FAIL)];
    Strings::TRANSLATE_STOP = literals[static_cast<size_t>(LangStringId::TRANSLATE_STOP)];
    Strings::TRANSLATE_STOPPED = literals[static_cast<size_t>(LangStringId::TRANSLATE_STOPPED)];
    Strings::TRANSLATE_STOPPING = literals[static_cast<size_t>(LangStringId::TRANSLATE_STOPPING)];
    Strings::TRANSLATE_TRANSLATING = literals[static_cast<size_t>(LangStringId::TRANSLATE_TRANSLATING)];
    Strings::TRANSLATE_TRANS_PLACEHOLDER = literals[static_cast<size_t>(LangStringId::TRANSLATE_TRANS_PLACEHOLDER)];
    Strings::TRANSLATE_TRANS_TITLE = literals[static_cast<size_t>(LangStringId::TRANSLATE_TRANS_TITLE)];
    Strings::TRANSLATE_TRY_LATER = literals[static_cast<size_t>(LangStringId::TRANSLATE_TRY_LATER)];
    Strings::UPGRADE_FAILED = literals[static_cast<size_t>(LangStringId::UPGRADE_FAILED)];
    Strings::UPGRADING = literals[static_cast<size_t>(LangStringId::UPGRADING)];
    Strings::VERSION = literals[static_cast<size_t>(LangStringId::VERSION)];
    Strings::VOICE_STARTING_NET = literals[static_cast<size_t>(LangStringId::VOICE_STARTING_NET)];
    Strings::VOLUME = literals[static_cast<size_t>(LangStringId::VOLUME)];
    Strings::WALLPAPER_CHIP_SHUTDOWN = literals[static_cast<size_t>(LangStringId::WALLPAPER_CHIP_SHUTDOWN)];
    Strings::WALLPAPER_CHIP_STANDBY = literals[static_cast<size_t>(LangStringId::WALLPAPER_CHIP_STANDBY)];
    Strings::WALLPAPER_DECODE_FAIL = literals[static_cast<size_t>(LangStringId::WALLPAPER_DECODE_FAIL)];
    Strings::WALLPAPER_DELETE_BTN = literals[static_cast<size_t>(LangStringId::WALLPAPER_DELETE_BTN)];
    Strings::WALLPAPER_DELETE_FAIL = literals[static_cast<size_t>(LangStringId::WALLPAPER_DELETE_FAIL)];
    Strings::WALLPAPER_DELETE_START_FAIL = literals[static_cast<size_t>(LangStringId::WALLPAPER_DELETE_START_FAIL)];
    Strings::WALLPAPER_DELETING = literals[static_cast<size_t>(LangStringId::WALLPAPER_DELETING)];
    Strings::WALLPAPER_EMPTY_FMT = literals[static_cast<size_t>(LangStringId::WALLPAPER_EMPTY_FMT)];
    Strings::WALLPAPER_ENABLE_FAIL = literals[static_cast<size_t>(LangStringId::WALLPAPER_ENABLE_FAIL)];
    Strings::WALLPAPER_FILE_INVALID = literals[static_cast<size_t>(LangStringId::WALLPAPER_FILE_INVALID)];
    Strings::WALLPAPER_FILE_MISSING = literals[static_cast<size_t>(LangStringId::WALLPAPER_FILE_MISSING)];
    Strings::WALLPAPER_LOADING = literals[static_cast<size_t>(LangStringId::WALLPAPER_LOADING)];
    Strings::WALLPAPER_NAME_INVALID = literals[static_cast<size_t>(LangStringId::WALLPAPER_NAME_INVALID)];
    Strings::WALLPAPER_NO_SD = literals[static_cast<size_t>(LangStringId::WALLPAPER_NO_SD)];
    Strings::WALLPAPER_PATH_INVALID = literals[static_cast<size_t>(LangStringId::WALLPAPER_PATH_INVALID)];
    Strings::WALLPAPER_PREVIEW_FAIL = literals[static_cast<size_t>(LangStringId::WALLPAPER_PREVIEW_FAIL)];
    Strings::WALLPAPER_PREVIEW_OOM = literals[static_cast<size_t>(LangStringId::WALLPAPER_PREVIEW_OOM)];
    Strings::WALLPAPER_PREVIEW_START_FAIL = literals[static_cast<size_t>(LangStringId::WALLPAPER_PREVIEW_START_FAIL)];
    Strings::WALLPAPER_SELECTED_FMT = literals[static_cast<size_t>(LangStringId::WALLPAPER_SELECTED_FMT)];
    Strings::WALLPAPER_SET_SHUTDOWN = literals[static_cast<size_t>(LangStringId::WALLPAPER_SET_SHUTDOWN)];
    Strings::WALLPAPER_SET_STANDBY = literals[static_cast<size_t>(LangStringId::WALLPAPER_SET_STANDBY)];
    Strings::WALLPAPER_SHUTDOWN_ON = literals[static_cast<size_t>(LangStringId::WALLPAPER_SHUTDOWN_ON)];
    Strings::WALLPAPER_STANDBY_ON = literals[static_cast<size_t>(LangStringId::WALLPAPER_STANDBY_ON)];
    Strings::WARNING = literals[static_cast<size_t>(LangStringId::WARNING)];
    Strings::WIFI_CONFIG_MODE = literals[static_cast<size_t>(LangStringId::WIFI_CONFIG_MODE)];
}

const char* const* LiteralsFor(const char* code) {
    if (code != nullptr && std::strcmp(code, "zh-CN") == 0) {
        return kLiterals_zh_cn;
    }
    return kLiterals_en_us;
}

const char* NormalizeCode(const char* code) {
    if (code != nullptr && std::strcmp(code, "zh-CN") == 0) {
        return "zh-CN";
    }
    return "en-US";
}

}  // namespace

const char* CODE = LANG_DEFAULT_CODE;

const char* Strings::ACCESS_VIA_BROWSER = "";
const char* Strings::ACTIVATION = "";
const char* Strings::ACTIVATION_CODE_FMT = "";
const char* Strings::BATTERY_CHARGING = "";
const char* Strings::BATTERY_FULL = "";
const char* Strings::BATTERY_LOW = "";
const char* Strings::BATTERY_NEED_CHARGE = "";
const char* Strings::BOOK_BAD_ARG = "";
const char* Strings::BOOK_BLANK = "";
const char* Strings::BOOK_BODY = "";
const char* Strings::BOOK_CANCELLED = "";
const char* Strings::BOOK_CHAPTER_FMT = "";
const char* Strings::BOOK_CHAPTER_NO_TEXT = "";
const char* Strings::BOOK_CHAPTER_PROGRESS = "";
const char* Strings::BOOK_CHECKSUM_FAIL = "";
const char* Strings::BOOK_CONN_FAIL = "";
const char* Strings::BOOK_CONTINUE = "";
const char* Strings::BOOK_COUNT_FMT = "";
const char* Strings::BOOK_COVER_TOO_LARGE = "";
const char* Strings::BOOK_DELETE_FAILED = "";
const char* Strings::BOOK_DETAIL = "";
const char* Strings::BOOK_DOWNLOAD_FAIL = "";
const char* Strings::BOOK_DURATION_HM_FMT = "";
const char* Strings::BOOK_DURATION_H_FMT = "";
const char* Strings::BOOK_DURATION_LT1MIN_FMT = "";
const char* Strings::BOOK_DURATION_M_FMT = "";
const char* Strings::BOOK_EMPTY_COVER = "";
const char* Strings::BOOK_EMPTY_FILE = "";
const char* Strings::BOOK_EMPTY_HOME_FMT = "";
const char* Strings::BOOK_EMPTY_SHELF_FMT = "";
const char* Strings::BOOK_FILE_TOO_LARGE = "";
const char* Strings::BOOK_FINISHED = "";
const char* Strings::BOOK_FONT_EMPTY = "";
const char* Strings::BOOK_FONT_NAME_INVALID = "";
const char* Strings::BOOK_FONT_TITLE = "";
const char* Strings::BOOK_IMAGE_PLACEHOLDER = "";
const char* Strings::BOOK_LAYOUT_BUSY = "";
const char* Strings::BOOK_LAYOUT_DONE = "";
const char* Strings::BOOK_LINE_GAP = "";
const char* Strings::BOOK_MARGIN = "";
const char* Strings::BOOK_MARGIN_NARROW = "";
const char* Strings::BOOK_MARGIN_STANDARD = "";
const char* Strings::BOOK_MARGIN_VERY_WIDE = "";
const char* Strings::BOOK_MARGIN_WIDE = "";
const char* Strings::BOOK_NET_NOT_READY = "";
const char* Strings::BOOK_NO_CONTINUE = "";
const char* Strings::BOOK_NO_COVER_URL = "";
const char* Strings::BOOK_NO_DOWNLOAD_URL = "";
const char* Strings::BOOK_NO_LOCAL_FILE = "";
const char* Strings::BOOK_NO_NETWORK = "";
const char* Strings::BOOK_NO_RECENT = "";
const char* Strings::BOOK_NO_SD = "";
const char* Strings::BOOK_NO_SD_SHORT = "";
const char* Strings::BOOK_OPENING = "";
const char* Strings::BOOK_OPEN_FAILED = "";
const char* Strings::BOOK_OPEN_FAILED_CHAPTER = "";
const char* Strings::BOOK_OPEN_FAILED_EPUB = "";
const char* Strings::BOOK_OPEN_FAILED_TASK = "";
const char* Strings::BOOK_OPEN_FAILED_TXT = "";
const char* Strings::BOOK_OUT_OF_MEMORY = "";
const char* Strings::BOOK_PAGE_LOAD_FAILED = "";
const char* Strings::BOOK_PATH_INVALID = "";
const char* Strings::BOOK_READ_DURATION = "";
const char* Strings::BOOK_READ_FAIL = "";
const char* Strings::BOOK_READ_FILE_FAIL = "";
const char* Strings::BOOK_READ_PCT_FMT = "";
const char* Strings::BOOK_READ_PROGRESS = "";
const char* Strings::BOOK_RECENT = "";
const char* Strings::BOOK_SAVE_FAIL = "";
const char* Strings::BOOK_SELECTED_FMT = "";
const char* Strings::BOOK_SHELF_TITLE = "";
const char* Strings::BOOK_SPACING_COMPACT = "";
const char* Strings::BOOK_SPACING_RELAXED = "";
const char* Strings::BOOK_SPACING_STANDARD = "";
const char* Strings::BOOK_SPACING_VERY_RELAXED = "";
const char* Strings::BOOK_START = "";
const char* Strings::BOOK_STORAGE_UNAVAIL = "";
const char* Strings::BOOK_SYNC_EMPTY_BODY = "";
const char* Strings::BOOK_TOC = "";
const char* Strings::BOOK_TOC_EMPTY = "";
const char* Strings::BOOK_TOC_PAGE_FMT = "";
const char* Strings::BOOK_TODAY_DURATION = "";
const char* Strings::BOOK_UNDERLINE = "";
const char* Strings::BOOK_UNDERLINE_DASHED = "";
const char* Strings::BOOK_UNDERLINE_SOLID = "";
const char* Strings::BOOK_WRITE_FAIL = "";
const char* Strings::BT_CALL_BTN = "";
const char* Strings::BT_CALL_MODE_SCO = "";
const char* Strings::BT_CONNECTING = "";
const char* Strings::BT_CONNECTING_FMT = "";
const char* Strings::BT_CONNECT_OK = "";
const char* Strings::BT_CONNECT_TIMEOUT = "";
const char* Strings::BT_DESC = "";
const char* Strings::BT_FOUND_FMT = "";
const char* Strings::BT_MODE1 = "";
const char* Strings::BT_MODE1_ACTIVE = "";
const char* Strings::BT_MODE1_SET = "";
const char* Strings::BT_MODE2 = "";
const char* Strings::BT_MODE2_SET = "";
const char* Strings::BT_MODE3 = "";
const char* Strings::BT_MODE3_SET = "";
const char* Strings::BT_MUSIC_BTN = "";
const char* Strings::BT_MUSIC_MODE_SCO = "";
const char* Strings::BT_NEED_CONNECT = "";
const char* Strings::BT_NEED_MODE2 = "";
const char* Strings::BT_PWR_RESET_OK = "";
const char* Strings::BT_PWR_RESET_UNSUP = "";
const char* Strings::BT_RESET_BTN = "";
const char* Strings::BT_RESET_HINT = "";
const char* Strings::BT_SCANNING = "";
const char* Strings::BT_SCAN_BTN = "";
const char* Strings::BT_SCAN_DONE_FMT = "";
const char* Strings::BT_SCAN_START = "";
const char* Strings::BT_SELECT_MODE = "";
const char* Strings::BT_SWITCH_CALL = "";
const char* Strings::BT_SWITCH_MODE1 = "";
const char* Strings::BT_SWITCH_MODE2 = "";
const char* Strings::BT_SWITCH_MODE3 = "";
const char* Strings::BT_SWITCH_MUSIC = "";
const char* Strings::BT_TITLE = "";
const char* Strings::BT_UART_NOT_INIT = "";
const char* Strings::CHECKING_NEW_VERSION = "";
const char* Strings::CHECK_NEW_VERSION_FAILED = "";
const char* Strings::CLOUD_API_ERROR = "";
const char* Strings::CLOUD_CANCELLED = "";
const char* Strings::CLOUD_CANCELLING = "";
const char* Strings::CLOUD_CONFIRM_DL_FMT = "";
const char* Strings::CLOUD_CONNECTING_NET = "";
const char* Strings::CLOUD_CONNECT_NET = "";
const char* Strings::CLOUD_CONN_FAIL = "";
const char* Strings::CLOUD_COVER_FAIL = "";
const char* Strings::CLOUD_DATA_FORMAT_ERR = "";
const char* Strings::CLOUD_DECODE_FAIL = "";
const char* Strings::CLOUD_DELETE_FAIL = "";
const char* Strings::CLOUD_DOWNLOAD = "";
const char* Strings::CLOUD_DOWNLOADING = "";
const char* Strings::CLOUD_DOWNLOAD_FAIL = "";
const char* Strings::CLOUD_EMPTY_ALL = "";
const char* Strings::CLOUD_EMPTY_BOOK = "";
const char* Strings::CLOUD_EMPTY_FONT = "";
const char* Strings::CLOUD_EMPTY_WALLPAPER = "";
const char* Strings::CLOUD_FETCHING_LIST = "";
const char* Strings::CLOUD_FILE_INVALID = "";
const char* Strings::CLOUD_IN_QUEUE = "";
const char* Strings::CLOUD_JSON_CREATE_FAIL = "";
const char* Strings::CLOUD_JSON_PARSE_FAIL = "";
const char* Strings::CLOUD_JSON_SERIALIZE_FAIL = "";
const char* Strings::CLOUD_LIST_TOO_LARGE = "";
const char* Strings::CLOUD_LOADING = "";
const char* Strings::CLOUD_LOAD_COVER = "";
const char* Strings::CLOUD_LOAD_FAIL = "";
const char* Strings::CLOUD_MISSING_TASK_ID = "";
const char* Strings::CLOUD_NEED_WIFI_CFG = "";
const char* Strings::CLOUD_NET_NOT_READY = "";
const char* Strings::CLOUD_NO_COVER = "";
const char* Strings::CLOUD_NO_DOWNLOAD_URL = "";
const char* Strings::CLOUD_NO_LOCAL_FILE = "";
const char* Strings::CLOUD_NO_NETWORK = "";
const char* Strings::CLOUD_NO_SD = "";
const char* Strings::CLOUD_PATH_INVALID = "";
const char* Strings::CLOUD_PENDING = "";
const char* Strings::CLOUD_PLEASE_WAIT = "";
const char* Strings::CLOUD_PREVIEW_FAIL = "";
const char* Strings::CLOUD_PREVIEW_START_FAIL = "";
const char* Strings::CLOUD_PREVIEW_URL_LONG = "";
const char* Strings::CLOUD_PUSH_BOOK = "";
const char* Strings::CLOUD_PUSH_FONT = "";
const char* Strings::CLOUD_PUSH_WALLPAPER = "";
const char* Strings::CLOUD_READ_TIMEOUT = "";
const char* Strings::CLOUD_REFRESH = "";
const char* Strings::CLOUD_REFRESHED = "";
const char* Strings::CLOUD_REFRESHING = "";
const char* Strings::CLOUD_REQUEST_FAIL = "";
const char* Strings::CLOUD_SAVE = "";
const char* Strings::CLOUD_SAVED_0_1 = "";
const char* Strings::CLOUD_SAVED_FMT = "";
const char* Strings::CLOUD_SAVED_SYNC_FAIL = "";
const char* Strings::CLOUD_SAVE_FAIL = "";
const char* Strings::CLOUD_SAVE_LOCAL = "";
const char* Strings::CLOUD_SAVING = "";
const char* Strings::CLOUD_SAVING_NAME_FMT = "";
const char* Strings::CLOUD_SELECTED_FMT = "";
const char* Strings::CLOUD_SELECT_FIRST = "";
const char* Strings::CLOUD_STORAGE_UNAVAIL = "";
const char* Strings::CLOUD_SYNC_FAIL = "";
const char* Strings::CLOUD_TAB_ALL = "";
const char* Strings::CLOUD_TAB_BOOK = "";
const char* Strings::CLOUD_TAB_FONT = "";
const char* Strings::CLOUD_TAB_WALLPAPER = "";
const char* Strings::CLOUD_TASK_FAIL = "";
const char* Strings::CLOUD_TYPE_BOOK = "";
const char* Strings::CLOUD_TYPE_FONT = "";
const char* Strings::CLOUD_TYPE_WALLPAPER = "";
const char* Strings::CLOUD_UNKNOWN_TYPE = "";
const char* Strings::CLOUD_WAIT_DOWNLOAD = "";
const char* Strings::COMMON_CANCEL = "";
const char* Strings::COMMON_DELETE = "";
const char* Strings::COMMON_EMPTY = "";
const char* Strings::COMMON_FAILED = "";
const char* Strings::COMMON_LOADING = "";
const char* Strings::COMMON_OFF = "";
const char* Strings::COMMON_OK = "";
const char* Strings::COMMON_ON = "";
const char* Strings::COMMON_REMOVE = "";
const char* Strings::COMMON_RETRY = "";
const char* Strings::COMMON_SELECT_ALL = "";
const char* Strings::COMMON_SUCCESS = "";
const char* Strings::COMMON_UNKNOWN = "";
const char* Strings::CONNECTED_TO = "";
const char* Strings::CONNECTING = "";
const char* Strings::CONNECTION_SUCCESSFUL = "";
const char* Strings::CONNECT_TO = "";
const char* Strings::CONNECT_TO_HOTSPOT = "";
const char* Strings::DETECTING_MODULE = "";
const char* Strings::DOWNLOAD_ASSETS_FAILED = "";
const char* Strings::ENTERING_WIFI_CONFIG_MODE = "";
const char* Strings::ERROR = "";
const char* Strings::FOUND_NEW_ASSETS = "";
const char* Strings::HELLO_MY_FRIEND = "";
const char* Strings::HOME_APP_ASSISTANT = "";
const char* Strings::HOME_APP_BOOK = "";
const char* Strings::HOME_APP_CLOUD = "";
const char* Strings::HOME_APP_SETTINGS = "";
const char* Strings::HOME_APP_TASK = "";
const char* Strings::HOME_APP_WALLPAPER = "";
const char* Strings::HOME_DATE_FMT = "";
const char* Strings::HOME_DATE_PLACEHOLDER = "";
const char* Strings::HOME_DATE_SLASH_FMT = "";
const char* Strings::HOME_DATE_SLASH_PLACEHOLDER = "";
const char* Strings::HOME_WDAY_FRI = "";
const char* Strings::HOME_WDAY_MON = "";
const char* Strings::HOME_WDAY_SAT = "";
const char* Strings::HOME_WDAY_SUN = "";
const char* Strings::HOME_WDAY_THU = "";
const char* Strings::HOME_WDAY_TUE = "";
const char* Strings::HOME_WDAY_WED = "";
const char* Strings::INFO = "";
const char* Strings::INITIALIZING = "";
const char* Strings::LISTENING = "";
const char* Strings::LOADING_ASSETS = "";
const char* Strings::LOADING_PROTOCOL = "";
const char* Strings::MAX_VOLUME = "";
const char* Strings::MUTED = "";
const char* Strings::NEED_WIFI_CFG = "";
const char* Strings::NETWORK_AUTH_OPEN = "";
const char* Strings::NETWORK_AUTH_SECURE = "";
const char* Strings::NETWORK_BUSY_CONNECT = "";
const char* Strings::NETWORK_CANCEL = "";
const char* Strings::NETWORK_CLEARED_OK = "";
const char* Strings::NETWORK_CLEAR_ALL = "";
const char* Strings::NETWORK_CONNECT = "";
const char* Strings::NETWORK_CONNECTED_FMT = "";
const char* Strings::NETWORK_CONNECTING_FMT = "";
const char* Strings::NETWORK_CONNECT_FAIL = "";
const char* Strings::NETWORK_CONNECT_TASK_FAIL = "";
const char* Strings::NETWORK_CONNECT_TIMEOUT = "";
const char* Strings::NETWORK_CONNECT_TIMEOUT_HINT = "";
const char* Strings::NETWORK_CONNECT_TO_FMT = "";
const char* Strings::NETWORK_DEFAULT_FMT = "";
const char* Strings::NETWORK_DELETE = "";
const char* Strings::NETWORK_DELETED_OK = "";
const char* Strings::NETWORK_ERR_AP_GONE = "";
const char* Strings::NETWORK_ERR_ASSOC = "";
const char* Strings::NETWORK_ERR_BAD_PASSWORD = "";
const char* Strings::NETWORK_ERR_REASON_FMT = "";
const char* Strings::NETWORK_ERR_WEAK = "";
const char* Strings::NETWORK_NEARBY_EMPTY = "";
const char* Strings::NETWORK_PWD_HINT = "";
const char* Strings::NETWORK_PWD_PLACEHOLDER = "";
const char* Strings::NETWORK_PWD_TOO_LONG = "";
const char* Strings::NETWORK_SAVED_EMPTY = "";
const char* Strings::NETWORK_SCAN = "";
const char* Strings::NETWORK_SCANNING = "";
const char* Strings::NETWORK_SCAN_DONE_FMT = "";
const char* Strings::NETWORK_SCAN_FAIL = "";
const char* Strings::NETWORK_SCAN_TASK_FAIL = "";
const char* Strings::NETWORK_SCAN_TIMEOUT = "";
const char* Strings::NETWORK_SET_DEFAULT = "";
const char* Strings::NETWORK_SET_DEFAULT_OK = "";
const char* Strings::NETWORK_SHOW_PWD = "";
const char* Strings::NETWORK_SSID_INVALID = "";
const char* Strings::NETWORK_TAB_NEARBY = "";
const char* Strings::NETWORK_TAB_SAVED = "";
const char* Strings::NETWORK_TITLE = "";
const char* Strings::NETWORK_WIFI_INIT = "";
const char* Strings::NETWORK_WIFI_INIT_FAIL = "";
const char* Strings::NEW_VERSION = "";
const char* Strings::OTA_ALREADY_LATEST = "";
const char* Strings::OTA_CHECK_FAILED = "";
const char* Strings::OTA_CONFIRM_FMT = "";
const char* Strings::OTA_CUR_VER_FMT = "";
const char* Strings::OTA_HINT = "";
const char* Strings::OTA_IGNORE_VERSION = "";
const char* Strings::OTA_MANUAL = "";
const char* Strings::OTA_NEW_VER_FMT = "";
const char* Strings::OTA_REMIND_LATER = "";
const char* Strings::OTA_SUCCESS_REBOOT = "";
const char* Strings::OTA_TITLE = "";
const char* Strings::OTA_UPGRADE = "";
const char* Strings::OTA_UPGRADE_NOW = "";
const char* Strings::PHONE_BUSY = "";
const char* Strings::PHONE_CHECKING_NET = "";
const char* Strings::PHONE_CHECK_CELL = "";
const char* Strings::PHONE_CONFIRM_SIM = "";
const char* Strings::PHONE_DIAL = "";
const char* Strings::PHONE_DIAL_FAIL = "";
const char* Strings::PHONE_HANGUP = "";
const char* Strings::PHONE_INTERNAL_SIM = "";
const char* Strings::PHONE_IN_CALL = "";
const char* Strings::PHONE_NO_4G = "";
const char* Strings::PHONE_WIFI_BLOCK = "";
const char* Strings::PIN_ERROR = "";
const char* Strings::PLEASE_WAIT = "";
const char* Strings::POWERED_OFF = "";
const char* Strings::RECORD_ASR_DONE = "";
const char* Strings::RECORD_ASR_EMPTY = "";
const char* Strings::RECORD_ASR_FAIL = "";
const char* Strings::RECORD_ASR_HINT = "";
const char* Strings::RECORD_ASR_HTTP_FMT = "";
const char* Strings::RECORD_ASR_NEED_NET = "";
const char* Strings::RECORD_ASR_PARSE_FAIL = "";
const char* Strings::RECORD_ASR_PENDING = "";
const char* Strings::RECORD_ASR_REQ_FAIL = "";
const char* Strings::RECORD_ASR_START_FAIL = "";
const char* Strings::RECORD_ASR_UPLOADED_HINT = "";
const char* Strings::RECORD_AUDIO_BUSY = "";
const char* Strings::RECORD_AUDIO_NOT_READY = "";
const char* Strings::RECORD_AUDIO_STARTING = "";
const char* Strings::RECORD_BTN_ASR = "";
const char* Strings::RECORD_BTN_PLAY = "";
const char* Strings::RECORD_BTN_SAVING = "";
const char* Strings::RECORD_BTN_START = "";
const char* Strings::RECORD_BTN_STOP = "";
const char* Strings::RECORD_BTN_STOP_PLAY = "";
const char* Strings::RECORD_DELETED = "";
const char* Strings::RECORD_DELETE_FAIL = "";
const char* Strings::RECORD_DURATION_SEC_FMT = "";
const char* Strings::RECORD_EMPTY = "";
const char* Strings::RECORD_FILE_CORRUPT = "";
const char* Strings::RECORD_FILE_TOO_LARGE = "";
const char* Strings::RECORD_HINT_START = "";
const char* Strings::RECORD_INSERT_SD = "";
const char* Strings::RECORD_MAX_MIN_FMT = "";
const char* Strings::RECORD_META_FMT = "";
const char* Strings::RECORD_MKDIR_FAIL = "";
const char* Strings::RECORD_NET_UNAVAIL = "";
const char* Strings::RECORD_NO_SD_HINT = "";
const char* Strings::RECORD_OOM = "";
const char* Strings::RECORD_OPEN_FAIL = "";
const char* Strings::RECORD_PLAYING = "";
const char* Strings::RECORD_PLAY_END = "";
const char* Strings::RECORD_PLAY_START_FAIL = "";
const char* Strings::RECORD_PLAY_STOPPED = "";
const char* Strings::RECORD_PLEASE_WAIT = "";
const char* Strings::RECORD_READ_FAIL = "";
const char* Strings::RECORD_RECORDING = "";
const char* Strings::RECORD_SAVED_SD = "";
const char* Strings::RECORD_STOP_FIRST = "";
const char* Strings::RECORD_SUMMARY = "";
const char* Strings::RECORD_TAB_LIST = "";
const char* Strings::RECORD_TAB_REC = "";
const char* Strings::RECORD_TASK_START_FAIL = "";
const char* Strings::RECORD_TOO_SHORT = "";
const char* Strings::RECORD_UPLOADING = "";
const char* Strings::RECORD_WRITE_FAIL = "";
const char* Strings::REGISTERING_NETWORK = "";
const char* Strings::REG_ERROR = "";
const char* Strings::RTC_MODE_OFF = "";
const char* Strings::RTC_MODE_ON = "";
const char* Strings::SCANNING_WIFI = "";
const char* Strings::SERVER_ERROR = "";
const char* Strings::SERVER_NOT_CONNECTED = "";
const char* Strings::SERVER_NOT_FOUND = "";
const char* Strings::SERVER_TIMEOUT = "";
const char* Strings::SETTINGS_ABOUT_BUILD = "";
const char* Strings::SETTINGS_ABOUT_CHIP = "";
const char* Strings::SETTINGS_ABOUT_CORES = "";
const char* Strings::SETTINGS_ABOUT_CORES_FMT = "";
const char* Strings::SETTINGS_ABOUT_FLASH = "";
const char* Strings::SETTINGS_ABOUT_FW = "";
const char* Strings::SETTINGS_ABOUT_FW_CHECK = "";
const char* Strings::SETTINGS_ABOUT_FW_VER_FMT = "";
const char* Strings::SETTINGS_ABOUT_MAC = "";
const char* Strings::SETTINGS_ABOUT_MODEL = "";
const char* Strings::SETTINGS_ABOUT_NONE = "";
const char* Strings::SETTINGS_ABOUT_PSRAM = "";
const char* Strings::SETTINGS_CONV_BUSY = "";
const char* Strings::SETTINGS_CONV_CONNECTING = "";
const char* Strings::SETTINGS_CONV_CONN_FAIL = "";
const char* Strings::SETTINGS_CONV_CUR_DASH = "";
const char* Strings::SETTINGS_CONV_CUR_OFF = "";
const char* Strings::SETTINGS_CONV_CUR_ON = "";
const char* Strings::SETTINGS_CONV_DISABLING = "";
const char* Strings::SETTINGS_CONV_ENABLING = "";
const char* Strings::SETTINGS_CONV_HINT = "";
const char* Strings::SETTINGS_CONV_MISSING_ENABLED = "";
const char* Strings::SETTINGS_CONV_NET_NOT_READY = "";
const char* Strings::SETTINGS_CONV_NO_NETWORK = "";
const char* Strings::SETTINGS_CONV_PARSE_FAIL = "";
const char* Strings::SETTINGS_CONV_REQUEST_FAIL = "";
const char* Strings::SETTINGS_CONV_TITLE = "";
const char* Strings::SETTINGS_CONV_TTS_OFF = "";
const char* Strings::SETTINGS_CONV_TTS_ON = "";
const char* Strings::SETTINGS_HAPTIC_CURRENT_OFF = "";
const char* Strings::SETTINGS_HAPTIC_CURRENT_ON = "";
const char* Strings::SETTINGS_HAPTIC_HINT = "";
const char* Strings::SETTINGS_HAPTIC_OFF = "";
const char* Strings::SETTINGS_HAPTIC_ON = "";
const char* Strings::SETTINGS_HAPTIC_TITLE = "";
const char* Strings::SETTINGS_LANG_CURRENT_EN = "";
const char* Strings::SETTINGS_LANG_CURRENT_ZH = "";
const char* Strings::SETTINGS_LANG_EN_US = "";
const char* Strings::SETTINGS_LANG_HINT = "";
const char* Strings::SETTINGS_LANG_TITLE = "";
const char* Strings::SETTINGS_LANG_ZH_CN = "";
const char* Strings::SETTINGS_NET_AP_TASK_FAIL = "";
const char* Strings::SETTINGS_NET_BOARD_NO_AP = "";
const char* Strings::SETTINGS_NET_CFUN0 = "";
const char* Strings::SETTINGS_NET_CFUN0_FAIL = "";
const char* Strings::SETTINGS_NET_CFUN1 = "";
const char* Strings::SETTINGS_NET_CURRENT_FMT = "";
const char* Strings::SETTINGS_NET_CUR_4G = "";
const char* Strings::SETTINGS_NET_CUR_DASH = "";
const char* Strings::SETTINGS_NET_CUR_WIFI = "";
const char* Strings::SETTINGS_NET_ECSIMCFG_FAIL = "";
const char* Strings::SETTINGS_NET_ENTER_AP = "";
const char* Strings::SETTINGS_NET_ENTER_CFG = "";
const char* Strings::SETTINGS_NET_HOTSPOT_NAME = "";
const char* Strings::SETTINGS_NET_MODE_4G = "";
const char* Strings::SETTINGS_NET_MODE_TITLE = "";
const char* Strings::SETTINGS_NET_MODE_WIFI = "";
const char* Strings::SETTINGS_NET_NOT_4G = "";
const char* Strings::SETTINGS_NET_NO_4G = "";
const char* Strings::SETTINGS_NET_REBOOTING = "";
const char* Strings::SETTINGS_NET_REBOOT_COUNT_FMT = "";
const char* Strings::SETTINGS_NET_REBOOT_HINT = "";
const char* Strings::SETTINGS_NET_SIM_EXT = "";
const char* Strings::SETTINGS_NET_SIM_INT = "";
const char* Strings::SETTINGS_NET_SIM_TASK_FAIL = "";
const char* Strings::SETTINGS_NET_SIM_TITLE = "";
const char* Strings::SETTINGS_NET_SWITCHED_FMT = "";
const char* Strings::SETTINGS_NET_SWITCH_4G = "";
const char* Strings::SETTINGS_NET_SWITCH_SIM_CFUN0_FMT = "";
const char* Strings::SETTINGS_NET_SWITCH_SIM_FMT = "";
const char* Strings::SETTINGS_NET_SWITCH_WIFI = "";
const char* Strings::SETTINGS_NET_WIFI_CFG_HINT = "";
const char* Strings::SETTINGS_NET_WIFI_CFG_TITLE = "";
const char* Strings::SETTINGS_POWER_10_MIN = "";
const char* Strings::SETTINGS_POWER_120_SEC = "";
const char* Strings::SETTINGS_POWER_30_MIN = "";
const char* Strings::SETTINGS_POWER_30_SEC = "";
const char* Strings::SETTINGS_POWER_3_MIN = "";
const char* Strings::SETTINGS_POWER_60_SEC = "";
const char* Strings::SETTINGS_POWER_IDLE_MHZ_HINT = "";
const char* Strings::SETTINGS_POWER_IDLE_MHZ_TITLE = "";
const char* Strings::SETTINGS_POWER_NET_GRACE_HINT = "";
const char* Strings::SETTINGS_POWER_NET_GRACE_TITLE = "";
const char* Strings::SETTINGS_POWER_OFF_HINT = "";
const char* Strings::SETTINGS_POWER_OFF_NEVER = "";
const char* Strings::SETTINGS_POWER_OFF_TITLE = "";
const char* Strings::SETTINGS_POWER_STANDBY_HINT = "";
const char* Strings::SETTINGS_POWER_STANDBY_TITLE = "";
const char* Strings::SETTINGS_STORAGE_CAP_DASH = "";
const char* Strings::SETTINGS_STORAGE_CAP_EXPORT = "";
const char* Strings::SETTINGS_STORAGE_CAP_FAIL = "";
const char* Strings::SETTINGS_STORAGE_CAP_FMT = "";
const char* Strings::SETTINGS_STORAGE_INSERTED = "";
const char* Strings::SETTINGS_STORAGE_MISSING = "";
const char* Strings::SETTINGS_STORAGE_PC_BUSY = "";
const char* Strings::SETTINGS_STORAGE_TITLE = "";
const char* Strings::SETTINGS_STORAGE_USB_DISABLED = "";
const char* Strings::SETTINGS_STORAGE_USB_HINT_DISABLED = "";
const char* Strings::SETTINGS_STORAGE_USB_HINT_DISABLE_FAIL = "";
const char* Strings::SETTINGS_STORAGE_USB_HINT_DISABLING = "";
const char* Strings::SETTINGS_STORAGE_USB_HINT_ENABLE_FAIL = "";
const char* Strings::SETTINGS_STORAGE_USB_HINT_ENABLING = "";
const char* Strings::SETTINGS_STORAGE_USB_HINT_FORMAT = "";
const char* Strings::SETTINGS_STORAGE_USB_HINT_HOST = "";
const char* Strings::SETTINGS_STORAGE_USB_HINT_HOST_BUSY = "";
const char* Strings::SETTINGS_STORAGE_USB_HINT_IDLE = "";
const char* Strings::SETTINGS_STORAGE_USB_HINT_LOCAL = "";
const char* Strings::SETTINGS_STORAGE_USB_HINT_NO_SD = "";
const char* Strings::SETTINGS_STORAGE_USB_HINT_SWITCHING = "";
const char* Strings::SETTINGS_STORAGE_USB_OFF = "";
const char* Strings::SETTINGS_STORAGE_USB_ON = "";
const char* Strings::SETTINGS_TAB_ABOUT = "";
const char* Strings::SETTINGS_TAB_BLUETOOTH = "";
const char* Strings::SETTINGS_TAB_CONVERSATION = "";
const char* Strings::SETTINGS_TAB_HAPTIC = "";
const char* Strings::SETTINGS_TAB_LANGUAGE = "";
const char* Strings::SETTINGS_TAB_NETWORK = "";
const char* Strings::SETTINGS_TAB_POWER = "";
const char* Strings::SETTINGS_TAB_STORAGE = "";
const char* Strings::SETTINGS_TAB_TEST = "";
const char* Strings::SETTINGS_TAB_THEME = "";
const char* Strings::SETTINGS_TEST_AGING = "";
const char* Strings::SETTINGS_TEST_AGING_RUNNING = "";
const char* Strings::SETTINGS_TEST_AUDIO_CONFIRM = "";
const char* Strings::SETTINGS_TEST_AUDIO_HOLD = "";
const char* Strings::SETTINGS_TEST_AUDIO_RECORDING = "";
const char* Strings::SETTINGS_TEST_AUDIO_TITLE = "";
const char* Strings::SETTINGS_TEST_AUTO = "";
const char* Strings::SETTINGS_TEST_BATTERY = "";
const char* Strings::SETTINGS_TEST_BATTERY_CHARGING = "";
const char* Strings::SETTINGS_TEST_BATTERY_CHG_CC = "";
const char* Strings::SETTINGS_TEST_BATTERY_CHG_CV = "";
const char* Strings::SETTINGS_TEST_BATTERY_CHG_EN = "";
const char* Strings::SETTINGS_TEST_BATTERY_CHG_NOT = "";
const char* Strings::SETTINGS_TEST_BATTERY_CHG_TOPOFF = "";
const char* Strings::SETTINGS_TEST_BATTERY_CHIP_CHG = "";
const char* Strings::SETTINGS_TEST_BATTERY_CURR = "";
const char* Strings::SETTINGS_TEST_BATTERY_DISCHARGING = "";
const char* Strings::SETTINGS_TEST_BATTERY_EN_OFF = "";
const char* Strings::SETTINGS_TEST_BATTERY_EN_ON = "";
const char* Strings::SETTINGS_TEST_BATTERY_GAUGE = "";
const char* Strings::SETTINGS_TEST_BATTERY_GAUGE_STAT = "";
const char* Strings::SETTINGS_TEST_BATTERY_HINT = "";
const char* Strings::SETTINGS_TEST_BATTERY_ICHG = "";
const char* Strings::SETTINGS_TEST_BATTERY_IDLE_LOAD = "";
const char* Strings::SETTINGS_TEST_BATTERY_MATCH = "";
const char* Strings::SETTINGS_TEST_BATTERY_MATCH_DPDM = "";
const char* Strings::SETTINGS_TEST_BATTERY_MATCH_OK = "";
const char* Strings::SETTINGS_TEST_BATTERY_MISMATCH_CHIP = "";
const char* Strings::SETTINGS_TEST_BATTERY_MISMATCH_GAUGE = "";
const char* Strings::SETTINGS_TEST_BATTERY_MISMATCH_NO_CHG = "";
const char* Strings::SETTINGS_TEST_BATTERY_MISMATCH_NO_IN = "";
const char* Strings::SETTINGS_TEST_BATTERY_SOC_RAW = "";
const char* Strings::SETTINGS_TEST_BATTERY_SOC_SMOOTH = "";
const char* Strings::SETTINGS_TEST_BATTERY_VBUS = "";
const char* Strings::SETTINGS_TEST_BATTERY_VBUS_ADP = "";
const char* Strings::SETTINGS_TEST_BATTERY_VBUS_CDP = "";
const char* Strings::SETTINGS_TEST_BATTERY_VBUS_DCP = "";
const char* Strings::SETTINGS_TEST_BATTERY_VBUS_NONE = "";
const char* Strings::SETTINGS_TEST_BATTERY_VBUS_NONSTD = "";
const char* Strings::SETTINGS_TEST_BATTERY_VBUS_OTG = "";
const char* Strings::SETTINGS_TEST_BATTERY_VBUS_SDP = "";
const char* Strings::SETTINGS_TEST_BATTERY_VBUS_UNKNOWN_IN = "";
const char* Strings::SETTINGS_TEST_BATTERY_VBUS_UNK_ADP = "";
const char* Strings::SETTINGS_TEST_BATTERY_VOLT = "";
const char* Strings::SETTINGS_TEST_BATTERY_VOLT_RANGE = "";
const char* Strings::SETTINGS_TEST_BATTERY_VREG = "";
const char* Strings::SETTINGS_TEST_BUSY = "";
const char* Strings::SETTINGS_TEST_CAMERA = "";
const char* Strings::SETTINGS_TEST_CAMERA_FAIL = "";
const char* Strings::SETTINGS_TEST_CAMERA_OK = "";
const char* Strings::SETTINGS_TEST_CAMERA_RESULT = "";
const char* Strings::SETTINGS_TEST_CAMERA_SHOT = "";
const char* Strings::SETTINGS_TEST_CAMERA_TIMEOUT = "";
const char* Strings::SETTINGS_TEST_CAMERA_USB_BUSY = "";
const char* Strings::SETTINGS_TEST_CAMERA_USB_IRQ = "";
const char* Strings::SETTINGS_TEST_CAPTURING = "";
const char* Strings::SETTINGS_TEST_CELL_CANCELLED = "";
const char* Strings::SETTINGS_TEST_CELL_CFUN0_FAIL = "";
const char* Strings::SETTINGS_TEST_CELL_CFUN1_FAIL = "";
const char* Strings::SETTINGS_TEST_CELL_EXT = "";
const char* Strings::SETTINGS_TEST_CELL_INT = "";
const char* Strings::SETTINGS_TEST_CELL_LOSS_FMT = "";
const char* Strings::SETTINGS_TEST_CELL_MODEM_NOT_READY = "";
const char* Strings::SETTINGS_TEST_CELL_MODEM_NO_RESP = "";
const char* Strings::SETTINGS_TEST_CELL_NEED_4G = "";
const char* Strings::SETTINGS_TEST_CELL_NO_RESOURCE = "";
const char* Strings::SETTINGS_TEST_CELL_NO_SIM = "";
const char* Strings::SETTINGS_TEST_CELL_PING_CMD_FAIL = "";
const char* Strings::SETTINGS_TEST_CELL_PING_FAIL = "";
const char* Strings::SETTINGS_TEST_CELL_PING_OK = "";
const char* Strings::SETTINGS_TEST_CELL_PING_WAIT_TIMEOUT = "";
const char* Strings::SETTINGS_TEST_CELL_REG_REJECT = "";
const char* Strings::SETTINGS_TEST_CELL_SEARCH_TIMEOUT = "";
const char* Strings::SETTINGS_TEST_CELL_SIM_SWITCH_FAIL = "";
const char* Strings::SETTINGS_TEST_CELL_SLOT_FAIL = "";
const char* Strings::SETTINGS_TEST_CELL_TESTING = "";
const char* Strings::SETTINGS_TEST_CELL_TEST_FAIL = "";
const char* Strings::SETTINGS_TEST_CELL_WAIT_RESOURCE = "";
const char* Strings::SETTINGS_TEST_DETECTING = "";
const char* Strings::SETTINGS_TEST_EXIT_REBOOT = "";
const char* Strings::SETTINGS_TEST_FAIL = "";
const char* Strings::SETTINGS_TEST_NO = "";
const char* Strings::SETTINGS_TEST_NOT_DETECTED = "";
const char* Strings::SETTINGS_TEST_NOT_PASS = "";
const char* Strings::SETTINGS_TEST_OK = "";
const char* Strings::SETTINGS_TEST_PASS = "";
const char* Strings::SETTINGS_TEST_REBOOTING = "";
const char* Strings::SETTINGS_TEST_SCANNING = "";
const char* Strings::SETTINGS_TEST_SDCARD = "";
const char* Strings::SETTINGS_TEST_SD_MOUNT_FAIL = "";
const char* Strings::SETTINGS_TEST_SIGNAL_BOARD_UNSUP = "";
const char* Strings::SETTINGS_TEST_SIGNAL_CELL_CSQ_FMT = "";
const char* Strings::SETTINGS_TEST_SIGNAL_CELL_CSQ_UNKNOWN = "";
const char* Strings::SETTINGS_TEST_SIGNAL_CELL_UNAVAIL = "";
const char* Strings::SETTINGS_TEST_SIGNAL_READING = "";
const char* Strings::SETTINGS_TEST_SIGNAL_TITLE = "";
const char* Strings::SETTINGS_TEST_SIGNAL_WIFI_DISCONNECTED = "";
const char* Strings::SETTINGS_TEST_SIGNAL_WIFI_READ_FAIL = "";
const char* Strings::SETTINGS_TEST_SIGNAL_WIFI_RSSI_FMT = "";
const char* Strings::SETTINGS_TEST_TASK_CREATE_FAIL = "";
const char* Strings::SETTINGS_TEST_TOUCH = "";
const char* Strings::SETTINGS_TEST_TOUCH_START = "";
const char* Strings::SETTINGS_TEST_WAITING = "";
const char* Strings::SETTINGS_TEST_WIFI_CLOSE = "";
const char* Strings::SETTINGS_TEST_WIFI_COUNT_FMT = "";
const char* Strings::SETTINGS_TEST_WIFI_FOUND_FMT = "";
const char* Strings::SETTINGS_TEST_WIFI_NEARBY = "";
const char* Strings::SETTINGS_TEST_WIFI_NEXT = "";
const char* Strings::SETTINGS_TEST_WIFI_NONE = "";
const char* Strings::SETTINGS_TEST_WIFI_PREV = "";
const char* Strings::SETTINGS_TEST_WIFI_RESCAN = "";
const char* Strings::SETTINGS_TEST_WIFI_SCANNING = "";
const char* Strings::SETTINGS_TEST_WIFI_SCAN_FAIL = "";
const char* Strings::SETTINGS_TEST_YES = "";
const char* Strings::SETTINGS_THEME_BORDER = "";
const char* Strings::SETTINGS_THEME_CURRENT_BORDER = "";
const char* Strings::SETTINGS_THEME_CURRENT_GRAY = "";
const char* Strings::SETTINGS_THEME_CURRENT_SLASH = "";
const char* Strings::SETTINGS_THEME_CURRENT_WHITE = "";
const char* Strings::SETTINGS_THEME_GRAY = "";
const char* Strings::SETTINGS_THEME_HINT = "";
const char* Strings::SETTINGS_THEME_SLASH = "";
const char* Strings::SETTINGS_THEME_TITLE = "";
const char* Strings::SETTINGS_THEME_WHITE = "";
const char* Strings::SPEAKING = "";
const char* Strings::STANDBY = "";
const char* Strings::STANDBY_CONN_FAIL = "";
const char* Strings::STANDBY_DATE_FMT = "";
const char* Strings::STANDBY_DATE_PLACEHOLDER = "";
const char* Strings::STANDBY_DECODE_FAIL = "";
const char* Strings::STANDBY_EMPTY_TODO = "";
const char* Strings::STANDBY_NO_DATA = "";
const char* Strings::STANDBY_NO_NETWORK = "";
const char* Strings::STANDBY_NO_TODO_CACHE = "";
const char* Strings::STANDBY_NO_WEATHER = "";
const char* Strings::STANDBY_PARSE_FAIL = "";
const char* Strings::STANDBY_REQUEST_FAIL = "";
const char* Strings::STANDBY_WEATHER_FAIL = "";
const char* Strings::STANDBY_WP_NOT_FOUND = "";
const char* Strings::SWITCH_TO_4G_NETWORK = "";
const char* Strings::SWITCH_TO_WIFI_NETWORK = "";
const char* Strings::TASK_API_ERROR = "";
const char* Strings::TASK_BATCH_COMPLETING = "";
const char* Strings::TASK_BATCH_FAIL = "";
const char* Strings::TASK_BATCH_REMOVING = "";
const char* Strings::TASK_BATCH_RESULT_FMT = "";
const char* Strings::TASK_BATCH_START_FAIL = "";
const char* Strings::TASK_CHOOSE_ACTION = "";
const char* Strings::TASK_COMPLETE = "";
const char* Strings::TASK_COMPLETED = "";
const char* Strings::TASK_COMPLETED_N_FMT = "";
const char* Strings::TASK_COMPLETE_FAIL = "";
const char* Strings::TASK_COMPLETE_START_FAIL = "";
const char* Strings::TASK_COMPLETE_TODO = "";
const char* Strings::TASK_COMPLETING = "";
const char* Strings::TASK_CONNECTING_NET = "";
const char* Strings::TASK_CONN_FAIL = "";
const char* Strings::TASK_DATA_FORMAT_ERR = "";
const char* Strings::TASK_DELETED = "";
const char* Strings::TASK_DELETE_FAIL = "";
const char* Strings::TASK_DELETE_START_FAIL = "";
const char* Strings::TASK_DELETE_TODO = "";
const char* Strings::TASK_DELETING = "";
const char* Strings::TASK_DONE_OK = "";
const char* Strings::TASK_EMPTY_DONE = "";
const char* Strings::TASK_EMPTY_TODO = "";
const char* Strings::TASK_INVALID = "";
const char* Strings::TASK_LOAD_FAIL = "";
const char* Strings::TASK_NEED_WIFI_CFG = "";
const char* Strings::TASK_NET_NOT_READY = "";
const char* Strings::TASK_NO_COMPLETABLE = "";
const char* Strings::TASK_NO_NETWORK = "";
const char* Strings::TASK_PARSE_FAIL = "";
const char* Strings::TASK_PLEASE_WAIT = "";
const char* Strings::TASK_REFRESH = "";
const char* Strings::TASK_REFRESHED = "";
const char* Strings::TASK_REFRESHING = "";
const char* Strings::TASK_REFRESH_START_FAIL = "";
const char* Strings::TASK_REMOVED_N_FMT = "";
const char* Strings::TASK_REQUEST_FAIL = "";
const char* Strings::TASK_SELECTED_FMT = "";
const char* Strings::TASK_SELECT_FIRST = "";
const char* Strings::TASK_TAB_DONE = "";
const char* Strings::TASK_TAB_TODO = "";
const char* Strings::TASK_TITLE_DONE = "";
const char* Strings::TASK_TITLE_TODO = "";
const char* Strings::TOUCH_MISSING_HINT = "";
const char* Strings::TRANSLATE_AUDIO_BUSY = "";
const char* Strings::TRANSLATE_CANCELLED = "";
const char* Strings::TRANSLATE_CONNECTING = "";
const char* Strings::TRANSLATE_CONNECT_FAIL = "";
const char* Strings::TRANSLATE_CONNECT_NET = "";
const char* Strings::TRANSLATE_ERROR = "";
const char* Strings::TRANSLATE_GET_TOKEN = "";
const char* Strings::TRANSLATE_GET_TOKEN_FAIL = "";
const char* Strings::TRANSLATE_HINT = "";
const char* Strings::TRANSLATE_LANG_INVALID = "";
const char* Strings::TRANSLATE_LANG_SAME = "";
const char* Strings::TRANSLATE_LIVE = "";
const char* Strings::TRANSLATE_NEED_WIFI_CFG = "";
const char* Strings::TRANSLATE_NET_CONNECTING = "";
const char* Strings::TRANSLATE_NET_NOT_READY = "";
const char* Strings::TRANSLATE_PICK_FROM = "";
const char* Strings::TRANSLATE_PICK_LANG = "";
const char* Strings::TRANSLATE_PICK_TO = "";
const char* Strings::TRANSLATE_READY_TIMEOUT = "";
const char* Strings::TRANSLATE_SOURCE_PLACEHOLDER = "";
const char* Strings::TRANSLATE_SOURCE_TITLE = "";
const char* Strings::TRANSLATE_START_FAIL = "";
const char* Strings::TRANSLATE_STOP = "";
const char* Strings::TRANSLATE_STOPPED = "";
const char* Strings::TRANSLATE_STOPPING = "";
const char* Strings::TRANSLATE_TRANSLATING = "";
const char* Strings::TRANSLATE_TRANS_PLACEHOLDER = "";
const char* Strings::TRANSLATE_TRANS_TITLE = "";
const char* Strings::TRANSLATE_TRY_LATER = "";
const char* Strings::UPGRADE_FAILED = "";
const char* Strings::UPGRADING = "";
const char* Strings::VERSION = "";
const char* Strings::VOICE_STARTING_NET = "";
const char* Strings::VOLUME = "";
const char* Strings::WALLPAPER_CHIP_SHUTDOWN = "";
const char* Strings::WALLPAPER_CHIP_STANDBY = "";
const char* Strings::WALLPAPER_DECODE_FAIL = "";
const char* Strings::WALLPAPER_DELETE_BTN = "";
const char* Strings::WALLPAPER_DELETE_FAIL = "";
const char* Strings::WALLPAPER_DELETE_START_FAIL = "";
const char* Strings::WALLPAPER_DELETING = "";
const char* Strings::WALLPAPER_EMPTY_FMT = "";
const char* Strings::WALLPAPER_ENABLE_FAIL = "";
const char* Strings::WALLPAPER_FILE_INVALID = "";
const char* Strings::WALLPAPER_FILE_MISSING = "";
const char* Strings::WALLPAPER_LOADING = "";
const char* Strings::WALLPAPER_NAME_INVALID = "";
const char* Strings::WALLPAPER_NO_SD = "";
const char* Strings::WALLPAPER_PATH_INVALID = "";
const char* Strings::WALLPAPER_PREVIEW_FAIL = "";
const char* Strings::WALLPAPER_PREVIEW_OOM = "";
const char* Strings::WALLPAPER_PREVIEW_START_FAIL = "";
const char* Strings::WALLPAPER_SELECTED_FMT = "";
const char* Strings::WALLPAPER_SET_SHUTDOWN = "";
const char* Strings::WALLPAPER_SET_STANDBY = "";
const char* Strings::WALLPAPER_SHUTDOWN_ON = "";
const char* Strings::WALLPAPER_STANDBY_ON = "";
const char* Strings::WARNING = "";
const char* Strings::WIFI_CONFIG_MODE = "";

const char* Current() {
    return CODE;
}

bool SetLanguage(const char* code, bool persist) {
    const char* normalized = NormalizeCode(code);
    const bool changed = (CODE == nullptr) || (std::strcmp(CODE, normalized) != 0);
    ApplyLiterals(LiteralsFor(normalized));
    CODE = normalized;
    if (persist) {
        Settings settings("ui", true);
        settings.SetString("language", normalized);
    }
    return changed;
}

void InitFromNvs() {
    Settings settings("ui", false);
    const std::string saved = settings.GetString("language", "");
    if (saved.empty()) {
        SetLanguage(LANG_DEFAULT_CODE, false);
    } else {
        SetLanguage(saved.c_str(), false);
    }
}

}  // namespace Lang

namespace {
struct LangStaticInit {
    LangStaticInit() {
        Lang::SetLanguage(LANG_DEFAULT_CODE, false);
    }
};
LangStaticInit g_lang_static_init;
}  // namespace
