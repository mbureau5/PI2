/*
 * Project PFC - ESILVA4 PI² - 2020/2021 - Hear & Know
 * Author : Loan Plichon, Antoine Grandin, Chloé Martin-Pauliac, Clément Martin, Maxence Bureau
 */

// ################################
// ########    Library     ########
// ################################

// Include by default
#include <Wire.h>
#include <WiFi.h>  
#include <DNSServer.h>
#include <WebServer.h>

// To download in libray manager
#include <M5Stack.h>
#include "DHT.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_NeoPixel.h>
#include <PubSubClient.h>

// To download on link below and install manually
#include <TimeLib.h> //https://github.com/PaulStoffregen/Time

// ################################
// ######## Initialisation ########
// ################################

//General Initialisation
#define DELAY 900000000 // Delay between two loop in us, 900000 milliseconds = 900 seconds = 15 min
#define DELAY_NIGHT 4320000000 // Delay between two loop in us, 4320000000 milliseconds = 43200 seconds = 720 min = 12 h
#define TIMER_DISPLAY 60000 // Timer of reading sensor value display in ms, 60000 ms = 60 s = 1 min


// CSV file's path
char filename[] = "/data.csv";


// Wifi initialisation
// Replace the next variables with your SSID/Password combination
const char* ssid = "AP";
const char* password = "123456789";
// Add your MQTT Broker IP address, example:
const char* mqtt_server = "broker-dev.enovact.fr";
WiFiClient m5Client;
PubSubClient client(m5Client);
long lastMsg = 0;
char msg[50];
int value = 0;


// Fan initialisation :
// Number of the Fan pin
#define fanPin1 16  // 16 corresponds to GPIO16
#define fanPin2 17  // 17 corresponds to GPIO17
// Setting PWM properties
#define freq_pwm 5000
#define fanChannel 0
#define resolution 8
int dutyCycle = 0; // Variable PWM image de la consigne de 0 à 255


// LED initialisation :
#define PIN_LED 1
// Parameter 1 = number of pixels in strip
// Parameter 2 = Arduino pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
//   NEO_RGBW    Pixels are wired for RGBW bitstream (NeoPixel RGBW products)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(30, PIN_LED, NEO_GRB + NEO_KHZ800);
// IMPORTANT: To reduce NeoPixel burnout risk, add 1000 uF capacitor across
// pixel power leads, add 300 - 500 Ohm resistor on first pixel's data input
// and minimize distance between Arduino and first pixel.  Avoid connecting
// on a live circuit...if you must, connect GND first.


// DHT22 initialisation : (Air T°C and Humidity)
#define DHTPIN 2 // DHT22 Pin
#define DHTTYPE DHT22 // DHT 22
DHT dht(DHTPIN, DHTTYPE); // Déclaration du capteur


// DS18B20 initialisation : (Water T°C)
#define ONE_WIRE_BUS 5  // DS18B20 on arduino pin2 corresponds to D4 on physical board
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature DS18B20(&oneWire);


// Photocell initialisation :
// Constants
#define R 1000 //ohm resistance value
#define VIN 5 // V power voltage
// Parameters
const int sensorPin = 35; // Pin connected to sensor
//Variables
int sensorVal; // Analog value from the sensor
int lux; //Lux value


// Time initialisation :
const char* ntpServer = "ntp.nict.jp";
const long  gmtOffset_sec = 1 * 3600;  // Heure Paris = UTC + 1
const int   daylightOffset_sec = 0;
int hh, mm, ss;
int retry = 5;


// ################################
// ########     SETUP      ########
// ################################

