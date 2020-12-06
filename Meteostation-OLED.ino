/*
    Meteostation with node-mcu esp8266
    Written by dev_iks
*/
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <NTPClient.h>
#include <DHT.h>
#include <MQ135.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "configs.h"
#include "functions.h"
#include "OpenWeather.h"


#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define DHTPIN D7 // DHT pin
#define DISPLAY_ADDR 0x3C // OLED address
#define GASPIN A0 // mq135 sensor pin
#define BUFFER_SIZE 100 // buffer size
#define MQTT_MAX_PACKET_SIZE 256
#define SECS_PER_MIN (60000)
#define HOURS_IN_MLSECS(hour) (hour * 60 * SECS_PER_MIN)
#define UTC_OFFSET 2 * 60 * 60 // Ukraine


bool _debug = true;

const char* ssid = STASSID; // SSID for wi-fi station
const char* password = STAPSK; // password for wi-fi station
const char* mqtt_server = MQTTSERVER; // mqtt server addr
const int mqtt_port = MQTTPORT; // mqtt port
const char* mqtt_user = MQTTUSER; // if mqtt anonymous with user and pass
const char* mqtt_pass = MQTTPASS;
const char* owm_token = OWMTOKEN;

const int LED_PIN = D5;
const int button = D4;

bool butt_flag = false;
bool highTempFlag = false;
bool butt;
unsigned long last_press = 0;
unsigned long last_time = 0;

String buffer_line_weather; // buffer for recieve response from weather api
String formattedDate;
String dayStamp;
String timeStamp;
String day, month, year;
String city = "Kharkiv";
String units = "metric";  // or "imperial"
String language = "en";
unsigned long updateTimeDHT = 0;
unsigned long updateTimeTelegramDHT = 0;
unsigned long updateTimeWeather = 0;
unsigned long updateTimeGasSensor = 0;
unsigned long updateTimeHighTemp = 0;
unsigned long updateLocalTime = 0;
unsigned long updateDrawDisplay = 0;
int ledStatus = 0; // led status
int current_mode = 0;
float humidity; // humidity - влажность воздуха получаемая на текущем модуле DHT-11
float temperature; // temperature - температура получаемая на текущем модуле DHT-11
float ppm; // co2 concentration in ppm


void oledPrint(String str, uint16_t x = 0, uint16_t y = 0, bool newLine = 1, uint16_t textSize = 1, uint16_t color = WHITE);
void drawCloud(uint16_t x = 0, uint16_t y = 0, bool d = false);
void drawLittleCloud(uint16_t x = 0, uint16_t y = 0, bool d = false);
void drawRaindrop(uint16_t x = 0, uint16_t y = 0, bool d = false);
void drawThermometer(uint16_t x = 0, uint16_t y = 0, bool d = false);
void callbackOnMessage(char* topic, byte* payload, unsigned int length);
void readDHT();
void gasSensorSetup();
void getWeather();
void printWeather();
void printDatetime();

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
DHT dht(DHTPIN, DHT11);
MQ135 gasSensor = MQ135(GASPIN);

WiFiUDP ntpUDP;
// pool.ntp.org ua.pool.ntp.org
NTPClient timeClient(ntpUDP, "pool.ntp.org", UTC_OFFSET);

WiFiClient wfc;
PubSubClient mqttClient(mqtt_server, mqtt_port, callbackOnMessage, wfc);

OpenWeather weather(owm_token);
OW_current* current_weather = new OW_current;

void setup() {
  Serial.begin(115200);
  //  gdbstub_init();
  pinMode(LED_PIN, OUTPUT);
  pinMode(button, INPUT);
  // Connect to WiFi network
  wifiBegin(ssid, password);

  if (!display.begin(SSD1306_SWITCHCAPVCC, DISPLAY_ADDR)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
  }
  display.clearDisplay();
  display.cp437(true);

  dht.begin();
  gasSensorSetup();
  getWeather();
  readDHT();
}

void loop() {
  butt = !digitalRead(button);
  mqttLoop();
  if (millis() - updateTimeDHT > 2000) {
    updateTimeDHT = millis();
    readDHT();
  }
  switchMode();
  displayedMode();
}

void switchMode() {
  if (butt == HIGH && butt_flag == false && millis() - last_press > 200) {
    butt_flag = true;
    current_mode = !current_mode;
    last_press = millis();
  }

  if (butt == LOW && butt_flag == true) {
    butt_flag = false;
  }
}

