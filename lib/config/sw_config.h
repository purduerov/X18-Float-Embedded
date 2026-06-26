#ifndef SW_CONFIG_H
#define SW_CONFIG_H

// ==========================================
// 1. ACTUATOR & PID PARAMETERS
// ==========================================
#define DEFAULT_ACTUATOR_POS 4095
#define DEFAULT_NEUTRAL_ADC 1850
#define ACT_POS_TOL 20
#define ACT_ADC_MAX 4095
#define ACT_MOVE_TIMEOUT_MS 12000
#define ACT_STALL_MS 600
#define ACT_RETRY_BACKOFF_MS 2000
#define ACT_MAX_RETRIES 1
#define ACT_FILTER_SIZE 5
#define ACT_STALL_THRESHOLD 2

#define ACT_KP 1.2f
#define ACT_KI 0.05f
#define ACT_KD 0.15f
#define ACT_LOOP_MS 20
#define ACT_PID_LIMIT 500.0f

#define ACT_VREF_PWM_WRAP 65535
#define ACT_VREF_MIN_DUTY 19859
#define ACT_VREF_MAX_DUTY 65535

#define ACT_DEADZONE_ENTER 20
#define ACT_DEADZONE_EXIT 50

#define DEPTH_PID_LOOP_MS 100
#define SURFACE_DEBUG_INTERVAL_MS 2000
#define DEEP_TOL_M 0.33f
#define SHALLOW_TOL_M 0.1f
#define DEEP_PROFILING_SAFETY_TIMEOUT_S 1000
#define SHALLOW_PROFILING_SAFETY_TIMEOUT_S 1000

// --- STALL DETECTION (Early Abort) ---
#define STALL_CHECK_DURATION_MS 15000
#define STALL_DEPTH_THRESHOLD_M 0.01f

#define DEFAULT_PID_P                                                          \
  120.0f // Moderate proportional gain to overcome motor/syringe friction
#define DEFAULT_PID_I                                                          \
  0.5f // Gentle integral accumulation for buoyancy centering
#define DEFAULT_PID_D                                                          \
  25.0f // Balanced derivative gain to damp overshoot and oscillation

#define TRANSIT_THRESHOLD_M 0.5f
#define TRANSIT_P_MULTIPLIER 2.0f



// --- Mission & Control Named Thresholds (avoids magic numbers in logic) ---
#define SHALLOW_BIASED_TARGET_M                                                \
  0.55f // Effective target for shallow hold to avoid surfacing
#define SURFACE_DETECTION_M                                                    \
  0.05f // Depth at which we consider the float surfaced
#define ADAPTIVE_HOLD_THRESHOLD_M                                              \
  0.15f // Max depth error to accumulate adaptive neutral ADC
#define NEUTRAL_ADC_MIN_VALID 1300U // Sanity bounds for learned neutral ADC
#define NEUTRAL_ADC_MAX_VALID 2500U

// ==========================================
// 2. MISSION & SENSOR FAIL-SAFES
// ==========================================
#define SENSOR_RESET_STRIKES 5
#define I2C_BUS_RESET_STRIKES 10
#define I2C_BUS_RESET_THROTTLE_MS 2000
#define SENSOR_ABORT_STRIKES 50

#define SAMPLE_INTERVAL_MS 1000
#define MAX_SAMPLES_PER_STAGE 50
#define MAX_RECORDED_SAMPLES                                                   \
  (MAX_SAMPLES_PER_STAGE * 2 * 3) // Enough for 3 profiles with 2 stages each

#define RADIO_DONE_BROADCAST_MS 3000
#define RADIO_DATA_RETRANSMIT_MS 2000
#define RADIO_TEST_TX_MS 1000
#define RADIO_MAX_DATA_RETRIES 10
#define SURFACE_DOWNLOAD_TIMEOUT_MS 60000

// ==========================================
// 3. SYSTEM SETTINGS
// ==========================================
#define FLOAT_ENABLE_USB_WAIT 1
#define SURFACE_ENABLE_USB_WAIT 1

// ==========================================
// 4. FIRMWARE VERSION
// ==========================================
#define FIRMWARE_VERSION 101

#endif // SW_CONFIG_H
