/**
 * @file command_cache.cpp
 * @brief Fixed Command Response Cache Implementation
 */

#include "command_cache.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

// Logging (ESP-IDF compatible)
#ifdef ESP_PLATFORM
#include "esp_log.h"
static const char* TAG = "cmd_cache";
#define LOG_I(fmt, ...) ESP_LOGI(TAG, fmt, ##__VA_ARGS__)
#define LOG_W(fmt, ...) ESP_LOGW(TAG, fmt, ##__VA_ARGS__)
#define LOG_E(fmt, ...) ESP_LOGE(TAG, fmt, ##__VA_ARGS__)
#define LOG_D(fmt, ...) ESP_LOGD(TAG, fmt, ##__VA_ARGS__)
#else
#define LOG_I(fmt, ...) printf("[INFO] " fmt "\n", ##__VA_ARGS__)
#define LOG_W(fmt, ...) printf("[WARN] " fmt "\n", ##__VA_ARGS__)
#define LOG_E(fmt, ...) printf("[ERROR] " fmt "\n", ##__VA_ARGS__)
#define LOG_D(fmt, ...) printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
#endif

// Cache storage
static cmd_cache_entry_t s_cache[CMD_CACHE_MAX_ENTRIES];
static int s_cache_count = 0;
static int s_fuzzy_threshold = CMD_FUZZY_THRESHOLD;
static cmd_cache_stats_t s_stats = {0};
static cmd_action_callback_t s_callbacks[CMD_ACTION_COUNT] = {NULL};
static bool s_initialized = false;

// ============================================================================
// Default Japanese Commands
// ============================================================================

