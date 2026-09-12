#include <Arduino.h>
#include <DHTesp.h>
#include <HTTPClient.h>
#include <WiFi.h>

// Datos de tu red WiFi
const char* WIFI_SSID = "Filia_parra";
const char* WIFI_PASSWORD = "Cristina197179";

// Cambia esta IP por la IP de tu computador
const char* API_URL =
    "http://192.168.1.6/red-mesh-climatica/api/recibir_datos.php";

// Debe existir en la tabla nodos de la base de datos
const char* NODE_IDENTIFIER = "NODO-001";

// D4 en ESP32 normalmente corresponde a GPIO 4
const uint8_t DHT_PIN = 4;

const unsigned long SEND_INTERVAL = 30000;

DHTesp dht;
unsigned long lastSend = 0;

void connectWiFi() {
    Serial.print("Conectando a WiFi");

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println();
    Serial.println("WiFi conectado");
    Serial.print("IP del ESP32: ");
    Serial.println(WiFi.localIP());
}

void sendData(float temperature, float humidity) {
    if (WiFi.status() != WL_CONNECTED) {
        connectWiFi();
    }

    WiFiClient client;
    HTTPClient http;

    http.setConnectTimeout(5000);
    http.setTimeout(5000);

    if (!http.begin(client, API_URL)) {
        Serial.println("No se pudo iniciar la conexion HTTP");
        return;
    }

    http.addHeader("Content-Type", "application/json");

    String json = "{";
    json += "\"identificador\":\"";
    json += NODE_IDENTIFIER;
    json += "\",";
    json += "\"temperatura\":";
    json += String(temperature, 2);
    json += ",";
    json += "\"humedad\":";
    json += String(humidity, 2);
    json += ",";
    json += "\"bateria\":100";
    json += "}";

    Serial.println("Enviando:");
    Serial.println(json);

    int responseCode = http.POST(json);

    Serial.print("Codigo HTTP: ");
    Serial.println(responseCode);

    if (responseCode > 0) {
        Serial.print("Respuesta: ");
        Serial.println(http.getString());
    } else {
        Serial.print("Error de conexion: ");
        Serial.println(http.errorToString(responseCode));
    }

    http.end();
}


void setup() {
    Serial.begin(115200);
    delay(1000);

    dht.setup(DHT_PIN, DHTesp::DHT11);
    connectWiFi();
}

void loop() {
    if (millis() - lastSend < SEND_INTERVAL) {
        delay(100);
        return;
    }

    lastSend = millis();

    TempAndHumidity data = dht.getTempAndHumidity();

    if (isnan(data.temperature) || isnan(data.humidity)) {
        Serial.println("Error leyendo el sensor DHT11");
        return;
    }

    Serial.print("Temperatura: ");
    Serial.print(data.temperature);
    Serial.println(" C");

    Serial.print("Humedad: ");
    Serial.print(data.humidity);
    Serial.println(" %");

    sendData(data.temperature, data.humidity);
}