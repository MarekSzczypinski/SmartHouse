#ifndef AWS_IOT_CLIENT_H
#define AWS_IOT_CLIENT_H

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "ExtremelySimpleLogger.h"
#include "secrets.h"

// AWS IoT Core Configuration
class AWSIoTClient {
private:
    WiFiClientSecure wifiClient;
    PubSubClient mqttClient;
    
    // AWS IoT Core endpoint (replace with your endpoint)
    // const char* awsIoTEndpoint = "xxxxxxx-ats.iot.us-east-1.amazonaws.com";
    const int awsIoTPort = 8883;
    
    // AWS IoT Core topic
    const char* publishTopic = "smarthouse/sensors";
    
    // AWS IoT Core certificates
    const char* rootCA;
    const char* deviceCert;
    const char* privateKey;
    
    // Device ID
    String deviceId;

public:
    AWSIoTClient() : mqttClient(wifiClient) {
        // Generate a unique device ID using ESP32's MAC address
        deviceId = "smarthouse-" + WiFi.macAddress();
        deviceId.replace(":", "");
    }
    
    void setup(const char* rootCAStr, const char* deviceCertStr, const char* privateKeyStr) {
        rootCA = rootCAStr;
        deviceCert = deviceCertStr;
        privateKey = privateKeyStr;
        
        // Configure secure WiFi client
        wifiClient.setCACert(rootCA);
        wifiClient.setCertificate(deviceCert);
        wifiClient.setPrivateKey(privateKey);
        
        // Configure MQTT client
        mqttClient.setServer(awsIoTEndpoint, awsIoTPort);
        mqttClient.setKeepAlive(60);
        mqttClient.setBufferSize(512);
    }
    
    bool connect() {
        LOG("Connecting to AWS IoT Core... ");
        bool connected = mqttClient.connect(deviceId.c_str());
        if (connected) {
            LOG_LN("Connected!");
        } else {
            LOG_PRINTF("Failed, rc=%d\n", mqttClient.state());
        }
        return connected;
    }
    
    bool publish(const char* payload) {
        if (!mqttClient.connected() && !connect()) {
            return false;
        }
        
        bool published = mqttClient.publish(publishTopic, payload);
        if (published) {
            LOG_PRINTF("Published to %s: %s\n", publishTopic, payload);
        } else {
            LOG_LN("Failed to publish message");
        }
        return published;
    }
    
    void loop() {
        if (!mqttClient.connected()) {
            connect();
        }
        mqttClient.loop();
    }
};

#endif // AWS_IOT_CLIENT_H