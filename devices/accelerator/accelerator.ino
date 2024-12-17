#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "LIS3DHTR.h"
#include <Wire.h>
LIS3DHTR<TwoWire> LIS; // IIC
#define WIRE Wire
#include "Adafruit_VL53L0X.h"

#ifdef ARDUINO_SAMD_VARIANT_COMPLIANCE
#define SERIAL SerialUSB
#else
#define SERIAL Serial
#endif

// AWS credentials
extern const char aws_root_ca_pem[];
extern const char certificate_pem_crt[];
extern const char private_pem_key[];

// WiFi credentials
const char *ssid = "your_ssid";
const char *password = "your_pwd";

// AWS Greengrass configuration
const char *thingName = "your_thing_name";
char topic[50];
const char *discoveryEndpoint = "your_discovery_endpoint";

// Global objects
WiFiClientSecure wifiClient;
PubSubClient mqttClient(wifiClient);

// Discovery response variables
String ggCoreHost;
int ggCorePort;

Adafruit_VL53L0X lox = Adafruit_VL53L0X();

// Function to perform discovery
bool performDiscovery()
{
    HTTPClient http;
    String url = String("https://") + discoveryEndpoint + ":8443/greengrass/discover/thing/" + thingName;

    wifiClient.setCACert(aws_root_ca_pem);
    wifiClient.setCertificate(certificate_pem_crt);
    wifiClient.setPrivateKey(private_pem_key);

    http.begin(wifiClient, url);

    int httpCode = http.GET();
    if (httpCode == 200)
    {
        String payload = http.getString();
        DynamicJsonDocument doc(4096);
        deserializeJson(doc, payload);

        Serial.println("Received JSON payload:");
        String jsonString;
        serializeJson(doc, jsonString);
        Serial.println(jsonString);

        // Extract core device details
        JsonArray connectivity = doc["GGGroups"][0]["Cores"][0]["Connectivity"];
        ggCoreHost = connectivity[0]["HostAddress"].as<String>();
        ggCorePort = connectivity[0]["PortNumber"].as<int>();

        // Set greengrass certificate
        String caCert = doc["GGGroups"][0]["CAs"][0];
        wifiClient.setCACert(caCert.c_str());

        Serial.printf("Discovered core: %s:%d\n", ggCoreHost.c_str(), ggCorePort);
        return true;
    }
    else
    {
        Serial.printf("Discovery failed, HTTP code: %d\n", httpCode);
        return false;
    }
}

// Function to connect to MQTT
void connectToMQTT()
{
    mqttClient.setServer(ggCoreHost.c_str(), ggCorePort);

    while (!mqttClient.connected())
    {
        Serial.println("Connecting to MQTT...");
        if (mqttClient.connect(thingName))
        {
            Serial.println("Connected to MQTT broker!");
            mqttClient.subscribe("response/topic"); // Example subscription
        }
        else
        {
            Serial.print("Failed to connect, retrying in 5 seconds. State: ");
            Serial.println(mqttClient.state());
            delay(5000);
        }
    }
}

// Function to connect to Wifi
void connectToWifi()
{
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi");

    // Perform discovery
    while (!performDiscovery())
    {
        Serial.println("Failed greengrass discovery, retrying in 5 seconds.");
        delay(5000);
    }
    connectToMQTT();
}

void setup()
{
    VL53L0X_Error Status = VL53L0X_ERROR_NONE;
    Serial.begin(115200);

    while (!Serial)
    {
        delay(1);
    };

    sprintf(topic, "devices/%s/data", thingName);

    // Distance setup
    if (!lox.begin())
    {
        Serial.println(F("Failed to boot VL53L0X"));
        while (1)
            ;
    }

    // Accelerator setup
    LIS.begin(WIRE, 0x19); // IIC init
    LIS.openTemp();        // If ADC3 is used, the temperature detection needs to be turned off.
    delay(100);
    LIS.setFullScaleRange(LIS3DHTR_RANGE_2G);
    LIS.setOutputDataRate(LIS3DHTR_DATARATE_50HZ);

    // Connect to WiFi
    connectToWifi();
}

void loop()
{
    if (!LIS)
    {
        Serial.println("LIS3DHTR didn't connect.");
        while (1)
            ;
        return;
    }

    // Ensure Wifi connextion stays alive
    if (WiFi.status() != WL_CONNECTED)
    {
        connectToWifi();
    }
    // Ensure MQTT connection stays alive
    if (!mqttClient.connected())
    {
        connectToMQTT();
    }
    mqttClient.loop();

    // Create a JSON document
    StaticJsonDocument<256> jsonDoc;

    // Get accelerator data
    jsonDoc["x"] = LIS.getAccelerationX();
    jsonDoc["y"] = LIS.getAccelerationY();
    jsonDoc["z"] = LIS.getAccelerationZ();
    jsonDoc["temperature"] = LIS.getTemperature();

    // Get distance data
    VL53L0X_RangingMeasurementData_t measure;
    lox.rangingTest(&measure, false);

    if (measure.RangeStatus != 4)
    { // phase failures have incorrect data
        jsonDoc["distance"] = measure.RangeMilliMeter;
    }

    // Serialize JSON to a string
    char payload[256];
    serializeJson(jsonDoc, payload);

    // Publish to MQTT
    mqttClient.publish(topic, payload);
    Serial.print("Published: ");
    Serial.println(payload);

    delay(500);
}
