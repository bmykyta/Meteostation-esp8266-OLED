#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#include <ArduinoJson.h>
#include "OpenWeather.h"

OpenWeather::OpenWeather(String owm_token)
{
  this->api_token = owm_token;
}

bool OpenWeather::getWeatherData(OW_current *current, String city, String units, String language)
{
  this->current  = current;
  String url = "https://api.openweathermap.org/data/2.5/weather?q=" + city + "&units=" + units + "&lang=" + language + "&appid=" + api_token;
  // Send GET request and feed the parser
  bool result = parseRequest(url);

  return result;
}

bool OpenWeather::parseRequest(String url)
{
  isParsed = false;
  uint32_t timeout = millis();
  const char*  host = "api.openweathermap.org";
  String response = "";
  BearSSL::WiFiClientSecure client;
  client.setInsecure();

  if (!client.connect(host, 443))
  {
    if (DEBUG_FLAG == true) {
      Serial.println("Connection failed.");
    }
    return isParsed;
  }
  client.print("GET " + url + " HTTP/1.1\r\n" + "Host: " + host + "\r\n" + "Connection: close\r\n\r\n");

  while (client.available() || client.connected())
  {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      if (DEBUG_FLAG == true) {
        Serial.println("Header end found");
      }
      break;
    }

    if ((millis() - timeout) > 5000UL)
    {
      if (DEBUG_FLAG == true) {
        Serial.println("HTTP header timeout");
      }
      client.stop();
      return isParsed;
    }
  }

  // Parse the JSON data, available() includes yields
  while (client.connected())
  {
    if (client.available()) {
      char c = client.read();
      if (c != '\n' && c != '\r') {
        response += c;
      }
    }

    if ((millis() - timeout) > 8000UL)
    {
      Serial.println("JSON client timeout");
      client.stop();
      return isParsed;
    }
  }

  // JSON buffer
  const size_t capacity = JSON_ARRAY_SIZE(3) + 2 * JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2) + 3 * JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(6) + JSON_OBJECT_SIZE(12) + 480;
  DynamicJsonDocument jsonBuffer(capacity);
  DeserializationError err = deserializeJson(jsonBuffer, response);
  if (err != DeserializationError::Ok) {
    Serial.print(F("deserializeJson() failed with code "));
    Serial.println(err.c_str());
    return false;
  }
  JsonObject weather = jsonBuffer["weather"][0];
  current->main = weather["main"].as<String>(); // "short weather forecast like - clear"
  current->description = weather["description"].as<String>(); // "full description like - clear sky"

  JsonObject main = jsonBuffer["main"];
  current->temp = main["temp"];
  current->feels_like = main["feels_like"];
  current->pressure = main["pressure"]; // int
  current->humidity = main["humidity"]; // int percantage
  current->visibility = jsonBuffer["visibility"]; // integer
  current->wind_speed = jsonBuffer["wind"]["speed"]; // int
  current->wind_deg = jsonBuffer["wind"]["deg"]; // int
  current->clouds = jsonBuffer["clouds"]["all"]; // int percantage
  int timezone = jsonBuffer["timezone"]; // 7200
  current->id = jsonBuffer["id"];
  current->city = jsonBuffer["name"].as<String>(); // "Kharkiv"
  int cod = jsonBuffer["cod"]; // 200

  isParsed = true;
  client.stop();   //Close connection

  return isParsed;
}
