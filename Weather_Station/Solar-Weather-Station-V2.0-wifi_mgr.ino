//----------------------------------------------------------------------------------------------------
//  Project Name : Solar Powered WiFi Weather Station V2.0 (original)
//  Authors: Keith Hungerford and Debasish Dutta
//  Website : www.opengreenenergy.com (https://www.instructables.com/id/Solar-Powered-WiFi-Weather-Station-V20/)
//  This code is derived from the example code of farmerkeith_BMP280.h library  
//  Download the library from https://github.com/farmerkeith/BMP280-library
//  Main microcontroller (ESP8266) and BME280 both sleep between measurements
//  BME280 is used in single shot mode ("forced mode")
//  Measurement read command is delayed,
//  By repeatedly checking the "measuring" bit of status register (0xF3) until ready

/*                                                                                                                      
   Release Note:
   =============
1. Connect to Wi-Fi, and upload the data to either Blynk App  or Thingspeak
2. Monitoring Weather parameters like Temperature, Pressure, Humidity and Altitude.
3. Extra Ports to add more Weather Sensors like UV Index, Light and Rain Guage etc.
4. Remote Battery Status Monitoring
5. Using Sleep mode to reduce the energy consumed

  //further modification by Soon Chye
6. v2.1 Modify to add GY-1145 UV sensor & DHT22 & do it as option in code (CSC)
7. v2.2 Add tzapu/WifiManger, allow user to enter Blynk authentication code or Thinkspeak API,
        only allow to enter either one, if both detected, only use the Blynk
8. v2.3 Add Double Reset Detector - to detect double press on reset button and wipe the wifi settings
9. v2.4 Define wake up interval - to save battery, default 10 mins, during wifi setup
10. v2.5 replace DoubleResetDetector with ESP_DoubleResetDetector (bug randomly caused wifi settings disappeared)
11. v.2.51 remove doubleresetdetector, caused unknown problem 

Todo
* Option to specify option to enable (Y/N) DHT22, UV sensor during wifi setup
* add OTA update function
*/
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <ESP8266WiFi.h>        // do we need this? Not when using Blynk
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <Wire.h>
#include <farmerkeith_BMP280.h> // library, download from https://github.com/farmerkeith/BMP280-library
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <BlynkSimpleEsp8266.h>

//#include <DoubleResetDetector.h>   //<<-- got bug, auto wipe of wifi manager settings happened
//#define ESP_DRD_USE_LITTLEFS    true    //false
//#define DOUBLERESETDETECTOR_DEBUG       true  //false
//#include <ESP_DoubleResetDetector.h>

// configuration constants
const bool bme280Debug = 0; // controls serial printing
// set to 1 to enable printing of BME280 or BMP280 transactions

// configure period between reports
//const long measurementInterval = 30000;  // measurement interval in ms

// for UV index, set to true if using
const bool UVSensor = false;
#include "Adafruit_SI1145.h" // for UV index
Adafruit_SI1145 uv = Adafruit_SI1145(); // create object for UVSensor  

// configuration control- either Blynk or Thingspeak
String App = "";  //Enable based on value (Blynk authentication code or ThinkSpeak Api Key enter by user)

#define BLYNK_PRINT Serial    // Comment this out to disable prints and save space

bme280 bme0 (0, bme280Debug) ; // creates object bme0 of type bme280, base address, or BMP280

// for DHT22, set to true if using
const bool dht22Sensor = false; // needed when using BMP280, BMP280 don't have humidity info
#include "DHT.h"
#define DHTPIN D5
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE); // create object for DHT22

// You should get Auth Token in the Blynk App.
// Go to the Project Settings (nut icon).

// Thingspeak Write API
const char* server = "api.thingspeak.com";

//define your default values here, if there are different values in config.json, they are overwritten.
char Blynk_Token[33] = "";
char ThinkSpeak_Api_Key[17] = "";
char strSleep_Time[3] = "10";

//Sleep freq in mins
int Sleep_Time = 10;

//flag for saving data using wifi manager page
bool shouldSaveConfig = false;

float UVindex = 0;
float Vis = 0;
float IR = 0;

float h = 0; //humidity
float t = 0; //temperature in C
float f = 0; //temperature in F

float hif = 0; // Compute heat index in Fahrenheit
float hic = 0; // Compute heat index in Celsius

#include "PTHsleep.h" // tab file

//WiFiServer server(serverPort); //server
WiFiClient client; //client
String readString;

// Number of seconds after reset during which a 
// subseqent reset will be considered a double reset.
//#define DRD_TIMEOUT 10

// RTC Memory Address for the DoubleResetDetector to use
//#define DRD_ADDRESS 0

