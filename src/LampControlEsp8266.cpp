/*
 *  Simple lamp controller for ESP8266. Turns on/off a lamp based on the level of a light sensor.
 */
#include <Arduino.h>
#include <arduino-timer.h>
#include <time.h>

#define IOTWEBCONF_PASSWORD_LEN 65
#include <IotWebConf.h>
#include <IotWebConfTParameter.h>
#include <IotWebConfUsing.h>  // This loads aliases for easier class names.

#include "DashboardPage.h"

// Relay status 0 = off, 1 = on
int relayState = 0;

// Turn relay off because it is dark?
bool lightConditionDark = false;

// light level 0 (dark) .. 1023 (bright)
int lightLevel = 1023;

// info string for debugging
String switchConditionInfo = "";

// number of seconds the current light state (dark/bright) is currently active
int currLightConditionCycles = 0;

// timer to check light status every second only
auto timer = timer_create_default();

// baud rate used for the serial interface and its settings menu
const unsigned long SERIAL_BAUD_RATE = 115200;

// method to be called by timer every 1 second
bool checkSwitchConditions(void *argument);

// method to be called by timer once a day to (re-)synchronize time via NTP
bool ntpDailySyncTimerCallback(void *argument);

// synchronize time via NTP, but only while online (WiFi connected, not AP mode)
void syncTimeViaNtp();

// current time formatted for display, or a placeholder if not yet synchronized
String getCurrentTimeString();

// switch relay off
void switchRelayOff();

// interactive serial settings menu (requires a physical USB/serial connection)
void runSerialSettingsMenu();

// blocks until a full line is entered on Serial, echoing typed characters back
String readSerialLine();

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
// Note: only the first 4 bytes are actually compared/stored (IOTWEBCONF_CONFIG_VERSION_LENGTH),
// so keep this at 3 characters (+ implicit null terminator) and change it within those 3 to force a reset.
const char *CONFIG_VERSION = "1.1";

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
// if light level is below this level for more than "delay switch seconds" the lamp will be turned on
// if light level is above this level for more than "delay switch seconds" the lamp will be turned off
iotwebconf::IntTParameter<int16_t> settingDarkLevelParam =
    iotwebconf::Builder<iotwebconf::IntTParameter<int16_t>>("settingDarkLevelParam").label("Dark level").defaultValue(25).min(1).max(100).step(1).placeholder("1..100").build();

// Parameter for the NTP server used for time synchronization
iotwebconf::TextTParameter<64> settingNtpServerParam =
    iotwebconf::Builder<iotwebconf::TextTParameter<64>>("settingNtpServerParam").label("NTP server").defaultValue("pool.ntp.org").build();

// Parameter for the timezone (POSIX TZ string, e.g. from TZ.h), used to convert
// the NTP-synchronized UTC time into local time including DST rules.
iotwebconf::TextTParameter<48> settingTimezoneParam =
    iotwebconf::Builder<iotwebconf::TextTParameter<48>>("settingTimezoneParam").label("Timezone").defaultValue("CET-1CEST,M3.5.0,M10.5.0/3").build();

// has the first successful NTP time sync already happened and been reported on serial?
bool initialTimeSyncDone = false;

// manual override for the relay; never persisted, always resets to AUTO on boot
LampMode lampMode = LAMP_MODE_AUTO;

// rolling min/max light level per minute, covering the last 15 minutes
const int MINUTE_BUCKET_COUNT = 15;
int minuteMinLevel[MINUTE_BUCKET_COUNT];
int minuteMaxLevel[MINUTE_BUCKET_COUNT];
int currentMinuteBucket = 0;

// rolling min/max light level per hour, covering the last 24 hours
const int HOURLY_BUCKET_COUNT = 24;
int hourlyMinLevel[HOURLY_BUCKET_COUNT];
int hourlyMaxLevel[HOURLY_BUCKET_COUNT];
int currentHourlyBucket = 0;

// method to be called by timer once a minute/hour to rotate the light level buckets
bool rotateMinuteBucket(void *argument);
bool rotateHourlyBucket(void *argument);

// IotWebConf: handles POST/GET requests to change the lamp mode (auto/on/off)
void handleSetMode();

// ##########################################
// General Setup ############################
// ##########################################

