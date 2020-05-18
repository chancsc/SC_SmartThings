//https://github.com/gopherxx/smartthings-sensor/blob/master/smartthings-sensor/smartthings-sensor.ino

#include <ESP8266WiFi.h>

//set pin for moisture sensor, A0 is an analog pin on nodemcu, to read analog value from moisture sensor
#define sensorPin   A0

const char* ssid = "dongdaemum";
const char* password = "1111111111";
const unsigned int serverPort = 80; // port to run the http server on

// Smartthings hub information
IPAddress hubIp(192, 168, 0, 8); // smartthings hub ip
const unsigned int hubPort = 39500; // smartthings hub port

// default time to report temp changes
//int reportInterval = 5; //in mins

//byte oldSensorState, currentSensorState;
//long debounceDelay = 10;    // the debounce time; increase if false positives

WiFiServer server(serverPort); //server
WiFiClient client; //client
String readString;

int checkFreq = 5; //in mins

void setup()
{
  Serial.begin(9600);
  delay(1);

  pinMode(sensorPin, INPUT);

  //power saving start
  WiFi.mode( WIFI_OFF);
  WiFi.forceSleepBegin();
  delay(1);
  //power saving end
}

void wificonnect(){
  IPAddress ip( 192, 168, 0, 20 );
  IPAddress gateway( 192, 168, 0, 1 );
  IPAddress subnet( 255, 255, 255, 0 );

  WiFi.forceSleepWake();
  delay(1);
  //Disable the WiFi persistence.  The ESP8266 will not load and save WiFi settings in the flash memory.
  WiFi.persistent( true );
  WiFi.mode (WIFI_STA);
  WiFi.config( ip, gateway, subnet );
  WiFi.begin(ssid, password);
  Serial.print("Connecting to wifi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected!");
  Serial.print( "Assigned IP address: " );
  Serial.println( WiFi.localIP() );
}

void loop()
{  
  wificonnect();

  if (sendNotify()) {
    Serial.println("sendNotify completed");
  }
  else {
    Serial.println("sendNotify have issue");
  }

  Serial.print("Deep sleep for : ");
  Serial.print(checkFreq);
  Serial.println(" mins");
  
  WiFi.disconnect( true );
  delay( 1 );
  ESP.deepSleep(checkFreq * 60 * 1000000, WAKE_RF_DISABLED);   //deepSleep is microseconds 1 sec = 1000000
  //ESP.deepSleep(30e6, WAKE_RF_DISABLED);  //30 secs
  delay(100);
}

int getMois() {
  int mois;
  mois = analogRead(sensorPin);
//  Serial.print("Sensor value: ");
//  Serial.println(mois);
  return mois;
}

// send json data to client connection
void sendMoisJSONData(WiFiClient client) {
  Serial.println("sendMoisJSONData");
  String tempString = String(getMois());
  Serial.print("Sensor value: ");
  Serial.println(tempString);
  //Use for testing without sensore
  //String tempString = "00";
  client.println(F("CONTENT-TYPE: application/json"));
  client.print(F("CONTENT-LENGTH: "));
  client.println(28+tempString.length());
  client.println();
  client.print(F("{\"name\":\"moisture\",\"value\":"));
  client.print(tempString);
  client.println(F("}"));
  // 31 chars plus temp;
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
    sendMoisJSONData(client);
    
    Serial.println("sendNotify completed");
  }
  else {
    //connection failed
    Serial.println("Connection to hub failed");
    returnStatus = 0;
  }

  // read any data returned from the POST
//  while(client.connected() && !client.available()) delay(1); //waits for data
//  while (client.connected() || client.available()) { //connected or data available
//    char c = client.read();
//  }
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
  return returnStatus;
}
