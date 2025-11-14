# ATmega2560 Timers og Counters - Hardwarekonfiguration

Dette dokument dokumenterer alle hardware-relaterede aspekter af ATmega2560's timer- og counter-funktionalitet, især med fokus på Arduino Mega 2560 implementering og Modbus RTU Server v3.6.1 brugsscenarier.

## Indholdsfortegnelse

1. [Timers Oversigt](#timers-oversigt)
2. [Eksterne Clock Input Pins](#eksterne-clock-input-pins)
3. [Arduino Mega 2560 Hardware Mapping](#arduino-mega-2560-hardware-mapping)
4. [Counter Implementering](#counter-implementering)
5. [Prescaler Arkitektur](#prescaler-arkitektur)
6. [Praktiske Begrænsninger](#praktiske-begrænsninger)

---

## Timers Oversigt

### Timer Typer på ATmega2560

ATmega2560 har **6 hardware timers**:

| Timer | Bit-Width | Type | Noter |
|-------|-----------|------|-------|
| **Timer0** | 8-bit | Standard | Brugt af Arduino kernel til `millis()` |
| **Timer1** | 16-bit | Standard | Normalt fri til brugerbrug |
| **Timer2** | 8-bit | Standard | Async mode, ingen eksterne clock |
| **Timer3** | 16-bit | Udvidet | Kun på Arduino Mega |
| **Timer4** | 16-bit | Udvidet | Kun på Arduino Mega |
| **Timer5** | 16-bit | Udvidet | Kun på Arduino Mega |

### Timer Funktioner

Alle timers kan arbejde i følgende modi:

1. **Normal Mode**: Tæller op, overløb ved max
2. **CTC Mode (Clear Timer on Compare)**: Tæller op til compare værdi, reset
3. **PWM Modes**: Fast PWM, Phase Correct PWM
4. **External Event Counting**: Tæller eksterne pulser på T-pin

---

## Eksterne Clock Input Pins

### Komplette Timer Clock Input Pins på ATmega2560 Chip

Følgende timers understøtter eksterne clock inputs via dedikerede T-pins:

| Timer | T-Pin Navn | Port/Pin | Arduino Mega Pin | Status |
|-------|-----------|----------|------------------|--------|
| **Timer0** | T0 | PD7 | Pin 38 | ✓ Tilgængelig |
| **Timer1** | T1 | PD6 | Ikke routed | ✗ Utilgængelig |
| **Timer2** | (ingen) | - | - | ✗ Ikke understøttet |
| **Timer3** | T3 | PE6 | Pin 9* | ⚠ Konflikt** |
| **Timer4** | T4 | PH7 | Pin 28* | ⚠ Ikke standard routed |
| **Timer5** | T5 | PE4 | Pin 2 | ✓ Tilgængelig |

**Noter:**
- *Pin 9 bruges som INT6 interrupt pin; kan være i konflikt med timer funktionalitet
- **Pin 28 og andre er fysisk tilgængelige på chippen men ikke standard routed til Arduino headers
- Timer2 understøtter IKKE eksterne clock inputs (kun asynkron crystal oscillator mode via TOSC1/TOSC2)

### Praktisk Tilgængelighed på Arduino Mega 2560

**KUN 2 timers er praktisk tilgængelige uden lodning:**

1. **Timer0** (Pin 38 / T0 / PD7)
   - ⚠️ PROBLEM: Arduino kernel bruger Timer0 til `millis()`, `delay()`, etc.
   - ❌ ANBEFALING: Undgå i produktionssystemer

2. **Timer5** (Pin 2 / T5 / PE4)
   - ✅ BEDSTE VALG: Fri, ingen kernel afhængigheder
   - ✅ ANBEFALING: Brug dette til hardware counters

### Hardware Modifikation til Flere Timers

Hvis du har brug for Timer1, Timer3 eller Timer4:

- **Timer1 (T1/PD6)**: Pin PD6 på chippen; ville kræve lodning
- **Timer3 (T3/PE6)**: Pin PE6; ville være i konflikt med pin 9 PWM/interrupt
- **Timer4 (T4/PH7)**: Pin PH7; ville kræve direkte chip lodning

**Alternativ Board:**
- **Seeedstudio Mega variant** har ALLE 5 timer clock input pins routed til headers
- Gør alle timers praktisk tilgængelige uden hardwaremodifikation

---

## Arduino Mega 2560 Hardware Mapping

### Timer5 Pin Mapping (Nuværende Implementation)

**Modbus RTU Server v3.6.1** bruger **Timer5** for hardware counting:

```
Hardware Counter Configuration:
├─ Timer: Timer5 (16-bit)
├─ External Clock Pin: T5 / PE4 / Arduino Pin 2
├─ Max Frequency: ~20 kHz (ved 16MHz system clock)
├─ TCCR5B Register: 0x07 (External clock mode, rising edge)
├─ Counter Register: TCNT5 (16-bit value)
└─ Usage: Hardware pulse/event counting in hw-mode:hw-t5
```

### Pin 2 Hardwired Forbindelser

```
Arduino Mega 2560 Pin 2
├─ Microcontroller Pin: PE4
├─ Function 1: T5 (External clock input for Timer5) ← USED
├─ Function 2: INT2 (External interrupt)
├─ Available: JA, NO CONFLICTS (INT2 kan bruges hvis nødvendigt)
└─ Discrete Input Mapping: Kan maps til discrete input via `gpio map 2 input <n>`
```

---

## Counter Implementering

### Counter Arkitektur (v3.6.1 Unified Prescaler)

Modbus RTU Server implementerer 4 uafhængige counters med tre operating modes:

#### Mode 1: SW (Software Polling)
```
Implementation: Polling af discrete input via di_read()
Tælling: Hvert edge detekteres via interrupt på GPIO pin
Hardware: Standard GPIO interrupt (INT0-INT5 eller polling)
Prescaler: Software implementeret (division ved output)
Precision: Afhængig af polling frekvens
```

#### Mode 2: SW-ISR (Software Interrupt-Driven)
```
Implementation: Hardware interrupt-baseret edge detection
Hardware Pins: INT0-INT5 på ATmega2560
├─ INT0: Pin 2 (PD0)
├─ INT1: Pin 3 (PD1)
├─ INT2: Pin 18 (PE4)
├─ INT3: Pin 19 (PE5)
├─ INT4: Pin 20 (PE6)  ← KONFLIKT! Se note nedenfor
└─ INT5: Pin 21 (PE7)

Tælling: ISR udløst på hver edge (deterministic, no polling delay)
Prescaler: Software implementeret
Precision: Høj, ISR-baseret tælling
```

**⚠️ VIGTIG NOTE - INT3 Konflikt:**
- Pin 20/PE6 bruges både som INT4 interrupt OG som T3 (Timer3 external clock)
- Aktuel dokumentation siger INT4 bruger PE6, men dette er FEJL i vores references
- Timer3 T3 pin er PE6, som kan være i konflikt med interrupt brug
- SE NEDENFOR under "Praktiske Begrænsninger" for resolution

#### Mode 3: HW (Hardware Timer5)
```
Implementation: Timer5 external clock mode
Hardware Pin: T5 / PE4 / Arduino Pin 2
Clock Input: TCCR5B = 0x07 (external clock, rising edge)
Counter Register: TCNT5 (16-bit, 0-65535)
Max Frequency: ~20 kHz

KRITISK HARDWARE BEGRÆNSNING (v3.4.7):
┌─────────────────────────────────────────────────────┐
│ ATmega2560 Timer5 Prescaler PROBLEM                 │
├─────────────────────────────────────────────────────┤
│ External Clock Mode (TCCR5B=0x07):                  │
│ ✓ Tæller pulser på T5 pin (100% funktioner)        │
│ ✗ Hardware prescaler registre IGNORERES             │
│                                                      │
│ Internal Clock Modes (TCCR5B=0x02-0x06):           │
│ ✗ Tæller 16MHz system clock, IGNORERER T5 pin      │
│ ✓ Hardware prescaler virker normalt                 │
│                                                      │
│ LØSNING:                                            │
│ → ALTID brug external clock mode (TCCR5B=0x07)    │
│ → Prescaler implementeres 100% i software           │
│ → Division sker KUN ved output (raw register)       │
└─────────────────────────────────────────────────────┘

Prescaler: Software implementeret (division ved output)
Precision: Høj, hardware tælling
```

---

## Prescaler Arkitektur

### Unified Prescaler Strategy (v3.6.0+)

#### Fundamental Principper

```
For ALLE tre counter modes (SW, SW-ISR, HW):

┌────────────────────────────────────────────┐
│ Input: Pulser på input                     │
│         ↓                                   │
│ Tælling: counterValue += 1 (hver puls)    │
│         (NO SKIPPING, ALLE pulser tælles) │
│         ↓                                   │
│ Output Registers:                         │
│ ├─ value-reg:   counterValue × scale      │
│ ├─ raw-reg:     counterValue / prescaler  │
│ └─ freq-reg:    Hz (0-20000)             │
└────────────────────────────────────────────┘

VIGTIG: Prescaler påvirker KUN output (raw register)
         Tælling sker ALTID på full resolution
```

#### Understøttede Prescaler Værdier

```
Prescaler: 1, 4, 8, 16, 64, 256, 1024

Eksempel (prescaler=64):
─────────────────────────
Input:  76800 pulser på T5 pin
        ↓
Counter: counterValue = 76800 (ALLE pulser tælles)
         ↓
Outputs:
├─ value-reg:     76800 × scale (hvis scale=1: 76800)
├─ raw-reg:       76800 / 64 = 1200 (reduced for register space)
└─ freq-reg:      Hz measured (ikke påvirket af prescaler)
```

#### Prescaler Implementering

| Mode | Tælling Location | Prescaler Sted | Precision |
|------|------------------|-----------------|-----------|
| **SW** | Software loop | Output division | Afhængig polling |
| **SW-ISR** | ISR funktion | Output division | Høj |
| **HW** | Timer5 register | Output division | Meget høj |

### ScaleFloat Parameter

ScaleFloat bruges til værdiskalering **uden** at påvirke prescaler:

```
Eksempel:
─────────
Tælling: 76800 pulser
Scale: 0.5
Prescaler: 64

Outputs:
├─ value-reg:     76800 × 0.5 = 38400 (scaled)
└─ raw-reg:       76800 / 64 = 1200 (prescaled, NOT affected by scale)
```

---

## Praktiske Begrænsninger

### 1. Timer0 - Kernel Konflikt ⚠️

```
Problem:
┌────────────────────────────────────────────┐
│ Timer0 brugt af Arduino kernel             │
│ ├─ millis() - millisekund timing           │
│ ├─ delay() - delay funktion                │
│ ├─ delayMicroseconds() - mikrosekund timer │
│ └─ Alle andere timing funktioner           │
│                                             │
│ Hvis du tager Timer0 over:                 │
│ ✗ millis() stoppes                        │
│ ✗ delay() virker ikke mere                │
│ ✗ Serial.begin() kan give problemer       │
│ ✗ WiFi/Bluetooth funktioner falder ud     │
└────────────────────────────────────────────┘

Anbefaling: UNDGÅ Timer0 til counter brug
```

### 2. Timer5 - Prescaler Hardware Limitation ⚠️

```
Hardware Feature: Timer5 eksterne clock
Problem: Hardware prescaler ignoreres i external clock mode

Solution Applied (v3.4.7+):
─────────────────────────────
1. TCCR5B = 0x07 (external clock mode, rising edge)
2. Prescaler implementeres i software som division
3. Output ved counterValue / prescaler
4. Garanteret konsistent med SW/SW-ISR modes

Implementering:
├─ ISR Timer5 overflow funktion
├─ Software division ved output
└─ Maksimum frekvens clamped til ~20 kHz
```

### 3. Int4 Pin Konflikt (Documented Issue) ⚠️

```
Potential Problem:
Pin PE6 (Arduino Pin 20) kan være INT4 interrupt OG T3 (Timer3)

Nuværende Status i Projektet:
┌─────────────────────────────────────────────┐
│ SW-ISR Mode bruge INT0-INT5 pins           │
│ ├─ INT0: Pin 2 (PD0) - Ledig ✓            │
│ ├─ INT1: Pin 3 (PD1) - Ledig ✓            │
│ ├─ INT2: Pin 18 (PE4) - Ledig ✓           │
│ ├─ INT3: Pin 19 (PE5) - Ledig ✓           │
│ ├─ INT4: Pin 20 (PE6) - KONFLIKT ⚠️       │
│ │         (Pin 20 kan være PE6=T3)        │
│ └─ INT5: Pin 21 (PE7) - Ledig ✓           │
│                                             │
│ Hvis INT4 bruges:                          │
│ → Undgå at sætte Timer3 i external mode  │
│ → Eller brug andre pins (0-3, 5)          │
└─────────────────────────────────────────────┘

Nuværende Workaround:
- Dokumenteret i CLAUDE.md
- INT4 kan bruges hvis Timer3 ikke bruges
- Bedst at bruge INT0-INT3 eller INT5
```

### 4. Timer3/Timer4 Pin Utilgængelighed ⚠️

```
Timer3 (T3/PE6) og Timer4 (T4/PH7):
├─ Eksisterer på ATmega2560 chip
├─ Hardware er fuldt funktionel
├─ MEN: Pins er ikke routed til Arduino headers
└─ Ville kræve direkte lodning på chippen

Hvis du skal bruge Timer3/Timer4:
└─ Overvej Seeedstudio Mega board i stedet
```

### 5. Maksimal Frekvens Begrænsning

```
Timer5 Maksimal Frequency:
┌──────────────────────────────────────────┐
│ Teori: Alt, hvad der kan spejles på pin  │
│ Praksis: ~20 kHz (valideret og clamped) │
│ Grund:  Signal stabilitet og jitter      │
│ i polling/interrupt processing            │
│                                           │
│ Timer5 frequency measurement:            │
│ ├─ Timing window: 1-2 sekunder          │
│ ├─ Validation: Delta check               │
│ ├─ Overflow detection                   │
│ └─ Clamping: 0-20000 Hz                │
└──────────────────────────────────────────┘
```

---

## Sammenfatning: Bedst Praksis

### Anbefalet Konfiguration (Modbus RTU Server)

✅ **Valgt løsning:** Timer5 for hardware counting
```
├─ Pin: 2 (T5/PE4)
├─ Mode: External clock mode (TCCR5B=0x07)
├─ Prescaler: Software implementeret
├─ Maksimum frekvens: ~20 kHz
└─ Konflikt-fri: JA, ingen kernel afhængigheder
```

✅ **Alternative modes:**
```
├─ SW mode: GPIO polling via discrete inputs
├─ SW-ISR mode: INT0-INT5 interrupt pins
│  └─ Undgå INT4 hvis muligt (PE6 konflikt)
└─ HW mode: Timer5 (ANBEFALET)
```

### Hvad Skal Undgås

❌ **Timer0** - Kernel afhængigheder
❌ **Timer1, Timer3, Timer4** - Ikke praktisk tilgængelige
❌ **Timer2** - Ingen externe clock support
❌ **Pin 9 (PE6) som INT4** - Konflikt med T3

### Fremtidigt Upgrade

Hvis du skal bruge flere hardware counters:
1. Overvej Seeedstudio Mega board (alle timers routed)
2. Eller implementer yderligere SW-ISR counters på INT0-INT5 pins
3. Eller implementer direkte chip lodning til Timer1/Timer3/Timer4

---

## Kilder

- ATmega2560 Datasheet (Microchip/Atmel)
- Arduino Mega 2560 Official Documentation
- Microchip External Timer/Counter Clock Configuration Guide
- Projekt research: v3.4.7-v3.6.1 timer/counter implementation

**Dokument Version:** 1.0
**Dato:** 2025-11-14
**Modbus RTU Server Version:** v3.6.1
