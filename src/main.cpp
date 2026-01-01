#include <Arduino.h>

// ESP32 provided libraries
#include <WiFi.h>
#include <WebServer.h>
#include <time.h>

// External libraries
#include <ArduinoBLE.h>
#include <ArduinoJson.h>

// Internal includes
#include "AddressRoomMap.h"
#include "ExtremelySimpleLogger.h"
#include "SensorsInfluxDBClient.h"

// Credentials and Certificates
#include "secrets.h"

// Start web server on port 80
WebServer server(80);

// InfluxDB client
SensorsInfluxDBClient sensorsInfluxDBClient;
bool cloudPublishingEnabled = false;

// Define service and characteristic UUIDs as constants
static const BLEUuid BATTERY_SERVICE_UUID("180F");
static const BLEUuid BATTERY_LEVEL_CHARACTERISTIC_UUID("2A19");
// Custom UUIDs for the Sensirion sensors
static const BLEUuid SENSIRION_HUMIDITY_SERVICE_UUID("00001234-B38D-4985-720E-0F993A68EE41");
static const BLEUuid SENSIRION_HUMIDITY_CHARACTERISTIC_UUID("00001235-B38D-4985-720E-0F993A68EE41");
static const BLEUuid SENSIRION_TEMPERATURE_SERVICE_UUID("00002234-B38D-4985-720E-0F993A68EE41");
static const BLEUuid SENSIRION_TEMPERATURE_CHARACTERISTIC_UUID("00002235-B38D-4985-720E-0F993A68EE41");
static const BLEUuid SENSIRION_SCD4X_CO2_SERVICE_UUID("00007000-B38D-4985-720E-0F993A68EE41");
static const BLEUuid SENSIRION_SCD4X_CO2_CHARACTERISTIC_UUID("00007001-B38D-4985-720E-0F993A68EE41");

struct SensirionPeripheral {
  String address;
  float humidity;
  float temperature;
  int co2Level;
  int batteryLevel;
  int rssi;

  SensirionPeripheral() : address(""), humidity(NAN), temperature(NAN), co2Level(-1), batteryLevel(-1), rssi(0) {}
};

static const int MAX_FOUND_PERIPHERALS = 10;
SensirionPeripheral knownPeripherals[MAX_FOUND_PERIPHERALS];

int getPeripheralIndexByAddress(const String& address) {
  for (int i = 0; i < MAX_FOUND_PERIPHERALS; i++) {
    if (knownPeripherals[i].address == address) {
      return i;
    }
  }
  return -1; // Not found
}

int getNextAvailableIndex() {
  for (int i = 0; i < MAX_FOUND_PERIPHERALS; i++) {
    if (knownPeripherals[i].address == "") {
      return i;
    }
  }
  return -1; // No available index
}

// Function to handle reading a float characteristic
void readFloatCharacteristicValue(BLECharacteristic& characteristic, const String& characteristicName, float& store) {
    const uint8_t* bytes = characteristic.value();
    uint8_t length = characteristic.valueLength();
    if (length >= 4) {
      float value;
      memcpy(&value, bytes, sizeof(float));
      store = value;
      LOG_PRINTF("%s: %.2f\n", characteristicName.c_str(), value);
    } else {
      LOG_LN("Received data for " + characteristicName + " too short!");
    }
}

void readCO2Value(BLECharacteristic& co2Characteristic, int& store) {
  const uint8_t* bytes = co2Characteristic.value();
  uint8_t length = co2Characteristic.valueLength();
  if (length >= 2) {
    uint16_t co2Level;
    memcpy(&co2Level, bytes, sizeof(uint16_t));
    store = co2Level;
    LOG_PRINTF("CO2 Level: %d ppm\n", co2Level);
  } else {
    LOG_LN("Received data for CO2 Level too short!");
  }
}

void readBatteryValue(BLECharacteristic& batteryLevelCharacteristic, int& store) {
  const uint8_t* bytes = batteryLevelCharacteristic.value();
  uint8_t length = batteryLevelCharacteristic.valueLength();
  if (length >= 1) {
    uint8_t batteryLevel;
    memcpy(&batteryLevel, bytes, sizeof(uint8_t));
    store = batteryLevel;
    LOG_PRINTF("Battery Level: %d %\n", batteryLevel);
  } else {
    LOG_LN("Received data for Battery Level too short!");
  }
}

void onHumidityUpdated(BLEDevice peripheral, BLECharacteristic characteristic) {
  int index = getPeripheralIndexByAddress(peripheral.address());
  readFloatCharacteristicValue(characteristic, "Humidity", knownPeripherals[index].humidity);
}