void displayedMode() {
  if (current_mode == 0) {
    if (millis() - updateDrawDisplay > 1000) {
      updateDrawDisplay = millis();
      drawPage1();
    }
  } else {
    if (millis() - updateDrawDisplay > 1000) {
      updateDrawDisplay = millis();
      drawPage2();
    }
  }

  if (millis() - updateTimeTelegramDHT > 15000) {
    updateTimeTelegramDHT = millis();
    publishDhtData(temperature, humidity, ppm);
  }
}

void drawPage1()
{
  display.clearDisplay();
  display.setTextColor(WHITE, BLACK);
  printDatetime();
  drawThermometer(4, 13);
  // print temperatute
  oledPrint(String(temperature), LITTLE_CLOUD_WIDTH + 2, 16, 0);
  display.write("C");
  display.write(248);
  // print humidity
  drawRaindrop(2, 32);
  oledPrint(String(humidity) + "%", LITTLE_CLOUD_WIDTH + 2, 37);
  // print co2 value
  drawLittleCloud(0, 49);
  int y_little_cloud = 52;
  oledPrint("CO", LITTLE_CLOUD_WIDTH + 3, y_little_cloud, 0);
  display.setCursor(LITTLE_CLOUD_WIDTH + 15, y_little_cloud + 3);
  display.write(253);
  oledPrint(":" + String(ppm) + " ppm", LITTLE_CLOUD_WIDTH + 19, y_little_cloud);

  display.display();
}


void drawPage2()
{
  display.clearDisplay();
  display.setTextColor(WHITE, BLACK);
  drawCloud(0, 10);
  // update weather one time per hour
  if (millis() - updateTimeWeather > HOURS_IN_MLSECS(1))
  {
    updateTimeWeather = millis();
    getWeather();
  }
  printWeather();
  display.display();
}

void printWeather()
{
  float   temp = current_weather->temp;
  float   feels_like = current_weather->feels_like;
  float   pressure = current_weather->pressure;
  uint8_t humidity = current_weather->humidity;
  uint8_t clouds = current_weather->clouds;
  int     wind_speed = current_weather->wind_speed;
  String  main = current_weather->main;
  String  description = current_weather->description;
  String  city = current_weather->city;
  
  oledPrint(city, 0, 0);
  oledPrint(description, 50, 0);
  oledPrint("Humidity:" + String(humidity) + "%", CLOUD_WIDTH - 2, 10);
  oledPrint("Wind: " + String(wind_speed) + "m/s", CLOUD_WIDTH + 2, 20);
  oledPrint("Clouds: " + String(clouds) + "%", CLOUD_WIDTH + 2, 30);
  oledPrint("Temp:" + String(temp), CLOUD_WIDTH + 1, 40, 0);
  display.write(248);
  display.write("C");
  oledPrint("Feels like:" + String(feels_like), 2 , 50, 0);
  display.write(248);
  display.write("C");
}

void getWeather()
{
  bool isParsed = weather.getWeatherData(current_weather, city, units, language);
  if (!isParsed)
  {
    Serial.println("Failed to get weather! Try again.");
  }
}

/*
   String str, uint16_t x = 0, uint16_t y = 0, bool newLine = 1, uint16_t textSize = 1, uint16_t color = WHITE
*/
void oledPrint(String str, uint16_t x, uint16_t y, bool newLine, uint16_t textSize, uint16_t color)
{
  display.setTextSize(textSize);
  display.setTextColor(color);
  display.setCursor(x, y);
  if (newLine == true) {
    display.println(str);
  } else {
    display.print(str);
  }
}

/*
   usigned int x: display coordinate
   usigned int y: display coordinate
   bool d = false: display
*/
void drawCloud(uint16_t x, uint16_t y, bool d) {
  display.drawBitmap(x, y, cloud, CLOUD_WIDTH, CLOUD_HEIGHT, 1);
  if (d == true) {
    display.display();
  }
}

/*
   usigned int x: display coordinate
   usigned int y: display coordinate
   bool d = false: display
*/
void drawRaindrop(uint16_t x, uint16_t y, bool d) {
  display.drawBitmap(x, y, raindrop, RAINDROP_WIDTH, RAINDROP_HEIGHT, 1);
  if (d == true) {
    display.display();
  }
}

/*
   usigned int x: display coordinate
   usigned int y: display coordinate
   bool d = false: display
*/
void drawThermometer(uint16_t x, uint16_t y, bool d) {
  display.drawBitmap(x, y, thermometer, THERMOMETER_WIDTH, THERMOMETER_HEIGHT, 1);
  if (d == true) {
    display.display();
  }
}

/*
   usigned int x: display coordinate
   usigned int y: display coordinate
   bool d = false: display
*/
void drawLittleCloud(uint16_t x, uint16_t y, bool d)
{
  display.drawBitmap(x, y, little_cloud, LITTLE_CLOUD_WIDTH, LITTLE_CLOUD_HEIGHT, 1);
  if (d == true) {
    display.display();
  }
}

