/**********************************************************************************
 *  TITLE: Plant Watering system using ESP32 Webserver AP, Moisture Sensor, DHT11 with 0.96" OLED
 *  Click on the following links to learn more. 
 *  YouTube Video: https://youtu.be/EX7sC-ksH4o
 *  Related Blog : https://iotcircuithub.com/esp32-projects/
 *  
 *  This code is provided free for project purpose and fair use only.
 *  Please do mail us to techstudycell@gmail.com if you want to use it commercially.
 *  Copyrighted © by Tech StudyCell
 *  
 *  Preferences--> Aditional boards Manager URLs : 
 *  https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_dev_index.json, http://arduino.esp8266.com/stable/package_esp8266com_index.json
 *  
 *  Download Board ESP32 (3.1.1) : https://github.com/espressif/arduino-esp32
 *
 *  Download the libraries 
 *  Adafruit_SSD1306 Library (2.5.13): https://github.com/adafruit/Adafruit_SSD1306
 *  AceButton Library (1.10.1): https://github.com/bxparks/AceButton
 *  Adafruit Unified Sensor library (1.1.15): https://github.com/adafruit/Adafruit_Sensor
 *  DHT Library (1.4.6): https://github.com/adafruit/DHT-sensor-library
 **********************************************************************************/
 

#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <AceButton.h>
using namespace ace_button;

#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 32  // OLED display height, in pixels
#define OLED_RESET -1    // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define SensorPin 34
#define DHTPin 14
#define RelayPin 25
#define wifiLed 2
#define RelayButtonPin 32
#define ModeSwitchPin 33
#define BuzzerPin 26
#define ModeLed 15

#define DHTTYPE DHT11
DHT dht(DHTPin, DHTTYPE);

ButtonConfig config1;
AceButton button1(&config1);
ButtonConfig config2;
AceButton button2(&config2);

void handleEvent1(AceButton*, uint8_t, uint8_t);
void handleEvent2(AceButton*, uint8_t, uint8_t);

const char *ssid = "ESP32_Plant_System";  //AP Name
const char *password = "12345678";  //AP Password

WebServer server(80);

//Set the maximum wet and maximum dry value measured by the sensor
int wetSoilVal = 1050 ;  //min value when soil is wet
int drySoilVal = 3050 ;  //max value when soil is dry

//Set ideal moisture range percentage(%) in soil
int moistPerLow =   20 ;  //min moisture %
int moistPerHigh =   80 ;  //max moisture %

int sensorVal;
int moisturePercentage;
bool toggleRelay = LOW;
bool prevMode = true;
int temperature1 = 0;
int humidity1 = 0;
String currMode = "A";

unsigned long previousMillis = 0;
const long interval = 2000;  //Interval for Sensor OLED
unsigned long previousBuzzerMillis = 0;
const long buzzerInterval = 3000;  //Interval for Buzzer

void handleRoot() {
    String html = "<!DOCTYPE html><html><head><title>Plant Monitor</title>";
    html += "<style>body { text-align:center; font-family:Arial; background:#1e1e2e; color:white; }";
    html += "h1 { color:#00c853; } .container { margin:auto; width:80%; } .box { padding:20px; border-radius:10px; margin:10px; background:#3b4252; }";
    html += ".button { padding:15px 30px; margin:10px; border:none; border-radius:5px; font-size:20px; cursor:pointer; display:inline-block; width:45%; }";
    html += "#pumpBtn { background:#ff5722; color:white; } #modeBtn { background:#2196F3; color:white; } #refreshBtn { background:#4CAF50; color:white; width:100%; }";
    html += "</style></head><body>";
    html += "<div class='container'><h1>Plant Watering System</h1>";
    html += "<div class='box'><h2>Temperature: " + String(temperature1) + " °C</h2></div>";
    html += "<div class='box'><h2>Humidity: " + String(humidity1) + " %</h2></div>";
    html += "<div class='box'><h2><b>Soil Moisture: " + String(moisturePercentage) + " %</b></h2></div>";
    html += "<div class='box'><h2>Mode: " + String(prevMode ? "Auto" : "Manual") + "</h2></div>";
    html += "<div class='box'><h2>Pump: " + String(toggleRelay ? "ON" : "OFF") + "</h2></div>";
    html += "<button id='pumpBtn' class='button' onclick='togglePump()'>Toggle Pump</button>";
    html += "<button id='modeBtn' class='button' onclick='toggleMode()'>Toggle Mode</button>";
    html += "<br><button id='refreshBtn' class='button' onclick='refreshPage()'>Refresh</button>";
    html += "<script>function togglePump(){ fetch('/toggle'); } function toggleMode(){ fetch('/mode'); } function refreshPage(){ location.reload(); }</script>";
    html += "</div></body></html>";
    
    server.send(200, "text/html", html);
}

