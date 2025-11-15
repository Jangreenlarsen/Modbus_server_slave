// ============================================================================
//  Filnavn : modbus_counters_hw.cpp
//  Projekt  : Modbus RTU Server / CLI
//  Version  : v3.6.1 (2025-11-14)
//  Forfatter: JanG at modbus_slave@laces.dk
//  Formål   : Hardware Counter Engine - Timer5 ONLY implementation.
//             ISR for Timer5 overflow events (external clock mode on Pin 2).
//             Supports deterministic, interrupt-driven pulse counting.
//             Unified prescaler strategy: ALL pulses counted, prescaler in software.
// ============================================================================

#include "modbus_counters_hw.h"

// ============================================================================
// Global HW Counter State (Timer5 only)
// ============================================================================

// Timer5 32-bit value = (hwCounter5Extend << 16) | TCNT5
// hwCounter5Extend holds overflow count (increments on overflow from TCNT5)
volatile uint32_t hwCounter5Extend = 0;

// Overflow tracking for frequency measurement
volatile uint16_t hwOverflowCount = 0;

// ============================================================================
// ISR - Timer5 Overflow (Pin 2 / PE4 / T5, external clock mode)
// ============================================================================
// CRITICAL: ISRs must NEVER call micros() or millis() - causes RAM corruption!
// Timer5 counts external pulses on T5 pin (Pin 2 / PE4) in TCCR5B = 0x07 mode.
// Overflow occurs when TCNT5 increments from 0xFFFF to 0x0000.
ISR(TIMER5_OVF_vect) {
  hwCounter5Extend++;  // Extend counter for 32-bit support
  hwOverflowCount++;   // Track overflows for frequency validation
}

// ============================================================================
// Initialize HW Counter (Timer5 only)
// ============================================================================
// counter_id: MUST be 4 (Timer5 is only supported timer)
// mode: 0 = stop timer, 1 = external clock mode (T5 pin, rising edge)
// start_value: initial counter value (32-bit: high 16 bits = extend, low 16 bits = TCNT5)
// Returns true if successful, false if counter_id != 4
//
// HARDWARE ARCHITECTURE:
// - Timer5 pin: Pin 47 (PL2/T5) on Arduino Mega 2560
//   CRITICAL v3.6.2 BUGFIX: Was incorrectly documented as Pin 2 (PE4)
//   See: https://docs.arduino.cc/retired/hacking/hardware/PinMapping2560/
// - Clock source: ONLY external clock mode (TCCR5B = 0x07) for pulse counting
// - Prescaler: Implemented 100% in software (not used in hardware)
//   Reason: ATmega2560 external clock mode does NOT support hardware prescaler
//   Internal prescaler modes (0x02-0x06) count system clock, NOT T5 pin pulses!
// - Overflow: TCNT5 increments from 0x0000 to 0xFFFF, then overflows (triggers ISR)
// - 32-bit support: hwCounter5Extend tracks overflows for values > 65535
// - NOTE v3.6.2: DO NOT GPIO-map Timer5 pin! GPIO polling interferes with counting!
bool hw_counter_init(uint8_t counter_id, uint8_t mode, uint32_t start_value) {
  // CRITICAL: ONLY Timer5 is supported!
  // Timer1, Timer3, Timer4 external clock inputs NOT routed on Arduino Mega 2560
  if (counter_id != 4) return false;

  // Validate mode: 0 = stop, 1 = external clock
  if (mode > 1) return false;

  // Configure TCCR5B for external or stopped
  uint8_t tccrb_clksel;
  if (mode == 0) {
    tccrb_clksel = 0x00;  // Stop timer (no clock source)
  } else {
    tccrb_clksel = 0x07;  // External clock mode: rising edge on T5 pin
  }

  // Parse 32-bit start_value into 16-bit parts
  uint16_t tcnt_val = (uint16_t)(start_value & 0xFFFF);      // Lower 16 bits for TCNT5
  uint16_t extend_val = (uint16_t)(start_value >> 16);       // Upper 16 bits for extension

  // Configure Timer5 (Pin 47 input / PL2 / T5)
  if (mode != 0) {
    // v3.6.2 BUGFIX: Timer5 T5 external clock is Pin 47 (PL2), NOT Pin 2!
    // DO NOT call pinMode() here - GPIO polling will interfere with hardware counting
    // Timer5 T5 external clock (Pin 47) must be read by hardware ONLY, not by GPIO polling
    // pinMode(47, INPUT) is NOT called because counters_config_set() explicitly prevents GPIO-mapping
  }

  // Atomic setup (disable interrupts during configuration)
  cli();

  TCCR5B = 0x00;                    // STOP timer (clear clock source bits)
  TCCR5A = 0x00;                    // Normal mode (not PWM)
  TCNT5 = tcnt_val;                 // Set initial counter value
  hwOverflowCount = 0;              // Clear overflow tracking
  hwCounter5Extend = extend_val;    // Set extension for 32-bit support
  TIFR5 = 0x01;                     // Clear overflow flag (TOV5, bit 0)
  TIMSK5 = 0x00;                    // Disable interrupt temporarily

  TCCR5B = tccrb_clksel;            // Apply clock source (start counting)
  TIMSK5 = (mode != 0) ? 0x01 : 0x00;  // Enable overflow interrupt if mode=1

  sei();  // Re-enable interrupts

  return true;
}

// ============================================================================
// Get Combined Counter Value (Timer5 only)
// ============================================================================
// Returns 32-bit value: (hwCounter5Extend << 16) | TCNT5
// Safely reads volatile registers with interrupt protection
uint32_t hw_counter_get_value(uint8_t counter_id) {
  // Only Timer5 (counter_id=4) is supported
  if (counter_id != 4) return 0;

  uint16_t tcnt = 0;
  uint32_t extend = 0;

  // Atomic read (protect against ISR modifications)
  cli();
  extend = hwCounter5Extend;
  tcnt = TCNT5;
  sei();

  // Combine: (extend << 16) | tcnt for 32-bit value
  return (extend << 16) | tcnt;
}

