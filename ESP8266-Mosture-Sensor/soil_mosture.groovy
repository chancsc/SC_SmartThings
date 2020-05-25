/**
 *  ESP8266 Soil Moisture Sensor
 *  Device Handler for SmartThings
 *  Version
 *	1.0.0 - initial version
 *  1.0.1 - update the definition if less than 400
 *  Author: Soon Chye 2020
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
 *  in compliance with the License. You may obtain a copy of the License at:
 *
 *	    http://www.apache.org/licenses/LICENSE-2.0
*/

metadata {
    definition (name: "ESP8266 Moisture Sensor", namespace: "sc", author: "Soon Chye") {
        capability "Sensor"
        capability "waterSensor"
//        capability "Configuration"
        
        attribute "lastUpdated", "String"
        attribute "sensorValue", "String"
    }
 
    simulator {}

	tiles(scale: 2) {
		multiAttributeTile(name:"moistureStatus", type: "water", width: 6, height: 4, canChangeIcon: false) {
			tileAttribute ("device.moistureStatus", key: "PRIMARY_CONTROL") {
                attributeState("wet", label:'${name}', backgroundColor:"#0000FF")
                attributeState("ok", label:'${name}', backgroundColor:"#79b821")
                attributeState("dry", label:'${name}', backgroundColor:"#ffa81e")
			}
			tileAttribute("device.lastCheckin", key: "SECONDARY_CONTROL") {
				attributeState "lastPressed", label:'Last update: ${currentValue}'
			}
		}

//		valueTile("sensorValue", "water", decoration: "flat", inactiveLabel: false, width: 2, height: 2) {
//			state "sensorValue", label: 'Sensor: ${currentValue}%', unit: ""
//		}

		//main (["moistureStatus", "sensorValue"])
        main (["moistureStatus"])
		details(["moistureStatus","lastCheckin"])
	}
    preferences {
        input("ip", "text", title: "IP Address", description: "ip", required: true)
        input("port", "number", title: "Port", description: "port", default: 9060, required: true)
        input("mac", "text", title: "MAC Addr (e.g. AB50E360E123)", description: "mac")
//		input "alert", "number", title: "Dry Alert threshold", description: "Define the Threshold for Dry", range: "1..500", displayDuringSetup: false
    }
}
def updated() {
    log.debug("Updated with settings: $settings")
    state.dni = ""
    updateDNI()
    updateSettings()
}
 
// parse events into attributes
def parse(String description) {
    def msg = parseLanMessage(description)
    if (!state.mac || state.mac != msg.mac) {
        log.debug "Setting deviceNetworkId to MAC address ${msg.mac}"
        state.mac = msg.mac
    }
    
    sendEvent(name: "lastCheckin", value: formatDate(), displayed: false)
    
    def result = []
    def bodyString = msg.body
    def jvalue = ""
    if (bodyString) {
        def json = msg.json;
		//for debug
		if (json) {
//        	log.debug("Value received: ${json}")
            log.debug("Value received: ${json.value}")
        }
        if( json?.name == "moisture") {
            
            if (json.value >= 900) {
            	jvalue = "dry"
            }
            else if (json.value >= 650) {
            	jvalue = "ok"
            }
            else if (json.value >= 400) {
            	jvalue = "wet"
            }
            else {
            	jvalue = "very wet"
            }
                
            log.debug "moisture status: ${jvalue}"
            
            sendEvent(name: "sensorValue", value: "${json.value}", isStateChange: true, display: false)
            sendEvent(name: "moistureStatus", value: "${jvalue}", isStateChange: true, display: false)
        }
    }
    
    result
}
 
private getHostAddress() {
    def ip = settings.ip
    def port = settings.port
 
    //log.debug "Using ip: ${ip} and port: ${port} for device: ${device.id}"
    return ip + ":" + port
}
 
private def updateDNI() {
    if (!state.dni || state.dni != device.deviceNetworkId || (state.mac && state.mac != device.deviceNetworkId)) {
        device.setDeviceNetworkId(createNetworkId(settings.ip, settings.port))
        state.dni = device.deviceNetworkId
    }
}
 
private String createNetworkId(ipaddr, port) {
    if (state.mac) {
        return state.mac
    }
    def hexIp = ipaddr.tokenize('.').collect {
        String.format('%02X', it.toInteger())
    }.join()
    def hexPort = String.format('%04X', port.toInteger())
    return "${hexIp}:${hexPort}"
}
 
def updateSettings() {
    def headers = [:]
    headers.put("HOST", getHostAddress())
    headers.put("Content-Type", "application/json")
    groovy.json.JsonBuilder json = new groovy.json.JsonBuilder ()
    def map = json {
        hubIp device.hub.getDataValue("localIP")
        hubPort device.hub.getDataValue("localSrvPortTCP").toInteger()
        deviceName device.name
    }    
    return new physicalgraph.device.HubAction(
        method: "POST",
        path: "/updateSettings",
        body: json.toString(),
        headers: headers
    )
}

def formatDate() {
	def correctedTimezone = ""
	def timeString = clockformat ? "HH:mm:ss" : "h:mm:ss aa"

	// If user's hub timezone is not set, display error messages in log and events log, and set timezone to GMT to avoid errors
	if (!(location.timeZone)) {
		correctedTimezone = TimeZone.getTimeZone("GMT")
		log.error "${device.displayName}: Time Zone not set, so GMT was used. Please set up your location in the SmartThings mobile app."
		sendEvent(name: "error", value: "", descriptionText: "ERROR: Time Zone not set, so GMT was used. Please set up your location in the SmartThings mobile app.")
	}
	else {
		correctedTimezone = location.timeZone
	}
	if (dateformat == "US" || dateformat == "" || dateformat == null) {
			return new Date().format("EEE MMM dd yyyy ${timeString}", correctedTimezone)
	}
	else if (dateformat == "UK") {
			return new Date().format("EEE dd MMM yyyy ${timeString}", correctedTimezone)
	}
	else {
			return new Date().format("EEE yyyy MMM dd ${timeString}", correctedTimezone)
	}
}