void getMoisture() {
    sensorVal = analogRead(SensorPin);
    moisturePercentage = map(sensorVal, drySoilVal, wetSoilVal, 0, 100);
    moisturePercentage = constrain(moisturePercentage, 0, 100);
}

void getWeather() {
    temperature1 = dht.readTemperature();
    humidity1 = dht.readHumidity();
    if (isnan(temperature1) || isnan(humidity1)) {
        Serial.println("Failed to read from DHT sensor!");
        temperature1 = 0;
        humidity1 = 0;
    }
}

void handleBuzzer() {
    if (!prevMode) {
      if ((moisturePercentage < moistPerLow) && !(digitalRead(RelayPin))) {
        unsigned long currentMillis = millis();
        if (currentMillis - previousBuzzerMillis >= buzzerInterval) {
            previousBuzzerMillis = currentMillis;
            digitalWrite(BuzzerPin, !digitalRead(BuzzerPin));
        }
      }
      if ((moisturePercentage > moistPerHigh) && digitalRead(RelayPin)) {
        unsigned long currentMillis = millis();
        if (currentMillis - previousBuzzerMillis >= buzzerInterval) {
            previousBuzzerMillis = currentMillis;
            digitalWrite(BuzzerPin, !digitalRead(BuzzerPin));
        }
      }  
    }
}

void controlBuzzer(int duration){
  digitalWrite(BuzzerPin, HIGH);
  delay(duration);
  digitalWrite(BuzzerPin, LOW);
}

void controlPump() {
    if (prevMode) {
        if (moisturePercentage < moistPerLow) {
            digitalWrite(RelayPin, HIGH);
            toggleRelay = HIGH;
        }
        if (moisturePercentage > moistPerHigh) {
            digitalWrite(RelayPin, LOW);
            toggleRelay = LOW;
        }
    }
}

void updateOLED() {
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(30,2);
    display.print(moisturePercentage); display.print(" %");
    display.setTextSize(1);
    display.setCursor(1,25);
    display.print("T:"); display.print(temperature1); display.print(" C, ");
    display.print("H:"); display.print(humidity1); display.print("%  ");
    display.print(prevMode ? "A:" : "M:");
    display.print(toggleRelay ? "1" : "0");
    display.display();
}

void setup() {
    Serial.begin(115200);
    WiFi.softAP(ssid, password);
    Serial.println("Access Point Started");
    
    dht.begin();
    pinMode(SensorPin, INPUT);
    pinMode(RelayPin, OUTPUT);
    pinMode(BuzzerPin, OUTPUT);
    pinMode(ModeLed, OUTPUT);
    pinMode(RelayButtonPin, INPUT_PULLUP);
    pinMode(ModeSwitchPin, INPUT_PULLUP);
    digitalWrite(RelayPin, LOW);
    digitalWrite(BuzzerPin, LOW);
    digitalWrite(ModeLed, LOW);

    config1.setEventHandler(button1Handler);
    config2.setEventHandler(button2Handler);
  
    button1.init(RelayButtonPin);
    button2.init(ModeSwitchPin);
    
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("SSD1306 allocation failed"));
        for (;;);
    }
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);

    server.on("/", handleRoot);
    server.on("/toggle", handleToggle);
    server.on("/mode", handleMode);
    server.on("/refresh", handleRoot);
    server.begin();
    Serial.println("HTTP server started");

    digitalWrite(ModeLed, prevMode);

    controlBuzzer(1000); 
    getMoisture();
}

void loop() {
    server.handleClient();
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        getMoisture();
        getWeather();
        updateOLED();
    }
    handleBuzzer();  //control buzzer in manual mode
    controlPump();   //control Pump in auto mode

    button1.check();
    button2.check();
}

void handleToggle() {
    if (!prevMode) {
        digitalWrite(RelayPin, !digitalRead(RelayPin));
        toggleRelay = !toggleRelay;
    }
    server.send(200, "text/plain", "Pump toggled");
    controlBuzzer(500);
}

void handleMode() {
    prevMode = !prevMode;
    digitalWrite(ModeLed, prevMode);
    server.send(200, "text/plain", "Mode toggled");
    controlBuzzer(500);
}

void button1Handler(AceButton* button, uint8_t eventType, uint8_t buttonState) {
  Serial.println("EVENT1");
  switch (eventType) {
    case AceButton::kEventReleased:
      //Serial.println("kEventReleased");
      if (!prevMode) {
            handleToggle();
        }
    break;
  }
}

void button2Handler(AceButton* button, uint8_t eventType, uint8_t buttonState) {
  Serial.println("EVENT2");
  switch (eventType) {
    case AceButton::kEventReleased:
      //Serial.println("kEventReleased");
      handleMode();
      break;
  }
}
