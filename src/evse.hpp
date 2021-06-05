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

// more open evse commands https://github.com/OpenEVSE/open_evse/blob/stable/firmware/open_evse/rapi_proc.h
void evse_enable_raw(bool enable) {
    if(enable) {
        client.publish("openevse/rapi/in/$FE", "");
    } else {
        client.publish("openevse/rapi/in/$FD", "");
    }
    last_enable = enable;
}

void evse_enable(bool enable) {
    if(enable != last_enable) {
        evse_enable_raw(enable);
    }
}

void evse_print(const char *msg) {
    char buff[64];
    snprintf(buff, sizeof(buff), "0 0 %s", msg);
    client.publish("openevse/rapi/in/$FP", buff);
}

void callback(char* topic, byte* payload, unsigned int length) {
    if(strcmp(topic, "openevse") == 0 && strncmp((char*) payload, "connected", length) == 0) {
        evse_print("Connected to Yun");
        evse_enable_raw(last_enable);
    }
}

void evse_connect() {
    static bool connected = false;
    bool status = client.connect(MQTT_ID, MQTT_USER, MQTT_PASS);
    if(status && !connected)
        client.subscribe("openevse/#");
    connected = status;
}

void evse_loop() {
    client.loop();
    evse_connect();
}

#endif