void setup() {
  Serial.begin(9600);

  // Initialisation of M5Stack
  M5.begin();
  M5.Lcd.setTextColor(TFT_WHITE,TFT_BLACK);
  M5.Lcd.setTextSize(2);


  // Wifi Setup
  setup_wifi();

  // MQTT Setup
  client.setServer(mqtt_server, 1883);

  // Init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  
  for(int i = 0; i < retry; i++){
    if(!getLocalTime(&timeinfo)){
      M5.Lcd.setTextColor(RED);
      Serial.println("Failed to obtain time");
      M5.Lcd.println("Failed to obtain time");
      if(i == retry - 1){
        return;
      }
    }else{
      M5.Lcd.setTextColor(GREEN);
      Serial.println("Connected to NTP Server!");
      M5.Lcd.println("Connected to NTP Server!");
      break;
    }
  }

  hh = timeinfo.tm_hour;
  mm = timeinfo.tm_min;
  ss = timeinfo.tm_sec;

  // Set current time only the first to values, hh,mm are needed
  // If set the current time to 14:27:00, December 14th, 2015
  // setTime(14, 27, 00, 14, 12, 2015);
  setTime(hh, mm, ss, 0, 0, 0);
  M5.Lcd.printf("%02d:%02d:%02d", hh, mm, ss);

  //disconnect WiFi as it's no longer needed
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  // Setup of Led strip
  strip.begin();
  strip.setBrightness(50);
  strip.show(); // Initialize all pixels to 'off'

  // Initialisation of DHT22
  dht.begin(); 

  // Fan Setup
  // Configure Fan PWM functionalitites
  ledcSetup(fanChannel, freq_pwm, resolution);
  // Attach the channel to the GPIO to be controlled
  ledcAttachPin(fanPin1, fanChannel);
  ledcAttachPin(fanPin2, fanChannel);
}


// ################################
// ########      LOOP      ########
// ################################

