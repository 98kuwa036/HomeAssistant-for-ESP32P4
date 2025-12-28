/**
 * @file command_cache.h
 * @brief Fixed Command Response Cache for LLM Optimization
 *
 * This module implements a pattern-matching command cache that handles
 * common voice commands locally without invoking the LLM, reducing
 * latency and API costs.
 *
 * Architecture:
 *   User Input → Pattern Match → [Hit] → Execute + Respond (local)
 *                              → [Miss] → Forward to LLM
 *
 * @author Omni-P4 Project
 * @date 2024
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Maximum length of command pattern
 */
#define CMD_PATTERN_MAX_LEN     64

/**
 * @brief Maximum length of response template
 */
#define CMD_RESPONSE_MAX_LEN    128

/**
 * @brief Maximum number of cached commands
 */
#define CMD_CACHE_MAX_ENTRIES   64

/**
 * @brief Fuzzy match threshold (0-100, higher = stricter)
 */
#define CMD_FUZZY_THRESHOLD     70

/**
 * @brief Command category for routing and logging
 */
typedef enum {
    CMD_CAT_LIGHTING = 0,    // 照明制御
    CMD_CAT_CLIMATE,         // 空調制御
    CMD_CAT_SENSOR,          // センサー情報
    CMD_CAT_MEDIA,           // メディア制御
    CMD_CAT_SYSTEM,          // システム操作
    CMD_CAT_TIMER,           // タイマー/アラーム
    CMD_CAT_CUSTOM,          // カスタムコマンド
    CMD_CAT_COUNT
} cmd_category_t;

/**
 * @brief Action type for command execution
 */
typedef enum {
    CMD_ACTION_NONE = 0,
    CMD_ACTION_LIGHT_ON,
    CMD_ACTION_LIGHT_OFF,
    CMD_ACTION_LIGHT_DIM,
    CMD_ACTION_LIGHT_BRIGHT,
    CMD_ACTION_AC_ON,
    CMD_ACTION_AC_OFF,
    CMD_ACTION_AC_TEMP_UP,
    CMD_ACTION_AC_TEMP_DOWN,
    CMD_ACTION_REPORT_TEMP,
    CMD_ACTION_REPORT_HUMIDITY,
    CMD_ACTION_REPORT_CO2,
    CMD_ACTION_REPORT_AIR_QUALITY,
    CMD_ACTION_REPORT_ALL_SENSORS,
    CMD_ACTION_MEDIA_PLAY,
    CMD_ACTION_MEDIA_PAUSE,
    CMD_ACTION_MEDIA_STOP,
    CMD_ACTION_VOLUME_UP,
    CMD_ACTION_VOLUME_DOWN,
    CMD_ACTION_VOLUME_MUTE,
    CMD_ACTION_TIMER_SET,
    CMD_ACTION_TIMER_CANCEL,
    CMD_ACTION_SYSTEM_STATUS,
    CMD_ACTION_CUSTOM,
    CMD_ACTION_COUNT
} cmd_action_t;

/**
 * @brief Command callback function type
 * @param param Optional parameter (e.g., brightness level, temperature)
 * @param response Buffer to write dynamic response
 * @param response_len Maximum response buffer length
 * @return true if action succeeded
 */
typedef bool (*cmd_action_callback_t)(int param, char* response, size_t response_len);

/**
 * @brief Single command cache entry
 */
typedef struct {
    char pattern[CMD_PATTERN_MAX_LEN];          // 入力パターン (e.g., "照明つけて")
    char pattern_alt[CMD_PATTERN_MAX_LEN];      // 代替パターン (e.g., "電気つけて")
    char response[CMD_RESPONSE_MAX_LEN];        // 応答テンプレート (printf形式対応)
    cmd_category_t category;                     // コマンドカテゴリ
    cmd_action_t action;                         // 実行アクション
    int default_param;                           // デフォルトパラメータ
    cmd_action_callback_t callback;              // カスタムコールバック (NULLでデフォルト)
    bool enabled;                                // 有効/無効フラグ
} cmd_cache_entry_t;

/**
 * @brief Command match result
 */
typedef struct {
    bool matched;                               // パターンマッチ成功
    int entry_index;                            // マッチしたエントリのインデックス
    int match_score;                            // マッチスコア (0-100)
    int extracted_param;                        // 抽出されたパラメータ
    char response[CMD_RESPONSE_MAX_LEN];        // 生成された応答文
} cmd_match_result_t;

/**
 * @brief Cache statistics
 */
typedef struct {
    uint32_t total_queries;                     // 総クエリ数
    uint32_t cache_hits;                        // キャッシュヒット数
    uint32_t cache_misses;                      // キャッシュミス数 (LLMフォールバック)
    uint32_t action_successes;                  // アクション成功数
    uint32_t action_failures;                   // アクション失敗数
    uint32_t category_hits[CMD_CAT_COUNT];      // カテゴリ別ヒット数
} cmd_cache_stats_t;

/**
 * @brief Initialize command cache with default Japanese commands
 * @return true if initialization succeeded
 */
bool cmd_cache_init(void);

/**
 * @brief Deinitialize command cache and free resources
 */
void cmd_cache_deinit(void);

/**
 * @brief Process input text and check for cached command match
 * @param input User input text (UTF-8)
 * @param result Pointer to result structure
 * @return true if command matched and was handled locally
 */
bool cmd_cache_process(const char* input, cmd_match_result_t* result);

/**
 * @brief Execute action for a matched command
 * @param entry_index Index of matched entry
 * @param param Parameter for action
 * @param response Buffer for response message
 * @param response_len Response buffer length
 * @return true if action executed successfully
 */
bool cmd_cache_execute(int entry_index, int param, char* response, size_t response_len);

/**
 * @brief Add custom command to cache
 * @param entry Command entry to add
 * @return Index of added entry, or -1 on failure
 */
int cmd_cache_add(const cmd_cache_entry_t* entry);

/**
 * @brief Remove command from cache
 * @param index Index of entry to remove
 * @return true if removed successfully
 */
bool cmd_cache_remove(int index);

/**
 * @brief Enable or disable a command entry
 * @param index Entry index
 * @param enabled Enable state
 */
void cmd_cache_set_enabled(int index, bool enabled);

/**
 * @brief Get cache statistics
 * @param stats Pointer to stats structure
 */
void cmd_cache_get_stats(cmd_cache_stats_t* stats);

/**
 * @brief Reset cache statistics
 */
void cmd_cache_reset_stats(void);

/**
 * @brief Register action callback for custom actions
 * @param action Action type
 * @param callback Callback function
 */
void cmd_cache_register_callback(cmd_action_t action, cmd_action_callback_t callback);

/**
 * @brief Set fuzzy match threshold
 * @param threshold Threshold value (0-100)
 */
void cmd_cache_set_threshold(int threshold);

/**
 * @brief Get number of cached commands
 * @return Number of entries
 */
int cmd_cache_get_count(void);

/**
 * @brief Print cache contents for debugging
 */
void cmd_cache_dump(void);

#ifdef __cplusplus
}
#endif
