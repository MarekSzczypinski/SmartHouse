#ifndef SENSORS_INFLUXDB_CLIENT_H
#define SENSORS_INFLUXDB_CLIENT_

#include <Arduino.h>

#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

#include "ExtremelySimpleLogger.h"
#include "secrets.h"

class SensorsInfluxDBClient {
private:
    InfluxDBClient influxDBClient;
    Point sensorPoint = Point("sensor_measurement");

public:
    SensorsInfluxDBClient() : influxDBClient(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert) {}

    void setup() {
        influxDBClient.setWriteOptions(WriteOptions().writePrecision(WritePrecision::S));
    }

    bool connect() {
        if (influxDBClient.validateConnection()) {
            LOG("Connected to InfluxDB: ");
            LOG_LN(influxDBClient.getServerUrl());
            return true;
        }
        LOG("InfluxDB connection failed: ");
        LOG_LN(influxDBClient.getLastErrorMessage());
        return false;
    }

    bool writeSensorData(const String &deviceId, const String &location, float temperature, float humidity, int co2, int battery, int rssi) {
        sensorPoint.clearFields();
        sensorPoint.clearTags();

        // Tags (for grouping/filtering)
        sensorPoint.addTag("deviceId", deviceId);
        sensorPoint.addTag("location", location);

        // Fields (measurements)
        sensorPoint.addField("temperature", temperature);
        sensorPoint.addField("humidity", humidity);
        sensorPoint.addField("co2", co2);
        sensorPoint.addField("battery", battery);
        sensorPoint.addField("rssi", rssi);

        // Timestamp
        sensorPoint.setTime(time(NULL));

        bool success = influxDBClient.writePoint(sensorPoint);
        if (success) {
            LOG_PRINTF("Data written to InfluxDB: %s, %s, %.2f, %.2f, %d, %d, %d\n", deviceId.c_str(), location.c_str(), temperature, humidity, co2, battery, rssi);
        }
        else {
            LOG_PRINTF("InfluxDB write failed: %s\n", influxDBClient.getLastErrorMessage().c_str());
        }
        return success;
    }
};

#endif // SENSORS_INFLUXDB_CLIENT_H
