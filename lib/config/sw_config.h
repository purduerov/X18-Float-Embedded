#ifndef SW_CONFIG_H
#define SW_CONFIG_H

// ==========================================
// 1. ACTUATOR & PID PARAMETERS
// ==========================================
#define DEFAULT_ACTUATOR_POS 4095
#define DEFAULT_NEUTRAL_ADC 2048
#define ACT_POS_TOL 20
#define ACT_ADC_MAX 4095
#define ACT_MOVE_TIMEOUT_MS 8000
#define ACT_STALL_MS 1000
#define ACT_RETRY_BACKOFF_MS 2000
#define ACT_MAX_RETRIES 1
#define ACT_FILTER_SIZE 5
#define ACT_STALL_THRESHOLD 2

#define ACT_KP 0.8f
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
#define ARRIVAL_BAND_M 0.33f
#define PROFILING_SAFETY_TIMEOUT_S 60
#define INTEGRAL_GATE_M 0.5f

// --- STALL DETECTION (Early Abort) ---
#define STALL_CHECK_DURATION_MS 15000
#define STALL_DEPTH_THRESHOLD_M 0.01f

#define DEFAULT_PID_P 4.00f  // Mapped to Braking Time Constant (seconds)
#define DEFAULT_PID_I 300.0f // Mapped to Braking Effort (ADC units)
#define DEFAULT_PID_D 0.15f  // Mapped to Hover Deadband (meters)

#define TRANSIT_THRESHOLD_M 0.5f
#define TRANSIT_P_MULTIPLIER 2.0f

// --- Hybrid Buoyancy Control Constants ---
#define VELOCITY_EMA_ALPHA 0.30f
#define BRAKING_TIME_S 4.00f
#define BRAKING_EFFORT_ADC 300
#define HOVER_DEADBAND_M 0.15f
#define HOVER_ASYMM_SHALLOW_UP 0.65f // Drift deep limit before nudging up
#define HOVER_ASYMM_SHALLOW_DOWN                                               \
  0.42f // Drift shallow limit before nudging down
#define NUDGE_STEP_ADC 100
#define NUDGE_WAIT_S 8.00f
#define HOVER_RECOVERY_M 0.50f

// ==========================================
// 2. MISSION & SENSOR FAIL-SAFES
// ==========================================
#define SENSOR_RESET_STRIKES 5
#define I2C_BUS_RESET_STRIKES 10
#define I2C_BUS_RESET_THROTTLE_MS 2000
#define SENSOR_ABORT_STRIKES 50

#define SAMPLE_INTERVAL_MS 1000
#define MAX_RECORDED_SAMPLES 1000

#define RADIO_DONE_BROADCAST_MS 3000
#define RADIO_DATA_RETRANSMIT_MS 2000
#define RADIO_TEST_TX_MS 1000

// ==========================================
// 3. SYSTEM SETTINGS
// ==========================================
#define FLOAT_ENABLE_USB_WAIT 0
#define SURFACE_ENABLE_USB_WAIT 1

// ==========================================
// 4. FIRMWARE VERSION
// ==========================================
#define FIRMWARE_VERSION 101

#endif // SW_CONFIG_H