static const cmd_cache_entry_t DEFAULT_COMMANDS[] = {
    // === 照明制御 ===
    {"照明つけて", "電気つけて", "はい、照明をつけます", CMD_CAT_LIGHTING, CMD_ACTION_LIGHT_ON, 100, NULL, true},
    {"照明消して", "電気消して", "はい、照明を消します", CMD_CAT_LIGHTING, CMD_ACTION_LIGHT_OFF, 0, NULL, true},
    {"明るくして", "もっと明るく", "照明を明るくします", CMD_CAT_LIGHTING, CMD_ACTION_LIGHT_BRIGHT, 20, NULL, true},
    {"暗くして", "もっと暗く", "照明を暗くします", CMD_CAT_LIGHTING, CMD_ACTION_LIGHT_DIM, -20, NULL, true},
    {"ライトオン", "ライトつけて", "ライトをつけます", CMD_CAT_LIGHTING, CMD_ACTION_LIGHT_ON, 100, NULL, true},
    {"ライトオフ", "ライト消して", "ライトを消します", CMD_CAT_LIGHTING, CMD_ACTION_LIGHT_OFF, 0, NULL, true},

    // === 空調制御 ===
    {"エアコンつけて", "冷房つけて", "エアコンを起動します", CMD_CAT_CLIMATE, CMD_ACTION_AC_ON, 26, NULL, true},
    {"エアコン消して", "冷房消して", "エアコンを停止します", CMD_CAT_CLIMATE, CMD_ACTION_AC_OFF, 0, NULL, true},
    {"暖房つけて", "ヒーターつけて", "暖房を起動します", CMD_CAT_CLIMATE, CMD_ACTION_AC_ON, 22, NULL, true},
    {"温度上げて", "もっと暖かく", "設定温度を上げます", CMD_CAT_CLIMATE, CMD_ACTION_AC_TEMP_UP, 1, NULL, true},
    {"温度下げて", "もっと涼しく", "設定温度を下げます", CMD_CAT_CLIMATE, CMD_ACTION_AC_TEMP_DOWN, -1, NULL, true},

    // === センサー情報 ===
    {"今何度", "室温教えて", "現在の室温は%d度です", CMD_CAT_SENSOR, CMD_ACTION_REPORT_TEMP, 0, NULL, true},
    {"湿度は", "湿度教えて", "現在の湿度は%d%%です", CMD_CAT_SENSOR, CMD_ACTION_REPORT_HUMIDITY, 0, NULL, true},
    {"CO2は", "二酸化炭素は", "現在のCO2濃度は%dppmです", CMD_CAT_SENSOR, CMD_ACTION_REPORT_CO2, 0, NULL, true},
    {"空気の状態", "空気質は", "空気質指数は%dです。%s", CMD_CAT_SENSOR, CMD_ACTION_REPORT_AIR_QUALITY, 0, NULL, true},
    {"センサー確認", "環境教えて", "室温%d度、湿度%d%%、CO2 %dppmです", CMD_CAT_SENSOR, CMD_ACTION_REPORT_ALL_SENSORS, 0, NULL, true},

    // === メディア制御 ===
    {"音楽かけて", "音楽再生", "音楽を再生します", CMD_CAT_MEDIA, CMD_ACTION_MEDIA_PLAY, 0, NULL, true},
    {"音楽止めて", "音楽停止", "音楽を停止します", CMD_CAT_MEDIA, CMD_ACTION_MEDIA_STOP, 0, NULL, true},
    {"一時停止", "ポーズ", "一時停止します", CMD_CAT_MEDIA, CMD_ACTION_MEDIA_PAUSE, 0, NULL, true},
    {"音量上げて", "ボリュームアップ", "音量を上げます", CMD_CAT_MEDIA, CMD_ACTION_VOLUME_UP, 10, NULL, true},
    {"音量下げて", "ボリュームダウン", "音量を下げます", CMD_CAT_MEDIA, CMD_ACTION_VOLUME_DOWN, -10, NULL, true},
    {"ミュート", "消音", "ミュートにします", CMD_CAT_MEDIA, CMD_ACTION_VOLUME_MUTE, 0, NULL, true},

    // === タイマー ===
    {"タイマーセット", "タイマー設定", "タイマーを設定しました", CMD_CAT_TIMER, CMD_ACTION_TIMER_SET, 0, NULL, true},
    {"タイマー解除", "タイマーキャンセル", "タイマーを解除しました", CMD_CAT_TIMER, CMD_ACTION_TIMER_CANCEL, 0, NULL, true},

    // === システム ===
    {"システム状態", "ステータス確認", "システムは正常に動作しています", CMD_CAT_SYSTEM, CMD_ACTION_SYSTEM_STATUS, 0, NULL, true},
    {"おはよう", "おはようございます", "おはようございます。今日も良い一日を", CMD_CAT_SYSTEM, CMD_ACTION_NONE, 0, NULL, true},
    {"おやすみ", "おやすみなさい", "おやすみなさい。照明を消しますか？", CMD_CAT_SYSTEM, CMD_ACTION_NONE, 0, NULL, true},
    {"ありがとう", "サンキュー", "どういたしまして", CMD_CAT_SYSTEM, CMD_ACTION_NONE, 0, NULL, true},
};

#define DEFAULT_COMMANDS_COUNT (sizeof(DEFAULT_COMMANDS) / sizeof(DEFAULT_COMMANDS[0]))

// ============================================================================
// Fuzzy Matching (Levenshtein Distance based)
// ============================================================================

/**
 * @brief Calculate Levenshtein distance between two UTF-8 strings
 */
static int levenshtein_distance(const char* s1, const char* s2) {
    int len1 = strlen(s1);
    int len2 = strlen(s2);

    // Simple implementation for short strings
    if (len1 == 0) return len2;
    if (len2 == 0) return len1;

    // Use stack allocation for small strings
    int matrix[64][64];
    if (len1 > 63 || len2 > 63) {
        // Fall back to simple comparison for long strings
        return (strcmp(s1, s2) == 0) ? 0 : (len1 + len2) / 2;
    }

    for (int i = 0; i <= len1; i++) matrix[i][0] = i;
    for (int j = 0; j <= len2; j++) matrix[0][j] = j;

    for (int i = 1; i <= len1; i++) {
        for (int j = 1; j <= len2; j++) {
            int cost = (s1[i-1] == s2[j-1]) ? 0 : 1;
            int del = matrix[i-1][j] + 1;
            int ins = matrix[i][j-1] + 1;
            int sub = matrix[i-1][j-1] + cost;

            matrix[i][j] = del;
            if (ins < matrix[i][j]) matrix[i][j] = ins;
            if (sub < matrix[i][j]) matrix[i][j] = sub;
        }
    }

    return matrix[len1][len2];
}

