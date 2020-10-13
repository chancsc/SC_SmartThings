//Project: Monitor water level (HC-SR04P) of Balacing tank and the Temperature of Water (DS18b20) / Ambient (DHT22) 
//         Send alert via Blynk app if water level is too high, or water temperature is higher than 28 C

/*
//https://community.blynk.cc/t/water-tank-level-indicator-with-low-level-warning-notifications/26271

//v1.0 - Wifi Manager + Blynk
//v1.1 - added deep sleep function - https://community.blynk.cc/t/esp-deep-sleep/5622/13
//v1.2 - added run time change of deep sleep interval via UI setup
//v1.3 - 

//**** Blynk Virtual Pin Assignments****
// V0 - Water temperature alert
// V1 - Tank Height (cm)
// V2 - Sensor distance from Tank (cm)
// V3 - Temperature
// V4 - Battery voltage
// V6 - Numeric Widget - Sleep interval (mins)
// V9 - Level V widget - Water Level %
//V10 - Numeric Widget - Level exceed then will trigger Alert
//V11 - Button Widget -  To enable / disable Alert
//V12 - Distance - height from sensor to water level

Hardware: 
1. Wemos D1 Pro
2. DS18B20 Temperature Sensor
3. Ultra Sonic Sensor
4. 4.7k resistor - put between D4 and VCC


Ultra sonic Sensor: Vcc - D7, Gnd - Gnd, Echo - D6, Trigger - D5
DS18820 - Vcc - D8, Gnd - Gnd, Data - D4

Todo
----
- if sonar sensor out of range --> don't save but Alert
- water temperature alert

*/

#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

char auth[] = "<blynk toeken>";

char ssid[] = "wifi ssid";
char pass[] = "wifi pwd";

BlynkTimer timer;

//HC-SR04 ultra sonic sensor
#define trigPin D5  // Trigger Pin
#define echoPin D6  // Echo Pin
#define RadarPowerPin D7 //GPIO12 = D6, GPIO14 = D5, GPIO15 = D8

//DS18820 temperature sensor
#define sensorPin D4
#define TempPowerPin D8

#define filterSamples   15              // filterSamples should  be an odd number, no smaller than 3

int alertLevelCounter = 0; //Counter starts at zero
//int volume1; // for smoothing algorithum
double wLevel; // for smoothing algorithum
int levelAlert;
int wateralert = 0;
//int sensSmoothArray1 [filterSamples];   // array for holding raw sensor values for sensor1 
int smoothDistance;                     // variables for sensor1 data

//double alertInMin = 0.5;                //Alert time following alert water level being reached (0.5 = 30secs)
//const int alertDuration = 2;            //Alert sent every 2 minutes

int checkFreq = 2;                //how often run the check / sleep for how long before wake up, in mins
int tankHeight = 100;             //tank's height
int heightFrTank = 20;            //sensor distance from tank
long duration, distance;          // Duration used to calculate distance

//init DS11820
OneWire oneWire(sensorPin);
DallasTemperature DS18B20(&oneWire);
float temp;
float volt;

BLYNK_CONNECTED() { // runs once at device startup, once connected to server.
  Blynk.syncVirtual(V1);
  Blynk.syncVirtual(V2);
  Blynk.syncVirtual(V6);
  Blynk.syncVirtual(V10);
  Blynk.syncVirtual(V11);
}

BLYNK_WRITE(V1) {
  tankHeight = param.asInt();
  Serial.print("\nTank's Height: ");
  Serial.println(tankHeight);
  Serial.println();
} 

BLYNK_WRITE(V2) {
  heightFrTank = param.asInt();
  Serial.print("Height from Tank: ");
  Serial.println(heightFrTank);
  Serial.println();
} 

BLYNK_WRITE(V6) {
  checkFreq = param.asInt();
  Serial.print("Sleep Interval: ");
  Serial.println(checkFreq);
  Serial.println();
} 

BLYNK_WRITE(V10) {
  levelAlert = param.asInt();
  Serial.print("Water Level Alert: ");
  Serial.println(levelAlert);
  Serial.println();
} 
   
BLYNK_WRITE(V11) { 

  int alertOff = param.asInt();

  if(alertOff == 0) {
    resetWaterLevelAlert();
    wateralert = 1;
    Serial.print("Level Alert is OFF.");
    Serial.println();
  }
  else {
    resetWaterLevelAlert();
    wateralert = 0;
    Serial.print("Level Alert is ON.");
    Serial.println();
  }

}

