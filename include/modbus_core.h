// ============================================================================
//  Filnavn : modbus_core.h
//  Projekt  : Modbus RTU Server / CLI
//  Version  : v3.1.2-patch1 (2025-11-03)
//  Forfatter: ChatGPT Automation
//  Formål   : Centrale konstanter, Modbus FC-definitioner, RTU-buffer, 
//             hardwarekonfiguration og PersistConfig (EEPROM).
//  Ændringer:
//    - v3.1.2-patch1: Tilføjet CounterConfig i PersistConfig (EEPROM schema v4)
//    - v3.1.0-patch1: CounterEngine integration (Opgave 5)
//    - v3.0.x        : TimerEngine, CLI, RS485, buffer utils
// ============================================================================

#pragma once
#include <Arduino.h>
#include "modbus_globals.h"
#include "modbus_timers.h"
#include "modbus_counters.h"

// ===== Platform config (MEGA2560) =====
#define MODBUS_SERIAL   Serial1
#define RS485_DIR_PIN   8
// SLAVE_ID and BAUDRATE are defined in modbus_globals.h

// 🧩 Buffer sizing for RTU stream
#define RXBUF_SIZE      128
#define MAX_RESP        128

// ===== Function codes =====
#define FC_READ_COILS             0x01
#define FC_READ_DISCRETE_INPUTS   0x02
#define FC_READ_HOLDING_REGS      0x03
#define FC_READ_INPUT_REGS        0x04
#define FC_WRITE_SINGLE_COIL      0x05
#define FC_WRITE_SINGLE_REG       0x06
#define FC_WRITE_MULTIPLE_COILS   0x0F
#define FC_WRITE_MULTIPLE_REGS    0x10

// ===== Exceptions =====
#define EX_ILLEGAL_FUNCTION       0x01
#define EX_ILLEGAL_DATA_ADDRESS   0x02
#define EX_ILLEGAL_DATA_VALUE     0x03

// ===== Utils =====
uint16_t calculateCRC16(uint8_t *buf, uint8_t len);
bool bitReadArray(const uint8_t *arr, uint16_t bitIndex);
void bitWriteArray(uint8_t *arr, uint16_t bitIndex, bool value);
void packBits(const uint8_t *src, uint16_t start, uint16_t qty, uint8_t *dst);
unsigned long rtuGapUs();
void rs485_tx_enable();
void rs485_rx_enable();
void printHex(uint8_t *b, uint8_t n);

// ===== TX/Exception =====
void sendResponse(uint8_t *response, uint8_t len, uint8_t slaveForTx);
void sendException(uint8_t rxSlave, uint8_t functionCode, uint8_t exceptionCode);

// ===== Modbus core =====
void initModbus();
void processModbusFrame(uint8_t *frame, uint8_t len);
void modbusLoop();

// ===== Status/info =====
void printStatistics();
void printStatus();
void printVersion();
void printHelp();

// ===== CLI =====
bool cli_active();
void cli_try_enter();
void cli_loop();

// ===== Version =====
#include "version.h"

// ===== Static-persist limits =====
#define MAX_STATIC_REGS   32
#define MAX_STATIC_COILS  64
#define NUM_GPIO          54   // antal digitale GPIO-pins på MEGA2560

// ===== Include dependent modules =====
#include "modbus_timers.h"
#include "modbus_counters.h"   // <-- NYT: CounterConfig

// ============================================================================
//  EEPROM-konfiguration (schema v4)
//  Indeholder: slave-id, baudrate, serverFlag, statiske maps, timere, counters,
//               GPIO mapping og checksum.
// ============================================================================


// ===== Version =====
#include "version.h"

// ===== Static-persist limits =====
#define MAX_STATIC_REGS   32
#define MAX_STATIC_COILS  64
#define NUM_GPIO          54   // antal digitale GPIO-pins på MEGA2560

// ===== Include dependent modules =====
#include "modbus_timers.h"
#include "modbus_counters.h"   // CounterConfig v3

// ============================================================================
//  EEPROM schema v8 – inkl. counter control arrays
// ============================================================================
//
//  Bemærk: schema = 8 tilføjer counterAutoStartEnable[4] array.
//
struct PersistConfig {
  uint16_t magic;          // 0xC0DE
  uint8_t  schema;         // 8 for CounterEngine v3 + individuel reset-on-read + auto-start
  uint8_t  reserved;       // alignment / fremtidigt brug

  uint8_t  slaveId;        // 1..247
  uint8_t  serverFlag;     // 0/1
  uint32_t baud;           // understøttet baud-rate

  // Statisk holding-registre (anvendes ved load)
  uint8_t  regStaticCount;
  uint16_t regStaticAddr[MAX_STATIC_REGS];
  uint16_t regStaticVal [MAX_STATIC_REGS];

  // Statisk coils (anvendes ved load)
  uint8_t  coilStaticCount;
  uint16_t coilStaticIdx [MAX_STATIC_COILS];
  uint8_t  coilStaticVal [MAX_STATIC_COILS];  // 0/1

  char hostname[16];       // CLI prompt navn (0-termineret)
  
  // --------------------------------------------
  // Globale timer status/control registre
  // --------------------------------------------
  uint16_t timerStatusReg;       // Globalt status-register for timere
  uint16_t timerStatusCtrlReg;   // Globalt control-register for timere

  // Timere
  uint8_t     timerCount;   // 0..4
  TimerConfig timer[4];

  // Counters (v3)
  uint8_t      counterCount;   // 0..4
  CounterConfig counter[4];    // fuld struktur inkl. scale/direction
  
  // --------------------------------------------
  // Global counter control arrays
  // --------------------------------------------
  uint8_t counterResetOnReadEnable[4];  // individuel reset-on-read pr. counter (0/1)
  uint8_t counterAutoStartEnable[4];    // individuel auto-start pr. counter (0/1)

  // GPIO pin-mapping
  int16_t gpioToCoil[NUM_GPIO];   // map pin -> coil index (eller -1 hvis ingen)
  int16_t gpioToInput[NUM_GPIO];  // map pin -> discrete input index (eller -1)

  // Integritet
  uint16_t crc;            // checksum (additiv) over alle felter undtagen crc
};

// ============================================================================
//  Prototyper for config-store
// ============================================================================
bool configLoad(PersistConfig &cfg);
void configDefaults(PersistConfig &cfg);
bool configSave(const PersistConfig &cfg);
void configApply(const PersistConfig &cfg);
