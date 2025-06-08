#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <MPU6050_light.h>
#include <math.h>

#define ONE_WIRE_BUS 5  // GPIO5 para DS18B20

// Conexion sensor MPU6050
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature tempSensor(&oneWire);
MPU6050 mpu(Wire);

// Conexion Wifi y MQTT
const char* ssid = "iPhone";
const char* password = "123456789";
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* mqtt_topic = "grupo3/sensores";

WiFiClient espClient;
PubSubClient client(espClient);

// Temporizadores independientes 
unsigned long lastTempMillis = 0;
unsigned long lastMpuMillis = 0;
const long tempInterval = 1000;  // 1 segundo
const long mpuInterval = 100;    // 100 ms

void setup_wifi() {
  delay(100);
  WiFi.begin(ssid, password);
  Serial.print("Conectando al wifi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConexion a wifi establecida");
  Serial.print("Direccion IP: ");
  Serial.println(WiFi.localIP());
}

void reconnect_mqtt() {
  while (!client.connected()) {
    Serial.println("Conectando a MQTT...");
    if (client.connect("ESP32Cliente")) {
      Serial.println("Conectado al broker MQTT");
    } else {
      Serial.print("Error, reintentando en 1s. Codigo: ");
      Serial.println(client.state());
      delay(1000);
    }
  }
}

// Funciones para ruido y SNR 
float calcular_ruido(float valores[], int n) {
  float media = 0.0, var = 0.0;
  for (int i = 0; i < n; i++) media += valores[i];
  media /= n;
  for (int i = 0; i < n; i++) var += pow(valores[i] - media, 2);
  return sqrt(var / n);
}

float calcular_snr(float media, float ruido) {
  if (ruido == 0) return 1000.0;
  return 20.0 * log10(fabs(media) / ruido);
}

void setup() {
  Serial.begin(9600);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  tempSensor.begin();
  Wire.begin(18, 22);  // SCL 22, SDA 18
  mpu.begin();
  mpu.calcOffsets(true, true);
}

void loop() {
  if (!client.connected()) reconnect_mqtt();
  client.loop();

  unsigned long currentMillis = millis();

  // Lectura y envío de temperatura (cada 1000 ms) 
  if (currentMillis - lastTempMillis >= tempInterval) {
    lastTempMillis = currentMillis;

    tempSensor.requestTemperatures();
    float temperatura = tempSensor.getTempCByIndex(0);

    float temp_data[10];
    for (int i = 0; i < 10; i++) {
      tempSensor.requestTemperatures();
      temp_data[i] = tempSensor.getTempCByIndex(0);
      delay(5);
    }
    float ruido_temp = calcular_ruido(temp_data, 10);
    float media_temp = 0;
    for (int i = 0; i < 10; i++) media_temp += temp_data[i];
    media_temp /= 10.0;
    float snr_temp = calcular_snr(media_temp, ruido_temp);

    StaticJsonDocument<256> doc;
    doc["timestamp"] = millis();

    JsonObject sensor2 = doc.createNestedObject("sensor2");
    sensor2["temperatura"] = temperatura;
    sensor2["ruido"] = ruido_temp;
    sensor2["snr"] = snr_temp;

    char buffer[256];
    serializeJson(doc, buffer);
    client.publish(mqtt_topic, buffer);

    Serial.println("Temp enviada:");
    serializeJsonPretty(doc, Serial);
    Serial.println();
  }

  //Lectura y envío de aceleración (cada 100 ms)
  if (currentMillis - lastMpuMillis >= mpuInterval) {
    lastMpuMillis = currentMillis;

    mpu.update();
    float ax = mpu.getAccX();
    float ay = mpu.getAccY();
    float az = mpu.getAccZ();

    float acc_z_data[10];
    for (int i = 0; i < 10; i++) {
      acc_z_data[i] = mpu.getAccZ();
      delay(2);
    }
    float ruido_acc = calcular_ruido(acc_z_data, 10);
    float media_acc = 0;
    for (int i = 0; i < 10; i++) media_acc += acc_z_data[i];
    media_acc /= 10.0;
    float snr_acc = calcular_snr(media_acc, ruido_acc);

    StaticJsonDocument<256> doc;
    doc["timestamp"] = millis();

    JsonObject sensor1 = doc.createNestedObject("sensor1");
    sensor1["Ax"] = ax;
    sensor1["Ay"] = ay;
    sensor1["Az"] = az;
    sensor1["ruido"] = ruido_acc;
    sensor1["snr"] = snr_acc;

    char buffer[256];
    serializeJson(doc, buffer);
    client.publish(mqtt_topic, buffer);

    Serial.println("MPU enviada:");
    serializeJsonPretty(doc, Serial);
    Serial.println();
  }
}