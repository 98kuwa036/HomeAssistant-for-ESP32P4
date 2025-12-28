/**
 * @file remo_client.c
 * @brief Nature Remo Local API Client Implementation
 */

#include "remo_client.h"
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_spiffs.h"
#include "mdns.h"
#include "cJSON.h"

static const char *TAG = "remo_client";

// ============================================================================
// Internal State
// ============================================================================

static remo_client_state_t s_remo = {0};

// ============================================================================
// SPIFFS Configuration Loader
// ============================================================================

static esp_err_t mount_spiffs(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        }
        return ret;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info("storage", &total, &used);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "SPIFFS: %d/%d bytes used", used, total);
    }

    return ESP_OK;
}

static remo_appliance_type_t parse_type(const char *type_str)
{
    if (!type_str) return REMO_TYPE_UNKNOWN;
    if (strcmp(type_str, "LIGHT") == 0) return REMO_TYPE_LIGHT;
    if (strcmp(type_str, "AC") == 0) return REMO_TYPE_AC;
    if (strcmp(type_str, "TV") == 0) return REMO_TYPE_TV;
    if (strcmp(type_str, "FAN") == 0) return REMO_TYPE_FAN;
    return REMO_TYPE_OTHER;
}

static esp_err_t load_appliances_from_spiffs(void)
{
    FILE *f = fopen("/spiffs/appliances.json", "r");
    if (!f) {
        ESP_LOGW(TAG, "appliances.json not found, using empty config");
        return ESP_OK;
    }

    // Get file size
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize > 32768) {
        ESP_LOGE(TAG, "appliances.json too large");
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    char *json_str = malloc(fsize + 1);
    if (!json_str) {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    fread(json_str, 1, fsize, f);
    json_str[fsize] = '\0';
    fclose(f);

    // Parse JSON
    cJSON *root = cJSON_Parse(json_str);
    free(json_str);

    if (!root) {
        ESP_LOGE(TAG, "Failed to parse appliances.json");
        return ESP_ERR_INVALID_ARG;
    }

    // Check for IP hint
    cJSON *ip_hint = cJSON_GetObjectItem(root, "remo_ip_hint");
    if (ip_hint && cJSON_IsString(ip_hint) && strlen(ip_hint->valuestring) > 0) {
        strncpy(s_remo.remo_ip, ip_hint->valuestring, sizeof(s_remo.remo_ip) - 1);
        s_remo.remo_port = 80;
        s_remo.remo_found = true;
        ESP_LOGI(TAG, "Using IP hint: %s", s_remo.remo_ip);
    }

    // Parse appliances array
    cJSON *appliances = cJSON_GetObjectItem(root, "appliances");
    if (appliances && cJSON_IsArray(appliances)) {
        int count = 0;
        cJSON *app;
        cJSON_ArrayForEach(app, appliances) {
            if (count >= REMO_MAX_APPLIANCES) break;

            remo_appliance_t *a = &s_remo.appliances[count];

            cJSON *id = cJSON_GetObjectItem(app, "id");
            cJSON *name = cJSON_GetObjectItem(app, "name");
            cJSON *type = cJSON_GetObjectItem(app, "type");
            cJSON *signals = cJSON_GetObjectItem(app, "signals");

            if (id && cJSON_IsString(id)) {
                strncpy(a->id, id->valuestring, REMO_MAX_NAME_LEN - 1);
            }
            if (name && cJSON_IsString(name)) {
                strncpy(a->name, name->valuestring, REMO_MAX_NAME_LEN - 1);
            }
            if (type && cJSON_IsString(type)) {
                a->type = parse_type(type->valuestring);
            }

            // Parse signals
            if (signals && cJSON_IsObject(signals)) {
                int sig_count = 0;
                cJSON *sig;
                cJSON_ArrayForEach(sig, signals) {
                    if (sig_count >= REMO_MAX_SIGNALS) break;
                    if (cJSON_IsString(sig)) {
                        strncpy(a->signals[sig_count].name, sig->string, REMO_MAX_NAME_LEN - 1);
                        strncpy(a->signals[sig_count].id, sig->valuestring, REMO_MAX_SIGNAL_LEN - 1);
                        sig_count++;
                    }
                }
                a->signal_count = sig_count;
            }

            count++;
        }
        s_remo.appliance_count = count;
        ESP_LOGI(TAG, "Loaded %d appliances", count);
    }

    cJSON_Delete(root);
    return ESP_OK;
}

// ============================================================================
// mDNS Discovery
// ============================================================================

esp_err_t remo_client_discover(uint32_t timeout_ms)
{
    ESP_LOGI(TAG, "Discovering Nature Remo via mDNS...");

    // Initialize mDNS if not already done
    esp_err_t ret = mdns_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "mDNS init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Search for _remo._tcp service
    mdns_result_t *results = NULL;
    ret = mdns_query_ptr("_remo", "_tcp", timeout_ms, 5, &results);

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "mDNS query failed: %s", esp_err_to_name(ret));
        return ret;
    }

    if (!results) {
        ESP_LOGW(TAG, "No Nature Remo found on network");
        return ESP_ERR_NOT_FOUND;
    }

    // Use first result
    mdns_result_t *r = results;
    if (r->addr && r->addr->addr.type == ESP_IPADDR_TYPE_V4) {
        snprintf(s_remo.remo_ip, sizeof(s_remo.remo_ip), IPSTR,
                 IP2STR(&r->addr->addr.u_addr.ip4));
        s_remo.remo_port = r->port ? r->port : 80;
        s_remo.remo_found = true;

        ESP_LOGI(TAG, "Found Nature Remo at %s:%d", s_remo.remo_ip, s_remo.remo_port);
    }

    mdns_query_results_free(results);
    return s_remo.remo_found ? ESP_OK : ESP_ERR_NOT_FOUND;
}

