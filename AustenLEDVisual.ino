#include <FastLED.h>
#include <ESP8266WiFi.h>
#include <mDNSResolver.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <SPI.h>
#include <PubSubClient.h>
#include <vector>
#define UNUSED(x) (void)x

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
const char* MQTT_ORDERS = "";

bool mqtt_led = false;

//WIFI SETUP
WiFiClient wifi;
WiFiUDP udp;
IPAddress mqtt_ip;

bool wifi_connect() {
  delay(10);
  //WiFi.mode(WIFI_STA) ;
  Serial.printf("Connecting to %s", ssid.c_str());
  WiFi.begin(ssid.c_str(), password.c_str());
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print(WiFi.localIP());
  Serial.print(" (");
  Serial.print(WiFi.subnetMask());
  Serial.println(")");
  return wifi.connected();
}

void print_wifi_status(int s) {
  switch(s) {
    case WL_CONNECTED:
      Serial.println("CONNECTED");
      break;
    case WL_NO_SHIELD:
      Serial.println("NO SHEILD");
      break;
    case WL_IDLE_STATUS:
      Serial.println("IDLE STATUS");
      break;
    case WL_NO_SSID_AVAIL:
      Serial.println("NO SSID AVAIL");
      break;
    case WL_SCAN_COMPLETED:
      Serial.println("SCAN COMPLETED");
      break;
    case WL_CONNECT_FAILED:
      Serial.println("CONNECT FAILED");
      break;
    case WL_CONNECTION_LOST:
      Serial.println("CONNECTION LOST");
      break;
    case WL_DISCONNECTED:
      Serial.println("DISCONNECTED");
      break;
    default:
      Serial.println("Unknown Status");
      break;
  }
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
#define LIGHT_ADVANCE_DAMPING 5

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
  const CRGB *old_color;
  const CRGB *new_color;
  
  if (pixel_period == 0) pixel_period = 1;
  uint16_t fade = ((millis() - timer) << 8) / pixel_period;
  if (fade > 255) {
    shift_pixel(true);
  }
  if (lights.size() < 1) return;
  leds[0] = blend(CRGB(0), *lights[0], fade);
  if (lights.size() < 2) return;
  if (lights.size() > NLEDS+1)
    lights.erase(lights.begin()+NLEDS+1, lights.end());
  
  new_color = lights[0];
  
  for(uint32_t i = 1; i < lights.size(); ++i) {
    //Perform a flow interpolation
    old_color = lights[i];
    leds[i] = blend(*old_color,*new_color, fade);
    new_color = old_color;
  }
  FastLED.show();
}

//MQTT
PubSubClient client(wifi);
String clientId;
void mqtt_connect() {
  while (!client.connected()) {
    Serial.printf("Connecting as %s\n", clientId.c_str());
    if (client.connect(clientId.c_str())){
      Serial.println("MQTT Connected");
      if (client.subscribe(MQTT_ORDERS,1)) {
        Serial.printf("Subscribed to %s\n", MQTT_ORDERS);
      } else {
        Serial.printf("Subscription to %s failed\n", MQTT_ORDERS);
      }
    } else {
      Serial.print("Failed rc:");
      Serial.println(client.state());
      Serial.print("Network: ");
      print_wifi_status(WiFi.status());
      delay(5000);
    }
  }
}

bool is_topic(const char* t, const char *tt){
  return strcmp(t, tt) == 0; 
}

void mqtt_callback(char *topic, uint8_t* payload, uint32_t len) {
  UNUSED(payload);
  UNUSED(len);
  if (is_topic(topic, MQTT_ORDERS)) {
    digitalWrite(0, mqtt_led ? HIGH : LOW);
    mqtt_led = !mqtt_led;
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
void setup_connections() {
  uint8_t error_count = 0;
  wifi_connect();
  Serial.print("Connecting MQTT...");
  while(udp.parsePacket() > 0);
  int res = WiFi.hostByName(MQTT_HOSTNAME, mqtt_ip);
  if (res != 1) {
    mDNSResolver::Resolver resolver(udp);
    resolver.setLocalIP(WiFi.localIP());
    mqtt_ip = resolver.search(MQTT_HOSTNAME);
    if(mqtt_ip == INADDR_NONE) {
      //HARDCODED IP
      Serial.print("Failing back to hardcoded IP");
      mqtt_ip.fromString(MQTT_IP);
    } 
  }
  Serial.println(mqtt_ip);
  clientId = MQTT_CLIENT_NAME;
  clientId += String(random(0xffff), HEX);
  client.setServer(mqtt_ip, MQTT_PORT);
  client.setCallback(mqtt_callback);
  udp.stop();
}

void setup() {
  Serial.begin(115200);
  Serial.println("Boot");
  
  pinMode(0, OUTPUT);
  digitalWrite(0, LOW);
  LEDS.addLeds<LPD8806, MOSI, SCL, BRG, DATA_RATE_MHZ(2)>(leds, NLEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  setup_connections();
  
  setup_ota();
  timer = millis();
  Serial.println("System Up");
}

void loop() {
  ArduinoOTA.handle();
  mqtt_connect();
  client.loop();
  update_leds();
}

