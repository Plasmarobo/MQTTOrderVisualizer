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
const String domain = "local";

//MQTT SETUP (set these)
#define MQTT_CLIENT_NAME ""
#define MQTT_HOSTNAME ""
#define MQTT_IP ""
#define MQTT_PORT 1883 // use 8883 for SSL
const char* MQTT_ORDERS = "levelup/visualization/order";

//WATCHDOG SETUP
//SimpleTimer watchdog;
#define MAX_ERRORS 3
#define DELAY 3000

void check_watchdog(uint8_t &errors) {
  if (errors > MAX_ERRORS) {
    Serial.println("!!");
    Serial.println("Watchdog: Too many Errors, rebooting...");
    pinMode(0, INPUT_PULLUP);
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
#define NLEDS 160
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
  //&AtomicLime,
  //&TangoRed,
  &GalacticBlue,
  //&PurpleHaze,
  //&YellowSubmarine,
  //&ArcticTeal,
  //&SickFlamingo,
  //&MintJulep,
  &NinjaOrange, 
};

bool led_enable;
std::vector<const CRGB*> lights;

#define MAX_LIGHT_ADVANCE_PERIOD 5000
#define LIGHT_ADVANCE_DAMPING 75

#define BRIGHTNESS 24
uint32_t pixel_period = MAX_LIGHT_ADVANCE_PERIOD; 
uint32_t timer = 0; //milliseconds at last pixel
uint32_t color_index = 0;
  
void shift_pixel(bool blank) {
  if(!blank) {
    color_index += 1;
    if (color_index >= (sizeof(colors)/sizeof(const CRGB*))) {
      color_index = 1;
    }
    lights.insert(lights.begin(), colors[color_index]);
    if (pixel_period > (millis() - timer))
      pixel_period = millis() - timer;
  } else {
      if (pixel_period < MAX_LIGHT_ADVANCE_PERIOD)
        pixel_period += LIGHT_ADVANCE_DAMPING;
      lights.insert(lights.begin(), colors[0]);
  }
  timer = millis();
}

void update_leds() {
  const CRGB *next_color;
  const CRGB *this_color;
  
  if (pixel_period == 0) pixel_period = 1;
  uint16_t fade = ((millis() - timer) << 8) / pixel_period;
  if (fade > 255) {
    shift_pixel(true);
  }
  if (lights.size() < 2) return;
  if (lights.size() > NLEDS+1)
    lights.erase(lights.begin()+NLEDS+1, lights.end());
  
  Serial.printf("Virtual pixel shift %d\n", fade);
  next_color = lights[0];
  for(uint32_t i = 1; i < lights.size(); ++i) {
    //Perform a flow interpolation
    this_color = next_color;
    next_color = lights[i];
    leds[i] = blend(*this_color,*next_color, 255-fade);
  }
  FastLED.show();
}

//MQTT
PubSubClient client(wifi);

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
  if (is_topic(topic, MQTT_ORDERS)) {
    shift_pixel(false);
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
  client.setServer(MQTT_IP, MQTT_PORT);
  client.setCallback(mqtt_callback);
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
  
  LEDS.addLeds<LPD8806, MOSI, SCL, BRG, DATA_RATE_MHZ(2)>(leds, NLEDS).setCorrection(TypicalLEDStrip);
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
  update_leds();
}