void onTemperatureUpdated(BLEDevice peripheral, BLECharacteristic characteristic) {
  int index = getPeripheralIndexByAddress(peripheral.address());
  readFloatCharacteristicValue(characteristic, "Temperature", knownPeripherals[index].temperature);
}

void onBatteryUpdated(BLEDevice peripheral, BLECharacteristic characteristic) {
  int index = getPeripheralIndexByAddress(peripheral.address());
  readBatteryValue(characteristic, knownPeripherals[index].batteryLevel);
}

void onCO2Updated(BLEDevice peripheral, BLECharacteristic characteristic) {
  int index = getPeripheralIndexByAddress(peripheral.address());
  if (index >= 0) {
    readCO2Value(characteristic, knownPeripherals[index].co2Level);
  }
}

void onPeripheralDiscovered(BLEDevice peripheral) {
  String name = peripheral.localName();
  if (name == "Smart Humigadget" || name == "SHT40 Gadget" || name == "MyCO2") {
    BLE.stopScan(); // Stop scanning to let connect to peripheral
    
    LOG_LN(name + ": " + peripheral.address());

    LOG_LN("Opening connection to found peripheral ... ");
    if (!peripheral.connect()) {
      LOG_LN("Failed to connect. Resuming scanning.");
      BLE.scan();
    }
  }
}

void onPeripheralConnected(BLEDevice peripheral) {
  int index = getPeripheralIndexByAddress(peripheral.address());
  if (index < 0) { // Not found, get next available index
    index = getNextAvailableIndex();
    if (index < 0) { // No available index
      LOG_LN("No available index for new peripheral.");
      return;
    }
  }
  knownPeripherals[index].address = peripheral.address();

  LOG_LN("Connected. Discovering attributes ...");
  if (!peripheral.discoverAttributes()) {
    LOG_LN("Attribute discovery failed! Disconnecting.");
    peripheral.disconnect();
    return;
  }
  LOG_LN("Attributes discovered");
    
  BLEService sensirionHumidityService = peripheral.service(SENSIRION_HUMIDITY_SERVICE_UUID.str());
  BLECharacteristic sensirionHumidityCharacteristic = sensirionHumidityService.characteristic(SENSIRION_HUMIDITY_CHARACTERISTIC_UUID.str());
  if (sensirionHumidityCharacteristic.canSubscribe()) {
    sensirionHumidityCharacteristic.setEventHandler(BLEUpdated, onHumidityUpdated);
    sensirionHumidityCharacteristic.subscribe();
  }
  BLEService sensirionTemperatureService = peripheral.service(SENSIRION_TEMPERATURE_SERVICE_UUID.str());
  BLECharacteristic sensirionTemperatureCharacteristic = sensirionTemperatureService.characteristic(SENSIRION_TEMPERATURE_CHARACTERISTIC_UUID.str());
  if (sensirionTemperatureCharacteristic.canSubscribe()) {
    sensirionTemperatureCharacteristic.setEventHandler(BLEUpdated, onTemperatureUpdated);
    sensirionTemperatureCharacteristic.subscribe();
  }

  BLEService batteryService = peripheral.service(BATTERY_SERVICE_UUID.str());
  BLECharacteristic batteryLevelCharacteristic = batteryService.characteristic(BATTERY_LEVEL_CHARACTERISTIC_UUID.str());
  if (batteryLevelCharacteristic.canRead()) {  // we're reading to get initial battery level value since updates are very rare
    batteryLevelCharacteristic.read();
    readBatteryValue(batteryLevelCharacteristic, knownPeripherals[index].batteryLevel);
  }
  if (batteryLevelCharacteristic.canSubscribe()) {
    batteryLevelCharacteristic.setEventHandler(BLEUpdated, onBatteryUpdated);
    batteryLevelCharacteristic.subscribe();
  }

  // No need to test for services as ArduinoBLE library handles missing services gracefully
  BLEService scd4xCO2Service = peripheral.service(SENSIRION_SCD4X_CO2_SERVICE_UUID.str());
  BLECharacteristic scd4xCO2LevelCharacteristic = scd4xCO2Service.characteristic(SENSIRION_SCD4X_CO2_CHARACTERISTIC_UUID.str());
  if (scd4xCO2LevelCharacteristic.canSubscribe()) {
    scd4xCO2LevelCharacteristic.setEventHandler(BLEUpdated, onCO2Updated);
    scd4xCO2LevelCharacteristic.subscribe();
  }    

  knownPeripherals[index].rssi = peripheral.rssi();

  // Once connected start scanning again
  BLE.scan();
}

