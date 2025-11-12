// ============================================================================
//  Filnavn : modbus_counters_hw.h
//  Projekt  : Modbus RTU Server / CLI
//  Version  : v3.3.0 (2025-11-11)
//  Forfatter: JanG at modbus_slave@laces.dk
//  Formål   : Hardware Counter Engine med interrupt-baseret telling.
//             Bruger Mega2560's Timer1, Timer3, Timer4, Timer5 til
//             uafhængig, deterministic counter functionality.
//  Ændringer:
//    - v3.3.0: Initial implementation af HW counter engine
//              Tilføjet ISRs for Timer1, Timer3, Timer4, Timer5
//              Support for external clock (pulse input) mode
// ============================================================================

#pragma once
#include <Arduino.h>
#include "modbus_globals.h"

// ============================================================================
// HW Counter State (per counter)
// ============================================================================

// Extension counters for 16-bit HW timers (supports > 16-bit values)
extern volatile uint32_t hwCounter1Extend;  // Timer1 overflow extension
extern volatile uint32_t hwCounter3Extend;  // Timer3 overflow extension
extern volatile uint32_t hwCounter4Extend;  // Timer4 overflow extension
extern volatile uint32_t hwCounter5Extend;  // Timer5 overflow extension

// Frequency tracking via overflow count (ISR-based, deterministic)
extern volatile uint16_t hwOverflowCount[4];          // Overflow count per counter

// ============================================================================
// API Functions
// ============================================================================

// Initialize HW timer for counter (id = 1..4 -> Timer1/3/4/5)
// prescaler: 0=no clock, 1=external clock (rising edge), 8/64/256/1024 for internal
// Returns true if successful, false if id invalid
bool hw_counter_init(uint8_t counter_id, uint8_t mode);

// Get combined counter value (extension + TCNT register)
// Returns 32-bit value combining HW counter overflow extension + current count
uint32_t hw_counter_get_value(uint8_t counter_id);

// Reset HW counter to zero
void hw_counter_reset(uint8_t counter_id);

// Stop/disable HW timer
void hw_counter_stop(uint8_t counter_id);

// Update frequency measurement based on overflow count (called from counters_loop)
void hw_counter_update_frequency(uint16_t freq_reg, uint8_t counter_id);

// Reset frequency measurement tracking (called after counter reset)
void hw_counter_reset_frequency(uint8_t counter_id);

