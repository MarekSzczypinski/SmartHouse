# SmartHouse IoT Project

A smart home monitoring system that collects temperature, humidity, and battery data from Sensirion BLE sensors and provides both local dashboard visualization and AWS cloud integration.

## Features

* **BLE Sensor Integration**: Automatically discovers and connects to Sensirion humidity/temperature sensors
* **Real-time Dashboard**: Web-based dashboard showing sensor readings with auto-refresh
* **Room Mapping**: Associates sensor MAC addresses with room names for easy identification
* **AWS IoT Core Integration**: Securely publishes sensor data to AWS cloud for storage and analysis
* **Cloud Toggle**: Enable/disable cloud publishing directly from the dashboard
* **NTP Time Synchronization**: Uses accurate UTC timestamps for data records
* **Debug Mode**: Optional memory monitoring and debug output
* **Multiple Build Configurations**: Production and debug builds via PlatformIO environments

## Hardware Requirements

* ESP32S3 board ([Seeed XIAO ESP32S3](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/))
* Sensirion Smart Humigadget or [SHT40 Gadget](https://sensirion.com/products/catalog/SHT4x-Smart-Gadget) sensors
* WiFi connection

## Setup

### Secrets

1. Copy src/secrets.h.template to src/secrets.h
2. Replace the placeholders with your AWS IoT Core, WiFi etc credentials

### AWS IoT Core Setup

1. Create an AWS IoT Thing for your ESP32 gateway
2. Generate and download certificates
3. Create a policy allowing connection and publishing
4. Update the certificates in src/secrets.h
5. Create new rule in Messages Routing
    * Set SQL statement to: `SELECT deviceId, location, humidity, temperature, battery, rssi, timestamp FROM 'smarthouse/sensors'`
    * Action: DynamoDBv2 -> Set table name and IAM Role that allows to put items into the DynamoDB table

## Automatic AWS IoT Core Setup

The automatic AWS IoT Core Setup can be done instead of the steps above. 

1. Run `deploy.sh` script from `cloud-backend` directory.
2. Update the certificates in `src/secrets.h` from `certificate.pem.crt` and `private.pem.key` files.
3. If running for the first time copy the IoT Endpoint from output of `cloud-backend/deploy.sh` script to `src/secrets.h`

Run `cloud-backend/destroy.sh` script to reverse all changes made by automatic AWS IoT Core setup.

**Note**: The private key is only available during certificate creation and cannot be retrieved later from CloudFormation or CDK.

### Room Configuration

Edit src/AddressRoomMap.h to map your sensor MAC addresses to room names:

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
   - PubSubClient by Nick O'Leary
   - ArduinoJson by Benoit Blanchon
5. Copy all files from `src/` folder to your Arduino sketch folder
6. Rename `main.cpp` to `main.ino`
7. Create separate tabs in Arduino IDE for each `.h` file
8. Compile and upload

## Usage

1. Power on the ESP32 and wait for it to connect to WiFi
2. Monitor the Serial output for connection status and IP address
3. Access the dashboard at `http://<esp32-ip-address>/dashboard`
4. View sensor readings that update every 10 seconds
5. Toggle cloud publishing using the styled button at the bottom of the dashboard
6. Access raw JSON data at `http://<esp32-ip-address>/`
7. Check memory usage (debug build only) via Serial monitor

## Project Structure

* **src/main.cpp**: Main application code
* **src/AddressRoomMap.h**: Maps BLE addresses to room names
* **src/ExtremelySimpleLogger.h**: Simple logging utility
* **src/AWSIoTClient.h**: AWS IoT Core client implementation
* **src/secrets.h**: WiFi and AWS credentials (not in repo)
* **platformio.ini**: Build configurations with shared common settings

## Libraries Used

* **ArduinoBLE**: For BLE sensor communication
* **PubSubClient**: For MQTT communication with AWS IoT
* **ArduinoJson**: For JSON parsing and generation
* **ESP32 WiFi**: For network connectivity
* **ESP32 WebServer**: For hosting the dashboard

## License
MIT License
