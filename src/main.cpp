// ============================================================================
//  MODBUS SERVER TEST - v3.1.7  (Aruba/Cisco-style CLI)
//  Board: Arduino Mega 2560
//  Konverteret til PlatformIO format
// ============================================================================

#include <Arduino.h>
#include "modbus_core.h"
#include "version.h"
#include <avr/wdt.h>

// Global config (avoid stack overflow - struct is >1KB)
PersistConfig globalConfig;

static void print_banner() {
  Serial.println(F("=== MODBUS RTU SLAVE ==="));
  Serial.print(F("Version: ")); Serial.println(F(VERSION_STRING_NY));
  Serial.print(F("Build: "));   Serial.println(F(VERSION_BUILD));
  Serial.println(F("==============================================="));
}

void setup() {
  // Disable watchdog immediately (prevent boot loop on EEPROM issues)
  wdt_disable();

  // CRITICAL: Disable Timer5 interrupt before any other init (v3.6.1+)
  // This prevents ISR corruption of Serial/timing during boot
  // NOTE: Timer5 is the ONLY timer used for HW counters on Arduino Mega
  // Timer1, Timer3, Timer4 are not routed to headers and not implemented
  TIMSK5 = 0x00;  // Timer5 external clock mode (for HW counter via pin 2)

  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  delay(500);

  print_banner();

  // Load persisted config (or defaults) and apply before initModbus()
  // Use global to avoid stack overflow (struct is >1KB)
  bool configValid = configLoad(globalConfig);

  if (!configValid) {
    Serial.println(F("% Loading defaults and saving to EEPROM"));
    // configLoad() already set globalConfig to defaults if invalid
    if (!configSave(globalConfig)) {
      Serial.println(F("! Warning: Could not save config to EEPROM"));
    } else {
      Serial.println(F("✓ Config saved"));
    }
  } else {
    Serial.println(F("✓ Config loaded from EEPROM"));
  }

  initModbus();

  configApply(globalConfig);
  Serial.println(F("% Modbus core initialized"));
  Serial.print(F("% ID: "));   Serial.print(currentSlaveID);
  Serial.print(F("  Baud: ")); Serial.println(currentBaudrate);
  Serial.print(F("% Server: ")); Serial.println(serverRunning ? F("RUNNING") : F("STOPPED"));
  Serial.println();

  Serial.println(F("% Enter CLI by typing: CLI"));
  Serial.println(F("% Line ending: NL or CR or Both, 115200 baud"));
  Serial.println(F("==============================================="));

  // Enable interrupts globally (Arduino disables them during setup)
  // This is required for HW counter ISRs to work
  sei();
}

void loop() {
  // CLI first
  if (cli_active()) {
    cli_loop();
  } else {
    cli_try_enter();
  }

  // Heartbeat (1 Hz)
  static unsigned long last = 0;
  if (millis() - last > 1000) {
    last = millis();
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }

  // Modbus when enabled
  if (serverRunning) {
    modbusLoop();
  }

  // Demo inputs (kept as before)
  static unsigned long demoT = 0;
  if (millis() - demoT > 1000) {
    inputRegs[0] = analogRead(A0);
    inputRegs[1] = millis() / 1000;
    inputRegs[2] = (uint16_t)(rand() % 1000);
    demoT = millis();
  }
}
