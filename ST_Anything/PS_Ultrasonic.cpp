//******************************************************************************************
//  File: PS_Ultrasonic.cpp
//  Authors:
//
//  Summary:  PS_Ultrasonic is a class which implements a custom Level device capability.
//			  It inherits from the st::PollingSensor class.
//
//
//			  Create an instance of this class in your sketch's global variable section
//			  For Example:  st::PS_Ultrasonic sensor1("ultrasonic1", 60, 0, PIN_ULTRASONIC_T, PIN_ULTRASONIC_E);
//
//			  st::PS_Ultrasonic() constructor requires the following arguments
//				- String &name - REQUIRED - the name of the object - must match the Groovy ST_Anything DeviceType tile name
//				- long interval - REQUIRED - the polling interval in seconds
//				- long offset - REQUIRED - the polling interval offset in seconds - used to prevent all polling sensors from executing at the same time
//				- byte digitalTriggerPin - REQUIRED - the Arduino Pin to be used as a digital output to trigger ultrasonic
//				- byte digitalEchoPin - REQUIRED - the Arduino Pin to be used as a digital input to read the echo
//
//
//  Change History:
//
//    Date        Who            What
//    ----        ---            ----
//    5 May 19    CSC            added noise rejection logic for HC-SR04 (ultra sonic sensor) 
//
//
//******************************************************************************************

#include "PS_Ultrasonic.h"

#include "Constants.h"
#include "Everything.h"

namespace st
{
//private


//public
	//constructor - called in your sketch's global variable declaration section
	PS_Ultrasonic::PS_Ultrasonic(const __FlashStringHelper *name, unsigned int interval, int offset, byte digitalTriggerPin, byte digitalEchoPin):
		PollingSensor(name, interval, offset),
		m_nSensorValue(0)
	{
		setPin(digitalTriggerPin,digitalEchoPin);
	}

	//destructor
	PS_Ultrasonic::~PS_Ultrasonic()
	{

	}

	//SmartThings Shield data handler (receives configuration data from ST - polling interval, and adjusts on the fly)
	void PS_Ultrasonic::beSmart(const String &str)
	{
		String s = str.substring(str.indexOf(' ') + 1);
		Serial.print("st string #####  ");
		Serial.println(str);
		if (s.toInt() != 0) {
			st::PollingSensor::setInterval(s.toInt() * 1000);
			if (st::PollingSensor::debug) {
				Serial.print(F("PS_Ultrasonic::beSmart set polling interval to "));
				Serial.println(s.toInt());
			}
		}
		else {
			if (st::PollingSensor::debug)
			{
				Serial.print(F("PS_Ultrasonic::beSmart cannot convert "));
				Serial.print(s);
				Serial.println(F(" to an Integer."));
			}
		}
	}

	//function to get data from sensor and queue results for transfer to ST Cloud
	/*
	void PS_Ultrasonic::getData()
	{

		//int m_nSensorValue=map(analogRead(m_nAnalogInputPin), SENSOR_LOW, SENSOR_HIGH, MAPPED_LOW, MAPPED_HIGH);
		long duration;
		// Clears the trigPin
		digitalWrite(m_nDigitalTriggerPin, LOW);
		delayMicroseconds(2);
		// Sets the trigPin on HIGH state for 10 micro seconds
		digitalWrite(m_nDigitalTriggerPin, HIGH);
		delayMicroseconds(10);
		digitalWrite(m_nDigitalTriggerPin, LOW);
		// Reads the echoPin, returns the sound wave travel time in microseconds
		duration = pulseIn(m_nDigitalEchoPin, HIGH);
		// Calculating the distance
		//m_nSensorValue = duration*0.034/2;
		m_nSensorValue = duration/2/29.1;  //change by SC

		Serial.print(m_nSensorValue);
  		Serial.println(" cm");

		// queue the distance to send to smartthings
		Everything::sendSmartString(getName() + " " + String(m_nSensorValue));
	}
	*/

	void PS_Ultrasonic::getData()
	{
		m_nSensorValue = mesureDistance();
		Serial.print(m_nSensorValue);
  	Serial.println(" cm");

		Everything::sendSmartString(getName() + " " + String(m_nSensorValue));
	}

	/**
	 *  take single distance mesurement and return the value
	 */
	int PS_Ultrasonic::mesureSingleDistance()
	{
	    long duration, distance;

	    digitalWrite(m_nDigitalTriggerPin, LOW);
	    delayMicroseconds(2);
	    digitalWrite(m_nDigitalTriggerPin, HIGH);
	    delayMicroseconds(10);
	    digitalWrite(m_nDigitalTriggerPin, LOW);
	    duration = pulseIn(m_nDigitalEchoPin, HIGH);
	    distance = (duration/2) / 29.1;

			Serial.print("Distance: ");
			Serial.println(distance);

	    return distance;
	}

	/**
	 * mresure distnce average delay 10ms
	 */
	int PS_Ultrasonic::mesureDistance()
	{
		//added by CSC
		const int ULTRASONIC_MIN_DISTANCE = 0;
		const int ULTRASONIC_MAX_DISTANCE = 170;

	    int i, total_count = 0, total_distance = 0, distance, values[10], average, temp, new_count = 0;

	    for(i=0; i<5; i++){
	        distance = mesureSingleDistance();
	        if(distance > ULTRASONIC_MIN_DISTANCE && distance < ULTRASONIC_MAX_DISTANCE) {
	            total_distance += distance;
	            values[total_count++]=distance;
	            Serial.printf("Value %d...\n", distance);
	        }
	    }

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
	            return total_distance / new_count;
	        }
	    }
	    return 0;
	}

	void PS_Ultrasonic::setPin(byte &trigPin,byte &echoPin)
	{
		m_nDigitalTriggerPin=trigPin;
		m_nDigitalEchoPin=echoPin;
		pinMode(m_nDigitalTriggerPin, OUTPUT); // Sets the trigPin as an Output
		pinMode(m_nDigitalEchoPin, INPUT); // Sets the echoPin as an Input
	}
}
