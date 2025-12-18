/**
 * @file paj7620.h
 * @brief PAJ7620U2 Gesture Recognition Sensor Driver
 *
 * PixArt PAJ7620U2 features:
 *   - 9 gesture recognition (up/down/left/right/forward/backward/CW/CCW/wave)
 *   - Built-in proximity detection
 *   - Interrupt output for gesture events
 *   - Low power consumption
 *
 * I2C Address: 0x73
 * I2C Speed: Up to 400kHz
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

// I2C Address
#define PAJ7620_I2C_ADDR            0x73

// Gesture types
typedef enum {
    PAJ7620_GESTURE_NONE = 0,
    PAJ7620_GESTURE_UP = 0x01,
    PAJ7620_GESTURE_DOWN = 0x02,
    PAJ7620_GESTURE_LEFT = 0x04,
    PAJ7620_GESTURE_RIGHT = 0x08,
    PAJ7620_GESTURE_FORWARD = 0x10,
    PAJ7620_GESTURE_BACKWARD = 0x20,
    PAJ7620_GESTURE_CLOCKWISE = 0x40,
    PAJ7620_GESTURE_COUNTERCLOCKWISE = 0x80,
    PAJ7620_GESTURE_WAVE = 0x100,
} paj7620_gesture_t;

// Operating modes
typedef enum {
    PAJ7620_MODE_GESTURE = 0,
    PAJ7620_MODE_PROXIMITY = 1,
} paj7620_mode_t;

/**
 * @brief Gesture data
 */
typedef struct {
    paj7620_gesture_t gesture;      // Detected gesture
    uint8_t wave_count;             // Number of waves (for wave gesture)
    bool gesture_detected;          // New gesture available
    uint32_t timestamp_ms;          // Detection timestamp
} paj7620_data_t;

/**
 * @brief Proximity data
 */
typedef struct {
    uint8_t brightness;             // Object brightness (0-255)
    uint16_t object_size;           // Object size
    bool object_detected;           // Object in range
} paj7620_proximity_t;

/**
 * @brief Configuration
 */
typedef struct {
    i2c_port_t i2c_port;
    paj7620_mode_t mode;
    int int_pin;                    // GPIO for interrupt (-1 to disable)
} paj7620_config_t;

#define PAJ7620_DEFAULT_CONFIG() { \
    .i2c_port = I2C_NUM_0, \
    .mode = PAJ7620_MODE_GESTURE, \
    .int_pin = -1, \
}

esp_err_t paj7620_init(const paj7620_config_t *config);
esp_err_t paj7620_deinit(void);
esp_err_t paj7620_read_gesture(paj7620_data_t *data);
esp_err_t paj7620_read_proximity(paj7620_proximity_t *data);
esp_err_t paj7620_set_mode(paj7620_mode_t mode);
bool paj7620_is_gesture_available(void);
bool paj7620_is_connected(void);
const char *paj7620_gesture_to_string(paj7620_gesture_t gesture);

#ifdef __cplusplus
}
#endif
