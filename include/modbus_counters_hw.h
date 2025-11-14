// ============================================================================
//  Filnavn : modbus_counters_hw.h
//  Projekt  : Modbus RTU Server / CLI
//  Version  : v3.6.1 (2025-11-14)
//  Forfatter: JanG at modbus_slave@laces.dk
//  Formål   : Hardware Counter Engine - Timer5 only implementation.
//             Bruger Mega2560's Timer5 (Pin 47, PL2/T5) til
//             deterministic, interrupt-baseret pulse counting.
//             Unified prescaler strategy (v3.6.0+).
//  Hardware Limitation:
//    - Only Timer5 (T5/PL2/Pin 47) praktisk accessible on Arduino Mega
//    - Timer1, Timer3, Timer4 NOT routed to board headers
//    - Timer5 external clock mode does NOT support HW prescaler
//    - Prescaler implemented 100% in software (see modbus_counters.cpp)
// ============================================================================

#pragma once
#include <Arduino.h>
#include "modbus_globals.h"

// ============================================================================
// HW Counter State (Timer5 only)
// ============================================================================

// Extension counter for 16-bit Timer5 (supports 32-bit total value)
extern volatile uint32_t hwCounter5Extend;  // Timer5 overflow extension (16 bit overflow counter)

// Overflow tracking for frequency measurement
extern volatile uint16_t hwOverflowCount;   // Overflow count for Timer5

// ============================================================================
// API Functions (Timer5 only)
// ============================================================================

// Initialize HW Timer5 for pulse counting via external clock (Pin 47 / PL2 / T5)
// mode: 0=stop timer, 1=external clock (rising edge on T5 pin)
// start_value: initial value to set the counter to (default 0)
// Returns true if successful, false if mode invalid
// NOTE: counter_id parameter kept for interface compatibility (only value 4 allowed)
bool hw_counter_init(uint8_t counter_id, uint8_t mode, uint32_t start_value = 0);

// Get combined counter value (extension + TCNT5 register)
// Returns 32-bit value: (hwCounter5Extend << 16) | TCNT5
// Supports counting beyond 16-bit overflow with software extension
uint32_t hw_counter_get_value(uint8_t counter_id);

// Reset Timer5 counter to zero (resets both TCNT5 and extension)
void hw_counter_reset(uint8_t counter_id);

// Stop/disable Timer5 (no clock source)
void hw_counter_stop(uint8_t counter_id);

// Update frequency measurement in Hz (1-2 sec timing window with validation)
// Called from counters_loop() approximately once per second
void hw_counter_update_frequency(uint16_t freq_reg, uint8_t counter_id);

// Reset frequency measurement tracking (called after counter reset)
void hw_counter_reset_frequency(uint8_t counter_id);

