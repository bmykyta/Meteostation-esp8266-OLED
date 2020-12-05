#pragma once

class OpenWeather 
{
  private:
    String api_token;
    String data_set;
    bool   isParsed;  
    OW_current  *current;
    uint16_t objectLevel;   // Object level, increments for new object, decrements at end
    String   currentKey;    // Name key of the name:value pair e.g "temperature"
    String   currentSet;    // Name key of the data set
    String   arrayPath;     // Path to name:value pair e.g.  "daily/data"
    uint16_t arrayIndex;    // Array index e.g. 5 for day 5 forecast, qualify with arrayPath
    uint16_t arrayLevel; 

  public:
  /*
   * owm_token - given token for using open weather map
   */
  OpenWeather(String owm_token);

  /*
   * Setup the weather request and save in ow_current struct
   * city - City name 
   * units - metric (Celsius), imperial (Fahrenheit) or standart (Kelvin)
   * language - en, ua (uk), de etc. watch: https://openweathermap.org/current#multi
   */
  bool getWeatherData(OW_current *current, String city, String units, String language);

  /*
   * Called by library (can by user sketch), sends HTTP GET request to url and parsing response
   * url - Url string 
   * return - true on success and false on fail
   */
  bool parseRequest(String url);
};
