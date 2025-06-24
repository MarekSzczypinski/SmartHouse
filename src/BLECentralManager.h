#ifndef BLE_CENTRAL_MANAGER_H
#define BLE_CENTRAL_MANAGER_H

#include <Arduino.h>

// ESP32 provided libraries
#include <time.h>

// External libraries
#include <ArduinoBLE.h>
#include <ArduinoJson.h>

// Internal includes
#include "ExtremelySimpleLogger.h"

// Define service and characteristic UUIDs as constants
static const BLEUuid BATTERY_SERVICE_UUID("180F");
static const BLEUuid BATTERY_LEVEL_CHARACTERISTIC_UUID("2A19");
// Custom UUIDs for the Sensirion sensors
static const BLEUuid SENSIRION_HUMIDITY_SERVICE_UUID("00001234-B38D-4985-720E-0F993A68EE41");
static const BLEUuid SENSIRION_HUMIDITY_CHARACTERISTIC_UUID("00001235-B38D-4985-720E-0F993A68EE41");
static const BLEUuid SENSIRION_TEMPERATURE_SERVICE_UUID("00002234-B38D-4985-720E-0F993A68EE41");
static const BLEUuid SENSIRION_TEMPERATURE_CHARACTERISTIC_UUID("00002235-B38D-4985-720E-0F993A68EE41");

static const int MAX_NUMBER_OF_PERIPHERALS = 10;

struct SensirionPeripheral {
  String address;
  String name;
  float humidity;
  float temperature;
  int batteryLevel;
  int rssi;

  SensirionPeripheral() : address(""), name(""), humidity(NAN), temperature(NAN), batteryLevel(-1), rssi(0) {}
};

class BLECentralManager {
public:
    static void addPeripheral(const SensirionPeripheral& peripheral);
    static void removePeripheral(const String& address);
    static SensirionPeripheral* getPeripheral(const String& address);
    static SensirionPeripheral* getPeripherals() {
        return peripherals;
    }
    static void setup();
    static void loop() {
        BLE.poll();
    }
private:
    static SensirionPeripheral peripherals[MAX_NUMBER_OF_PERIPHERALS];
    static int getPeripheralIndexByAddress(const String& address);
    static int getNextAvailableIndex();
    static void readFloatCharacteristicValue(BLECharacteristic& characteristic, const String& characteristicName, float& store);
    static void readBatteryValue(BLECharacteristic& batteryLevelCharacteristic, int& store);
    static void onHumidityUpdated(BLEDevice peripheral, BLECharacteristic characteristic);
    static void onTemperatureUpdated(BLEDevice peripheral, BLECharacteristic characteristic);
    static void onBatteryUpdated(BLEDevice peripheral, BLECharacteristic characteristic);
    static void onPeripheralDiscovered(BLEDevice peripheral);
    static void onPeripheralConnected(BLEDevice peripheral);
    static void onPeripheralDisconnected(BLEDevice peripheral);

};

void BLECentralManager::setup() {
  // Initialize foundPeripherals array
  for (int i = 0; i < MAX_NUMBER_OF_PERIPHERALS; i++) {
      peripherals[i] = SensirionPeripheral();
  }
  // initialize the Bluetooth® Low Energy hardware
  if (!BLE.begin()) {
      LOG_LN("Failed to initialize BLE!");
      return;
  }
  // setup callbacks
  BLE.setEventHandler(BLEDiscovered, BLECentralManager::onPeripheralDiscovered);
  BLE.setEventHandler(BLEConnected, BLECentralManager::onPeripheralConnected);
  BLE.setEventHandler(BLEDisconnected, BLECentralManager::onPeripheralDisconnected);
  // start scanning for peripherals
  BLE.scan();
}

int BLECentralManager::getPeripheralIndexByAddress(const String& address) {
  for (int i = 0; i < MAX_NUMBER_OF_PERIPHERALS; i++) {
    if (peripherals[i].address == address) {
      return i;
    }
  }
  return -1; // Not found
}

int BLECentralManager::getNextAvailableIndex() {
  for (int i = 0; i < MAX_NUMBER_OF_PERIPHERALS; i++) {
    if (peripherals[i].address == "") {
      return i;
    }
  }
  return -1; // No available index
}

// Function to handle reading a float characteristic
void BLECentralManager::readFloatCharacteristicValue(BLECharacteristic& characteristic, const String& characteristicName, float& store) {
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

void BLECentralManager::readBatteryValue(BLECharacteristic& batteryLevelCharacteristic, int& store) {
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

void BLECentralManager::onHumidityUpdated(BLEDevice peripheral, BLECharacteristic characteristic) {
  int index = getPeripheralIndexByAddress(peripheral.address());
  readFloatCharacteristicValue(characteristic, "Humidity", peripherals[index].humidity);
}

void BLECentralManager::onTemperatureUpdated(BLEDevice peripheral, BLECharacteristic characteristic) {
  int index = getPeripheralIndexByAddress(peripheral.address());
  readFloatCharacteristicValue(characteristic, "Temperature", peripherals[index].temperature);
}

void BLECentralManager::onBatteryUpdated(BLEDevice peripheral, BLECharacteristic characteristic) {
  int index = getPeripheralIndexByAddress(peripheral.address());
  readBatteryValue(characteristic, peripherals[index].batteryLevel);
}

void BLECentralManager::onPeripheralDiscovered(BLEDevice peripheral) {
  String name = peripheral.localName();
  if (name == "Smart Humigadget" || name == "SHT40 Gadget") {
    BLE.stopScan(); // Stop scanning to let connect to peripheral
    
    LOG_LN(name + ": " + peripheral.address());

    LOG_LN("Opening connection to found peripheral ... ");
    if (!peripheral.connect()) {
      LOG_LN("Failed to connect. Resuming scanning.");
      BLE.scan();
    }
  }
}

void BLECentralManager::onPeripheralConnected(BLEDevice peripheral) {
  int index = getPeripheralIndexByAddress(peripheral.address());
  if (index < 0) { // Not found, get next available index
    index = getNextAvailableIndex();
    if (index < 0) { // No available index
      LOG_LN("No available index for new peripheral.");
      return;
    }
  }
  peripherals[index].address = peripheral.address();
  peripherals[index].name = peripheral.localName();

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
  if (batteryLevelCharacteristic.canRead()) {
    batteryLevelCharacteristic.read();
    readBatteryValue(batteryLevelCharacteristic, peripherals[index].batteryLevel);
  }
  if (batteryLevelCharacteristic.canSubscribe()) {
    batteryLevelCharacteristic.setEventHandler(BLEUpdated, onBatteryUpdated);
    batteryLevelCharacteristic.subscribe();
  }

  peripherals[index].rssi = peripheral.rssi();

  // Once connected start scanning again
  BLE.scan();
}

void BLECentralManager::onPeripheralDisconnected(BLEDevice peripheral) {
  LOG("Disconnected from peripheral: ");
  LOG_LN(peripheral.address());
  int index = getPeripheralIndexByAddress(peripheral.address());
  peripherals[index] = SensirionPeripheral();
  // Never stopped scanning so no need to call BLE.scan() again
}

#endif // BLE_CENTRAL_MANAGER_H