void setup() {

    Serial.begin(SERIAL_BAUD_RATE);
    delay(100);
    Serial.println("Initializing");

    // PIN init
    pinMode(D5, OUTPUT);
    pinMode(D8, OUTPUT);

    // -- Light level history buckets start out empty.
    for (int i = 0; i < MINUTE_BUCKET_COUNT; i++) {
        minuteMinLevel[i] = LIGHT_BUCKET_NO_DATA;
        minuteMaxLevel[i] = LIGHT_BUCKET_NO_DATA;
    }
    for (int i = 0; i < HOURLY_BUCKET_COUNT; i++) {
        hourlyMinLevel[i] = LIGHT_BUCKET_NO_DATA;
        hourlyMaxLevel[i] = LIGHT_BUCKET_NO_DATA;
    }

    // -- Initializing the configuration.
    groupSettings.addItem(&settingDelayParam);
    groupSettings.addItem(&settingDarkLevelParam);
    groupSettings.addItem(&settingNtpServerParam);
    groupSettings.addItem(&settingTimezoneParam);
    settingTimezoneParam.setPlaceholder("POSIX TZ, e.g. CET-1CEST,M3.5.0,M10.5.0/3 for Europe/Berlin");
    iotWebConf.addParameterGroup(&groupSettings);

    iotWebConf.setWifiConnectionCallback(&wifiConnected);
    iotWebConf.setConfigSavedCallback(&configSaved);
    iotWebConf.setStatusPin(LED_BUILTIN);
    // iotWebConf.setConfigPin(D5);
    iotWebConf.init();

    // -- Offer the interactive settings menu on serial. This can only be
    //    triggered with a physical USB/serial connection, never over WLAN.
    Serial.println("Press 's' + Enter within 30 seconds to open the settings menu...");
    unsigned long settingsMenuDeadline = millis() + 30000;
    while (millis() < settingsMenuDeadline) {
        if (Serial.available()) {
            String input = readSerialLine();
            if (input.equalsIgnoreCase("s")) {
                runSerialSettingsMenu();
            }
            break;
        }
    }

    // -- Set up required URL handlers on the web server.
    server.on("/", handleRoot);
    server.on("/config", []{ iotWebConf.handleConfig(); });
    server.on("/mode", handleSetMode);
    server.onNotFound([]() { iotWebConf.handleNotFound(); });

    // turn relay off on start
    switchRelayOff();

    // check light condition every second
    timer.every(1000L, checkSwitchConditions);

    // re-synchronize time via NTP once a day (initial sync happens on WiFi connect)
    timer.every(86400000UL, ntpDailySyncTimerCallback);

    // rotate the light level history buckets once a minute / once an hour
    timer.every(60000UL, rotateMinuteBucket);
    timer.every(3600000UL, rotateHourlyBucket);
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
    digitalWrite(D5, HIGH);  // Relay off
    digitalWrite(D8, LOW);   // LED off
    relayState = 0;
}

/**
 * Records lightLevel into the currently active minute/hour history buckets,
 * tracking the min and max value seen so far in each.
 */
void updateLightLevelBuckets() {
    if (minuteMinLevel[currentMinuteBucket] == LIGHT_BUCKET_NO_DATA) {
        minuteMinLevel[currentMinuteBucket] = lightLevel;
        minuteMaxLevel[currentMinuteBucket] = lightLevel;
    } else {
        minuteMinLevel[currentMinuteBucket] = min(minuteMinLevel[currentMinuteBucket], lightLevel);
        minuteMaxLevel[currentMinuteBucket] = max(minuteMaxLevel[currentMinuteBucket], lightLevel);
    }

    if (hourlyMinLevel[currentHourlyBucket] == LIGHT_BUCKET_NO_DATA) {
        hourlyMinLevel[currentHourlyBucket] = lightLevel;
        hourlyMaxLevel[currentHourlyBucket] = lightLevel;
    } else {
        hourlyMinLevel[currentHourlyBucket] = min(hourlyMinLevel[currentHourlyBucket], lightLevel);
        hourlyMaxLevel[currentHourlyBucket] = max(hourlyMaxLevel[currentHourlyBucket], lightLevel);
    }
}

/**
 * Advances to the next minute bucket, once a minute.
 */
bool rotateMinuteBucket(void *argument) {
    currentMinuteBucket = (currentMinuteBucket + 1) % MINUTE_BUCKET_COUNT;
    minuteMinLevel[currentMinuteBucket] = LIGHT_BUCKET_NO_DATA;
    minuteMaxLevel[currentMinuteBucket] = LIGHT_BUCKET_NO_DATA;
    return true;
}

/**
 * Advances to the next hourly bucket, once an hour.
 */
