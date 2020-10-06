// tab file for functions used by powerSaverPressureAndTemperatureAndHumidity
// BME280 configuration settings
const byte osrs_t = 2; // setting for temperature oversampling
// No. of samples = 2 ^ (osrs_t-1) or 0 for osrs_t==0
const byte osrs_p = 5; // setting for pressure oversampling
// No. of samples = 2 ^ (osrs_p-1) or 0 for osrs_p==0
const byte osrs_h = 5; // setting for humidity oversampling
// No. of samples = 2 ^ (osrs_h-1) or 0 for osrs_h==0

void measurementEvent() { 
    
  //*********** Measures Pressure, Temperature, Humidity, Voltage and calculates Altitude
  // then reports all of these to Blynk or Thingspeak
  
  while (bme0.readRegister(0xF3) >> 3); // loop until F3bit 3 ==0, measurement is ready
  double temperature, pressure;
  double humidity = bme0.readHumidity (temperature, pressure); // measure pressure, temperature and humidity
  float altitude = bme0.calcAltitude (pressure);

  Serial.println("\nBMP280 Sensor");
  Serial.print("Atmosphere Pressure (hPa) = ");
  Serial.println(pressure, 2); // print with 2 decimal places
  Serial.print("Temperature (degree C) = ");
  Serial.println(temperature, 2); // print with 2 decimal places
  Serial.print( "Humidity (%RH) = ");
  Serial.println(humidity, 2); // print with 2 decimal places
  Serial.print("Altitude (m) = ");
  Serial.println(altitude, 2); // print with 2 decimal places
  
  if (UVSensor == true)
  {

    Serial.println("GY-1145 Sensor");
    if (! uv.begin()) {
      Serial.println("Didn't find Si1145");
      while (1);
    }
  
    Vis = uv.readVisible();
    IR = uv.readIR();
  
    Serial.print("Vis: "); 
    Serial.println(Vis);
    Serial.print("IR: "); 
    Serial.println(IR);
  
    UVindex = uv.readUV();
    // the index is multiplied by 100 so to get the
    // integer index, divide by 100!
    UVindex /= 100.0;  
    Serial.print("UV: ");  Serial.println(UVindex);

    Blynk.virtualWrite(5, UVindex); 
    Blynk.virtualWrite(6, IR);
  }

  if (dht22Sensor == true) {
    dht.begin();
    delay(2000);

    // Reading temperature or humidity takes about 250 milliseconds!
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
    h = dht.readHumidity();
    // Read temperature as Celsius (the default)
    t = dht.readTemperature();
    // Read temperature as Fahrenheit (isFahrenheit = true)
    f = dht.readTemperature(true);

    // Compute heat index in Fahrenheit (the default)
    hif = dht.computeHeatIndex(f, h);
    // Compute heat index in Celsius (isFahreheit = false)
    hic = dht.computeHeatIndex(t, h, false);

    Serial.print(F("Humidity (%): "));
    Serial.println(h);
    Serial.print(F("Temperature: "));
    Serial.print(t);
    Serial.print(F("°C "));
    Serial.print(f);
    Serial.println(F("°F "));
    Serial.print(F("Heat index (what you feel): "));
    Serial.print(hic);
    Serial.print(F("°C "));
    Serial.print(hif);
    Serial.println(F("°F"));
  }

  //******Battery Voltage Monitoring*********************************************
  //depend on 220k or 100k resistor used, toggle between this 2 calculation

  // using 220k
  // Voltage divider R1=100k, R2 = (220k + 100k) + 220k = 540k
  // Calculation to get the factor: 4.2v * 100/540 = 0.7777, 0.7777 * y = 4.2, y = 5.4
//  float calib_factor = 5.28; // change this value to calibrate the battery voltage, 5.28 is so call calibrated

  // using 100k
  // Voltage divider R1 = 100k, R2 = (220k + 100k) + 100k = 420k; 
  // Calculation to get the factor: 4.2v * 100/420 = 1, 1 * y = 4.2, y = 4.2
  float calib_factor = 4.2;
  
  unsigned long raw = analogRead(A0);
  Serial.print("raw analog = ");
  Serial.println(raw);
  float volt= raw * calib_factor/1024; 
  
  Serial.print( "\nVoltage = ");
  Serial.print(volt, 2); // print with 2 decimal places
  Serial.println (" V");
  
 //*******************************************************************************
  // code block for uploading data to BLYNK App
  
  if (App == "BLYNK") { // choose application
    Blynk.virtualWrite(0, pressure);    // virtual pin 0 
    Blynk.virtualWrite(1, temperature); // virtual pin 1
    Blynk.virtualWrite(2, humidity);    // virtual pin 2
    Blynk.virtualWrite(3, altitude);    // virtual pin 3
    Blynk.virtualWrite(4, volt);        // virtual pin 4  
    if (UVSensor == true) {
        Blynk.virtualWrite(5, UVindex); 
        Blynk.virtualWrite(6, IR);
     }
    if (dht22Sensor == true) {
      Blynk.virtualWrite(7, h);
      Blynk.virtualWrite(8, t);
      Blynk.virtualWrite(9, f); 
      Blynk.virtualWrite(10, hic);
      Blynk.virtualWrite(11, hif);
    }
  } 
 //*******************************************************************************
 // code block for uploading data to Thingspeak website
 
  else if (App == "Thingspeak") {
  // Send data to ThingSpeak 
  WiFiClient client;  
  if (client.connect(server,80)) {
  Serial.println("Connect to ThingSpeak - OK"); 

  String postStr = "";
  postStr+="GET /update?api_key=";
  postStr+=api_key;   
  postStr+="&field1=";
  postStr+=String(pressure);
  postStr+="&field2=";
  postStr+=String(temperature); //  postStr+=String();
  postStr+="&field3=";
  postStr+=String(h);  //postStr+=String(humidity);
  postStr+="&field4=";
  postStr+=String(altitude);
  postStr+="&field5=";
  postStr+=String(volt);    
  
  postStr+="&field6=";
  postStr+=String(UVindex);    
  postStr+="&field7=";
  postStr+=String(IR);    
  postStr+="&field8=";
  postStr+=String(t);    

  postStr+=" HTTP/1.1\r\nHost: a.c.d\r\nConnection: close\r\n\r\n";
  postStr+="";
  client.print(postStr);
 
 //*******************************************************************************

}
 while(client.available()){
    String line = client.readStringUntil('\r');
    Serial.print(line);
  }
}

  //  Serial.print (" Counter= ");
  //  Serial.println (eventCounter);

} // end of void measurementEvent()

   
// ----------recover Event counter----------------------------
int recoverCounter() {
  // read value of counter back from bmp0
  byte bme0F4value = bme0.readF4Sleep(); // 0 to 63
  byte bme0F5value = bme0.readF5Sleep(); // 0 to 63
  return bme0F5value * 64 + bme0F4value; // 0 to 4095
  if (bme280Debug) {
    Serial.print ("bme0F4,F5values from bme0 Registers 0xF4,F5 =");
    Serial.print (bme0F4value);
    Serial.print (" ");
    Serial.println (bme0F5value);
  } // end of if (bme280Debug)
} // end of void recoverCounter()


void saveCounter(int counter) {
  // write value of counter into bme0
  bme0.updateF4ControlSleep(counter & 0x3F); // store eventCounter
  bme0.updateF5ConfigSleep((counter / 64) & 0x3F); // store eventCounter
  // this also puts bme0 to sleep
  if (bme280Debug) {
    Serial.print ("saved counter to bme0F4,F5 as ");
    Serial.print (counter & 0x3F);
    Serial.print (", ");
    Serial.println ((counter / 64) & 0x3F);
  } // end of if (bme280Debug)
} // end of void saveCounter()


void goToSleep() {
  // calculate required sleep time and go to sleep
  long sleepTime = measurementInterval - millis(); // in milliseconds
  if (sleepTime < 100) sleepTime = 100; // set minimum sleep of 0.1 second

  //  Serial.print ("This is report cycle No. ");
  //  Serial.println(eventCounter);
  Serial.print (" Going to sleep now for 3 mins");

  ESP.deepSleep(sleepTime * 5000); // convert to microseconds
  delay(100);

} // end of void goToSleep()
