// ============================================================================
//  MODBUS SERVER TEST - v3.1.7  (Aruba/Cisco-style CLI)
//  Board: Arduino Mega 2560
//  Konverteret til PlatformIO format
// ============================================================================

#include <Arduino.h>
#include "modbus_core.h"
#include "version.h"

static void print_banner() {
  Serial.println(F("=== MODBUS RTU SLAVE ==="));
  Serial.print(F("Version: ")); Serial.println(F(VERSION_STRING));
  Serial.print(F("Build: "));   Serial.println(F(VERSION_BUILD));
  Serial.println(F("==============================================="));
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  delay(500);

  print_banner();

  // Load persisted config (or defaults) and apply before initModbus()
  PersistConfig cfg;
  if (!configLoad(cfg)) {
    Serial.println(F("% No valid config in EEPROM -> using defaults"));
    configDefaults(cfg);
    (void)configSave(cfg);
  }
  configApply(cfg);

  initModbus();
  Serial.println(F("% Modbus core initialized"));
  Serial.print(F("% ID: "));   Serial.print(currentSlaveID);
  Serial.print(F("  Baud: ")); Serial.println(currentBaudrate);
  Serial.print(F("% Server: ")); Serial.println(serverRunning ? F("RUNNING") : F("STOPPED"));
  Serial.println();

  Serial.println(F("% Enter CLI by typing: CLI"));
  Serial.println(F("% Line ending: NL or CR or Both, 115200 baud"));
  Serial.println(F("==============================================="));
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
