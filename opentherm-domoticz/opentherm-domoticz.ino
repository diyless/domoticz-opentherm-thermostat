/*************************************************************
  This example runs directly on ESP8266 chip.

  Please be sure to select the right ESP8266 module
  in the Tools -> Board -> WeMos D1 Mini

  Adjust settings in Config.h before run
 *************************************************************/

#include <ESP8266WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <OpenTherm.h>
#include "config.h"

const unsigned long statusUpdateInterval_ms = 1000;

float  sp = 15, //set point
       t = 15, //current temperature
       t_last = 0, //prior temperature
       ierr = 25, //integral error
       dt = 0, //time between measurements
       op = 0; //PID controller output
unsigned long ts = 0, new_ts = 0; //timestamp
unsigned long lastUpdate = 0;
unsigned long lastTempSet = 0;

bool heatingEnabled = true;

#define MSG_BUFFER_SIZE  (50)
char msg[MSG_BUFFER_SIZE];

OneWire oneWire(ROOM_TEMP_SENSOR_PIN);
DallasTemperature sensors(&oneWire);
OpenTherm ot(OT_IN_PIN, OT_OUT_PIN);
WiFiClient espClient;
PubSubClient client(espClient);

void ICACHE_RAM_ATTR handleInterrupt() {
  ot.handleInterrupt();
}

float getTemp() {
  return sensors.getTempCByIndex(0);
}

float pid(float sp, float pv, float pv_last, float& ierr, float dt) {
  float KP = 10;
  float KI = 0.02;
  // upper and lower bounds on heater level
  float ophi = 63;
  float oplo = 20;
  // calculate the error
  float error = sp - pv;
  // calculate the integral error
  ierr = ierr + KI * error * dt;
  // calculate the measurement derivative
  //float dpv = (pv - pv_last) / dt;
  // calculate the PID output
  float P = KP * error; //proportional contribution
  float I = ierr; //integral contribution
  float op = P + I;
  // implement anti-reset windup
  if ((op < oplo) || (op > ophi)) {
    I = I - KI * error * dt;
    // clip output
    op = max(oplo, min(ophi, op));
  }
  ierr = I;

  Serial.println("sp=" + String(sp) + " pv=" + String(pv) + " dt=" + String(dt) + " op=" + String(op) + " P=" + String(P) + " I=" + String(I));

  return op;
}

// This function calculates temperature and sends data to MQTT every second.
void updateData()
{
  //Set/Get Boiler Status
  bool enableHotWater = true;
  bool enableCooling = false;
  unsigned long response = ot.setBoilerStatus(heatingEnabled, enableHotWater, enableCooling);
  OpenThermResponseStatus responseStatus = ot.getLastResponseStatus();
  if (responseStatus != OpenThermResponseStatus::SUCCESS) {
    Serial.println("Error: Invalid boiler response " + String(response, HEX));
  }

  t = getTemp();
  new_ts = millis();
  dt = (new_ts - ts) / 1000.0;
  ts = new_ts;
  if (responseStatus == OpenThermResponseStatus::SUCCESS) {
    op = pid(sp, t, t_last, ierr, dt);
    ot.setBoilerTemperature(op);
  }
  t_last = t;
  sensors.requestTemperatures(); //async temperature request

  snprintf (msg, MSG_BUFFER_SIZE, "%s", String(op).c_str());
  //client.publish(TEMP_BOILER_TARGET_GET_TOPIC, msg);

  snprintf (msg, MSG_BUFFER_SIZE, "%s", String(t).c_str());
  //client.publish(CURRENT_TEMP_GET_TOPIC, msg);

  float bt = ot.getBoilerTemperature();
  snprintf (msg, MSG_BUFFER_SIZE, "%s", String(bt).c_str());
  //client.publish(TEMP_BOILER_GET_TOPIC, msg);

  snprintf (msg, MSG_BUFFER_SIZE, "%s", String(sp).c_str());
  //client.publish(TEMP_SETPOINT_GET_TOPIC, msg);

  snprintf (msg, MSG_BUFFER_SIZE, "%s", heatingEnabled ? "heat" : "off");
  //client.publish(MODE_GET_TOPIC, msg);

  Serial.println("Current temperature: " + String(t) + " °C ");
}

String convertPayloadToStr(byte* payload, unsigned int length) {
  char s[length + 1];
  s[length] = 0;
  for (int i = 0; i < length; ++i)
    s[i] = payload[i];
  String tempRequestStr(s);
  return tempRequestStr;
}

DynamicJsonDocument doc(768);

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println("Callback");

  const String topicStr(topic);
  String payloadStr = convertPayloadToStr(payload, length);

  deserializeJson(doc, payloadStr);

  const String id((const char*)doc["id"]);

  Serial.println("Received, id=" + id);

  if (id == "00040004") {
    const String targetTemp((const char*)doc["svalue1"]);
    sp = targetTemp.toFloat();
  }
  else if (id == "00040001") {
    const String targetMode((const char*)doc["svalue1"]);
    Serial.println("Set mode " + targetMode);
    if (targetMode == "0")
      heatingEnabled = false;
    else if (targetMode == "10" || targetMode == "20")
      heatingEnabled = true;
    else
      Serial.println("Unknown mode");
  }

  doc.clear();
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    const char* clientId = "opentherm-thermostat-test";
    if (client.connect(clientId, mqtt_user, mqtt_password)) {
      Serial.println("ok");

      client.subscribe(OUT_TOPIC);
      client.setBufferSize(1024);
    } else {
      Serial.print(" failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup()
{
  Serial.begin(115200);

  Serial.println("Connecting to " + String(ssid));
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  int deadCounter = 20;
  while (WiFi.status() != WL_CONNECTED && deadCounter-- > 0) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Failed to connect to " + String(ssid));
    while (true);
  }
  else {
    Serial.println("ok");
  }

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  ot.begin(handleInterrupt);

  //Init DS18B20 sensor
  sensors.begin();
  sensors.requestTemperatures();
  sensors.setWaitForConversion(false); //switch to async mode
  t, t_last = getTemp();
  ts = millis();
}

void loop()
{
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long now = millis();
  if (now - lastUpdate > statusUpdateInterval_ms) {
    lastUpdate = now;
    updateData();
  }
}
