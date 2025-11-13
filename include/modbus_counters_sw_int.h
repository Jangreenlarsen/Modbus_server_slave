// ============================================================================
//  Filnavn : modbus_counters_sw_int.h
//  Projekt  : Modbus RTU Server / CLI
//  Version  : v3.3.0 (2025-11-13)
//  Forfatter: JanG at modbus_slave@laces.dk
//  Formål   : External interrupt support for SW-mode counters.
//             Allows edge detection via hardware interrupts on INT0-INT5 pins
//             to prevent CLI operations from blocking counter inputs.
//
//  Arduino Mega 2560 External Interrupt Pins:
//    - INT0 (pin 21)  - attachInterrupt(0, ...)
//    - INT1 (pin 20)  - attachInterrupt(1, ...)
//    - INT2 (pin 19)  - attachInterrupt(2, ...)
//    - INT3 (pin 18)  - attachInterrupt(3, ...)
//    - INT4 (pin 2)   - attachInterrupt(4, ...)
//    - INT5 (pin 3)   - attachInterrupt(5, ...)
//
//  IMPORTANT: Only these 6 pins support hardware interrupts on Mega 2560
//             All other pins cannot be used for interrupt-based counters
// ============================================================================

#pragma once
#include <Arduino.h>
#include "modbus_counters.h"

// ============================================================================
// Valid Interrupt Pins for Arduino Mega 2560
// ============================================================================

enum IntPin : uint8_t {
  INT_PIN_2  = 2,    // INT4
  INT_PIN_3  = 3,    // INT5
  INT_PIN_18 = 18,   // INT3
  INT_PIN_19 = 19,   // INT2
  INT_PIN_20 = 20,   // INT1
  INT_PIN_21 = 21    // INT0
};

// ============================================================================
// API Functions
// ============================================================================

// Check if a pin is a valid interrupt pin on Arduino Mega 2560
bool sw_counter_is_valid_interrupt_pin(uint8_t pin);

// Convert GPIO pin to Arduino interrupt number, or -1 if invalid
// Uses digitalPinToInterrupt() for correct mapping on Arduino Mega 2560
int8_t sw_counter_pin_to_interrupt(uint8_t pin);

// Attach interrupt to a counter
// Returns true if successful, false if pin is invalid or already in use
bool sw_counter_attach_interrupt(uint8_t counter_id, uint8_t pin);

// Detach interrupt from a counter
void sw_counter_detach_interrupt(uint8_t counter_id);

// ISR trigger handler (called from ISR context)
// Processes edge detection and counter increment
// counter_id: 1..4
void sw_counter_interrupt_handler(uint8_t counter_id);
