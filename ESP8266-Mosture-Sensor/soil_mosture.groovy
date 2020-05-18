/**
 *  ESP8266 Soil Moisture Sensor
 *  Device Handler for SmartThings
 *  Version 1.0
 *  Author: Soon Chye 2020
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
 *  in compliance with the License. You may obtain a copy of the License at:
 *
 *	    http://www.apache.org/licenses/LICENSE-2.0
*/

metadata {
    definition (name: "ESP8266 Moisture Sensor", namespace: "cschwer", author: "Charles Schwer") {
        capability "Sensor"
        capability "waterSensor"
        
        attribute "lastUpdated", "String"
    }
 
    // simulator metadata
    simulator {}
 
 /*
    // UI tile definitions
    tiles (scale: 2) {
        standardTile("water", "device.water", width: 2, height: 2) {
            state("wet", label:'${name}', icon:"st.water.water.wet", backgroundColor:"#0000FF")
            state("ok", label:'${name}', icon:"st.water.water.ok", backgroundColor:"#79b821")
            state("dry", label:'${name}', icon:"st.water.water.dry", backgroundColor:"#ffa81e")
       }
        main "water"
        details (["water", "refresh"])
        //details (["water"]) 
 }
*/

	tiles(scale: 2) {
		multiAttributeTile(name:"moistureStatus", type: "lighting", width: 6, height: 4, canChangeIcon: false) {
			tileAttribute ("device.moistureStatus", key: "PRIMARY_CONTROL") {
				//attributeState("default", label:'Moisture', backgroundColor:"#79b821", icon:"")
                attributeState("wet", label:'${name}', backgroundColor:"#0000FF")
                attributeState("ok", label:'${name}', backgroundColor:"#79b821")
                attributeState("dry", label:'${name}', backgroundColor:"#ffa81e")
			}
			tileAttribute("device.lastCheckin", key: "SECONDARY_CONTROL") {
				attributeState "lastPressed", label:'Last update: ${currentValue}'
			}
		}

		main (["moistureStatus"])
		details(["moistureStatus","lastCheckin"])
	}
    preferences {
        input("ip", "text", title: "IP Address", description: "ip", required: true)
        input("port", "number", title: "Port", description: "port", default: 9060, required: true)
        input("mac", "text", title: "MAC Addr (e.g. AB50E360E123)", description: "mac")
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
    def value = ""
    if (bodyString) {
        def json = msg.json;
		//for debug
		if (json) {
        	log.debug("Value received: ${json}")
            log.debug("Value received: ${json.value}")
        }
        if( json?.name == "moisture") {
        //    value = json.status == 1 ? "wet" : "dry"
            
            if (json.value >= 900) {
            	value = "dry"
            }
            else if (json.value >= 650) {
            	value = "ok"
            }
            else if (json.value >= 400) {
            	value = "wet"
            }
                
            log.debug "moisture status: ${value}"
            //result << createEvent(name: "soil", value: value)
            //sendEvent(name: "moistureStatus", value: ${value})
            sendEvent(name: "moistureStatus", value: "${value}", isStateChange: true, display: false)
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
