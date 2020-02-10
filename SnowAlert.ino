/*
  SnowAlert
  Connects to wifi and alerts you via IFTTT if it has snowed today.

  Instructions can be found on Instructables.
*/

//Ncessary for file-system access for configuration
#include <FS.h>

// Used to connect to the wifi appropriately.
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <ESP8266WiFiMulti.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

// Used to manage the wifi connection on startup and save parameters.
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

// Used for the sensor
#include "SoftwareSerial.h"
#include "TFmini.h"

// Used to get the current time in UTC from the internet
#include <NTPClient.h>

// Used to detect two back-to-back clicks on the reset button to allow changing parameters
#define ESP_DRD_USE_SPIFFS      true
#define ESP_DRD_USE_EEPROM      false
#define ESP8266_DRD_USE_RTC     false
#define DOUBLERESETDETECTOR_DEBUG       true
#define DRD_TIMEOUT 1
#define DRD_ADDRESS 0
// This import must come after the define's for it
#include <ESP_DoubleResetDetector.h>

// Configures constants for the project
#define READINGS_PER_MEASUREMENT 100
// In ms (not seconds)
#define DELAY_BETWEEN_READINGS 100
#define DELAY_BETWEEN_SLEEPS 60000
#define PIN1 4
#define PIN2 5

// Setup global objects for imported libraries.
ESP8266WiFiMulti WiFiMulti;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
DoubleResetDetector* drd;
SoftwareSerial mySerial(PIN1, PIN2);
TFmini tfmini;

// Variables used for configuration via the web interface.
int startHour = 0;
int startMinutes = 0;
int endHour = 0;
int endMinutes = 0;
int minDistance = 0;

char startHourStr[5] = "01";
char startMinutesStr[5] = "00";
char endHourStr[5] = "06";
char endMinutesStr[5] = "30";
char minDistanceStr[5] = "1";
char webhookURLAlert[100] = "YOUR_WEBHOOK_URL";
char webhookURLMeasure[100] = "YOUR_WEBHOOK_URL";

// Used to indicate when new configurations have been entered.
bool shouldSaveConfig = false;
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}


// Runs on startup
void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("mounting FS...");

  loadConfig();
  runWifiManager();

  Serial.println("local ip");
  Serial.println(WiFi.localIP());

  Serial.print("Start Hour: ");
  Serial.println(startHour);
  Serial.print("Start Minutes: ");
  Serial.println(startMinutes);
  Serial.print("End Hour: ");
  Serial.println(endHour);
  Serial.print("End Minutes: ");
  Serial.println(endMinutes);
  Serial.print("MinDistance: ");
  Serial.println(minDistance);
  Serial.print("Webhook URL Alert: ");
  Serial.println(webhookURLAlert);
  Serial.print("Webhook URL Measure: ");
  Serial.println(webhookURLMeasure);

  timeClient.begin();

  mySerial.begin(TFmini::DEFAULT_BAUDRATE);
  tfmini.attach(mySerial);
  tfmini.setOutputDataUnit(TFmini::OutputDataUnit::MM);
}

void loadConfig() {
  // Read the file config.json from the onboard memory to load existing configurations.
  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonDocument doc(256);
        auto error = deserializeJson(doc, buf.get());
        serializeJson(doc, Serial);
        if (!error) {
          Serial.println("\nparsed json");

          startHour = doc["startHour"];
          startMinutes = doc["startMinutes"];
          endHour = doc["endHour"];
          endMinutes = doc["endMinutes"];
          minDistance = doc["minDistance"];
          itoa(startHour, startHourStr, 10);
          itoa(startMinutes, startMinutesStr, 10);
          itoa(endHour, endHourStr, 10);
          itoa(endMinutes, endMinutesStr, 10);
          itoa(minDistance, minDistanceStr, 10);
          strcpy(webhookURLAlert, doc["webhookURLAlert"]);
          strcpy(webhookURLMeasure, doc["webhookURLMeasure"]);

        } else {
          Serial.println("failed to load json config");
        }
        configFile.close();
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read
}

