/*
 *  Simple lamp controller for ESP8266. Turns on/off a lamp based on the level of a light sensor.
 */
#include <Arduino.h>
#include <arduino-timer.h>

#define IOTWEBCONF_PASSWORD_LEN 65
#include <IotWebConf.h>
#include <IotWebConfTParameter.h>
#include <IotWebConfUsing.h>  // This loads aliases for easier class names.

// Relay Status 0 = aus, 1 = an
int relayState = 0;

// Turn relay of because it is dark?
bool lightConditionDark = false;

// light level 0 (dark) .. 1023 (bright)
int lightLevel = 1023;

// info string for debugging
String switchConditionInfo = "";

// number of seconds the current light state (dark/bright) is currently active
int currLightConditionCycles = 0;

// timer to check light status every second only
auto timer = timer_create_default();

// method to be called by timer every 1 second
bool checkSwitchConditions(void *argument);

// switch relay off
void switchRelayOff();

// ### IotWebConf #############################################################
// ############################################################################

// Name server
DNSServer dnsServer;

// Web server
WebServer server(80);

// Is a reset required?
boolean needReset = false;

// Is wifi connected?
boolean connected = false;

// IotWebConf: Modifying the config version will probably cause a loss of the existig configuration. Be careful!
const char *CONFIG_VERSION = "1.0.2";

// IotWebConf: Access point SSID
const char *WIFI_AP_SSID = "LampControl";

// IotWebConf: Default access point password
const char *WIFI_AP_DEFAULT_PASSWORD = "";

// IotWebConf: Method for handling access to / on web config
void handleRoot();

// IotWebConf: Called when connection to wifi was established
void wifiConnected();

// IotWebConf: Called when configuration saved
void configSaved();

// last known WiFi network state
iotwebconf::NetworkState lastNetWorkState = iotwebconf::NetworkState::OffLine;

IotWebConf iotWebConf(WIFI_AP_SSID, &dnsServer, &server, WIFI_AP_DEFAULT_PASSWORD, CONFIG_VERSION);

// Parameter group for settings
IotWebConfParameterGroup groupSettings = IotWebConfParameterGroup("groupSettings", "Settings");

// Parameter for seconds to delay on/off switch
iotwebconf::IntTParameter<int16_t> settingDelayParam =
    iotwebconf::Builder<iotwebconf::IntTParameter<int16_t>>("settingDelayParam").label("Delay switch seconds").defaultValue(30).min(1).max(100).step(1).placeholder("1..100").build();

// Parameter for light level which is treated as "dark"
// if light level is below this level for more then "delay switch seconds" the lamp will be turned on
// if light level is above this level for more then "delay switch seconds" the lamp will be turned of
iotwebconf::IntTParameter<int16_t> settingDarkLevelParam =
    iotwebconf::Builder<iotwebconf::IntTParameter<int16_t>>("settingDarkLevelParam").label("Dark level").defaultValue(25).min(1).max(100).step(1).placeholder("1..100").build();

// ##########################################
// General Setup ############################
// ##########################################

void setup() {

    Serial.begin(115200);
    delay(100);
    Serial.println("Initializing");

    // PIN init
    pinMode(D5, OUTPUT);
    pinMode(D8, OUTPUT);


    // -- Initializing the configuration.
    groupSettings.addItem(&settingDelayParam);
    groupSettings.addItem(&settingDarkLevelParam);
    iotWebConf.addParameterGroup(&groupSettings);

    iotWebConf.setWifiConnectionCallback(&wifiConnected);
    iotWebConf.setConfigSavedCallback(&configSaved);
    iotWebConf.setStatusPin(LED_BUILTIN);
    // iotWebConf.setConfigPin(D5);
    iotWebConf.init();

    // -- Set up required URL handlers on the web server.
    server.on("/", handleRoot);
    server.on("/config", []{ iotWebConf.handleConfig(); });
    server.onNotFound([]() { iotWebConf.handleNotFound(); });

    // turn relay of on start
    switchRelayOff();

    // check light condition every second
    timer.every(1000L, checkSwitchConditions);
}

