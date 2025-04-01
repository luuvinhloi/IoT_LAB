# PlatformIO-RTOS_DHT11-Task

This project demonstrates the use of FreeRTOS tasks on an ESP32 to read data from a DHT11 sensor and send it to a ThingsBoard server via MQTT. The project is implemented using PlatformIO.

## Features

- **WiFi Connectivity**: Connects to a WiFi network.
- **DHT11 Sensor Integration**: Reads temperature and humidity data.
- **ThingsBoard Integration**: Sends telemetry data to a ThingsBoard server.
- **FreeRTOS Tasks**: Implements tasks for WiFi management, ThingsBoard connection, sensor data reading, and reconnection handling.

## Hardware Requirements

- ESP32 development board
- DHT11 temperature and humidity sensor
- Jumper wires
- Breadboard (optional)

## Wiring Diagram

| ESP32 Pin | DHT11 Pin |
|-----------|-----------|
| G14       | Data      |
| 3.3V      | VCC       |
| GND       | GND       |

## Configuration

1. Update the WiFi credentials in `main.cpp`:
   ```cpp
   constexpr char WIFI_SSID[] = "your_wifi_ssid";
   constexpr char WIFI_PASSWORD[] = "your_wifi_password";
   ```

2. Update the ThingsBoard server details in `main.cpp`:
   ```cpp
   constexpr char TOKEN[] = "your_thingsboard_token";
   constexpr char THINGSBOARD_SERVER[] = "your_thingsboard_server";
   constexpr uint16_t THINGSBOARD_PORT = 1883U;
   ```

## Installation and Usage

1. Clone the repository:
   ```bash
   git clone <repository_url>
   ```

2. Open the project in PlatformIO.

3. Connect your ESP32 board to your computer.

4. Build and upload the firmware:
   ```bash
   pio run --target upload
   ```

5. Open the Serial Monitor to view logs:
   ```bash
   pio device monitor
   ```

## FreeRTOS Tasks

- **WiFiTask**: Manages WiFi connection.
- **ThingsBoardTask**: Handles ThingsBoard MQTT connection and data transmission.
- **ReconnectTask**: Periodically checks and reconnects to WiFi and ThingsBoard if disconnected.
- **DHTSensorTask**: Reads temperature and humidity data from the DHT11 sensor and sends it to ThingsBoard.

## Troubleshooting

- Ensure the DHT11 sensor is properly connected to the ESP32.
- Verify WiFi credentials and ThingsBoard server details.
- Check the Serial Monitor for error messages.

## License

This project is licensed under the MIT License. See the `LICENSE` file for details.

# Espressif 32: development platform for [PlatformIO](https://platformio.org)

[![Build Status](https://github.com/platformio/platform-espressif32/workflows/Examples/badge.svg)](https://github.com/platformio/platform-espressif32/actions)

ESP32 is a series of low-cost, low-power system on a chip microcontrollers with integrated Wi-Fi and Bluetooth. ESP32 integrates an antenna switch, RF balun, power amplifier, low-noise receive amplifier, filters, and power management modules.

* [Home](https://registry.platformio.org/platforms/platformio/espressif32) (home page in the PlatformIO Registry)
* [Documentation](https://docs.platformio.org/page/platforms/espressif32.html) (advanced usage, packages, boards, frameworks, etc.)

# Usage

1. [Install PlatformIO](https://platformio.org)
2. Create PlatformIO project and configure a platform option in [platformio.ini](https://docs.platformio.org/page/projectconf.html) file:

## Stable version

See `platform` [documentation](https://docs.platformio.org/en/latest/projectconf/sections/env/options/platform/platform.html#projectconf-env-platform) for details.

```ini
[env:stable]
; recommended to pin to a version, see https://github.com/platformio/platform-espressif32/releases
; platform = espressif32 @ ^6.0.1
platform = espressif32
board = yolo_uno
framework = arduino
monitor_speed = 115200

build_flags =
    -D ARDUINO_USB_MODE=1
    -D ARDUINO_USB_CDC_ON_BOOT=1

## Development version

```ini
[env:development]
platform = https://github.com/platformio/platform-espressif32.git
board = yolo_uno
framework = arduino
monitor_speed = 115200
build_flags =
    -D ARDUINO_USB_MODE=1
    -D ARDUINO_USB_CDC_ON_BOOT=1