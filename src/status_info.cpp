#include "modbus_core.h"

void printStatistics() {
  Serial.println("=== STATS ===");
  Serial.print("Total: "); Serial.println(totalFrames);
  Serial.print("Valid: "); Serial.println(validFrames);
  Serial.print("CRC Err: "); Serial.println(crcErrors);
  Serial.print("Wrong ID: "); Serial.println(wrongSlaveID);
  Serial.print("TX: "); Serial.println(responsesSent);
  Serial.println("=============");
}

void printStatus() {
  Serial.println("=== STATUS ===");
  Serial.print("Ver: "); Serial.println(VERSION_STRING_NY);
  Serial.print("Build: "); Serial.println(VERSION_BUILD);
  Serial.print("State: "); Serial.println(serverRunning ? "RUNNING" : "STOPPED");
  Serial.print("Mode: "); Serial.println(monitorMode ? "MONITOR" : "SERVER");
  Serial.print("ID: "); Serial.print(currentSlaveID);
  if (listenToAll) Serial.print(" (ALL)");
  Serial.println();
  Serial.print("Baud: "); Serial.println(currentBaudrate);
  Serial.print("RTU gap (us): "); Serial.println(frameGapUs);
  Serial.print("Regs: "); Serial.println(NUM_REGS);
  Serial.println("==============");
}

void printVersion() {
  Serial.println("=== VERSION ===");
  Serial.println(VERSION_STRING_NY);
  Serial.println("Build: " VERSION_BUILD);
  Serial.println("===============");
}

void printHelp() {
  Serial.println("=== COMMANDS ===");
  Serial.println("START/STOP             - Control server");
  Serial.println("MONITOR/SERVER         - Set mode");
  Serial.println("ID=n                   - Set slave ID (0=all)");
  Serial.println("BAUD=n                 - Set baudrate (300..115200)");
  Serial.println("STATUS/?               - Show status");
  Serial.println("STATS/S                - Statistics");
  Serial.println("REGS/R                 - Dump registers (first 20)");
  Serial.println("TEST/T                 - Write test values to first 5 holding regs");
  Serial.println("VER/V/VERSION          - Version info");
  Serial.println("HELP/H                 - This help");
  Serial.println("CLI                    - Enter interactive CLI mode");
  Serial.println("================");
}