// ============================================================================
// Reset HW Counter (Timer5 only)
// ============================================================================
// Resets both TCNT5 and extension counter to 0
void hw_counter_reset(uint8_t counter_id) {
  // Only Timer5 (counter_id=4) is supported
  if (counter_id != 4) return;

  cli();
  TCNT5 = 0;
  hwCounter5Extend = 0;
  hwOverflowCount = 0;
  sei();
}

// ============================================================================
// Reset Timer5 Counter to Specific Value (for reset-on-read)
// ============================================================================
void hw_counter_reset_to_value(uint8_t counter_id, uint32_t start_value) {
  // Only Timer5 (counter_id=4) is supported
  if (counter_id != 4) return;

  // Parse 32-bit start_value into 16-bit parts
  uint16_t tcnt_val = (uint16_t)(start_value & 0xFFFF);      // Lower 16 bits for TCNT5
  uint16_t extend_val = (uint16_t)(start_value >> 16);       // Upper 16 bits for extension

  cli();
  TCNT5 = tcnt_val;
  hwCounter5Extend = extend_val;
  hwOverflowCount = 0;
  sei();
}

// ============================================================================
// Reset Frequency Measurement Tracking
// ============================================================================
// NOTE: This function accesses static variables from hw_counter_update_frequency
// by declaring extern static arrays here - not ideal but necessary for resetting
// the frequency tracking state when counter is reset.
void hw_counter_reset_frequency(uint8_t counter_id) {
  if (counter_id < 1 || counter_id > 4) return;

  // We need to mark this counter as needing reinitialization
  // This is handled by the initialized flag in hw_counter_update_frequency
  // Since we can't access static variables from another function, we rely on
  // the fact that after reset, hwOverflowCount is 0, and the first frequency
  // update will detect this anomaly and reinitialize automatically
}

// ============================================================================
// Stop HW Counter (Timer5 only)
// ============================================================================
// Stops Timer5 (no clock source) and disables overflow interrupt
void hw_counter_stop(uint8_t counter_id) {
  // Only Timer5 (counter_id=4) is supported
  if (counter_id != 4) return;

  cli();
  TCCR5B = 0x00;   // Clear clock source (stop timer)
  TIMSK5 = 0x00;   // Disable overflow interrupt
  sei();
}

// ============================================================================
// Update Frequency Measurement (Timer5 only)
// ============================================================================
// Measures frequency in Hz with 1-2 second timing window
// Updates freq_reg with value in range 0-20000 Hz
// Called from counters_loop() approximately once per second
void hw_counter_update_frequency(uint16_t freq_reg, uint8_t counter_id) {
  // Validate parameters
  if (freq_reg >= NUM_REGS || counter_id != 4) return;

  // Static tracking variables for Timer5 frequency measurement
  static uint32_t lastCounterValue = 0;
  static unsigned long lastFreqUpdateMs = 0;
  static bool initialized = false;

  unsigned long nowMs = millis();

  // Get current counter value (combines hwCounter5Extend + TCNT5)
  uint32_t currentCounterValue = hw_counter_get_value(counter_id);

  // Initialize on first call
  if (!initialized) {
    lastCounterValue = currentCounterValue;
    lastFreqUpdateMs = nowMs;
    initialized = true;
    holdingRegs[freq_reg] = 0;
    return;
  }

  unsigned long timeDeltaMs = nowMs - lastFreqUpdateMs;

  // Detect counter reset: counter value decreased (went backwards)
  // This happens when hw_counter_reset() is called
  if (currentCounterValue < lastCounterValue) {
    // Counter was reset - reinitialize tracking
    lastCounterValue = currentCounterValue;
    lastFreqUpdateMs = nowMs;
    holdingRegs[freq_reg] = 0;
    return;
  }

  // Update frequency every ~1 second (1000-2000 ms window for stability)
  if (timeDeltaMs >= 1000 && timeDeltaMs <= 2000) {
    // Calculate pulse delta from counter value
    // NOTE: Hardware always counts ALL pulses (external clock mode).
    // Prescaler is handled in software (see modbus_counters.cpp).
    // So pulseDelta represents actual pulse count without prescaler compensation.
    uint32_t pulseDelta = currentCounterValue - lastCounterValue;

    // Sanity check: if pulseDelta is unreasonably large, skip measurement
    // (e.g., pulseDelta > 100000 @ 1sec = >100kHz, but design max is 20kHz)
    if (pulseDelta > 100000UL) {
      // Skip this measurement as anomalous
      lastCounterValue = currentCounterValue;
      lastFreqUpdateMs = nowMs;
      return;
    }

    // Frequency = pulses per second
    // pulseDelta / timeDeltaMs * 1000 = Hz
    uint32_t freqHz = (pulseDelta * 1000UL) / timeDeltaMs;

    // Clamp to reasonable range (0-20000 Hz by design)
    if (freqHz > 20000UL) freqHz = 20000UL;

    // Write to Modbus register
    holdingRegs[freq_reg] = (uint16_t)freqHz;

    // Update tracking for next measurement
    lastCounterValue = currentCounterValue;
    lastFreqUpdateMs = nowMs;
  }
  // If time delta is > 5000ms, reset tracking to avoid stale measurements
  else if (timeDeltaMs > 5000) {
    lastCounterValue = currentCounterValue;
    lastFreqUpdateMs = nowMs;
    holdingRegs[freq_reg] = 0;  // Reset frequency to 0 (timeout)
  }
}

