# Labuino ard-rasp-ist
Arduino + RaspberryPi Physics Laboratories

## Thermodynamics Experiments

### Calorimetry Lab 

Compiling Instructions:  
1. Install [PlatformIO CLI tools](https://docs.platformio.org/en/latest/core/installation/methods/installer-script.html#super-quick-macos-linux)
2. (Alternative: Install [VS Code GUI](https://code.visualstudio.com) plus PlatformIO *Extensions*.)
3. Checkout *ard-rasp-ist* Github [Project](https://github.com/ipfn-hpl/ard-rasp-ist)  
```bash
git repo clone https://github.com/ipfn-hpl/ard-rasp-ist.git 
```  
    1. Hardware [M5StickC PLUS2](https://docs.m5stack.com/en/core/M5StickC%20PLUS2)
```bash
cd ard-rasp-ist/code/arduino/m5scp2/ds18b20-influx
```
    2. Hardware [ESP32-C3 0.42-Inch Oled](https://www.aliexpress.com/item/1005007715518362.html)
```bash
cd ard-rasp-ist/code/arduino/m5scp2/esp32-c3-oled
```
4. Compile, Upload firmware an monitor serial output
```bash
pio run -t upload -t monitor
```
5. If necessary change the WiFi SSID/passords in `include/arduino_secrets.h` 

### Stirling Engine
