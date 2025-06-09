#include <Arduino.h>

#include <ArduinoBLE.h>
#include <WiFi.h>
#include <WebServer.h>

#include "AddressRoomMap.h"
#include "ExtremelySimpleLogger.h"

// Wifi credentials
const char* ssid = "o-scypki";
const char* password = "szczy70pandy^";

// Start web server on port 80
WebServer server(80);

// Define service and characteristic UUIDs as constants
static const BLEUuid BATTERY_SERVICE_UUID("180F");
static const BLEUuid BATTERY_LEVEL_CHARACTERISTIC_UUID("2A19");
// Custom UUIDs for the Sensirion sensors
static const BLEUuid HUMIDITY_SERVICE_UUID("00001234-B38D-4985-720E-0F993A68EE41");
static const BLEUuid HUMIDITY_CHARACTERISTIC_UUID("00001235-B38D-4985-720E-0F993A68EE41");
static const BLEUuid TEMPERATURE_SERVICE_UUID("00002234-B38D-4985-720E-0F993A68EE41");
static const BLEUuid TEMPERATURE_CHARACTERISTIC_UUID("00002235-B38D-4985-720E-0F993A68EE41");


struct SensirionPeripheral {
  String address;
  String name;
  float humidity;
  float temperature;
  int batteryLevel;
  int rssi;

  SensirionPeripheral() : address(""), name(""), humidity(NAN), temperature(NAN), batteryLevel(-1), rssi(0) {}
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
      LOG(characteristicName);
      LOG(": ");
      LOG_LN(value);
    } else {
      LOG_LN("Received data for " + characteristicName + " too short!");
    }
}

void readBatteryValue(BLECharacteristic& batteryLevelCharacteristic, int& store) {
  const uint8_t* bytes = batteryLevelCharacteristic.value();
  uint8_t length = batteryLevelCharacteristic.valueLength();
  if (length >= 1) {
    uint8_t batteryLevel;
    memcpy(&batteryLevel, bytes, sizeof(uint8_t));
    store = batteryLevel;
    LOG("Battery Level: ");
    LOG(batteryLevel);
    LOG_LN("%");
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

void onPeripheralDiscovered(BLEDevice peripheral) {
  String name = peripheral.localName();
  if (name == "Smart Humigadget" || name == "SHT40 Gadget") {
    BLE.stopScan(); // Stop scanning to let connect to peripheral

    LOG("Found Gadget! ");
    LOG_LN(peripheral.address());

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
  knownPeripherals[index].name = peripheral.localName();

  LOG_LN("Connected. Discovering attributes ...");
  if (!peripheral.discoverAttributes()) {
    LOG_LN("Attribute discovery failed! Disconnecting.");
    peripheral.disconnect();
    return;
  }
  LOG_LN("Attributes discovered");
    
  BLEService humidityService = peripheral.service(HUMIDITY_SERVICE_UUID.str());
  BLECharacteristic humidityCharacteristic = humidityService.characteristic(HUMIDITY_CHARACTERISTIC_UUID.str());
  if (humidityCharacteristic.canSubscribe()) {
    humidityCharacteristic.setEventHandler(BLEUpdated, onHumidityUpdated);
    humidityCharacteristic.subscribe();
  }
    
  BLEService temperatureService = peripheral.service(TEMPERATURE_SERVICE_UUID.str());
  BLECharacteristic temperatureCharacteristic = temperatureService.characteristic(TEMPERATURE_CHARACTERISTIC_UUID.str());
  if (temperatureCharacteristic.canSubscribe()) {
    temperatureCharacteristic.setEventHandler(BLEUpdated, onTemperatureUpdated);
    temperatureCharacteristic.subscribe();
  }

  BLEService batteryService = peripheral.service(BATTERY_SERVICE_UUID.str());
  BLECharacteristic batteryLevelCharacteristic = batteryService.characteristic(BATTERY_LEVEL_CHARACTERISTIC_UUID.str());
  if (batteryLevelCharacteristic.canRead()) {
    batteryLevelCharacteristic.read();
    readBatteryValue(batteryLevelCharacteristic, knownPeripherals[index].batteryLevel);
  }
  if (batteryLevelCharacteristic.canSubscribe()) {
    batteryLevelCharacteristic.setEventHandler(BLEUpdated, onBatteryUpdated);
    batteryLevelCharacteristic.subscribe();
  }

  knownPeripherals[index].rssi = peripheral.rssi();

  // Once connected start scanning again
  BLE.scan();
}

void onPeripheralDisconnected(BLEDevice peripheral) {
  LOG("Disconnected from peripheral: ");
  LOG_LN(peripheral.address());
  int index = getPeripheralIndexByAddress(peripheral.address());
  knownPeripherals[index] = SensirionPeripheral();
  // Never stopped scanning so no need to call BLE.scan() again
}

// HTTP handler
void handleRoot() {
  String json = "[";
  bool first = true;
  for (int i = 0; i < MAX_FOUND_PERIPHERALS; i++) {
      if (!first) json += ",";
      first = false;
      json += "{";
      json += "\"address\": \"" + knownPeripherals[i].address + "\"";
      json += ", \"humidity\": ";
      json += isnan(knownPeripherals[i].humidity) ? "null" : String(knownPeripherals[i].humidity, 2);
      json += ", \"temperature\": ";
      json += isnan(knownPeripherals[i].temperature) ? "null" : String(knownPeripherals[i].temperature, 2);
      json += ", \"battery\": ";
      json += (knownPeripherals[i].batteryLevel < 0) ? "null" : String(knownPeripherals[i].batteryLevel);
      json += ", \"rssi\": ";
      json += knownPeripherals[i].rssi;
      json += "}";
  }
  json += "]";
  server.send(200, "application/json", json);
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
    </body>
    </html>
  )rawliteral";
  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);

  // Initialize foundPeripherals array
  for (int i = 0; i < MAX_FOUND_PERIPHERALS; i++) {
    knownPeripherals[i] = SensirionPeripheral();
  }

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected. IP address: " + WiFi.localIP().toString());

  // HTTP server setup
  server.on("/", handleRoot);
  server.on("/dashboard", handleDashboard);
  server.begin();
  Serial.println("HTTP server started");

  // initialize the Bluetooth® Low Energy hardware
  BLE.begin();
  // setup cllbacks
  BLE.setEventHandler(BLEDiscovered, onPeripheralDiscovered);
  BLE.setEventHandler(BLEConnected, onPeripheralConnected);
  BLE.setEventHandler(BLEDisconnected, onPeripheralDisconnected);
  // start scanning for peripherals
  BLE.scan();
}

void loop() {
  BLE.poll(); // poll for events
  server.handleClient(); // handle HTTP requests
}