void resetWaterLevelAlert() {
  alertLevelCounter = 0;
  wateralert = 0;
}

void checkWaterLevel() {
  Serial.print("Water Level (%) = ");
  Serial.println(wLevel);
  Serial.print("Level Alert (%) = ");
  Serial.println(levelAlert);
  Serial.print("Alert (1 = off, 0 = on) = ");
  Serial.println(wateralert);

  if(wLevel > levelAlert && wateralert == 0){
//    alertLevelCounter += 1;
    Blynk.notify("Water Tank Level Alert"); // send push notification to blynk app

  }

//  if(alertLevelCounter > alertInMin * 60 && wateralert == 0){
//    Blynk.notify("Water Tank Level Alert"); // send push notification to blynk app
//    wateralert = 1;
//    resetWaterLevelAlert();
//  }
}

void MeasureCmForSmoothing() {

  //take multiple reading

  digitalWrite(RadarPowerPin, HIGH);  //provide power
  delay(1000);
  
    smoothDistance = mesureDistance();
    
  digitalWrite(RadarPowerPin, LOW);  //turn off power
  delay(100);
  
  Serial.print("Smoothed Distance: ");
  Serial.println(smoothDistance);

  //tank height 100 cm, sensor is 20 cm above the tank
  wLevel = (tankHeight + heightFrTank - smoothDistance);

  Serial.print("Water Level (cm): ");
  Serial.println(wLevel);

  //convert to %
  wLevel = wLevel / tankHeight * 100;

  Serial.print("Water Level (%): ");
  Serial.println(wLevel);

  Blynk.virtualWrite(V12, smoothDistance); // virtual pin
  Blynk.virtualWrite(V9, wLevel); // virtual pin
  delay(1000);
} 

void UploadMeasureCmForSmoothing() {

  Blynk.virtualWrite(V3, temp); // Water level %
  Blynk.virtualWrite(V4, volt); // battery 
  Blynk.virtualWrite(V9, wLevel); // Water level %
  Blynk.virtualWrite(V12, smoothDistance); // virtual pin
  
}


void setup() {
  Serial.begin(115200);
  Blynk.begin(auth, ssid, pass);
  
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  //pin to provide power to ultra sonic sensor
  pinMode(RadarPowerPin, OUTPUT);

  //pin to provide power to temperature sensor
  pinMode(TempPowerPin, OUTPUT);
  
  Serial.println("Measure Water Temperature");
  measurementEvent();

  Serial.println("\nMeasure Water Level");
  MeasureCmForSmoothing();

  Serial.println("Upload Data");
  UploadMeasureCmForSmoothing();

  Serial.println("check WaterLevel Alert");
  checkWaterLevel();

  goToSleep();

}
 
void loop() {

}

int digitalSmooth(int rawIn, int *sensSmoothArray){     // "int *sensSmoothArray" passes an array to the function - the asterisk indicates the array name is a pointer
int j, k, temp, top, bottom;
long total;
static int i;
// static int raw[filterSamples];
static int sorted[filterSamples];
boolean done;

i = (i + 1) % filterSamples;    // increment counter and roll over if necc. -  % (modulo operator) rolls over variable
sensSmoothArray[i] = rawIn;                 // input new data into the oldest slot

// Serial.print("raw = ");

 for (j=0; j<filterSamples; j++){     // transfer data array into anther array for sorting and averaging
     sorted[j] = sensSmoothArray[j];
 }

   done = 0;                // flag to know when we're done sorting              
 while(done != 1){        // simple swap sort, sorts numbers from lowest to highest
      done = 1;
        for(j = 0; j < (filterSamples - 1); j++){
           if(sorted[j] > sorted[j + 1]){     // numbers are out of order - swap
             temp = sorted[j + 1];
             sorted [j+1] =  sorted[j] ;
             sorted [j] = temp;
             done = 0;
           }
        }
  }

   // throw out top and bottom 15% of samples - limit to throw out at least one from top and bottom
   //  bottom = max(((filterSamples * 15)  / 100), 1); 
   //  top = min((((filterSamples * 85) / 100) + 1  ), (filterSamples - 1));   // the + 1 is to make up for asymmetry caused by integer rounding
   bottom = max(((filterSamples * 20)  / 100), 1); 
   top = min((((filterSamples * 80) / 100) + 1  ), (filterSamples - 1));   // the + 1 is to make up for asymmetry caused by integer rounding
   k = 0;
   total = 0;
  for( j = bottom; j< top; j++){
 total += sorted[j];  // total remaining indices
 k++; 
 //Serial.print(sorted[j]); 
 //Serial.print("   "); 
 //terminal.print(sorted[j]); 
 //terminal.println("   ");
  
  }
   //terminal.print("average: ");
   //terminal.println(total/k);
   //terminal.flush();
   //Serial.println();
   //Serial.print("average = ");
   //Serial.println(total/k);
  return total / k;    // divide by number of samples
}