void loop() {
  M5.Lcd.println("Test general capteur : ");
  M5.Lcd.println();

  
  // ########      Reading of Luminosity      ########

  sensorVal = analogRead(sensorPin);
  lux=sensorRawToPhys(sensorVal);
  
  // Display of luminosity :
  M5.Lcd.print("> Luminosite : ");
  M5.Lcd.print(lux); // the analog reading
  M5.Lcd.println(F(" lumen")); // the analog reading
  M5.Lcd.println();


  // ########        Reading Water T°C       ########
  
  float celsius;
  DS18B20.begin();
  DS18B20.requestTemperatures();
  celsius = DS18B20.getTempCByIndex(0);

  // Display of water T°C :
  M5.Lcd.println("> Eau : " + (String)celsius + " C");


  // ########   Reading the air T°C and Humidity   ########
  
  M5.Lcd.println();
  // La lecture du capteur prend 250ms
  // Les valeurs lues peuvet etre vieilles de jusqu'a 2 secondes (le capteur est lent)
  float h = dht.readHumidity(); // on lit l'hygrometrie
  float t = dht.readTemperature(); // on lit la temperature en celsius (par defaut) 
  
  // Display of T°C and Humidity :
  M5.Lcd.print("> Humidite : ");
  M5.Lcd.print(h);
  M5.Lcd.println(" %");
  M5.Lcd.println();
  M5.Lcd.print("> Air : ");
  M5.Lcd.print(t);
  M5.Lcd.println(" C ");


  // ########   Filling CSV file in SD card   ########
  
  String dataString = "\r\n";
  dataString += (String)t + ";";
  dataString += (String)h + ";";
  dataString += (String)lux + ";";
  dataString += (String)celsius + ";";
  dataString += (String)("%02d:%02d:%02d", hh, mm, ss);

  File logfile = SD.open(filename, FILE_APPEND); 

  if (SD.exists(filename)) {
    Serial.println("file exists.");
    logfile.print(dataString);
    Serial.println("file filled. ");
  } else {
    Serial.println("file doesn't exist.");
  }
  
  logfile.close();


  // ########        Display timer        ########
  
  delay(TIMER_DISPLAY);
  M5.Lcd.clear();
  M5.Lcd.setCursor(1,1);


  // ########      Actuator settings      ########
  
  // Fan
  // dutyCycle = 0 // Fan stop
  // dutyCycle = 150 // Speed at half 
  // dutyCycle = 255 // Max speed
  // If humidity < 70%, fan off
  // The fans are turned on for one hour a day at 10am regardless of the humidity
  if(h > 70 || hh == 10){
    dutyCycle = 250;
  } else {
    dutyCycle = 0;
  }
  ledcWrite(fanChannel, dutyCycle);  
  
  // Leds
  // Some example procedures showing how to display to the pixels:
  colorWipe(strip.Color(255, 0, 0)); // Red
  //colorWipe(strip.Color(0, 255, 0)); // Green
  //colorWipe(strip.Color(0, 0, 255)); // Blue
  //colorWipe(strip.Color(255, 255, 255)); // White RGBW


  // ########        Check Wifi Status        ########
  
  if (!client.connected()) {
    reconnect();
  }
  client.loop();


  // ########     Send sensors data to MQTT     ########
  
  long now = millis();
  if (now - lastMsg > 5000) {
    lastMsg = now;
  
    // Temperature in Celsius
    long temperature = t;
    long humidity = h;
    long waterTemp = celsuis;
    long luminosity = lux;
     
    char tempString[8];
    char humString[8];
    char waterTempString[8];
    char luminosityTempString[8];

    //Publish of humidity
    dtostrf(humidity, 1, 2, humString);
    Serial.print("Humidity: ");
    Serial.println(humString);
    client.publish("pfc/humidity", humString);
    M5.Lcd.println("\nHumidity sent");

    //Publish of temperature
    dtostrf(temperature, 1, 2, tempString);
    Serial.print("Temperature: ");
    Serial.println(tempString);
    client.publish("pfc/temperature", tempString);
    M5.Lcd.println("\nTemperature sent");

    //Publish of water temperature
    dtostrf(waterTemp, 1, 2, waterTempString);
    Serial.print("Warter temperature: ");
    Serial.println(waterTempString);
    client.publish("pfc/watertemperature", waterTempString);
    M5.Lcd.println("\nWater temperature sent");

    //Publish of luminosity
    dtostrf(luminosity, 1, 2, luminosityTempString);
    Serial.print("Luminosity: ");
    Serial.println(tempString);
    client.publish("pfc/luminosity", luminosityTempString);
    M5.Lcd.println("\nLuminosity sent");
  }

  // ########        Night Mode        ########

  // After 8pm, LED and microcontroller (M5STACK) : switch off
  if( hh > 20){
    strip.clear();
    esp_sleep_enable_timer_wakeup(DELAY_NIGHT);
  } else {
    esp_sleep_enable_timer_wakeup(DELAY); 
  }


  // ########      Sleep to save power      ########
  
  // Light Sleep : In this mode, digital peripherals, most of the RAM, and CPUs are clock-gated. 
  // When light sleep mode exits, peripherals and CPUs resume operation, their internal state is preserved.
  esp_light_sleep_start();
}


// ###########################################
// ########    Sensor/Actuator fct    ########
// ###########################################

int sensorRawToPhys(int raw){
  // Conversion rule
  float Vout = float(raw) * (VIN / float(4095));// Conversion analog to voltage
  float RLDR = (R * (VIN - Vout))/Vout; // Conversion voltage to resistance
  int phys=500/(RLDR/1000); // Conversion resitance to lumen
  return phys;
}

// Fill the dots one after the other with a color
void colorWipe(uint32_t c) {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
    strip.show();
  }
}

// ################################
// ########    Wifi fct    ########
// ################################

void setup_wifi() {
  delay(10);
  M5.Lcd.print("Connecting to ");
  M5.Lcd.println(ssid);
  
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  M5.Lcd.println("\nConnected");
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("M5Stack", "PFC01", "pfc01")) {
      Serial.println("connected");
      // Subscribe
      client.subscribe("esp32/output");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      M5.Lcd.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