bool rotateHourlyBucket(void *argument) {
    currentHourlyBucket = (currentHourlyBucket + 1) % HOURLY_BUCKET_COUNT;
    hourlyMinLevel[currentHourlyBucket] = LIGHT_BUCKET_NO_DATA;
    hourlyMaxLevel[currentHourlyBucket] = LIGHT_BUCKET_NO_DATA;
    return true;
}

/**
 * Update light value and check whether to turn relay on or off
 */
void updateLightValue() {
    lightLevel = analogRead(A0);
    updateLightLevelBuckets();
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
 * Update light value and check whether to turn relay on or off
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

    if (lampMode == LAMP_MODE_ON) {
        if (relayState == 0) {
            switchRelayOn();
        }
    } else if (lampMode == LAMP_MODE_OFF) {
        if (relayState == 1) {
            switchRelayOff();
        }
    } else {
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
    }

    // keep timer running
    return true;
}

/**
 * Blocks until a full line is entered on Serial, echoing each typed character back.
 * Accepts '\r', '\n' or "\r\n"/"\n\r" as line ending, since terminals differ in what
 * they send for Enter; without this, Serial's built-in read timeout can otherwise cut
 * off input before Enter is pressed.
 */
String readSerialLine() {
    String line = "";
    while (true) {
        if (Serial.available()) {
            char c = Serial.read();
            if (c == '\r' || c == '\n') {
                delay(2);
                if (Serial.available()) {
                    char next = Serial.peek();
                    if ((next == '\r' || next == '\n') && next != c) {
                        Serial.read();
                    }
                }
                Serial.println();
                break;
            } else if (c == '\b' || c == 127) {
                if (line.length() > 0) {
                    line.remove(line.length() - 1);
                    Serial.print("\b \b");
                }
            } else {
                line += c;
                Serial.write(c);
            }
        } else {
            delay(10);
        }
    }
    line.trim();
    return line;
}

/**
 * Prompts for a new value on serial and writes it into the parameter's buffer.
 * Does not persist the change; call iotWebConf.saveConfig() afterwards.
 */
bool updateParameterFromSerial(iotwebconf::Parameter *parameter, const char *label) {
    Serial.print("New value for ");
    Serial.print(label);
    Serial.print(" (max ");
    Serial.print(parameter->getLength() - 1);
    Serial.println(" chars, empty = cancel):");
    Serial.print("> ");
    String value = readSerialLine();
    if (value.length() == 0) {
        Serial.println("Cancelled.");
        return false;
    }
    strncpy(parameter->valueBuffer, value.c_str(), parameter->getLength() - 1);
    parameter->valueBuffer[parameter->getLength() - 1] = '\0';
    Serial.println("Value updated (not yet saved).");
    return true;
}

/**
 * Prints a numbered menu entry followed by the parameter's current value, tab-aligned.
 * The number of tabs is calculated (assuming 8-column tab stops) so the value always
 * starts at the same column, regardless of how long the label is.
 */
void printMenuItem(const char *number, const char *label, iotwebconf::Parameter *parameter) {
    const int tabWidth = 8;
    const int targetColumn = 40;

    String prefix = String(number) + ") " + label;
    Serial.print(prefix);
    int column = prefix.length();
    do {
        Serial.print('\t');
        column += tabWidth - (column % tabWidth);
    } while (column < targetColumn);

    String value(parameter->valueBuffer);
    Serial.println(value.length() == 0 ? String("[not set]") : "[" + value + "]");
}

/**
 * Interactive serial menu to update WiFi/config portal credentials.
 * Only reachable via a physical USB/serial connection, never over WLAN.
 */
void runSerialSettingsMenu() {
    bool changed = false;
    while (true) {
        Serial.println();
        Serial.println("=== Settings menu ===");
        printMenuItem("1", "Set WiFi SSID", iotWebConf.getWifiSsidParameter());
        printMenuItem("2", "Set WiFi password", iotWebConf.getWifiPasswordParameter());
        printMenuItem("3", "Set config portal password", iotWebConf.getApPasswordParameter());
        Serial.println("4) Save and restart");
        Serial.println("5) Exit without saving");
        Serial.println("6) Reset all settings to defaults");
        Serial.print("> ");

        String choice = readSerialLine();

        if (choice == "1") {
            changed |= updateParameterFromSerial(iotWebConf.getWifiSsidParameter(), "WiFi SSID");
        } else if (choice == "2") {
            changed |= updateParameterFromSerial(iotWebConf.getWifiPasswordParameter(), "WiFi password");
        } else if (choice == "3") {
            changed |= updateParameterFromSerial(iotWebConf.getApPasswordParameter(), "config portal password");
        } else if (choice == "4") {
            if (changed) {
                iotWebConf.saveConfig();
                Serial.println("Settings saved. Restarting...");
                delay(500);
                ESP.restart();
            } else {
                Serial.println("Nothing changed.");
            }
        } else if (choice == "5") {
            Serial.println("Exiting settings menu without saving.");
            return;
        } else if (choice == "6") {
            iotWebConf.getSystemParameterGroup()->applyDefaultValue();
            changed = true;
            Serial.println("All settings reset to defaults (not yet saved).");
        } else {
            Serial.println("Invalid option.");
        }
    }
}

