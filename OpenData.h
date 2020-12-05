/*
 * struct for storing open weather data
 */

typedef struct OW_current {

  // current
  uint32_t dt = 0;
  float    temp = 0;
  float    feels_like = 0;
  float    pressure = 0;
  uint8_t  humidity = 0;
  uint8_t  clouds = 0;
  uint32_t visibility = 0;
  float    wind_speed = 0;
  float    wind_gust = 0;
  uint16_t wind_deg = 0;
  float    rain = 0;
  float    snow = 0;

  // current.weather
  uint16_t id = 0;
  String   city;
  String   main;
  String   params;
  String   description;

} OW_current;