/**
 * @brief Calculate similarity score (0-100)
 */
static int calculate_similarity(const char* input, const char* pattern) {
    if (!input || !pattern) return 0;

    // Check for exact match first
    if (strcmp(input, pattern) == 0) return 100;

    // Check if input contains pattern
    if (strstr(input, pattern) != NULL) return 95;

    // Check if pattern contains input
    if (strstr(pattern, input) != NULL) return 90;

    // Calculate Levenshtein-based similarity
    int distance = levenshtein_distance(input, pattern);
    int max_len = strlen(input);
    if ((int)strlen(pattern) > max_len) max_len = strlen(pattern);

    if (max_len == 0) return 0;

    int similarity = 100 - (distance * 100 / max_len);
    if (similarity < 0) similarity = 0;

    return similarity;
}

/**
 * @brief Check if input matches pattern with fuzzy matching
 */
static int match_pattern(const char* input, const cmd_cache_entry_t* entry) {
    int score1 = calculate_similarity(input, entry->pattern);
    int score2 = calculate_similarity(input, entry->pattern_alt);

    return (score1 > score2) ? score1 : score2;
}

// ============================================================================
// Public API Implementation
// ============================================================================

bool cmd_cache_init(void) {
    if (s_initialized) {
        LOG_W("Command cache already initialized");
        return true;
    }

    memset(s_cache, 0, sizeof(s_cache));
    memset(&s_stats, 0, sizeof(s_stats));
    memset(s_callbacks, 0, sizeof(s_callbacks));
    s_cache_count = 0;

    // Load default commands
    for (size_t i = 0; i < DEFAULT_COMMANDS_COUNT && s_cache_count < CMD_CACHE_MAX_ENTRIES; i++) {
        memcpy(&s_cache[s_cache_count], &DEFAULT_COMMANDS[i], sizeof(cmd_cache_entry_t));
        s_cache_count++;
    }

    s_initialized = true;
    LOG_I("Command cache initialized with %d default commands", s_cache_count);

    return true;
}

void cmd_cache_deinit(void) {
    if (!s_initialized) return;

    memset(s_cache, 0, sizeof(s_cache));
    s_cache_count = 0;
    s_initialized = false;

    LOG_I("Command cache deinitialized");
}

bool cmd_cache_process(const char* input, cmd_match_result_t* result) {
    if (!s_initialized || !input || !result) {
        return false;
    }

    memset(result, 0, sizeof(cmd_match_result_t));
    s_stats.total_queries++;

    int best_score = 0;
    int best_index = -1;

    // Find best matching command
    for (int i = 0; i < s_cache_count; i++) {
        if (!s_cache[i].enabled) continue;

        int score = match_pattern(input, &s_cache[i]);
        if (score > best_score) {
            best_score = score;
            best_index = i;
        }
    }

    // Check if best match meets threshold
    if (best_score >= s_fuzzy_threshold && best_index >= 0) {
        result->matched = true;
        result->entry_index = best_index;
        result->match_score = best_score;

        // Execute action and generate response
        cmd_cache_execute(best_index, s_cache[best_index].default_param,
                         result->response, sizeof(result->response));

        s_stats.cache_hits++;
        s_stats.category_hits[s_cache[best_index].category]++;

        LOG_D("Cache hit: '%s' -> '%s' (score: %d)",
              input, s_cache[best_index].pattern, best_score);

        return true;
    }

    // No match - will need LLM fallback
    s_stats.cache_misses++;
    LOG_D("Cache miss: '%s' (best score: %d)", input, best_score);

    return false;
}

