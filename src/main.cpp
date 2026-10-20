#include <Arduino.h>
#include <DHTesp.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <WiFi.h>

// Reemplaza los valores de ejemplo antes de cargar el programa.
const char* WIFI_SSID = "Ambalema";
const char* WIFI_PASSWORD = "Juanca11+";

// Usa la IPv4 local del computador donde se ejecuta Mosquitto.
const char* MQTT_HOST = "192.168.18.242";
const uint16_t MQTT_PORT = 1883;
const char* MQTT_USERNAME = "mqtt_user";
const char* MQTT_PASSWORD = "123";

// Usa la IPv4 local del computador donde se ejecuta XAMPP.
const char* API_URL =
    "http://192.168.18.242/red-mesh-climatica/api/recibir_datos.php";
const char* NODE_IDENTIFIER = "NODO-001";

constexpr uint8_t DHT_PIN = 4;
constexpr unsigned long SEND_INTERVAL_MS = 30000;
constexpr unsigned long WIFI_RETRY_INTERVAL_MS = 10000;
constexpr unsigned long MQTT_RETRY_INTERVAL_MS = 5000;

DHTesp dht;
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

unsigned long lastSend = 0;
unsigned long lastWiFiAttempt = 0;
unsigned long lastMqttAttempt = 0;
bool firstReading = true;

String mqttTopic() {
    return String("red-mesh/nodos/") + NODE_IDENTIFIER + "/lecturas";
}

void connectWiFi() {
    if (WiFi.status() == WL_CONNECTED) {
        return;
    }

    const unsigned long now = millis();
    if (lastWiFiAttempt != 0 &&
        now - lastWiFiAttempt < WIFI_RETRY_INTERVAL_MS) {
        return;
    }

    lastWiFiAttempt = now;
    Serial.printf("Conectando a WiFi: %s\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void connectMqtt() {
    if (WiFi.status() != WL_CONNECTED || mqttClient.connected()) {
        return;
    }

    const unsigned long now = millis();
    if (lastMqttAttempt != 0 &&
        now - lastMqttAttempt < MQTT_RETRY_INTERVAL_MS) {
        return;
    }

    lastMqttAttempt = now;
    const String clientId = String("esp32-") + NODE_IDENTIFIER;
    Serial.printf("Conectando a MQTT en %s:%u...\n", MQTT_HOST, MQTT_PORT);

    bool connected;
    if (MQTT_USERNAME[0] != '\0') {
        connected = mqttClient.connect(
            clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD);
    } else {
        connected = mqttClient.connect(clientId.c_str());
    }

    if (connected) {
        Serial.println("Conectado al broker MQTT");
    } else {
        Serial.printf("Error MQTT, estado=%d\n", mqttClient.state());
    }
}

String makePayload(float temperature, float humidity) {
    String json;
    json.reserve(100);
    json = "{\"identificador\":\"";
    json += NODE_IDENTIFIER;
    json += "\",\"temperatura\":";
    json += String(temperature, 2);
    json += ",\"humedad\":";
    json += String(humidity, 2);
    json += ",\"bateria\":100}";
    return json;
}

bool sendHttpFallback(const String& json) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("HTTP alternativo omitido: WiFi desconectado");
        return false;
    }

    WiFiClient client;
    HTTPClient http;
    http.setConnectTimeout(10000);
    http.setTimeout(15000);

    if (!http.begin(client, API_URL)) {
        Serial.println("No se pudo iniciar la conexion HTTP");
        return false;
    }

    http.addHeader("Content-Type", "application/json");
    const int responseCode = http.POST(json);
    Serial.printf("Codigo HTTP: %d\n", responseCode);

    if (responseCode > 0) {
        Serial.println(http.getString());
    } else {
        Serial.printf("Error HTTP: %s\n",
                      http.errorToString(responseCode).c_str());
    }

    http.end();
    return responseCode >= 200 && responseCode < 300;
}

void sendData(float temperature, float humidity) {
    const String json = makePayload(temperature, humidity);
    Serial.printf("Lectura: %s\n", json.c_str());

    if (mqttClient.connected() &&
        mqttClient.publish(mqttTopic().c_str(), json.c_str())) {
        Serial.println("Lectura publicada por MQTT");
        return;
    }

    Serial.println("MQTT no disponible; intentando envio HTTP");
    if (!sendHttpFallback(json)) {
        Serial.println("No se pudo enviar la lectura por MQTT ni HTTP");
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    dht.setup(DHT_PIN, DHTesp::DHT11);
    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    mqttClient.setBufferSize(256);

    connectWiFi();
}

void loop() {
    connectWiFi();

    if (WiFi.status() == WL_CONNECTED) {
        static bool reportedWiFi = false;
        if (!reportedWiFi) {
            Serial.println("WiFi conectado");
            Serial.print("IP del ESP32: ");
            Serial.println(WiFi.localIP());
            reportedWiFi = true;
        }
        connectMqtt();
    }

    if (mqttClient.connected()) {
        mqttClient.loop();
    }

    const unsigned long now = millis();
    if (!firstReading && now - lastSend < SEND_INTERVAL_MS) {
        delay(50);
        return;
    }

    firstReading = false;
    lastSend = now;

    const TempAndHumidity data = dht.getTempAndHumidity();
    if (isnan(data.temperature) || isnan(data.humidity)) {
        Serial.println("Error leyendo el sensor DHT11");
        return;
    }

    Serial.printf("Temperatura: %.2f C\n", data.temperature);
    Serial.printf("Humedad: %.2f %%\n", data.humidity);
    sendData(data.temperature, data.humidity);
}