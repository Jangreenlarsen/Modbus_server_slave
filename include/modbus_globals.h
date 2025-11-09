// ============================================================================
//  Filnavn : modbus_globals.h
//  Projekt  : Modbus RTU Server / CLI
//  Version  : v3.1.3-patch1 (2025-11-03)
//  Forfatter: ChatGPT Automation
//  Formål   : Deklaration af alle globale Modbus-data og hjælpefunktioner.
//             Kompatibel med CLEAN build uden demo-data.
// ============================================================================

#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------------
//  Systemkonstanter
// ---------------------------------------------------------------------------
#define NUM_COILS        256
#define NUM_DISCRETE     256
#define NUM_REGS         160
#define NUM_INPUTS       160
#define NUM_GPIO         54
#define MAX_STATIC_REGS  32
#define MAX_STATIC_COILS 64

#define SLAVE_ID         1
#define BAUDRATE         9600

// ---------------------------------------------------------------------------
//  Globale buffere
// ---------------------------------------------------------------------------
extern uint8_t  coils[NUM_COILS / 8];
extern uint8_t  discreteInputs[NUM_DISCRETE / 8];
extern uint16_t holdingRegs[NUM_REGS];
extern uint16_t inputRegs[NUM_INPUTS];

// GPIO-mapping
extern int16_t gpioToCoil[NUM_GPIO];
extern int16_t gpioToInput[NUM_GPIO];

// Statisk mapping
extern uint8_t  regStaticCount;
extern uint16_t regStaticAddr[MAX_STATIC_REGS];
extern uint16_t regStaticVal [MAX_STATIC_REGS];
extern uint8_t  coilStaticCount;
extern uint16_t coilStaticIdx [MAX_STATIC_COILS];
extern uint8_t  coilStaticVal [MAX_STATIC_COILS];

// Runtime-status
extern uint8_t  currentSlaveID;
extern uint32_t currentBaudrate;
extern bool     serverRunning;
extern unsigned long frameGapUs;
extern unsigned long lastFrameTs;

// Statistik og tilstande
extern bool monitorMode;
extern bool listenToAll;
extern uint32_t totalFrames;
extern uint32_t validFrames;
extern uint32_t crcErrors;
extern uint32_t wrongSlaveID;
extern uint32_t responsesSent;

extern char cliHostname[16];

// ---------------------------------------------------------------------------
//  Funktioner
// ---------------------------------------------------------------------------
void modbus_init_globals();

bool  bitReadArray (const uint8_t *arr, uint16_t bitIndex);
void  bitWriteArray(uint8_t *arr, uint16_t bitIndex, bool value);
void  packBits     (const uint8_t *src, uint16_t start, uint16_t qty, uint8_t *dst);

// Beregner RTU-gap (defineret i modbus_utils.cpp)
unsigned long rtuGapUs(void);
