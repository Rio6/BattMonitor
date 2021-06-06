// Functions to communicate with open evse charger with MQTT and rapi commands
// more open evse commands https://github.com/OpenEVSE/open_evse/blob/stable/firmware/open_evse/rapi_proc.h
#ifndef EVSE_HPP
#define EVSE_HPP

#include "config.h"

#include <BridgeClient.h>
#include <PubSubClient.h>
#include <string.h>

void callback(char* topic, byte* payload, unsigned int length);

BridgeClient net;
IPAddress server(127, 0, 0, 1);
PubSubClient client(server, 1883, callback, net);

bool last_enable = false;

// Send enable/disable command to charger
void evse_enable(bool enable) {
    if(enable) {
        client.publish("openevse/rapi/in/$FE", "");
    } else {
        client.publish("openevse/rapi/in/$FD", "");
    }
    last_enable = enable;
}

// Called when newly connected to a charger, send and message and the last enable/disable state
void evse_connected() {
    client.publish("openevse/rapi/in/$FP", "0 0 Connected to Yun");
    evse_enable(last_enable);
}

// Called when recieved a message
void callback(char* topic, byte* payload, unsigned int length) {
    if(strcmp(topic, "openevse") == 0 && strncmp((char*) payload, "connected", length) == 0) {
        evse_connected();
    }
}

// Check connection status and try reconnect with MQTT when not connected
void evse_connect() {
    static bool connected = false;
    bool status = client.connect(MQTT_ID, MQTT_USER, MQTT_PASS,
         "openevse/rapi/in/$FP", 0, false, "0 0 Disconnected from Yun");
    if(status && !connected) {
        // Connected, subscribe to openevse topic
        client.subscribe("openevse/#");
        evse_connected();
    }
    connected = status;
}

// Call this every loop
void evse_loop() {
    client.loop();
    evse_connect();
}

#endif