void configSaved() {
    Serial.println("config saved");
    needReset = true;
}

/**
 * Waits until the system clock reports a plausible time (i.e. NTP sync succeeded)
 * or the timeout elapses. Returns true if the sync was detected in time.
 */
bool waitForTimeSync(unsigned long timeoutMs) {
    unsigned long start = millis();
    while (time(nullptr) < NTP_SYNC_MIN_EPOCH) {
        if (millis() - start > timeoutMs) {
            return false;
        }
        delay(200);
    }
    return true;
}

/**
 * Synchronizes the system time via NTP, but only while online (WiFi connected in
 * station mode, not serving the config access point). After the first successful
 * sync, the resulting time is printed to serial.
 */
void syncTimeViaNtp() {
    if (iotWebConf.getState() != iotwebconf::OnLine) {
        return;
    }

    Serial.print("Synchronizing time via NTP server ");
    Serial.print(settingNtpServerParam.value());
    Serial.print(" (timezone: ");
    Serial.print(settingTimezoneParam.value());
    Serial.println(")");
    configTime(settingTimezoneParam.value(), settingNtpServerParam.value());

    if (!initialTimeSyncDone) {
        if (waitForTimeSync(10000)) {
            Serial.print("Time synchronized: ");
            Serial.println(getCurrentTimeString());
            initialTimeSyncDone = true;
        } else {
            Serial.println("Time sync did not complete within timeout; will retry.");
        }
    }
}

bool ntpDailySyncTimerCallback(void *argument) {
    syncTimeViaNtp();
    return true;
}

/**
 * Returns the current local time (per the configured timezone) formatted as
 * "YYYY-MM-DD HH:MM:SS ZZZ", or a placeholder string if NTP has not synchronized yet.
 */
String getCurrentTimeString() {
    time_t now = time(nullptr);
    if (now < NTP_SYNC_MIN_EPOCH) {
        return "not yet synchronized";
    }
    char timeBuffer[32];
    strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S %Z", localtime(&now));
    return String(timeBuffer);
}

void wifiConnected() {
    connected = true;
    Serial.println("### WiFi connected ###");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    syncTimeViaNtp();
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

    DashboardData data;
    data.relayState = relayState;
    data.lightLevel = lightLevel;
    data.darkLevel = settingDarkLevelParam.value();
    data.delaySeconds = settingDelayParam.value();
    data.currLightConditionCycles = currLightConditionCycles;
    data.lampMode = lampMode;
    data.ntpServer = settingNtpServerParam.value();
    data.timezone = settingTimezoneParam.value();
    data.wifiSsid = iotWebConf.getWifiSsidParameter()->valueBuffer;
    data.currentTimeString = getCurrentTimeString();
    data.minuteMinLevel = minuteMinLevel;
    data.minuteMaxLevel = minuteMaxLevel;
    data.minuteBucketCount = MINUTE_BUCKET_COUNT;
    data.currentMinuteBucket = currentMinuteBucket;
    data.hourlyMinLevel = hourlyMinLevel;
    data.hourlyMaxLevel = hourlyMaxLevel;
    data.hourlyBucketCount = HOURLY_BUCKET_COUNT;
    data.currentHourlyBucket = currentHourlyBucket;

    server.send(200, "text/html", renderDashboardPage(data));
}

/**
 * Handle requests to "/mode" to switch between automatic and manual lamp control.
 * Not persisted: lampMode always resets to LAMP_MODE_AUTO on boot.
 */
void handleSetMode() {
    String value = server.arg("value");
    if (value == "on") {
        lampMode = LAMP_MODE_ON;
    } else if (value == "off") {
        lampMode = LAMP_MODE_OFF;
    } else {
        lampMode = LAMP_MODE_AUTO;
    }
    server.send(204);
}