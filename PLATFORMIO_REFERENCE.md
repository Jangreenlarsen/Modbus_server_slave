# PlatformIO Hurtig Reference

## Vigtige Genveje (VS Code)

| Handling | Genvej | Knap |
|----------|--------|------|
| Build/Compile | `Ctrl+Alt+B` | ✓ |
| Upload | `Ctrl+Alt+U` | → |
| Serial Monitor | `Ctrl+Alt+S` | 🔌 |
| Clean | `Ctrl+Alt+C` | 🗑️ |

## Terminal Kommandoer

Åbn Terminal i VS Code (`Ctrl + ´`) og brug:

### Build & Upload
```bash
pio run                    # Build projektet
pio run -t upload          # Build og upload
pio run -t clean           # Ryd build artifacts
```

### Serial Monitor
```bash
pio device monitor         # Start serial monitor
pio device list            # Vis tilgængelige enheder
```

### Libraries
```bash
pio lib search modbus      # Søg efter libraries
pio lib install <id>       # Installer library
pio lib list               # Vis installerede libraries
```

### Info
```bash
pio system info            # System information
pio boards mega            # Vis Mega boards
```

## Projektstruktur

```
ModbusGateway/
├── platformio.ini         # Konfigurationsfil ⚙️
├── src/                   # Source code (.cpp, .ino) 📝
├── include/               # Header filer (.h) 📄
├── lib/                   # Custom libraries 📚
├── test/                  # Unit tests 🧪
└── .pio/                  # Build artifacts (auto) 🔨
```

## platformio.ini Eksempler

### Basis konfiguration
```ini
[env:megaatmega2560]
platform = atmelavr
board = megaatmega2560
framework = arduino
monitor_speed = 115200
```

### Med libraries
```ini
lib_deps = 
    emelianov/modbus-esp8266@^4.1.0
    bblanchon/ArduinoJson@^7.0.0
```

### Build flags
```ini
build_flags = 
    -D DEBUG=1
    -D VERSION=\"3.1.7\"
```

### Multiple environments
```ini
[env:mega_debug]
platform = atmelavr
board = megaatmega2560
build_flags = -D DEBUG=1

[env:mega_release]
platform = atmelavr
board = megaatmega2560
build_flags = -O3
```

## Debugging Tips

### Verbose output
```bash
pio run -v                 # Verbose build
pio run -t upload -v       # Verbose upload
```

### Check konfiguration
```bash
pio project config         # Vis parsed config
```

### Clean everything
```bash
pio run -t clean
rm -rf .pio                # Slet hele build cache
```

## Serial Monitor Tips

### Basic
```bash
pio device monitor
```

### Med filter
```bash
pio device monitor --filter direct
pio device monitor --filter time
pio device monitor --filter colorize
```

### Forskellige baudrates
Sæt i platformio.ini:
```ini
monitor_speed = 115200
```

## Almindelige Fejl

### "Please specify `upload_port`"
**Fix**: PlatformIO finder ikke Arduino
```ini
upload_port = COM3          # Windows
upload_port = /dev/ttyUSB0  # Linux
```

### "Cannot open include file"
**Fix**: Header fil ikke fundet - check include/ mappe

### "Undefined reference to..."
**Fix**: .cpp fil mangler eller ikke kompileret

### Build hænger
**Fix**: 
```bash
rm -rf .pio
pio run
```

## Nyttige Links

- PlatformIO Docs: https://docs.platformio.org
- Board Explorer: https://docs.platformio.org/en/latest/boards/
- Library Registry: https://registry.platformio.org

## Pro Tips

1. **IntelliSense virker ikke?**
   - Tryk `Ctrl+Shift+P`
   - Vælg "PlatformIO: Rebuild IntelliSense Index"

2. **Hurtigere builds?**
   ```ini
   build_flags = -j4  # Parallel builds
   ```

3. **Custom upload speed?**
   ```ini
   upload_speed = 115200
   ```

4. **Test på forskellige boards?**
   Opret flere [env:xxx] i platformio.ini

5. **Library opdateringer?**
   ```bash
   pio lib update
   ```
