/*
 *  DashboardPage.h -- Renders the Lamp Control status/config dashboard page.
 */
#ifndef DashboardPage_h
#define DashboardPage_h

#include <Arduino.h>
#include <time.h>

// epoch time of 2020-01-01 00:00:00 UTC, used to detect a successful NTP sync
const time_t NTP_SYNC_MIN_EPOCH = 1577836800;

// sentinel value for a light level bucket that has not received a reading yet
const int LIGHT_BUCKET_NO_DATA = -1;

enum LampMode {
    LAMP_MODE_AUTO,
    LAMP_MODE_ON,
    LAMP_MODE_OFF
};

// All the live values needed to render the dashboard page. Populated by the
// caller from current sensor/config/state; this module only turns it into HTML.
struct DashboardData {
    int relayState;
    int lightLevel;
    int darkLevel;
    int delaySeconds;
    int currLightConditionCycles;
    LampMode lampMode;

    const char *ntpServer;
    const char *timezone;
    const char *wifiSsid;
    String currentTimeString;

    const int *minuteMinLevel;
    const int *minuteMaxLevel;
    int minuteBucketCount;
    int currentMinuteBucket;

    const int *hourlyMinLevel;
    const int *hourlyMaxLevel;
    int hourlyBucketCount;
    int currentHourlyBucket;
};

String renderDashboardPage(const DashboardData &data);

#endif
