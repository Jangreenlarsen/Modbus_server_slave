// ============================================================================
//  Filnavn : modbus_globals.cpp
//  Projekt  : Modbus RTU Server / CLI
//  Version  : v3.1.3-patch1 (2025-11-03)
//  Forfatter: JanG at modbus_slave@laces.dk
//  Formål   : Globale Modbus-buffere, status- og statistikvariabler,
//             samt initialisering (CLEAN BUILD – ingen demo-data).
// ============================================================================

#include "modbus_globals.h"

// ---------------------------------------------------------------------------
//  Globale Modbus-buffere
// ---------------------------------------------------------------------------
uint8_t  coils[NUM_COILS / 8];
uint8_t  discreteInputs[NUM_DISCRETE / 8];
uint16_t holdingRegs[NUM_REGS];
uint16_t inputRegs[NUM_INPUTS];

// ---------------------------------------------------------------------------
//  GPIO-mappings
// ---------------------------------------------------------------------------
int16_t gpioToCoil [NUM_GPIO];
int16_t gpioToInput[NUM_GPIO];

// ---------------------------------------------------------------------------
//  Statisk mapping (EEPROM-persistens)
// ---------------------------------------------------------------------------
uint8_t  regStaticCount  = 0;
uint16_t regStaticAddr[MAX_STATIC_REGS];
uint16_t regStaticVal [MAX_STATIC_REGS];

uint8_t  coilStaticCount = 0;
uint16_t coilStaticIdx [MAX_STATIC_COILS];
uint8_t  coilStaticVal [MAX_STATIC_COILS];

// ---------------------------------------------------------------------------
//  Runtime-kontrol og status
// ---------------------------------------------------------------------------
uint8_t  currentSlaveID  = SLAVE_ID;
uint32_t currentBaudrate = BAUDRATE;
bool     serverRunning   = true;

unsigned long frameGapUs  = 0;
unsigned long lastFrameTs = 0;

// ---------------------------------------------------------------------------
//  Statistik og driftstilstande
// ---------------------------------------------------------------------------
bool monitorMode   = false;
bool listenToAll   = false;
uint32_t totalFrames   = 0;
uint32_t validFrames   = 0;
uint32_t crcErrors     = 0;
uint32_t wrongSlaveID  = 0;
uint32_t responsesSent = 0;

char cliHostname[16] = "Greens-modbus";

// ---------------------------------------------------------------------------
//  Init – nulstiller alle globale strukturer
// ---------------------------------------------------------------------------
void modbus_init_globals() {
  memset(coils,          0, sizeof(coils));
  memset(discreteInputs, 0, sizeof(discreteInputs));
  memset(holdingRegs,    0, sizeof(holdingRegs));
  memset(inputRegs,      0, sizeof(inputRegs));

  regStaticCount  = 0;
  coilStaticCount = 0;
  memset(regStaticAddr, 0, sizeof(regStaticAddr));
  memset(regStaticVal,  0, sizeof(regStaticVal));
  memset(coilStaticIdx, 0, sizeof(coilStaticIdx));
  memset(coilStaticVal, 0, sizeof(coilStaticVal));

  for (uint8_t p = 0; p < NUM_GPIO; ++p) {
    gpioToCoil[p]  = -1;
    gpioToInput[p] = -1;
  }

  currentSlaveID  = SLAVE_ID;
  currentBaudrate = BAUDRATE;
  serverRunning   = true;

  frameGapUs = rtuGapUs();

  monitorMode   = false;
  listenToAll   = false;
  totalFrames   = 0;
  validFrames   = 0;
  crcErrors     = 0;
  wrongSlaveID  = 0;
  responsesSent = 0;
}
