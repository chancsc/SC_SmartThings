/*
ESP8266 Soil Moisture Sensor
----------------------------
Version history:
1.0.0 - integration with Blynk
1.0.1 - integration with SmartThings (dropped Blynk)
1.0.1 - added Deep Sleep feature (30 mins wake up interval)
1.0.2 - added WIFI manager feature
1.0.3 - added OTA update capability
1.0.4 - control power supply to sensor via GPIO12 = D6
1.0.5 - accept SmartThings hub IP & port setup during Wifi setup. Execute everything in the setup, as loop is not req
1.0.6 - minor code update, to check if st_port and st_ip is not empty, otherwise go back to setup mode

Todo:
1. Ability to define Wake up freq at runtime
2. Blink green LED when upload success, red when failed
3. Customize URL for OTA

References:
===========
Using Blynk & Moisture Sensor:
    https://www.viralsciencecreativity.com/post/nodemcu-esp8266-soil-moisture-water-plants-notification-blynk
on using resistor to convert sensor analog output to below 1V:
    https://www.reddit.com/r/AskElectronics/comments/c3drqc/using_a_voltage_divider_with_a_soil_moisture/
ST integration: 
    https://github.com/gopherxx/smartthings-sensor/blob/master/smartthings-sensor/smartthings-sensor.ino
Deep Sleep ref: 
    https://www.open-homeautomation.com/2016/06/12/battery-powered-esp8266-sensor/
OTA Update (must ensure set flash size to at least 1M SPIFFS): 
    https://www.bakke.online/index.php/2017/06/02/self-updating-ota-firmware-for-esp8266/

*/

#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <ESP8266WiFi.h>
#include <DNSServer.h>
//#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

//for LED status
//#include <Ticker.h>
//Ticker ticker;

//set pin for moisture sensor, A0 is an analog pin on nodemcu, to read analog value from moisture sensor
#define sensorPin A0

//provide power to the sensor, so it's not always on as the sensor will eroded fast
#define powerPin 12 //GPIO12 = D6

//D2/GPIO04 Green (success), D1/GPIO05 (failed) Red
#define greenLed 04
#define redLed 05

//check frequency
int checkFreq = 10; //in mins - wake interval

//current firmware version
const int FW_VERSION = 1005;

// Smartthings hub information
IPAddress hubIp; // smartthings hub ip
int hubPort = 39500; // smartthings hub port

int char2int (char *array, size_t n); 

//define your default values here, if there are different values in config.json, they are overwritten.
char st_hub[15];
char st_port[6] = "39500";

//flag for saving data using wifi manager page
bool shouldSaveConfig = false;

//firmware location
const char* fwUrlBase = "http://192.168.0.26/esp8266/ota/";

//WiFiServer server(serverPort); //server
WiFiClient client; //client
String readString;

void setup() {
    Serial.begin(115200);
    delay(1);

    Serial.println("ESP8266 woke up");

    //moisture sensor pin
    pinMode(sensorPin, INPUT);
    //pin to provide power to sensor
    pinMode(powerPin, OUTPUT);
    //pin to flash the upload status
    pinMode(greenLed, OUTPUT);
    pinMode(redLed, OUTPUT);

    //read from FS
    readFS();

    //connect to wifi
    wificonnect();

    //check for firmware update
    checkFWUpdates();

    //send message to ST
    if (sendNotify()) {
      Serial.println("sendNotify completed");
      sendStatusToWeb("sendNotify_completed");
      digitalWrite(greenLed, HIGH);   // turn the LED on (HIGH is the voltage level)
      delay(100);                       // wait for a second
      digitalWrite(greenLed, LOW);    // turn the LED off by making the voltage LOW
    }
    else {
      Serial.println("sendNotify have issue");

      sendStatusToWeb("sendNotify_failed");
      digitalWrite(redLed, HIGH);   // turn the LED on (HIGH is the voltage level)
      delay(100);                       // wait for a second
      digitalWrite(redLed, LOW);    // turn the LED off by making the voltage LOW
    }

    // Deep sleep mode for 30 seconds
    //the ESP8266 wakes up by itself when GPIO 16 (D0 in NodeMCU/Wemos board) is connected to the RESET pin
    Serial.print("I'm going into deep sleep mode for ");
    Serial.print(checkFreq);
    Serial.println(" mins");

    sendStatusToWeb("sleeping_now");
  
    //ESP.deepSleep(20e6); 
    ESP.deepSleep(checkFreq * 60 * 1000000, WAKE_RF_DEFAULT);   //deepSleep is microseconds 1 sec = 1000000
    delay(100);
}

void loop() {
    // put your main code here, to run repeatedly:
}

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
          strcpy(st_hub, json["st_hub"]);
          strcpy(st_port, json["st_port"]);

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
//  //set led pin as output
//  pinMode(BUILTIN_LED, OUTPUT);
//  // start ticker with 0.5 because we start in AP mode and try to connect
//  ticker.attach(0.6, tick);

  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_st_hub("SmartThings Hub", "SmartThings Hub IP", st_hub, 15);
  WiFiManagerParameter custom_st_port("port", "SmartThings Hub Port", st_port, 6);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //add all your parameters here
  wifiManager.addParameter(&custom_st_hub);
  wifiManager.addParameter(&custom_st_port);

  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("Soil_Moisture_Sensor", "configme")) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  }

  Serial.println("Wifi Connected");

  //read updated parameters
  strcpy(st_hub, custom_st_hub.getValue());
  strcpy(st_port, custom_st_port.getValue());