void runWifiManager() {
  WiFiManagerParameter custom_startHour("startHour", "startHour", startHourStr, 10);
  WiFiManagerParameter custom_startMinutes("startMinutes", "startMinutes", startMinutesStr, 10);
  WiFiManagerParameter custom_endHour("endHour", "endHour", endHourStr, 10);
  WiFiManagerParameter custom_endMinutes("endMinutes", "endMinutes", endMinutesStr, 10);
  WiFiManagerParameter custom_minDistance("minDistance", "minDistance", minDistanceStr, 10);
  WiFiManagerParameter custom_webhookURLAlert("webhookURLAlert", "webhookURLAlert", webhookURLAlert, 100);
  WiFiManagerParameter custom_webhookURLMeasure("webhookURLMeasure", "webhookURLMeasure", webhookURLMeasure, 100);

  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  // Check if the reset button was clicked twice.  If so, clear the wifi parameters so we can re-configure the settings.
  drd = new DoubleResetDetector(DRD_TIMEOUT, DRD_ADDRESS);
  if (drd->detectDoubleReset()) {
    wifiManager.resetSettings();
  }

  wifiManager.setSaveConfigCallback(saveConfigCallback);

  // Set up parameters that can be configured in the Wifi Manager
  wifiManager.addParameter(&custom_startHour);
  wifiManager.addParameter(&custom_startMinutes);
  wifiManager.addParameter(&custom_endHour);
  wifiManager.addParameter(&custom_endMinutes);
  wifiManager.addParameter(&custom_minDistance);
  wifiManager.addParameter(&custom_webhookURLAlert);
  wifiManager.addParameter(&custom_webhookURLMeasure);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("SnowMeasure")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");

  //read updated parameters
  startHour = atoi(custom_startHour.getValue());
  startMinutes = atoi(custom_startMinutes.getValue());
  endHour = atoi(custom_endHour.getValue());
  endMinutes = atoi(custom_endMinutes.getValue());
  minDistance = atoi(custom_minDistance.getValue());
  strcpy(webhookURLAlert, custom_webhookURLAlert.getValue());
  strcpy(webhookURLMeasure, custom_webhookURLMeasure.getValue());


  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonDocument doc(1024);
    doc["startHour"] = startHour;
    doc["startMinutes"] = startMinutes;
    doc["endHour"] = endHour;
    doc["endMinutes"] = endMinutes;
    doc["minDistance"] = minDistance;
    doc["webhookURLAlert"] = webhookURLAlert;
    doc["webhookURLMeasure"] = webhookURLMeasure;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    serializeJson(doc, Serial);
    serializeJson(doc, configFile);
    configFile.close();
    //end save
  }
}

double getDistance() {
  double max_result = -1;
  for (int i = 0; i < READINGS_PER_MEASUREMENT; i++) {
    if (tfmini.available()){
      double result = tfmini.getDistance();
      Serial.print("Measured: ");
      Serial.println(result);
      if(result > max_result){
        max_result = result;
      }
    }
    delay(DELAY_BETWEEN_READINGS);
    
  }
  Serial.print("Max Distance measured: ");
  Serial.println(max_result);
  return max_result;
}

int getHour() {
  return timeClient.getHours();
}

int getMinute() {
  return timeClient.getMinutes();
}

int state = 0; // 0-Waiting for first measurement.  1-Waiting for second measurement.  2-Waiting to reset (midnight)
double start_distance = 10000;
double end_distance = 0;

void trySendAlert(int hour, int minute, double current_distance) {
  Serial.print("Checking status at: ");
  Serial.print(hour);
  Serial.print(":");
  Serial.println(minute);
  Serial.print("State: ");
  Serial.println(state);

  boolean past_start = hour >= startHour && minute >= startMinutes;
  boolean past_end = hour >= endHour && minute >= endMinutes;

  if (state == 0 && past_start && !past_end) {
    start_distance = current_distance;
    state = 1;
    Serial.println("Advancing from 0 -> 1");
  }

  if (state == 1 && past_end) {
    end_distance = current_distance;
    double delta_distance = start_distance - end_distance;
    if (delta_distance > minDistance) {
      sendAlert(end_distance - start_distance);
    }
    state = 2;
    Serial.println("Advancing from 1->2");
  }

  if (state == 2 && !past_end) {
    state = 0;
    Serial.println("Advancing from 2->1");
  }
}

void sendAlert(double distance) {
  sendMessage(distance, webhookURLAlert);
}

void sendMeasurement(double distance) {
  sendMessage(distance, webhookURLMeasure);
}

void sendMessage(double distance, char* webhookURL) {
  if ((WiFiMulti.run() == WL_CONNECTED)) {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;

    char bigBuf[200];
    sprintf_P(bigBuf, "%s%s%.02f%s%s%s%s", webhookURL, "?value1=", distance, "&value2=", "--", "&value3=", "--");

    Serial.println();
    Serial.print("Making request: ");
    Serial.println(bigBuf);

    Serial.print("[HTTP] begin...\n");
    if (http.begin(client, bigBuf)) {

      Serial.print("[HTTP] GET...\n");
      // start connection and send HTTP header
      int httpCode = http.GET();

      // httpCode will be negative on error
      if (httpCode > 0) {
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTP] GET... code: %d\n", httpCode);

        // file found at server
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          String payload = http.getString();
          Serial.println(payload);
        }
      } else {
        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
      }

      http.end();
    } else {
      Serial.printf("[HTTP} Unable to connect\n");
    }
  }
}

void loop() {
  // put your main code here, to run repeatedly:
  drd->loop();
  timeClient.update();

  double current_measurement = getDistance();
  sendMeasurement(current_measurement);
  trySendAlert(getHour(), getMinute(), current_measurement);
  
  delay(DELAY_BETWEEN_SLEEPS);
}
