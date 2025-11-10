# TODO - Modbus RTU Server v3.2.0

**Status:** ✅ Core project COMPLETE
**Fokus:** Future enhancements and optimizations

---

## ✅ Completed (v3.2.0)

### Core Implementation
- [x] Modbus RTU Server (FC01-06, FC0F-10)
- [x] RS-485 direction control
- [x] CRC16 validation
- [x] CLI system (256 char buffer, history, echo, backspace)
- [x] TimerEngine v2 (4 modes, global status/ctrl)
- [x] CounterEngine v3 (scale, direction, debounce)
- [x] EEPROM persistence (schema v9)
- [x] GPIO management
- [x] Static reg/coil configuration

### v3.2.0 Features
- [x] Counter frequency measurement (Hz)
- [x] Configurable raw register
- [x] Consistent parameter naming (index-reg, raw-reg, freq-reg, etc.)
- [x] Frequency stability improvements
- [x] CLI usability improvements
- [x] Backward compatibility for old parameter names
- [x] Updated manual (HTML)

### Documentation
- [x] Modbus server V3.2.0 Manual (dansk)
- [x] README.md
- [x] INSTALLATION.md
- [x] PLATFORMIO_REFERENCE.md
- [x] STATUS.md
- [x] TODO.md

### Git & Version Control
- [x] Git repository initialized
- [x] GitHub remote repository
- [x] Version tagging (v3.2.0)
- [x] .gitignore configured

---

## 🔮 Future Enhancements (Nice-to-Have)

### Testing & Quality Assurance
- [ ] **Unit Tests**
  - Test framework (Unity/ArduinoUnit)
  - Counter edge detection tests
  - Timer mode tests
  - CRC16 validation tests
  - EEPROM save/load tests

- [ ] **Integration Tests**
  - Modbus protocol compliance tests
  - RS-485 communication tests
  - CLI command tests

- [ ] **Performance Profiling**
  - Response time measurements
  - RAM usage optimization
  - Flash optimization
  - Interrupt latency profiling

### Hardware Variants
- [ ] **ESP32 Port**
  - Modbus TCP/IP support
  - WiFi configuration via web interface
  - OTA (Over-The-Air) updates
  - Dual UART for Modbus RTU + TCP

- [ ] **Modbus TCP/IP (Ethernet)**
  - W5500 Ethernet module support
  - TCP server implementation
  - Web interface for configuration

### Advanced Features
- [ ] **MQTT Integration**
  - Publish counter/timer status to MQTT broker
  - Subscribe to control commands
  - JSON payload format

- [ ] **Web Interface**
  - Real-time dashboard
  - Configuration editor
  - Log viewer
  - Firmware update interface

- [ ] **Data Logging**
  - SD card logging
  - Circular buffer in RAM
  - Log download via CLI or web

- [ ] **Enhanced CLI**
  - Tab completion
  - Command syntax highlighting
  - Multi-line commands
  - Scripting support

- [ ] **Modbus Gateway Mode**
  - Multi-slave gateway
  - Protocol translation (RTU ↔ TCP)
  - Slave device management

### Monitoring & Diagnostics
- [ ] **Advanced Diagnostics**
  - Bus traffic statistics
  - Error rate monitoring
  - Performance metrics
  - Memory leak detection

- [ ] **Watchdog & Recovery**
  - Hardware watchdog timer
  - Automatic crash recovery
  - Boot count tracking
  - Last reset reason logging

### Security (hvis relevant)
- [ ] **Access Control**
  - CLI password protection
  - Modbus function code filtering
  - Register access control lists

- [ ] **Audit Logging**
  - Configuration change log
  - Command execution log
  - Modbus transaction log

---

## 🐛 Known Issues / Limitations

### Current Limitations
- Frequency measurement: 0-20 kHz max (by design)
- Counter max: 64-bit (sufficient for most applications)
- Timers: 4 max (hardware limitation)
- Counters: 4 max (hardware limitation)
- CLI buffer: 256 chars (can be increased if needed)

### Minor Issues
- None currently reported ✅

### Future Considerations
- RAM usage: 56.8% - comfortable, but watch for expansion
- Flash usage: 20.1% - plenty of room for features
- EEPROM schema: v9 - consider migration path for v10+

---

## 📋 Development Roadmap

### Phase 1: Completed ✅
- Core Modbus RTU implementation
- Timer & Counter engines
- CLI system
- EEPROM persistence
- **v3.2.0: Frequency measurement & naming consistency**

### Phase 2: Testing (Optional)
- Unit test framework
- Integration tests
- Automated testing on hardware

### Phase 3: Connectivity (Optional)
- ESP32 port with WiFi
- Modbus TCP/IP
- Web interface

### Phase 4: Advanced Features (Optional)
- MQTT integration
- Data logging
- Enhanced diagnostics

---

## 🔧 Maintenance Tasks

### Regular Reviews
- [ ] Review RAM usage after major features
- [ ] Update manual with new features
- [ ] Version bump and tagging
- [ ] GitHub release notes

### Code Quality
- [ ] Code comments review
- [ ] Function documentation (Doxygen?)
- [ ] Code style consistency
- [ ] Dead code removal

### Security Updates
- [ ] Arduino core updates
- [ ] PlatformIO updates
- [ ] Library dependency updates

---

## 💡 Ideas & Experiments

### Experimental Features
- [ ] Dynamic register allocation
- [ ] User-defined function codes
- [ ] Lua/Python scripting engine
- [ ] Machine learning for anomaly detection
- [ ] Voice control via Alexa/Google Home integration

### Performance Experiments
- [ ] DMA for serial communication
- [ ] Interrupt-driven Modbus parsing
- [ ] Zero-copy buffer management
- [ ] Assembly optimization for critical paths

---

## 📝 Notes

### Priority Legend
- 🔥 **High Priority** - Needed soon
- 🌟 **Medium Priority** - Nice to have
- 💭 **Low Priority** - Future consideration
- 🧪 **Experimental** - Proof of concept

### Current Focus
✅ **v3.2.0 is complete and stable**
- Focus: Deployment and real-world testing
- Next: Gather user feedback for v3.3.0 planning

### Contributing
- Fork the repo
- Create feature branch
- Submit pull request
- Follow existing code style

---

## 🎯 Next Release Planning

### v3.3.0 (Future)
Ideas for next release (pending user feedback):
- TBD based on real-world usage
- Potential: Enhanced logging
- Potential: Performance profiling tools
- Potential: Additional Modbus function codes

### v4.0.0 (Future)
Major version considerations:
- ESP32 port
- Modbus TCP/IP
- Breaking changes to API (if needed)

---

## ✨ Conclusion

**Current Status:** Production-ready and stable ✅

The project has achieved all core objectives and is ready for deployment.
Future enhancements are optional and based on specific use cases.

**Focus:** Real-world testing and user feedback collection for v3.3.0 planning.