if (strlen(st_hub) == 0 || strlen(st_port) == 0) {
    Serial.println("Failed to get SmartThings Hub IP or Port from FS, go back to setup mode..");
    wifiManager.resetSettings();
    ESP.reset();
    delay(1000);
  }

  //convert IP address from string (json) to char array (for ipAddress)
  if (hubIp.fromString(st_hub)) {   // str = char * or String
    Serial.println("Successfully get the IP of ST Hub from FS");
  }

  //convert Port form string (json) to int
  size_t slen = strlen (st_port);
  hubPort = char2int(st_port, slen);
  
  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["st_hub"] = st_hub;
    json["st_port"] = st_port;
    //json["blynk_token"] = blynk_token;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }

}


int getMois() {
  int mois;

  digitalWrite(powerPin, HIGH);  //provide power
  delay(500);
  
  mois = analogRead(sensorPin);

  digitalWrite(powerPin, LOW);  //turn off power
  return mois;
}

// send json data to client connection
void sendMoisJSONData() {
  Serial.println("sendMoisJSONData");
  String tempString = String(getMois());
  Serial.print("Sensor value: ");
  Serial.println(tempString);
  //Use for testing without sensore
  //String tempString = "00";
  client.println(F("CONTENT-TYPE: application/json"));
  client.print(F("CONTENT-LENGTH: "));
  client.println(28+tempString.length());  // the 28 is the total string lenght, should find a way to auto calculate
  client.println();
  client.print(F("{\"name\":\"moisture\",\"value\":"));
  client.print(tempString);
  client.println(F("}"));

  delay(1);
}

// send data to ST
int sendNotify() //client function to send/receieve POST data.
{
  int returnStatus = 1;
  Serial.println("sendNotify");
  if (client.connect(hubIp, hubPort)) {
    client.println(F("POST / HTTP/1.1"));
    client.print(F("HOST: "));
    client.print(hubIp);
    client.print(F(":"));
    client.println(hubPort);
    sendMoisJSONData();
    
    Serial.println("sendNotify completed");
  }
  else {
    //connection failed
    Serial.println("Connection to hub failed");
    returnStatus = 0;
  }

    int retries = 0;
    while (client.connected())
    {
      retries++;
      if( retries == 60 ){  //00 seconds
        Serial.println("Unable to get response from Hub...");
        return 0;
      }
      
      Serial.println("reading response from hub..");
      if (client.available())
      {
        String response = client.readStringUntil('\r');    // Read entire response
        Serial.print("value: ");
        Serial.println(response);
      }
      delay(1000);
    }

  delay(1);
  client.stop();
  delay(100);
  return returnStatus;
}

void checkFWUpdates() {

  String mac = getMAC();
  String fwURL = String( fwUrlBase );
  fwURL.concat( mac );
  String fwVersionURL = fwURL;
  fwVersionURL.concat( ".version" );

  Serial.println( "Checking for firmware updates." );
  Serial.print( "MAC address: " );
  Serial.println( mac );
  Serial.print( "Firmware version URL: " );
  Serial.println( fwVersionURL );

  HTTPClient httpClient;
  httpClient.begin( fwVersionURL );
//  httpClient.begin("http://192.168.0.26/esp8266/ota/BCDDC29EA245.version");
  int httpCode = httpClient.GET(); 
  if( httpCode == 200 ) {
    String newFWVersion = httpClient.getString();

    Serial.print( "Current firmware version: " );
    Serial.println( FW_VERSION );
    Serial.print( "Available firmware version: " );
    Serial.println( newFWVersion );

    int newVersion = newFWVersion.toInt();

    if( newVersion > FW_VERSION ) {
      Serial.println( "Preparing to update" );

      String fwImageURL = fwURL;
      fwImageURL.concat( ".bin" );
      t_httpUpdate_return ret = ESPhttpUpdate.update( fwImageURL );

      switch(ret) {
        case HTTP_UPDATE_FAILED:
          Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
          break;

        case HTTP_UPDATE_NO_UPDATES:
          Serial.println("HTTP_UPDATE_NO_UPDATES");
          break;
      }
    }
    else {
      Serial.println( "Already on latest version" );
    }
  }
  else {
    Serial.print( "Firmware version check failed, got HTTP response code " );
    Serial.println( httpCode );
  }
  httpClient.end();
}

String getMAC()
{
  uint8_t mac[6];
  char result[14];
  WiFi.macAddress( mac );

  snprintf( result, sizeof( result ), "%02X%02X%02X%02X%02X%02X", mac[ 0 ], mac[ 1 ], mac[ 2 ], mac[ 3 ], mac[ 4 ], mac[ 5 ] );

  return String( result );
}

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

/* convert character array to integer - this is a simplified version, assuming the char to convert is really number */
int char2int (char *array, size_t n)
{    
    int number = 0;
    int mult = 1;

    n = (int)n < 0 ? -n : n;       /* quick absolute value check  */

    /* for each character in array */
    while (n--)
    {
        /* convert digit to numeric value   */
            number += (array[n] - '0') * mult;
            mult *= 10;
    }

    return number;
}

void sendStatusToWeb(String sts)
{
  HTTPClient httpClient;
  String url = "http://192.168.0.26/esp8266/ota/";
  url.concat(sts);
  
  httpClient.begin( url );

  int httpCode = httpClient.GET();

  httpClient.end();
  
}
