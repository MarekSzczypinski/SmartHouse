# SmartHouse IoT Project

A smart home monitoring system that collects temperature, humidity, and battery data from Sensirion BLE sensors and stores them in a local InfluxDB time-series database with professional Grafana visualization. Additionally ESP32 provides simple dashboard with current data accessible by web browser.

## Features

* **BLE Sensor Integration**: Automatically discovers and connects to Sensirion humidity/temperature sensors
* **Real-time Dashboard**: Web-based dashboard showing sensor readings with auto-refresh
* **Room Mapping**: Associates sensor MAC addresses with room names for easy identification
* **InfluxDB Integration**: Local time-series database for efficient sensor data storage
* **Grafana Dashboards**: Professional visualization with pre-configured dashboard
* **Data Toggle**: Enable/disable data publishing directly from the ESP32 dashboard
* **NTP Time Synchronization**: Uses accurate UTC timestamps for data records
* **Debug Mode**: Optional memory monitoring and debug output
* **Multiple Build Configurations**: Production and debug builds via PlatformIO environments

## Hardware Requirements

* ESP32S3 board ([Seeed XIAO ESP32S3](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/))
* Sensirion Smart Humigadget or [SHT40 Gadget](https://sensirion.com/products/catalog/SHT4x-Smart-Gadget) sensors
* WiFi connection

## Setup

### Secrets

1. Copy `src/secrets.h.template` to `src/secrets.h`
2. Replace the placeholders with your WiFi and InfluxDB credentials

### InfluxDB + Grafana Setup

1. Copy `.env.template` to `.env` and configure:
   ```bash
   INFLUXDB_USERNAME=your-influxdb-userame
   INFLUXDB_PASSWORD=your-influxdb-password
   INFLUXDB_ORG=smarthouse
   INFLUXDB_BUCKET=sensors
   GRAFANA_USERNAME=your-grafana-username
   GRAFANA_PASSWORD=your-grafana-password
   INFLUXDB_GRAFANA_TOKEN=your-grafana-read-influxdb-token
   ```
2. Start the stack: `docker-compose up influxdb2 -d`
3. Access InfluxDB at `http://localhost:8086` and generate two API tokens:
   * one that writes to sensors bucket - for ESP32
   * one that reads from sensors bucket - for Grafana
4. Update the token in `.env` file and run: `docker-compose up grafana -d`
5. Access Grafana at `http://localhost:3000` (admin/your-grafana-password)
6. Update InfluxDB credentials in `src/secrets.h` with the `write-to-sensors` token. Make sure you're using the IP address not `localhost` for the URL.

### Room Configuration

Edit `src/AddressRoomMap.h` to map your sensor MAC addresses to room names:

```cpp
static const AddressRoomPair roomSimpleMap[] = {
    {"f9:3f:1d:46:f4:0c", "Kitchen"},
    {"f8:ce:3f:2b:5e:55", "Living Room"},
    {"eb:d9:7e:a1:e1:08", "Computer Desk"},
};
```

## Development Environment Setup

### Using PlatformIO (Recommended)

1. Install [PlatformIO](https://platformio.org/install)
2. Clone this repository
3. Open the project folder in your IDE with PlatformIO extension
4. Choose your build environment:
   - `seeed_xiao_esp32s3`: Production build
   - `seeed_xiao_esp32s3_debug`: Debug build with memory monitoring
5. Build and upload to your ESP32S3 board

### Using Arduino IDE

1. Install [Arduino IDE](https://www.arduino.cc/en/software)
2. Add ESP32 board support:
   - Go to File → Preferences
   - Add `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json` to Additional Board Manager URLs
   - Go to Tools → Board → Boards Manager
   - Search for "ESP32" and install "esp32 by Espressif Systems"
3. Select your board: Tools → Board → ESP32 Arduino → XIAO_ESP32S3
4. Install required libraries via Library Manager (Tools → Manage Libraries):
   - ArduinoBLE by Arduino
   - ArduinoJson by Benoit Blanchon
   - ESP8266 Influxdb by Tobias Schürg
5. Copy all files from `src/` folder to your Arduino sketch folder
6. Rename `main.cpp` to `main.ino`
7. Create separate tabs in Arduino IDE for each `.h` file
8. Compile and upload

## Usage

### ESP32 Dashboard
1. Power on the ESP32 and wait for it to connect to WiFi
2. Monitor the Serial output for connection status and IP address
3. Access the dashboard at `http://<esp32-ip-address>/dashboard`
4. View sensor readings that update every 10 seconds
5. Toggle data publishing using the styled button at the bottom of the dashboard
6. Access raw JSON data at `http://<esp32-ip-address>/`
7. Check memory usage (debug build only) via Serial monitor

### Grafana Dashboards
1. Access Grafana at `http://localhost:3000`
2. Navigate to the pre-configured "Smart House Dashboard"
3. View time-series charts for temperature, humidity, battery, and RSSI
4. Data updates automatically as ESP32 writes to InfluxDB
5. Use Grafana's built-in features for data analysis and alerting

### Data Storage
- **InfluxDB**: All sensor data is stored locally in time-series format
- **Grafana**: Visualizes data with automatic refresh and historical analysis
- **Toggle**: Enable/disable data publishing via ESP32 dashboard

## Project Structure

### ESP32 Code
* **src/main.cpp**: Main application code with BLE sensor management
* **src/AddressRoomMap.h**: Maps BLE addresses to room names
* **src/ExtremelySimpleLogger.h**: Simple logging utility
* **src/SensorsInfluxDBClient.h**: InfluxDB client wrapper
* **src/secrets.h**: WiFi and InfluxDB credentials (not in repo)
* **platformio.ini**: Build configurations

### Infrastructure
* **docker-compose.yaml**: InfluxDB + Grafana stack
* **grafana/provisioning/**: Pre-configured dashboards and data sources
* **grafana/dashboards/**: Dashboard JSON definitions
* **.env**: Environment variables for Docker services (not in repo)

## Libraries Used

### ESP32 Libraries
* **ArduinoBLE**: For BLE sensor communication
* **ArduinoJson**: For JSON parsing and generation
* **ESP8266 Influxdb**: For time-series data storage
* **ESP32 WiFi**: For network connectivity
* **ESP32 WebServer**: For hosting the dashboard

### Infrastructure
* **InfluxDB 2.x**: Time-series database
* **Grafana 12.x**: Data visualization and dashboards
* **Docker Compose**: Container orchestration

## License
MIT License
