// Your WiFi credentials.
// Set password to "" for open networks.
const char* ssid = "xxxxxx";
const char* pass = "xxxxxx";

// Your MQTT broker address and credentials
const char* mqtt_server = "xxx.xxx.xxx.xxx";
const char* mqtt_user = "xxxxxx";
const char* mqtt_password = "xxxxxx";
const int mqtt_port = 1883;

// Master OpenTherm Shield pins configuration
const int OT_IN_PIN = 4;  //for Arduino, 4 for ESP8266 (D2), 21 for ESP32
const int OT_OUT_PIN = 5; //for Arduino, 5 for ESP8266 (D1), 22 for ESP32

// Temperature sensor pin
const int ROOM_TEMP_SENSOR_PIN = 14; //for Arduino, 14 for ESP8266 (D5), 18 for ESP32

const char* OUT_TOPIC = "domoticz/out";
const char* IN_TOPIC = "domoticz/in";

const String SETPOINT_NORMAL_ID = "xxxxxx";
const String CONTROL_ID = "xxxxxx";
