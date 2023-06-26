#include "constants.h"

WiFiClientSecure wifiClient;
PubSubClient mqttClient(wifiClient);

StaticJsonDocument<JSON_OBJECT_SIZE(4)> outputDoc;
StaticJsonDocument<JSON_OBJECT_SIZE(64)> inputDoc;

BH1750 lightMeter;
char outputBuffer[128];

void getShadow() {
  mqttClient.publish(GET_TOPIC, "");
}

void reportLuxState() {
  float lux = lightMeter.readLightLevel();
  outputDoc["state"]["reported"]["lux"] = lux;
  outputDoc["state"]["reported"]["id"] = MQTT_CLIENT_ID;
  serializeJson(outputDoc, outputBuffer);
  mqttClient.publish(GET_UPDATED, outputBuffer);
}

void setLedState(){
  float lux = lightMeter.readLightLevel();
  analogWrite(LED_PIN, lightEmitted(lux));
  reportLuxState();
}

void callback(const char* topic, byte* payload, unsigned int length) {
  String message = getMqttMessage(payload, length);
  Serial.println("Message from topic " + String(topic) + ": " + message);
  if (!deserializeJson(inputDoc, payload)){
    const char* luxStr = inputDoc["lux"];
    if (luxStr != nullptr && strlen(luxStr) > 0) {
      int lux = atoi(luxStr);        
      setLedState();
    }
  }
}

void subscribeToMqtt(const char* topic){
      if (mqttClient.subscribe(topic)) {
      Serial.println("Subscribed to " + String(topic));
    } else {
      Serial.println("Can't subscribe to " + String(topic));
    }
}

boolean mqttClientConnect() {
  Serial.println("Connecting to " + String(MQTT_BROKER));
  if (mqttClient.connect(MQTT_CLIENT_ID)) {
    Serial.println("Connected to " + String(MQTT_BROKER));
    subscribeToMqtt(GET_ACCEPTED_TOPIC);
    Serial.println("Getting shadow");
    getShadow();
  } else {
    Serial.println("Can't connect to " + String(MQTT_BROKER));
  }
  return mqttClient.connected();
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  
  Serial.begin(115200);
  Wire.begin(26, 25);
  lightMeter.begin();

  WiFiManager wm;
  //wm.resetSettings();
  bool res;
  res = wm.autoConnect("farol_esp32","12345678");
  if(!res) {
      Serial.println("Coneccion Fallida");
      ESP.restart();
  } 
  else { 
      Serial.println("Conectado a WiFi!!");
  }

  wifiClient.setCACert(AMAZON_ROOT_CA1);
  wifiClient.setCertificate(CERTIFICATE);
  wifiClient.setPrivateKey(PRIVATE_KEY);

  mqttClient.setBufferSize(4096);
  mqttClient.setServer(MQTT_BROKER, MQTT_BROKER_PORT);
  mqttClient.setCallback(callback);
}

unsigned long previousConnectMillis = 0;

void loop() {
  unsigned long now = millis();
  if (!mqttClient.connected()) {
    if (now - previousConnectMillis >= 2000) {
      previousConnectMillis = now;
      if (mqttClientConnect()) {
        previousConnectMillis = 0;
      } else {
        delay(1000);
      }
    }
  } else {
    setLedState();
    mqttClient.loop();
    delay(5000);
  }
}