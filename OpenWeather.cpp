#include <ESP8266WiFi.h>


#include <ArduinoJson.h>
#include "OpenWeather.h"

OpenWeather::OpenWeather(String owm_token)
{
  this->api_token = owm_token;
}

bool OpenWeather::getWeatherData(OW_current *current, String city, String units, String language)
{
  this->current  = current;
  String url = "https://api.openweathermap.org/data/2.5/weather?q=" + city + "&units=" + units + "&lang=" + language + "&appid=" + api_key;
   // Send GET request and feed the parser
  bool result = parseRequest(url);
}