void onPeripheralDisconnected(BLEDevice peripheral) {
  LOG_PRINTF("Disconnected from peripheral: %s\n", peripheral.address().c_str());
  int index = getPeripheralIndexByAddress(peripheral.address());
  knownPeripherals[index] = SensirionPeripheral();
  // Never stopped scanning so no need to call BLE.scan() again
}

// HTTP handler
void handleRoot() {
  StaticJsonDocument<1024> respJsonDoc;
  JsonArray array = respJsonDoc.to<JsonArray>();
  for (int i = 0; i < MAX_FOUND_PERIPHERALS; i++) {
    if (knownPeripherals[i].address == "") { 
      continue; // Skip empty entries 
    }
    JsonObject responseObj = array.createNestedObject();
    responseObj["address"] = knownPeripherals[i].address;
    responseObj["humidity"] = isnan(knownPeripherals[i].humidity) ? "null" : String(knownPeripherals[i].humidity, 2);
    responseObj["temperature"] = isnan(knownPeripherals[i].temperature) ? "null" : String(knownPeripherals[i].temperature, 2);
    responseObj["co2"] = (knownPeripherals[i].co2Level < 0) ? "null" : String(knownPeripherals[i].co2Level);
    responseObj["battery"] = (knownPeripherals[i].batteryLevel < 0) ? "null" : String(knownPeripherals[i].batteryLevel);
    responseObj["rssi"] = knownPeripherals[i].rssi;
  }

  String jsonString;
  serializeJsonPretty(array, jsonString);
  server.send(200, "application/json", jsonString);
}

// Handle toggle cloud publishing
void handleToggleCloud() {
  if (server.hasArg("enabled")) {
    String state = server.arg("enabled");
    cloudPublishingEnabled = (state == "true" || state == "1");
    LOG_PRINTF("Cloud publishing %s\n", cloudPublishingEnabled ? "enabled" : "disabled");
  }
  
  String response = "{\"cloudPublishing\": " + String(cloudPublishingEnabled ? "true" : "false") + "}";
  server.send(200, "application/json", response);
}

// Better dashboard
void handleDashboard() {
  String html = R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
      <title>Sensor Dashboard</title>
      <meta http-equiv="refresh" content="10">
      <style>
        body { font-family: Arial; text-align: center; background: #f0f0f0; }
        .tile {
          display: inline-block;
          background: #fff;
          border-radius: 10px;
          box-shadow: 0 2px 8px rgba(0,0,0,0.1);
          margin: 20px;
          padding: 20px 40px;
          min-width: 220px;
        }
        .value { font-size: 2em; }
        .addr { font-size: 0.8em; color: #888; }
        button#cloudBtn {
          padding: 12px 24px;
          font-size: 16px;
          font-weight: bold;
          border-radius: 8px;
          border: none;
          cursor: pointer;
          transition: all 0.3s ease;
          box-shadow: 0 4px 6px rgba(0,0,0,0.1);
          background-color: #4CAF50;
          color: white;
          margin: 20px auto;
          display: block;
        }

        button#cloudBtn:hover {
          background-color: #45a049;
          box-shadow: 0 6px 8px rgba(0,0,0,0.15);
          transform: translateY(-2px);
        }

        button#cloudBtn.off {
          background-color: #f44336;
        }

        button#cloudBtn.off:hover {
          background-color: #d32f2f;
        }
      </style>
    </head>
    <body>
      <h2>Humidity & Temperature Dashboard</h2>
  )rawliteral";
  for (int i = 0; i < MAX_FOUND_PERIPHERALS; i++) {
    if (knownPeripherals[i].address != "") {
      html += "<div class='tile'>";
      html += "<div><b>" + getRoomNameByAddress(knownPeripherals[i].address) + "</b></div>";
      html += "<div class='addr'>" + knownPeripherals[i].address + "</div>";
      html += "<div>Humidity: <span class='value'>";
      html += isnan(knownPeripherals[i].humidity) ? "N/A" : String(knownPeripherals[i].humidity, 1);
      html += " %</span></div>";
      html += "<div>Temperature: <span class='value'>";
      html += isnan(knownPeripherals[i].temperature) ? "N/A" : String(knownPeripherals[i].temperature, 1);
      html += " &deg;C</span></div>";
      html += "<div>CO2: <span class='value'>";
      html += (knownPeripherals[i].co2Level < 0) ? "N/A" : String(knownPeripherals[i].co2Level) + " ppm";
      html += "</span></div>";
      html += "<div>Battery: <span class='value'>";
      html += (knownPeripherals[i].batteryLevel < 0) ? "N/A" : String(knownPeripherals[i].batteryLevel) + " %";
      html += "</span></div>";
      html += "<div>RSSI: <span class='value'>";
      html += String(knownPeripherals[i].rssi) + " dBm";
      html += "</span></div>";
      html += "</div>";
    }
  }
  html += R"rawliteral(
    <div style="margin-top: 30px;">
      <button onclick="toggleCloud()" id="cloudBtn">
        Turn Cloud Publishing OFF
      </button>
    </div>
    <script>
      function toggleCloud() {
        const btn = document.getElementById('cloudBtn');
        const newState = btn.innerText.includes('OFF') ? 'false' : 'true';
        fetch('/api/cloud?enabled=' + newState)
          .then(response => response.json())
          .then(data => {
            const isEnabled = data.cloudPublishing;
            btn.innerText = 'Turn Cloud Publishing ' + 
              (data.cloudPublishing ? 'OFF' : 'ON');
            btn.className = isEnabled ? 'on' : 'off';
          });
      }
      // Update button state on load
      fetch('/api/cloud')
        .then(response => response.json())
        .then(data => {
          const btn = document.getElementById('cloudBtn');
          const isEnabled = data.cloudPublishing;
          btn.innerText = 'Turn Cloud Publishing ' + (isEnabled ? 'OFF' : 'ON');
          btn.className = isEnabled ? 'on' : 'off';
        });
    </script>
  )rawliteral";
  html += R"rawliteral(
    </body>
    </html>
  )rawliteral";
  server.send(200, "text/html", html);
}

