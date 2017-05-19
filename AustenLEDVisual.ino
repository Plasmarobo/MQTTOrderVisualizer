#include <FastLED.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <SPI.h>
#include <PubSubClient.h>
#include <vector>
#define UNUSED(x) (void)x

//Secrets
#define OTA_PASSWORD ""
#define OTA_HOSTNAME ""
const String ssid = "";//SET THIS
const String password = "";//SET THIS

//MQTT SETUP (set these)
#define MQTT_CLIENT_NAME ""
#define MQTT_SERVER      ""
#define MQTT_SERVERPORT  1883                   // use 8883 for SSL
const char* MQTT_ORDERS = "";

//WATCHDOG SETUP
//SimpleTimer watchdog;
#define MAX_ERRORS 3
#define DELAY 3000

void check_watchdog(uint8_t &errors) {
  if (errors > MAX_ERRORS) {
    Serial.println("!!");
    Serial.println("Watchdog: Too many Errors, rebooting...");
    ESP.restart();
  }
}

void pet_watchdog() {
  uint8_t error_count = 0;
  while (!wifi_connect()) {
    ++error_count;
    check_watchdog(error_count);
    Serial.println("WiFi failure");
    delay(DELAY);
  }
  while (!mqtt_connect()){
    ++error_count;
    check_watchdog(error_count);
    Serial.println("MQTT failure");
    delay(DELAY);
  }
}

//WIFI SETUP
WiFiClient wifi;

bool wifi_connect() {
  if (wifi.connected()) {
    return true;
  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  if (WiFi.waitForConnectResult() == WL_CONNECTED) {
    return true;
  }
  return false;
}

//LED STRIP SETUP
#define NLEDS 148
#define SCL 14
#define MOSI 13

CRGB leds[NLEDS];

const CRGB MidnightBlack   = { 0, 0, 0 };
const CRGB SoothingGreen   = { 0, 255, 0 };
const CRGB AtomicLime      = { 134, 255, 0 };
const CRGB TangoRed        = { 255, 0, 0 };
const CRGB GalacticBlue    = { 0, 0, 255 };
const CRGB PurpleHaze      = { 124, 0, 153 };
const CRGB YellowSubmarine = { 255, 255, 0 };
const CRGB ArcticTeal      = { 117, 255, 176 };
const CRGB SickFlamingo    = { 255, 10, 128 };
const CRGB MintJulep       = { 45, 255, 27 };
const CRGB NinjaOrange     = { 255, 32, 0 };

const CRGB* colors[] = { &MidnightBlack,
  &SoothingGreen,
  &AtomicLime,
  &TangoRed,
  &GalacticBlue,
  &PurpleHaze,
  &YellowSubmarine,
  &ArcticTeal,
  &SickFlamingo,
  &MintJulep,
  &NinjaOrange, };

bool led_enable;
std::vector<const CRGB*> lights;
#define LIGHT_ADVANCE_RATE 250
#define BRIGHTNESS 32

void update_leds() {
  if (lights.size() > NLEDS)
    lights.erase(lights.begin()+NLEDS, lights.end());
  
  for(uint32_t i = 0; i < lights.size(); ++i) {
    leds[i] = *(lights[i]);
  }
  FastLED.show();
}

uint32_t timer = 0;

void advance_visual(bool blank) {
  uint32_t index;
  if (blank) {
    index = 0;
  } else {
    index = 1 + rand() % ((sizeof(colors)/sizeof(CRGB*))-1);
  }
  lights.insert(lights.begin(), colors[index]);
}

//MQTT
PubSubClient client(MQTT_SERVER, MQTT_SERVERPORT, mqtt_callback, wifi);

bool mqtt_connect() {
  if (client.connected())
  {
    return true;
  }
  if (client.connect(MQTT_CLIENT_NAME)){
    client.subscribe(MQTT_ORDERS);
    return true;
  }
  return false;
}

bool is_topic(const char* t, const char *tt){
  return strcmp(t, tt) == 0; 
}

void mqtt_callback(char *topic, uint8_t* payload, uint32_t len) {
  UNUSED(payload);
  UNUSED(len);
  Serial.print("MQTT: ");
  Serial.println(topic);
  if (is_topic(topic, MQTT_ORDERS)) {
    advance_visual(false);
    timer = millis();
  }
}

//OTA setup
void setup_ota() {
  // Port defaults to 8266
  ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(OTA_HOSTNAME);

  // No authentication by default
  ArduinoOTA.setPassword((const char *)OTA_PASSWORD);

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
}
  
//==ENTRY==
void connect_all() {
  uint8_t error_count = 0;
  Serial.print("Connecting Wifi...");
  while (!wifi_connect()) {
    ++error_count;
    check_watchdog(error_count);
    Serial.print("X");
    delay(DELAY);
  }
  Serial.println("Ok");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Connecting MQTT...");
  while (!mqtt_connect()){
    ++error_count;
    check_watchdog(error_count);
    Serial.print("X");
    delay(DELAY);
  }
  Serial.println("Ok");
}

void setup() {
  Serial.begin(115200);
  Serial.println("Boot");
  
  LEDS.addLeds<LPD8806, MOSI, SCL, GRB, DATA_RATE_MHZ(2)>(leds, NLEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  connect_all();
  
  setup_ota();
  timer = millis();
  Serial.println("System Up");
}

void loop() {
  ArduinoOTA.handle();
  pet_watchdog();
  client.loop();
  if ((millis() - timer) > LIGHT_ADVANCE_RATE) {
    advance_visual(true);
    timer = millis();
  }
  update_leds();
}