bool cmd_cache_execute(int entry_index, int param, char* response, size_t response_len) {
    if (entry_index < 0 || entry_index >= s_cache_count || !response) {
        return false;
    }

    cmd_cache_entry_t* entry = &s_cache[entry_index];
    cmd_action_t action = entry->action;

    // Use custom callback if registered
    if (entry->callback) {
        bool success = entry->callback(param, response, response_len);
        if (success) s_stats.action_successes++;
        else s_stats.action_failures++;
        return success;
    }

    // Use registered action callback
    if (action < CMD_ACTION_COUNT && s_callbacks[action]) {
        bool success = s_callbacks[action](param, response, response_len);
        if (success) s_stats.action_successes++;
        else s_stats.action_failures++;
        return success;
    }

    // Default: copy response template
    snprintf(response, response_len, "%s", entry->response);
    s_stats.action_successes++;

    return true;
}

int cmd_cache_add(const cmd_cache_entry_t* entry) {
    if (!s_initialized || !entry || s_cache_count >= CMD_CACHE_MAX_ENTRIES) {
        return -1;
    }

    memcpy(&s_cache[s_cache_count], entry, sizeof(cmd_cache_entry_t));
    int index = s_cache_count;
    s_cache_count++;

    LOG_I("Added command: '%s' at index %d", entry->pattern, index);
    return index;
}

bool cmd_cache_remove(int index) {
    if (index < 0 || index >= s_cache_count) {
        return false;
    }

    // Shift remaining entries
    for (int i = index; i < s_cache_count - 1; i++) {
        memcpy(&s_cache[i], &s_cache[i + 1], sizeof(cmd_cache_entry_t));
    }
    s_cache_count--;

    LOG_I("Removed command at index %d", index);
    return true;
}

void cmd_cache_set_enabled(int index, bool enabled) {
    if (index >= 0 && index < s_cache_count) {
        s_cache[index].enabled = enabled;
    }
}

void cmd_cache_get_stats(cmd_cache_stats_t* stats) {
    if (stats) {
        memcpy(stats, &s_stats, sizeof(cmd_cache_stats_t));
    }
}

void cmd_cache_reset_stats(void) {
    memset(&s_stats, 0, sizeof(s_stats));
}

void cmd_cache_register_callback(cmd_action_t action, cmd_action_callback_t callback) {
    if (action < CMD_ACTION_COUNT) {
        s_callbacks[action] = callback;
        LOG_D("Registered callback for action %d", action);
    }
}

void cmd_cache_set_threshold(int threshold) {
    if (threshold >= 0 && threshold <= 100) {
        s_fuzzy_threshold = threshold;
        LOG_I("Fuzzy threshold set to %d", threshold);
    }
}

int cmd_cache_get_count(void) {
    return s_cache_count;
}

void cmd_cache_dump(void) {
    LOG_I("=== Command Cache Dump ===");
    LOG_I("Total entries: %d", s_cache_count);
    LOG_I("Fuzzy threshold: %d", s_fuzzy_threshold);

    for (int i = 0; i < s_cache_count; i++) {
        LOG_I("[%d] %s: '%s' -> '%s' (cat:%d, act:%d, %s)",
              i,
              s_cache[i].enabled ? "ON " : "OFF",
              s_cache[i].pattern,
              s_cache[i].response,
              s_cache[i].category,
              s_cache[i].action,
              s_cache[i].callback ? "custom" : "default");
    }

    LOG_I("=== Statistics ===");
    LOG_I("Queries: %lu, Hits: %lu, Misses: %lu",
          (unsigned long)s_stats.total_queries,
          (unsigned long)s_stats.cache_hits,
          (unsigned long)s_stats.cache_misses);

    float hit_rate = (s_stats.total_queries > 0)
        ? (float)s_stats.cache_hits * 100.0f / s_stats.total_queries
        : 0.0f;
    LOG_I("Hit rate: %.1f%%", hit_rate);
}
