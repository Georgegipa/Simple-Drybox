#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <AsyncTCP.h>
#include "SPIFFS.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
// libraries for DHT sensor
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include "config.h"

DHT dht(DHTPIN, DHTTYPE);

AsyncWebServer server(80);

// Create an Event Source on /events
AsyncEventSource events("/events");
char output[200];

uint8_t tempRange[2] = {TEMP_MIN, TEMP_MAX};
uint8_t humidityRange[2] = {HUM_MIN, HUM_MAX};

float temperature = 0.0;
float humidity = 0.0;
bool heaterState = false;
bool fanState = false;
bool autoMode = true;

char buffer[3];

//helper function to send the range values to the client
void sendRange(String topic1, String topic2, uint8_t range[2])
{
  sprintf(buffer, "%d", range[0]);
  events.send(buffer, topic1.c_str(), millis());
  sprintf(buffer, "%d", range[1]);
  events.send(buffer, topic2.c_str(), millis());
}

//set the temperature range
void setTempRange(uint8_t min, uint8_t max)
{
  tempRange[0] = min;
  tempRange[1] = max;
  sendRange("tempMin", "tempMax", tempRange);
  Serial.printf("New temperature range: %d - %d\n", tempRange[0], tempRange[1]);
}

//set the humidity range
void setHumidityRange(uint8_t min, uint8_t max)
{
  humidityRange[0] = min;
  humidityRange[1] = max;
  sendRange("humMin", "humMax", humidityRange);
  Serial.printf("New humidity range: %d - %d\n", humidityRange[0], humidityRange[1]);
}

// turn on or off the heater
void heaterControl(bool state)
{
  //if the state is the same, exit the function
  if (state == heaterState)
    return;
  digitalWrite(PTC_HEATER_RELAY, state);
  heaterState = state;
  events.send(heaterState ? "on" : "off", "heater", millis());
  Serial.print("Turning heater: ");
  Serial.println(heaterState ? "on" : "off");
}

// control autom mode
void autoControl(bool state)
{
  //if the state is the same, exit the function
  if (state == autoMode)
    return;
  autoMode = state;
  events.send(autoMode ? "on" : "off", "auto", millis());
  Serial.print("Turning auto mode: ");
  Serial.println(autoMode ? "on" : "off");
}

// turn on or off the fan
void fanControl(bool state)
{
  //if the state is the same, exit the function
  if (state == fanState)
    return;

  digitalWrite(DRY_FAN_RELAY, state);
  fanState = state;
  events.send(fanState ? "on" : "off", "fan", millis());
  Serial.print("Turning fan: ");
  Serial.println(fanState ? "on" : "off");
}