//DoubleResetDetector* drd;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void setup() {
  Serial.begin(115200);
  //  Serial.begin(9600); // use this if you get errors with the faster rate
  Serial.println("\nStart of Solar WiFi Weather Station V2.2");

  //read from FS
  readFS();

  //connect to wifi
  wificonnect();

  byte temperatureSamples = pow(2, osrs_t - 1);
  byte pressureSamples = pow(2, osrs_p - 1);
  byte humiditySamples = pow(2, osrs_h - 1);

  // Wire.begin(); // initialise I2C protocol - not needed here since it is in bmp library
  bme0.begin(osrs_t, osrs_p, 1, 0, 0, 0, osrs_h);
  // parameters are (osrs_t, osrs_p, mode, t_sb, filter, spi3W_en, osrs_h)
  // see BME280 data sheet for definitions
  // this is single shot mode with no filtering

  measurementEvent();

  //  eventCounter ++;
  //  saveCounter(eventCounter);         // this also puts bme0 to sleep
  bme0.updateF4Control16xSleep(); // use instead of saveCounter if counter is not required

  // Call the double reset detector loop method every so often,
  // so that it can recognise when the timeout expires.
  // You can also call drd.stop() when you wish to no longer
  // consider the next reset as a double reset.
//  drd->loop();
  
  goToSleep();
} // end of void setup()


void loop() {
} // end of void loop()

//gets called when WiFiManager enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
//  //entered config mode, make led toggle faster
//  ticker.attach(0.2, tick);
}

void readFS(){
  //read configuration from FS json
    Serial.println("mounting FS...");

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
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");
          strcpy(Blynk_Token, json["Blynk_Token"]);
          strcpy(ThinkSpeak_Api_Key, json["ThinkSpeak_Api_Key"]);

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

void wificonnect(){
  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_Blynk_Token("Blynk", "Blynk Authentication Token", Blynk_Token, 33);
  WiFiManagerParameter custom_ThinkSpeak_Api_Key("ThinkSpeak", "ThinkSpeak Api Key", ThinkSpeak_Api_Key, 17);
  WiFiManagerParameter custom_Sleep_Time("strSleep_Time", "Sleep Time", strSleep_Time, 2);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

//  drd = new DoubleResetDetector(DRD_TIMEOUT, DRD_ADDRESS);
//  if (drd->detectDoubleReset()) 
//  {
//    Serial.println("Double Reset Detected");
//    wifiManager.resetSettings();
//    ESP.reset();
//    delay(1000);
//  } 
//  else 
//  {
//    Serial.println("No Double Reset Detected");
//  }

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //add all your parameters here
  wifiManager.addParameter(&custom_Blynk_Token);
  wifiManager.addParameter(&custom_ThinkSpeak_Api_Key);
  wifiManager.addParameter(&custom_Sleep_Time);

  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);

  //timeout - this will quit WiFiManager if it's not configured in 3 minutes, causing a restart
  wifiManager.setConfigPortalTimeout(60);

  if (!wifiManager.autoConnect("Weather Station", "configme")) {
    Serial.println("failed to connect to Wifi, AP mode started");
//    drd->loop(); //don't trigger double press reset
    goToSleep(); 
//    ESP.reset();
//    delay(1000);
  }

  Serial.println("Wifi Connected");

  //read updated parameters
  strcpy(Blynk_Token, custom_Blynk_Token.getValue());
  strcpy(ThinkSpeak_Api_Key, custom_ThinkSpeak_Api_Key.getValue());
  strcpy(strSleep_Time, custom_Sleep_Time.getValue());

  if (strlen(Blynk_Token) == 0 && strlen(ThinkSpeak_Api_Key) == 0) {
      Serial.println("Failed to get Blynk Token or ThinkSpeak Api Key from FS, go back to setup mode..");
      wifiManager.resetSettings();
      ESP.reset();
      delay(1000);
    }

  if (strlen(strSleep_Time) == 0) {
    String tmp = "10";
    tmp.toCharArray(strSleep_Time, 3);
  }
  else
  {
    Sleep_Time = atoi(strSleep_Time);
  }

  Serial.print("Sleep_Time = ");
  Serial.println(Sleep_Time);

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["Blynk_Token"] = Blynk_Token;
    json["ThinkSpeak_Api_Key"] = ThinkSpeak_Api_Key;
    json["Sleep_Time"] = strSleep_Time;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }

  if (strlen(Blynk_Token) > 0) {
    App = "Blynk";
    
      //configure blynk
      delay(1000);
      Blynk.config(Blynk_Token);
      bool result = Blynk.connect(180);
      if (result != true) {
        Serial.println("Blynk Connection Fail");
        //needs testing on whether this line should be commented out:
        //wifiManager.resetSettings();
//        drd->loop(); //don't trigger double press reset
        ESP.reset();
        delay (100);
      }
      else  {
        Serial.println("Blynk Connected");
      }

      //Connect to blynk using an already-open internet connection and preset configuration
      if(!Blynk.connect()) {
        Serial.println("Blynk connection timed out.");
//        drd->loop(); //don't trigger double press reset
        ESP.reset();
        delay (100);
      }
      
      Blynk.run();
      
  }
  else if (strlen(ThinkSpeak_Api_Key) > 0) {
    App = "Thingspeak";
  }

  Serial.print("\nApp = ");
  Serial.println(App);

}
