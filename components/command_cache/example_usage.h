/**
 * @file example_usage.h
 * @brief Example integration of Command Cache with Voice Recognition + LLM
 *
 * This file demonstrates how to use the command cache to optimize
 * voice command processing by handling common commands locally
 * and falling back to LLM only when needed.
 */

#pragma once

#include "command_cache.h"
#include <stdio.h>

// Forward declarations (implement these based on your hardware)
extern bool home_assistant_call_service(const char* domain, const char* service, const char* entity_id);
extern bool llm_generate_response(const char* prompt, char* response, size_t len);
extern void tts_speak(const char* text);

// Sensor data (from sensor_hub)
extern float get_temperature(void);
extern float get_humidity(void);
extern int get_co2_ppm(void);
extern int get_aqi(void);

/**
 * @brief Example: Report temperature callback
 */
static bool action_report_temp(int param, char* response, size_t response_len) {
    float temp = get_temperature();
    snprintf(response, response_len, "現在の室温は%.1f度です", temp);
    return true;
}

/**
 * @brief Example: Report humidity callback
 */
static bool action_report_humidity(int param, char* response, size_t response_len) {
    float humidity = get_humidity();
    snprintf(response, response_len, "現在の湿度は%.0f%%です", humidity);
    return true;
}

/**
 * @brief Example: Report CO2 callback
 */
static bool action_report_co2(int param, char* response, size_t response_len) {
    int co2 = get_co2_ppm();
    const char* status = (co2 < 800) ? "良好です" :
                         (co2 < 1000) ? "やや高めです" :
                         (co2 < 1500) ? "換気をおすすめします" : "すぐに換気してください";
    snprintf(response, response_len, "CO2濃度は%dppmです。%s", co2, status);
    return true;
}

/**
 * @brief Example: Report all sensors callback
 */
static bool action_report_all(int param, char* response, size_t response_len) {
    float temp = get_temperature();
    float humidity = get_humidity();
    int co2 = get_co2_ppm();
    snprintf(response, response_len, "室温%.1f度、湿度%.0f%%、CO2濃度%dppmです",
             temp, humidity, co2);
    return true;
}

/**
 * @brief Example: Light on callback (via Home Assistant)
 */
static bool action_light_on(int param, char* response, size_t response_len) {
    bool success = home_assistant_call_service("light", "turn_on", "light.living_room");
    if (success) {
        snprintf(response, response_len, "照明をつけました");
    } else {
        snprintf(response, response_len, "照明の操作に失敗しました");
    }
    return success;
}

/**
 * @brief Example: Light off callback (via Home Assistant)
 */
static bool action_light_off(int param, char* response, size_t response_len) {
    bool success = home_assistant_call_service("light", "turn_off", "light.living_room");
    if (success) {
        snprintf(response, response_len, "照明を消しました");
    } else {
        snprintf(response, response_len, "照明の操作に失敗しました");
    }
    return success;
}

/**
 * @brief Initialize command cache with sensor callbacks
 */
static void setup_command_cache(void) {
    // Initialize cache
    cmd_cache_init();

    // Register action callbacks
    cmd_cache_register_callback(CMD_ACTION_REPORT_TEMP, action_report_temp);
    cmd_cache_register_callback(CMD_ACTION_REPORT_HUMIDITY, action_report_humidity);
    cmd_cache_register_callback(CMD_ACTION_REPORT_CO2, action_report_co2);
    cmd_cache_register_callback(CMD_ACTION_REPORT_ALL_SENSORS, action_report_all);
    cmd_cache_register_callback(CMD_ACTION_LIGHT_ON, action_light_on);
    cmd_cache_register_callback(CMD_ACTION_LIGHT_OFF, action_light_off);

    // Optional: Add custom commands
    cmd_cache_entry_t custom = {
        .pattern = "おやすみモード",
        .pattern_alt = "おやすみ設定",
        .response = "おやすみモードを開始します。照明を消して、エアコンを調整します。",
        .category = CMD_CAT_SYSTEM,
        .action = CMD_ACTION_CUSTOM,
        .default_param = 0,
        .callback = NULL,  // Implement custom callback if needed
        .enabled = true
    };
    cmd_cache_add(&custom);

    printf("Command cache setup complete. %d commands registered.\n",
           cmd_cache_get_count());
}

