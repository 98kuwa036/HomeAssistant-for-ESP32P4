/**
 * @file remo_client.h
 * @brief Nature Remo Local API Client
 *
 * Provides local network control of home appliances via Nature Remo.
 * Uses mDNS for auto-discovery and HTTP POST for IR signal transmission.
 *
 * Features:
 * - mDNS discovery of Nature Remo devices (_remo._tcp)
 * - SPIFFS-based appliance configuration
 * - Offline operation (no cloud required)
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Constants
// ============================================================================

#define REMO_MAX_APPLIANCES     16
#define REMO_MAX_SIGNALS        8
#define REMO_MAX_NAME_LEN       32
#define REMO_MAX_SIGNAL_LEN     64

// ============================================================================
// Data Structures
// ============================================================================

/**
 * @brief Appliance type enumeration
 */
typedef enum {
    REMO_TYPE_UNKNOWN = 0,
    REMO_TYPE_LIGHT,
    REMO_TYPE_AC,
    REMO_TYPE_TV,
    REMO_TYPE_FAN,
    REMO_TYPE_OTHER
} remo_appliance_type_t;

/**
 * @brief Signal definition (IR code identifier)
 */
typedef struct {
    char name[REMO_MAX_NAME_LEN];       // Signal name (e.g., "on", "off", "cool_25")
    char id[REMO_MAX_SIGNAL_LEN];       // Signal ID for API call
} remo_signal_t;

/**
 * @brief Appliance definition
 */
typedef struct {
    char id[REMO_MAX_NAME_LEN];         // Unique identifier
    char name[REMO_MAX_NAME_LEN];       // Display name
    remo_appliance_type_t type;         // Appliance type
    remo_signal_t signals[REMO_MAX_SIGNALS];
    int signal_count;
} remo_appliance_t;

/**
 * @brief Remo client state
 */
typedef struct {
    bool initialized;
    bool remo_found;
    char remo_ip[16];                   // IP address of Nature Remo
    uint16_t remo_port;                 // HTTP port (usually 80)
    remo_appliance_t appliances[REMO_MAX_APPLIANCES];
    int appliance_count;
} remo_client_state_t;

// ============================================================================
// Public API
// ============================================================================

/**
 * @brief Initialize Remo client
 *
 * Loads appliance configuration from SPIFFS and starts mDNS discovery.
 *
 * @return ESP_OK on success
 */
esp_err_t remo_client_init(void);

/**
 * @brief Deinitialize Remo client
 */
void remo_client_deinit(void);

/**
 * @brief Discover Nature Remo via mDNS
 *
 * Searches for _remo._tcp service on local network.
 * Blocks for up to timeout_ms milliseconds.
 *
 * @param timeout_ms Discovery timeout in milliseconds
 * @return ESP_OK if found, ESP_ERR_NOT_FOUND otherwise
 */
esp_err_t remo_client_discover(uint32_t timeout_ms);

/**
 * @brief Send IR signal to control appliance
 *
 * @param appliance_name Name of the appliance (e.g., "living_room_light")
 * @param signal_name Name of the signal (e.g., "on", "off")
 * @return ESP_OK on success
 */
esp_err_t remo_client_send_quick(const char *appliance_name, const char *signal_name);

/**
 * @brief Send raw IR signal by ID
 *
 * @param signal_id Signal ID from appliance configuration
 * @return ESP_OK on success
 */
esp_err_t remo_client_send_signal(const char *signal_id);

/**
 * @brief Get appliance by name
 *
 * @param name Appliance name
 * @return Pointer to appliance or NULL if not found
 */
const remo_appliance_t *remo_client_get_appliance(const char *name);

/**
 * @brief Get all appliances
 *
 * @param count Output: number of appliances
 * @return Array of appliances
 */
const remo_appliance_t *remo_client_get_appliances(int *count);

/**
 * @brief Check if Remo is available
 *
 * @return true if Remo was discovered and is reachable
 */
bool remo_client_is_available(void);

/**
 * @brief Get current client state
 *
 * @param state Output state structure
 */
void remo_client_get_state(remo_client_state_t *state);

/**
 * @brief Manually set Remo IP address
 *
 * Use this if mDNS discovery fails.
 *
 * @param ip IP address string (e.g., "192.168.1.100")
 * @return ESP_OK on success
 */
esp_err_t remo_client_set_ip(const char *ip);

#ifdef __cplusplus
}
#endif