//setup mDNS , the device will be accessible at drybox.local
void mdnsSetup()
{
  if (!MDNS.begin(local_hostname))
  {
    Serial.println("Error setting up MDNS responder!");
    while (1)
    {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");
}

//helper function to handle the bool api requests
void boolApiHelper(AsyncWebServerRequest *request, bool &state)
{
  if (!(request->hasArg("enabled")))
  {
    request->send(400, "text/plain", "Bad Request");
    return;
  }
  else
  {
    if (request->arg("enabled") == "true")
    {
      state = true;
    }
    else if (request->arg("enabled") == "false")
    {
      state = false;
    }
    else
    {
      request->send(400, "text/plain", "Bad Request");
      return;
    }
    request->send(200, "text/plain", "OK");
  }
}

void webserverSetup()
{
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request)
  {
    request->send(SPIFFS, "/index.html", String(), false);
  });

  server.on("/api/", HTTP_GET, [](AsyncWebServerRequest * request)
  {
    request->send(SPIFFS, "/api.html", String(), false);
  });

  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest * request)
  {
    request->send(SPIFFS, "/style.css", "text/css");
  });

  server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest * request)
  {
    request->send(SPIFFS, "/script.js", "text/javascript");
  });

  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest * request)
  {
    JsonDocument doc;

    doc["temperature"] = temperature;
    doc["humidity"] = humidity;
    doc["heater_state"] = heaterState;
    doc["cooling_fan_state"] = fanState;
    doc["auto_mode"] = autoMode;

    //add arrays for the temperature and humidity range
    JsonArray temperature_range = doc["temperature_range"].to<JsonArray>();
    temperature_range.add(tempRange[0]);
    temperature_range.add(tempRange[1]);

    JsonArray humidity_range = doc["humidity_range"].to<JsonArray>();
    humidity_range.add(humidityRange[0]);
    humidity_range.add(humidityRange[1]);

    doc.shrinkToFit(); // optional

    serializeJson(doc, output);

    request->send(200, "application/json", output);
  });

  // endpoint to turn on or off the ptc heater
  server.on("/api/heater", HTTP_GET, [](AsyncWebServerRequest * request) {
    bool state;
    boolApiHelper(request, state);
    heaterControl(state);
  });

  // enable or disable the auto mode
  server.on("/api/auto", HTTP_GET, [](AsyncWebServerRequest * request) {
    bool state;
    boolApiHelper(request, state);
    autoControl(state);
  });

  // endpoint to turn on or off the cooling fan
  server.on("/api/fan", HTTP_GET, [](AsyncWebServerRequest * request) {
    bool state;
    boolApiHelper(request, state);
    fanControl(state);
  });

  server.on("/api/setTempRange", HTTP_GET, [](AsyncWebServerRequest * request) {
    if (!(request->hasArg("min") && request->hasArg("max")))
    {
      request->send(400, "text/plain", "Bad Request");
      return;
    }
    else
    {
      uint8_t min = request->arg("min").toInt();
      uint8_t max = request->arg("max").toInt();

      //check if the values are within the range
      if (min < TEMP_MIN || min > TEMP_MAX || max < TEMP_MIN || max > TEMP_MAX)
      {
        request->send(400, "text/plain", "Invalid Range");
        sendRange("tempMin", "tempMax", tempRange);
        return;
      }
      //check if the min is less than max
      if (min > max)
      {
        request->send(400, "text/plain", "Min should be less than max");
        sendRange("tempMin", "tempMax", tempRange);
        return;
      }

      if (max < min)
      {
        request->send(400, "text/plain", "Max should be greater than min");
        sendRange("tempMin", "tempMax", tempRange);
        return;
      }

      setTempRange(min, max);
      request->send(200, "text/plain", "OK");
    }
  });

  server.on("/api/setHumRange", HTTP_GET, [](AsyncWebServerRequest * request) {
    if (!(request->hasArg("min") && request->hasArg("max")))
    {
      request->send(400, "text/plain", "Bad Request");
      return;
    }
    else
    {
      uint8_t min = request->arg("min").toInt();
      uint8_t max = request->arg("max").toInt();

      //check if the values are within the range
      if (min < HUM_MIN || min > HUM_MAX || max < HUM_MIN || max > HUM_MAX)
      {
        request->send(400, "text/plain", "Invalid Range");
        sendRange("humMin", "humMax", humidityRange);
        return;
      }
      //check if the min is less than max
      if (min > max)
      {
        request->send(400, "text/plain", "Min should be less than max");
        sendRange("humMin", "humMax", humidityRange);
        return;
      }

      if (max < min)
      {
        request->send(400, "text/plain", "Max should be greater than min");
        sendRange("humMin", "humMax", humidityRange);
        return;
      }

      setHumidityRange(min, max);
      request->send(200, "text/plain", "OK");
    }
  });

  // endpoint to reboot the device
  server.on("/api/reboot", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(200, "text/plain", "Rebooting...");
    ESP.restart();
  });

  // Handle Web Server Events
  events.onConnect([](AsyncEventSourceClient * client) {
    if (client->lastId()) {
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    //send the values once the device is connected
    client->send(String((int)temperature).c_str(), "temperature", millis(), 10000);
    client->send(String((int)humidity).c_str(), "humidity", millis(), 10000);
    client->send(heaterState ? "on" : "off", "heater", millis(), 10000);
    client->send(fanState ? "on" : "off", "fan", millis(), 10000);
    client->send(autoMode ? "on" : "off", "auto", millis(), 10000);
    //send the values of the target ranges
    sendRange("tempMin", "tempMax", tempRange);
    sendRange("humMin", "humMax", humidityRange);
  });

  server.addHandler(&events);
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  server.begin();
}

void setup()
{
  Serial.begin(115200);
  // setup relay pins
  pinMode(PTC_HEATER_RELAY, OUTPUT);
  pinMode(DRY_FAN_RELAY, OUTPUT);

  //Initialize the DHT sensor

  dht.begin();

  // try to connect to wifi

  Serial.println("");
  Serial.print("Connecting to ");
  Serial.print(ssid);

  if (!SPIFFS.begin())
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
  // connect to your local wi-fi network
  WiFi.begin(ssid, password);

  // wait for connection
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected..!");

  Serial.print("Got IP: ");
  Serial.println(WiFi.localIP());

  //setup mDNS , the hostname will be drybox.local
  mdnsSetup();

  // before starting the server, read from the dht11 sensor
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();

  //start the webserver
  webserverSetup();
}

void smartControl()
{
  // create a simple control system to keep the temperature and humidity in the desired range

  // the temperature can be in the range , below and above
  //if it is in the range
  if (temperature > tempRange[0] && temperature < tempRange[1])
  {
    heaterControl(false);
  }
  //if it is below the range
  else if (temperature < tempRange[0])
  {
    heaterControl(true);
  }
  //if it is above the range
  else if (temperature > tempRange[1])
  {
    //turn of the fan for cooling
    fanControl(true);
  }

  // if humidity is in the range turn off the fan
  if (humidity > humidityRange[0] && humidity < humidityRange[1])
  {
    fanControl(false);
  }
  else
  {
    //regulate the air
    fanControl(true);
  }
}

unsigned long previousMillis = 0;
void loop()
{
  // every 2.5 seconds, update the sensor values and send them to the client
  if (millis() - previousMillis > 2500 * 2)
  {
    auto temp = dht.readTemperature();
    auto hum = dht.readHumidity();
    previousMillis = millis();

    // check if temperature or humidity is NaN, so we don't send invalid data
    if (!isnan(temp) && !isnan(hum))
    {
      temperature = temp;
      humidity = hum;

      Serial.print("Temperature: ");
      Serial.print(temperature);
      Serial.print(" Humidity: ");
      Serial.println(humidity);

      events.send(String((int)temperature).c_str(), "temperature", millis());
      events.send(String((int)humidity).c_str(), "humidity", millis());
    }
    else
    {
      Serial.println("Failed to read from DHT sensor!");
    }
    //if the auto mode is on, let the device control the heater and fans
    if (autoMode)
    {
      smartControl();
    }
  }
}