void writeSensorDataToInfluxDB() {
  // Skip if cloud publishing is disabled
  if (!cloudPublishingEnabled) {
    return;
  }
  // Process each peripheral individually
  for (int i = 0; i < MAX_FOUND_PERIPHERALS; i++) {
    if (knownPeripherals[i].address != "") {
      sensorsInfluxDBClient.writeSensorData(knownPeripherals[i].address, getRoomNameByAddress(knownPeripherals[i].address), knownPeripherals[i].temperature, knownPeripherals[i].humidity, knownPeripherals[i].co2Level, knownPeripherals[i].batteryLevel, knownPeripherals[i].rssi);

      // Small delay to prevent overwhelming the database
      delay(100);
    }
  }
}

#if MEMORY_DEBUG
void printMemoryInfo() {
  Serial.println();
  Serial.println("=== Memory Info ===");
  Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
  Serial.printf("Largest free block: %d bytes\n", ESP.getMaxAllocHeap());
  Serial.printf("Total heap: %d bytes\n", ESP.getHeapSize());
  Serial.printf("Free PSRAM: %d bytes\n", ESP.getFreePsram());
  Serial.printf("Flash size: %d bytes\n", ESP.getFlashChipSize());
  Serial.printf("Sketch size: %d bytes\n", ESP.getSketchSize());
  Serial.printf("Free sketch space: %d bytes\n", ESP.getFreeSketchSpace());
  Serial.println("===================");
  Serial.println();
}
#endif

void setup() {
  Serial.begin(115200);

  // Initialize foundPeripherals array
  for (int i = 0; i < MAX_FOUND_PERIPHERALS; i++) {
    knownPeripherals[i] = SensirionPeripheral();
  }

  // Connect to WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi ");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected. IP address: " + WiFi.localIP().toString());

  // Set NTP details UTC time only
  configTime(0, 0, "pool.ntp.org", "europe.pool.ntp.org");

  // Setup InfluxDB Client
  sensorsInfluxDBClient.setup();
  sensorsInfluxDBClient.connect();

  // HTTP server setup
  server.on("/", handleRoot);
  server.on("/dashboard", handleDashboard);
  server.on("/api/cloud", handleToggleCloud);
  server.begin();
  Serial.println("HTTP server started");

  // initialize the Bluetooth® Low Energy hardware
  BLE.begin();
  // setup callbacks
  BLE.setEventHandler(BLEDiscovered, onPeripheralDiscovered);
  BLE.setEventHandler(BLEConnected, onPeripheralConnected);
  BLE.setEventHandler(BLEDisconnected, onPeripheralDisconnected);
  // start scanning for peripherals
  BLE.scan();

  #if MEMORY_DEBUG
  printMemoryInfo();
  #endif
}

// Timer variables for periodic publishing
unsigned long previousMillis = 0;
const long publishInterval = 60000; // Publish every 60 seconds

void loop() {
  BLE.poll(); // poll for events
  server.handleClient(); // handle HTTP requests
  
  // Publish data periodically
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= publishInterval) {
    previousMillis = currentMillis;
    writeSensorDataToInfluxDB();
  }

  #if MEMORY_DEBUG
  static unsigned long lastMemCheck = 0;
  if (millis() - lastMemCheck > 30000) {
    lastMemCheck = millis();
    printMemoryInfo();
  }
  #endif
}