/**
 * @brief Main voice command handler
 *
 * Flow:
 *   1. Try local cache match
 *   2. If hit → execute + respond
 *   3. If miss → forward to LLM
 *
 * @param voice_input Transcribed voice input (from SEN0540 or cloud STT)
 */
static void handle_voice_command(const char* voice_input) {
    cmd_match_result_t result;

    printf("Voice input: '%s'\n", voice_input);

    // Step 1: Try local cache
    if (cmd_cache_process(voice_input, &result)) {
        // Cache hit - respond immediately
        printf("Cache HIT (score: %d): %s\n", result.match_score, result.response);
        tts_speak(result.response);
        return;
    }

    // Step 2: Cache miss - forward to LLM
    printf("Cache MISS - forwarding to LLM...\n");

    char llm_response[256];
    if (llm_generate_response(voice_input, llm_response, sizeof(llm_response))) {
        printf("LLM response: %s\n", llm_response);
        tts_speak(llm_response);
    } else {
        printf("LLM failed, using fallback\n");
        tts_speak("すみません、よく分かりませんでした。");
    }
}

/**
 * @brief Example: Print cache statistics
 */
static void print_cache_stats(void) {
    cmd_cache_stats_t stats;
    cmd_cache_get_stats(&stats);

    float hit_rate = (stats.total_queries > 0)
        ? (float)stats.cache_hits * 100.0f / stats.total_queries
        : 0.0f;

    printf("\n=== Command Cache Statistics ===\n");
    printf("Total queries:    %lu\n", (unsigned long)stats.total_queries);
    printf("Cache hits:       %lu\n", (unsigned long)stats.cache_hits);
    printf("Cache misses:     %lu (LLM fallbacks)\n", (unsigned long)stats.cache_misses);
    printf("Hit rate:         %.1f%%\n", hit_rate);
    printf("Action successes: %lu\n", (unsigned long)stats.action_successes);
    printf("Action failures:  %lu\n", (unsigned long)stats.action_failures);
    printf("\nCategory breakdown:\n");
    printf("  Lighting: %lu\n", (unsigned long)stats.category_hits[CMD_CAT_LIGHTING]);
    printf("  Climate:  %lu\n", (unsigned long)stats.category_hits[CMD_CAT_CLIMATE]);
    printf("  Sensor:   %lu\n", (unsigned long)stats.category_hits[CMD_CAT_SENSOR]);
    printf("  Media:    %lu\n", (unsigned long)stats.category_hits[CMD_CAT_MEDIA]);
    printf("  System:   %lu\n", (unsigned long)stats.category_hits[CMD_CAT_SYSTEM]);
    printf("================================\n\n");
}

/*
 * Integration with SEN0540 (Offline Voice Recognition):
 *
 * The SEN0540 can detect fixed keywords locally. Combine with this cache:
 *
 *   SEN0540 Keywords    →  Command Cache   →  LLM Fallback
 *   ────────────────       ─────────────      ────────────
 *   "ライトオン"        →  Light ON         →  (not needed)
 *   "ライトオフ"        →  Light OFF        →  (not needed)
 *   "今何度"            →  Report Temp      →  (not needed)
 *   <unknown>           →  Cache lookup     →  LLM if miss
 *
 * This three-tier approach minimizes latency and API costs:
 *   Tier 1: SEN0540 keyword → instant (<10ms)
 *   Tier 2: Command cache   → fast (~10-50ms)
 *   Tier 3: LLM API call    → slow (~500-2000ms) + cost
 */