int mesureSingleDistance() {
      long duration, distance;

      digitalWrite(trigPin, LOW);
      delayMicroseconds(2);
      digitalWrite(trigPin, HIGH);
      delayMicroseconds(10);
      digitalWrite(trigPin, LOW);
      duration = pulseIn(echoPin, HIGH);
      distance = (duration/2) / 29.1;

      Serial.print("Distance: ");
      Serial.println(distance);

      return distance;
  }

/**
 * mresure distnce average delay 10ms
 */
int mesureDistance() {
    const int ULTRASONIC_MIN_DISTANCE = heightFrTank;  //20
    const int ULTRASONIC_MAX_DISTANCE = tankHeight + heightFrTank;  //120

      int i, total_count = 0, total_distance = 0, distance, values[10], average, temp, new_count = 0;

      for(i=0; i<5; i++){
          distance = mesureSingleDistance();
          //check if out of range
          if(distance > ULTRASONIC_MIN_DISTANCE && distance < ULTRASONIC_MAX_DISTANCE) {
              total_distance += distance;
              values[total_count++]=distance;
              Serial.printf("Value %d...\n", distance);
          }
      }

      Serial.print("total_count = ");
      Serial.println(total_count);
      if(total_count > 0) {

          average = total_distance / total_count;
          total_distance = 0;
          Serial.printf("Average %d...\n", average);

          for(i=0; i<total_count; i++){
              temp = (average - values[i])*100/average;
              Serial.printf("Analyze %d....%d...\n", values[i],temp);
              if(temp < 10 && temp > -10){
                  total_distance += values[i];
                  new_count++;
              }
          }

          if(new_count > 0) {
//              Serial.print("total_distance = ");
//              Serial.println(total_distance);
//              Serial.print("new_count = ");
//              Serial.println(new_count);
              return total_distance / new_count;
          }
      }
      Serial.println("Sonar sensor out of range");
      return 0;  //return 0 meaning the sensor out of range
  }

void goToSleep() {
  // calculate required sleep time and go to sleep

  Serial.print(" Going to sleep now for ");
  Serial.print(checkFreq);
  Serial.println(" mins");

  ESP.deepSleep(checkFreq * 60 * 1000000); //deepSleep is microseconds 1 sec = 1000000
  delay(100);

} // end of void goToSleep()

void measurementEvent() {


  //******Measure Water Temperature*********************************************
  digitalWrite(TempPowerPin, HIGH);  //provide power
  delay(2000);
  
  DS18B20.begin();
  DS18B20.requestTemperatures(); 
  temp = DS18B20.getTempCByIndex(0); // Celcius
  Serial.print("Temperature (C) = ");
  Serial.println(temp);

  digitalWrite(TempPowerPin, LOW);  //turn off power
  delay(100);

  //******Battery Voltage Monitoring*********************************************
  //depend on 220k or 100k resistor used, toggle between this 2 calculation

  // using 220k
  // Voltage divider R1=100k, R2 = (220k + 100k) + 220k = 540k
  // Calculation to get the factor: 4.2v * 100/540 = 0.7777, 0.7777 * y = 4.2, y = 5.4
//  float calib_factor = 5.28; // change this value to calibrate the battery voltage, 5.28 is so call calibrated
//  float calib_factor = 5.4;

  // using 100k
  // Voltage divider R1 = 100k, R2 = (220k + 100k) + 100k = 420k; 
  // Calculation to get the factor: 4.2v * 100/420 = 1, 1 * y = 4.2, y = 4.2
  float calib_factor = 4.2;
  
  unsigned long raw = analogRead(A0);
//  Serial.print("raw analog = ");
//  Serial.println(raw);
  float volt = raw * calib_factor/1024; 
  
  Serial.print( "\nVoltage = ");
  Serial.print(volt, 2); // print with 2 decimal places
  Serial.println (" V");
  
 //*******************************************************************************
}