void getDateTime()
{
  timeClient.update();
  formattedDate = timeClient.getFormattedDate();
  // Extract date
  int splitT = formattedDate.indexOf("T");
  dayStamp = formattedDate.substring(0, splitT);
  year = dayStamp.substring(0, 4);
  month = dayStamp.substring(5, 7);
  day = dayStamp.substring(8);
  // Format dateStamp
  dayStamp = day + "." + month + "." + year;
  // Format time
  timeStamp = formattedDate.substring(splitT + 1, formattedDate.length() - 4);
}

void printDatetime()
{
  getDateTime();
  oledPrint(dayStamp, 2, 0);
  oledPrint(timeStamp, 88, 0);
  /*if (millis() - updateLocalTime > 5000)
  {
    updateLocalTime = millis();
    oledPrint(dayStamp, 2, 0);
    oledPrint(timeStamp, 88, 0);
  }*/
}

void readDHT() {
  humidity = dht.readHumidity();
  temperature = dht.readTemperature();

  if (temperature >= 31 && !highTempFlag) {
    highTempFlag = true;
    if (millis() - updateTimeHighTemp >= 300000)  { // one message per 5 minutes
      updateTimeHighTemp = millis();
      //      highTemperatureAlert();
    }
  } else {
    highTempFlag = false;
  }

  if (isnan(humidity) || isnan(temperature)) {
    // Check. If we can't get values, outputing «Error», and program exits
    if (_debug) {
      Serial.println("Error dht");
    }
    display.clearDisplay();
    oledPrint("Error!", 0, 0);
    return;
  }
  if (millis() - updateTimeGasSensor >= 5000) {
    updateTimeGasSensor = millis();
    ppm = gasSensor.getPPM();
  }
  if (isnan(ppm)) {
    if (_debug) {
      Serial.println("Error gas sensor");
    }
    display.clearDisplay();
    oledPrint("Err: ppm", 0, 0);
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP8266Client-1";
    // Attempt to connect
    if (mqttClient.connect(clientId.c_str())) {
      Serial.print("connected  state = ");
      Serial.println(mqttClient.state());
      mqttClient.subscribe("room/+");
    } else {
      Serial.print("failed, rc = ");
      Serial.print(mqttClient.state());
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void mqttReconnection() {
  if (!mqttClient.connected()) {
    Serial.println("reconnect!....");
    reconnect();
  }
}

void mqttLoop() {
  mqttReconnection();
  mqttClient.loop();
}

void gasSensorSetup() {
  float rzero = gasSensor.getRZero();
  delay(3000);
  Serial.print("MQ135 RZERO Calibration Value : ");
  Serial.println(rzero);
}

//handle arrived messages from mqtt broker
void callbackOnMessage(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  String _topic = String(topic);
  if (_topic == "room/led") {
    int ledState = String((char*)payload).toInt();
    ledStatus = ledState;
    digitalWrite(LED_PIN, ledState);
  } else if (_topic == "room/led-status") {
    char _ledStat[2];
    itoa(ledStatus, _ledStat, 10);
    mqttClient.publish("room/led-status/telegram", _ledStat);
  } else if (_topic == "room/getmeteodata") {
    readDHT();
    publishMeteodataTelegram();
  } else if (_topic == "room/getweather") {
    buffer_line_weather = String((char*)payload);
    //    weather = jsonParser.getWeatherData(buffer_line_weather);
  }
}

void publishMeteodataTelegram() {
  String rez = "{";
  rez += "\"temperature\":" + String(temperature) + ",";
  rez += "\"humidity\":" + String(humidity) + ",";
  rez += "\"ppm\":" + String(ppm);
  rez += "}";
  int rezLen = 2 * rez.length();
  char msgBuffer[rezLen];
  rez.toCharArray(msgBuffer, rezLen);
  Serial.println(msgBuffer);
  mqttClient.publish("room/meteodata/telegram", msgBuffer);
}

void publishDhtData(float temperature, float humidity, float ppm) {
  char msgBuffer[20];
  mqttClient.publish("room/temp", dtostrf(temperature, 5, 2, msgBuffer));
  mqttClient.publish("room/humidity", dtostrf(humidity, 5, 2, msgBuffer));
  mqttClient.publish("room/co2", dtostrf(ppm, 7, 0, msgBuffer));
}

void highTemperatureAlert() {
  char msgBuffer[5];
  mqttClient.publish("room/highTemp/telegram", dtostrf(temperature, 5, 2, msgBuffer));
}