// ##########################################
// Main Loop ################################
// ##########################################

void loop() {
    if (needReset) {
        // config changes require reset
        Serial.println("restart in 1 sec");
        delay(1000);
        ESP.restart();
    }

    timer.tick();
    iotWebConf.doLoop();
}

/**
 * Relay on
 */
void switchRelayOn() {
    digitalWrite(D5, LOW);   // Relay on
    digitalWrite(D8, HIGH);  // LED on
    relayState = 1;
}

/**
 *  Relay off
 */
void switchRelayOff() {
    digitalWrite(D5, HIGH);  // Relay of
    digitalWrite(D8, LOW);   // LED off
    relayState = 0;
}

/**
 * Update light value and check wheather to turn relay on or off
 */
void updateLightValue() {
    lightLevel = analogRead(A0);
    boolean newLightConditionDark = false;

    int darkLevelSetting = settingDarkLevelParam.value();
    if (lightLevel <= darkLevelSetting) {
        newLightConditionDark = true;
    } else {
        newLightConditionDark = false;
    }

    // remember time of current light state to prevent relay toggling
    if (newLightConditionDark == lightConditionDark) {
        currLightConditionCycles = currLightConditionCycles + 1;
    } else {
        currLightConditionCycles = 0;
    }

    lightConditionDark = newLightConditionDark;

    // prevent int overflow
    int cyclesRequiredForRelayChange = settingDelayParam.value();
    if (currLightConditionCycles > cyclesRequiredForRelayChange) {
        currLightConditionCycles = cyclesRequiredForRelayChange;
    }
}

/**
 * Update light value and check wheather to turn relay on or off
 */
bool checkSwitchConditions(void *argument) {
    //Serial.println("Checking light conditions");

    // read sensor
    updateLightValue();

    // check if switch on/off is required
    int cyclesRequiredForRelayChange = settingDelayParam.value();
    int darkLevelSetting = settingDarkLevelParam.value();

    boolean switchAllowedByTime = currLightConditionCycles >= cyclesRequiredForRelayChange;
    switchConditionInfo = "Info: lightLevel: " + String(lightLevel) + ", DARK_IS_WHEN_LEVEL_LOWER_EQ: " + String(darkLevelSetting) + ", switchAllowedByTime: " + String(switchAllowedByTime) + ", relayState: " + String(relayState) + ", lightConditionDark: " + String(lightConditionDark) + ", currLightConditionCycles: " + String(currLightConditionCycles);

    // light off - turn on?
    if (relayState == 0) {
        if (lightConditionDark && switchAllowedByTime) {
            switchRelayOn();
        }
    }
    // light on - turn off?
    else {
        if (!lightConditionDark && switchAllowedByTime) {
            switchRelayOff();
        }
    }

    // keep timer running
    return true;
}

void configSaved() {
    Serial.println("config saved");
    needReset = true;
}

void wifiConnected() {
    connected = true;
    Serial.println("### WiFi connected ###");
}

/**
 * Handle web requests to "/" path.
 */
void handleRoot() {
    // -- Let IotWebConf test and handle captive portal requests.
    if (iotWebConf.handleCaptivePortal()) {
        // -- Captive portal request were already served.
        return;
    }
    String s = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>";
    s += "<title>Lamp control parameters and values</title></head><body>Current settings and values";
    s += "<ul>";
    s += "<li>Delay param value: ";
    s += settingDelayParam.value();
    s += "<li>Dark value param value: ";
    s += settingDarkLevelParam.value();
    s += "<li>Current light status value: ";
    s += relayState;
    s += "<li>Current light level value: ";
    s += lightLevel;
    s += "<li>Current seconds on light level value: ";
    s += currLightConditionCycles;
    s += "</ul>";
    s += "Go to <a href='config'>configure page</a> to change values.";
    s += "</body></html>\n";

    server.send(200, "text/html", s);
}