// ============================================================================
// HTTP Client for IR Signal Transmission
// ============================================================================

esp_err_t remo_client_send_signal(const char *signal_id)
{
    if (!s_remo.remo_found || !signal_id) {
        return ESP_ERR_INVALID_STATE;
    }

    char url[128];
    snprintf(url, sizeof(url), "http://%s:%d/messages",
             s_remo.remo_ip, s_remo.remo_port);

    ESP_LOGI(TAG, "Sending signal %s to %s", signal_id, url);

    // Prepare POST body
    char post_data[256];
    snprintf(post_data, sizeof(post_data), "{\"id\":\"%s\"}", signal_id);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 5000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        return ESP_ERR_NO_MEM;
    }

    // IMPORTANT: Nature Remo local API requires this header
    esp_http_client_set_header(client, "X-Requested-With", "curl");
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    esp_err_t ret = esp_http_client_perform(client);
    if (ret == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP Status: %d", status);
        if (status != 200) {
            ret = ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(ret));
    }

    esp_http_client_cleanup(client);
    return ret;
}

// ============================================================================
// Public API Implementation
// ============================================================================

esp_err_t remo_client_init(void)
{
    if (s_remo.initialized) {
        ESP_LOGW(TAG, "Remo client already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing Nature Remo client...");

    memset(&s_remo, 0, sizeof(s_remo));
    s_remo.remo_port = 80;

    // Mount SPIFFS and load configuration
    esp_err_t ret = mount_spiffs();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SPIFFS mount failed, continuing without config");
    } else {
        load_appliances_from_spiffs();
    }

    // Try mDNS discovery if no IP hint was provided
    if (!s_remo.remo_found) {
        remo_client_discover(3000);
    }

    s_remo.initialized = true;
    ESP_LOGI(TAG, "Remo client initialized. Remo %s",
             s_remo.remo_found ? "found" : "not found");

    return ESP_OK;
}

void remo_client_deinit(void)
{
    if (!s_remo.initialized) return;

    esp_vfs_spiffs_unregister("storage");
    memset(&s_remo, 0, sizeof(s_remo));
}

esp_err_t remo_client_send_quick(const char *appliance_name, const char *signal_name)
{
    if (!s_remo.initialized || !appliance_name || !signal_name) {
        return ESP_ERR_INVALID_ARG;
    }

    // Find appliance
    const remo_appliance_t *app = remo_client_get_appliance(appliance_name);
    if (!app) {
        ESP_LOGW(TAG, "Appliance '%s' not found", appliance_name);
        return ESP_ERR_NOT_FOUND;
    }

    // Find signal
    for (int i = 0; i < app->signal_count; i++) {
        if (strcmp(app->signals[i].name, signal_name) == 0) {
            return remo_client_send_signal(app->signals[i].id);
        }
    }

    ESP_LOGW(TAG, "Signal '%s' not found in appliance '%s'", signal_name, appliance_name);
    return ESP_ERR_NOT_FOUND;
}

const remo_appliance_t *remo_client_get_appliance(const char *name)
{
    if (!name) return NULL;

    for (int i = 0; i < s_remo.appliance_count; i++) {
        if (strcmp(s_remo.appliances[i].id, name) == 0 ||
            strcmp(s_remo.appliances[i].name, name) == 0) {
            return &s_remo.appliances[i];
        }
    }
    return NULL;
}

const remo_appliance_t *remo_client_get_appliances(int *count)
{
    if (count) {
        *count = s_remo.appliance_count;
    }
    return s_remo.appliances;
}

bool remo_client_is_available(void)
{
    return s_remo.initialized && s_remo.remo_found;
}

void remo_client_get_state(remo_client_state_t *state)
{
    if (state) {
        memcpy(state, &s_remo, sizeof(remo_client_state_t));
    }
}

esp_err_t remo_client_set_ip(const char *ip)
{
    if (!ip || strlen(ip) == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    strncpy(s_remo.remo_ip, ip, sizeof(s_remo.remo_ip) - 1);
    s_remo.remo_port = 80;
    s_remo.remo_found = true;

    ESP_LOGI(TAG, "Remo IP manually set to: %s", s_remo.remo_ip);
    return ESP_OK;
}
