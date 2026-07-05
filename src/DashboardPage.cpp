/*
 *  DashboardPage.cpp -- Renders the Lamp Control status/config dashboard page.
 */
#include "DashboardPage.h"
#include <functional>

namespace {

const char PAGE_HEAD[] PROGMEM = R"HTML(<!DOCTYPE html><html lang="en"><head>
<meta charset="UTF-8">
<title>Lamp Control</title>
<meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
<style>
  :root {
    --bg: #12132a;
    --card-bg: #1f2244;
    --track-bg: #363a66;
    --text-primary: #f5f6fb;
    --text-muted: #9296b8;
    --accent-cyan: #29d3ff;
    --accent-green: #35d488;
    --radius: 14px;
  }
  * { box-sizing: border-box; }
  body {
    margin: 0;
    background: var(--bg);
    color: var(--text-primary);
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif;
    min-height: 100vh;
    display: flex;
    flex-direction: column;
  }
  .dashboard {
    display: grid;
    grid-template-columns: 1fr 1.68fr 1fr;
    grid-template-rows: auto auto;
    gap: 16px;
    padding: 16px;
    max-width: 1400px;
    margin: 0 auto;
    width: 100%;
    flex: 1;
  }
  .card {
    background: var(--card-bg);
    border-radius: var(--radius);
    padding: 20px 24px;
    display: flex;
    flex-direction: column;
  }
  .card-title {
    font-size: 0.78rem;
    font-weight: 700;
    letter-spacing: 0.03em;
    color: var(--text-primary);
    margin: 0 0 12px 0;
    text-transform: uppercase;
  }
  .big-number {
    font-size: 3rem;
    font-weight: 700;
    line-height: 1;
    margin: 8px 0;
  }
  .big-number .unit {
    font-size: 1.4rem;
    font-weight: 600;
    color: var(--text-muted);
  }
  .sub-label {
    color: var(--text-muted);
    font-size: 0.9rem;
    margin-top: 4px;
  }
  .progress-track {
    height: 6px;
    border-radius: 3px;
    background: var(--track-bg);
    margin-top: 24px;
    overflow: hidden;
  }
  .progress-fill {
    height: 100%;
    background: #29d3ff;
    border-radius: 3px;
  }
  .progress-labels {
    display: flex;
    justify-content: space-between;
    font-size: 0.75rem;
    color: #29d3ff;
    margin-top: 4px;
  }
  .status-on { color: #35d488; }
  .status-off { color: var(--text-muted); }
  .settings-list {
    list-style: none;
    margin: 0;
    padding: 0;
    display: flex;
    flex-direction: column;
  }
  .settings-list li {
    display: flex;
    justify-content: space-between;
    align-items: center;
    gap: 12px;
    padding: 10px 0;
    border-bottom: 1px solid rgba(255,255,255,0.06);
    font-size: 0.92rem;
  }
  .settings-list li:last-child { border-bottom: none; }
  .mode-select {
    background: var(--track-bg);
    color: var(--text-primary);
    border: none;
    border-radius: 6px;
    padding: 6px 10px;
    font-size: 0.9rem;
    font-family: inherit;
  }
  .settings-list .val {
    color: var(--text-muted);
    text-align: right;
    max-width: 60%;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
  }
  .settings-link {
    display: block;
    margin-top: 16px;
    padding-top: 14px;
    border-top: 1px solid rgba(255,255,255,0.06);
    color: #29d3ff;
    text-decoration: none;
    font-size: 0.9rem;
    font-weight: 600;
  }
  .settings-link:hover { text-decoration: underline; }
  .chart-svg { width: 100%; height: 100%; min-height: 160px; flex: 1; margin-top: 4px; }
  .chart-label { fill: var(--text-muted); font-size: 9px; font-family: inherit; }
  .chart-axis { stroke: rgba(255,255,255,0.18); stroke-width: 1; }
  .chart-grid { stroke: rgba(255,255,255,0.08); stroke-width: 1; }
  .chart-empty { color: var(--text-muted); font-size: 0.85rem; margin-top: 12px; }
  footer {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 16px 24px 24px;
    max-width: 1400px;
    margin: 0 auto;
    width: 100%;
    color: var(--text-muted);
    font-size: 0.9rem;
  }
  footer .brand {
    display: flex;
    align-items: center;
    gap: 10px;
    font-size: 1.1rem;
    font-weight: 700;
    color: var(--text-primary);
  }
  footer .dot {
    width: 10px; height: 10px; border-radius: 50%; background: #35d488;
    display: inline-block;
  }
  .control-bar {
    max-width: 1400px;
    margin: 0 auto;
    width: 100%;
    padding: 16px 16px 0;
  }
  .control-bar-inner {
    background: var(--card-bg);
    border-radius: var(--radius);
    padding: 12px 24px;
    display: flex;
    align-items: center;
    gap: 12px;
    font-size: 0.9rem;
  }
  .control-bar label {
    color: var(--text-muted);
    font-weight: 600;
  }
  .control-bar select {
    background: var(--track-bg);
    color: var(--text-primary);
    border: none;
    border-radius: 6px;
    padding: 6px 10px;
    font-size: 0.9rem;
    font-family: inherit;
  }
  .control-bar .countdown {
    color: var(--accent-cyan);
    margin-left: auto;
  }
  .grid-relay    { grid-column: 1; grid-row: 1; }
  .grid-15min    { grid-column: 2; grid-row: 1; }
  .grid-24h      { grid-column: 2; grid-row: 2; }
  .grid-seconds  { grid-column: 1; grid-row: 2; }
  .grid-settings { grid-column: 3; grid-row: 1 / span 2; }
  @media (max-width: 900px) {
    .dashboard { grid-template-columns: 1fr; }
    .grid-relay, .grid-15min, .grid-24h, .grid-seconds, .grid-settings {
      grid-column: 1;
      grid-row: auto;
    }
    .big-number { font-size: 2.4rem; }
  }
</style>
</head>
<body>
<div class="control-bar">
  <div class="control-bar-inner">
    <label for="refreshInterval">Auto refresh</label>
    <select id="refreshInterval">
      <option value="5">5s</option>
      <option value="15">15s</option>
      <option value="30" selected>30s</option>
      <option value="60">60s</option>
    </select>
    <span id="refreshCountdown" class="countdown">Refreshing in 30s</span>
  </div>
</div>
<div class="dashboard">
)HTML";

const char PAGE_SCRIPT[] PROGMEM = R"HTML(<script>
(function () {
  var STORAGE_KEY = 'lampControlRefreshInterval';
  var select = document.getElementById('refreshInterval');
  var countdownEl = document.getElementById('refreshCountdown');

  var storedValue = localStorage.getItem(STORAGE_KEY);
  if (storedValue && select.querySelector('option[value="' + storedValue + '"]')) {
    select.value = storedValue;
  }

  var remaining = parseInt(select.value, 10);

  function updateLabel() {
    countdownEl.textContent = 'Refreshing in ' + remaining + 's';
  }

  select.addEventListener('change', function () {
    remaining = parseInt(select.value, 10);
    localStorage.setItem(STORAGE_KEY, select.value);
    updateLabel();
  });

  updateLabel();

  setInterval(function () {
    remaining -= 1;
    if (remaining <= 0) {
      location.reload();
      return;
    }
    updateLabel();
  }, 1000);

  var modeSelect = document.getElementById('modeSelect');
  modeSelect.addEventListener('change', function () {
    fetch('/mode?value=' + modeSelect.value).then(function () {
      location.reload();
    });
  });
})();
</script>
</body>
</html>
)HTML";

/**
 * Renders a min/max range-bar chart as inline SVG. Auto-scales to the actual
 * observed value range (with a little padding) so small fluctuations stay visible.
 * Buckets are read starting right after currentIndex (the oldest surviving data)
 * through currentIndex itself (the newest, still-filling bucket).
 */
String renderRangeChartSvg(
    const int *minValues, const int *maxValues, int bucketCount, int currentIndex,
    std::function<String(int)> tooltipLabel,
    std::function<String(int)> axisLabel,
    const char *barColor
) {
    bool hasData = false;
    int globalMin = 0;
    int globalMax = 0;
    for (int i = 0; i < bucketCount; i++) {
        if (minValues[i] == LIGHT_BUCKET_NO_DATA) {
            continue;
        }
        if (!hasData) {
            globalMin = minValues[i];
            globalMax = maxValues[i];
            hasData = true;
        } else {
            if (minValues[i] < globalMin) globalMin = minValues[i];
            if (maxValues[i] > globalMax) globalMax = maxValues[i];
        }
    }

    if (!hasData) {
        return "<p class=\"chart-empty\">No data yet</p>";
    }

    int range = globalMax - globalMin;
    if (range < 10) {
        range = 10;
    }
    int padding = range / 10;
    int scaleMin = globalMin - padding;
    int scaleMax = globalMax + padding;
    if (scaleMin < 0) {
        scaleMin = 0;
    }
    if (scaleMax == scaleMin) {
        scaleMax = scaleMin + 1;
    }

    const int leftMargin = 32;
    const int rightMargin = 18;
    const int plotTop = 10;
    const int plotBottom = 115;
    const int viewWidth = 340;
    int plotLeft = leftMargin;
    int plotRight = viewWidth - rightMargin;
    int plotWidth = plotRight - plotLeft;

    String svg = "<svg viewBox=\"0 0 340 150\" class=\"chart-svg\" preserveAspectRatio=\"xMidYMin meet\">";

    for (int t = 0; t <= 3; t++) {
        float y = plotTop + t * (plotBottom - plotTop) / 3.0f;
        int value = scaleMax - t * (scaleMax - scaleMin) / 3;
        svg += "<line x1=\"" + String(plotLeft) + "\" y1=\"" + String(y, 1) + "\" x2=\"" + String(plotRight) + "\" y2=\"" + String(y, 1) + "\" class=\"chart-grid\"/>";
        svg += "<text x=\"" + String(plotLeft - 5) + "\" y=\"" + String(y + 3, 1) + "\" text-anchor=\"end\" class=\"chart-label\">" + String(value) + "</text>";
    }
    svg += "<line x1=\"" + String(plotLeft) + "\" y1=\"" + String(plotTop) + "\" x2=\"" + String(plotLeft) + "\" y2=\"" + String(plotBottom) + "\" class=\"chart-axis\"/>";

    float slot = (float)plotWidth / bucketCount;
    float barWidth = bucketCount > 18 ? 5.0f : 8.0f;

    for (int i = 0; i < bucketCount; i++) {
        int idx = (currentIndex + 1 + i) % bucketCount;
        if (minValues[idx] == LIGHT_BUCKET_NO_DATA) {
            continue;
        }
        float x = plotLeft + slot * i + slot / 2.0f;
        float y1 = plotBottom - ((float)(minValues[idx] - scaleMin) / (float)(scaleMax - scaleMin)) * (plotBottom - plotTop);
        float y2 = plotBottom - ((float)(maxValues[idx] - scaleMin) / (float)(scaleMax - scaleMin)) * (plotBottom - plotTop);
        svg += "<line x1=\"" + String(x, 1) + "\" y1=\"" + String(y1, 1) + "\" y2=\"" + String(y2, 1) + "\" x2=\"" + String(x, 1) +
               "\" stroke=\"" + barColor + "\" stroke-width=\"" + String(barWidth, 0) + "\" stroke-linecap=\"round\"><title>" +
               tooltipLabel(i) + ": min " + String(minValues[idx]) + " / max " + String(maxValues[idx]) + "</title></line>";
        String label = axisLabel(i);
        if (label.length() > 0) {
            const char *anchor = (i == 0) ? "start" : (i == bucketCount - 1 ? "end" : "middle");
            svg += "<text x=\"" + String(x, 1) + "\" y=\"140\" text-anchor=\"" + anchor + "\" class=\"chart-label\">" + label + "</text>";
        }
    }

    svg += "</svg>";
    return svg;
}

String renderMinuteChart(const DashboardData &data) {
    int bucketCount = data.minuteBucketCount;
    return renderRangeChartSvg(
        data.minuteMinLevel, data.minuteMaxLevel, bucketCount, data.currentMinuteBucket,
        [bucketCount](int i) { return "-" + String(bucketCount - i) + "min"; },
        [bucketCount](int i) {
            if (i == 0) return "-" + String(bucketCount) + "min";
            if (i == bucketCount - 1) return String("now");
            return String("");
        },
        "#29d3ff"
    );
}

String hourlyTooltipLabel(int chronologicalIndex, int bucketCount) {
    int hoursAgo = bucketCount - 1 - chronologicalIndex;
    time_t now = time(nullptr);
    if (now >= NTP_SYNC_MIN_EPOCH) {
        time_t bucketTime = now - (time_t)hoursAgo * 3600;
        char buf[6];
        strftime(buf, sizeof(buf), "%H:00", localtime(&bucketTime));
        return String(buf);
    }
    return "-" + String(hoursAgo) + "h";
}

String renderHourlyChart(const DashboardData &data) {
    int bucketCount = data.hourlyBucketCount;
    return renderRangeChartSvg(
        data.hourlyMinLevel, data.hourlyMaxLevel, bucketCount, data.currentHourlyBucket,
        [bucketCount](int i) { return hourlyTooltipLabel(i, bucketCount); },
        [bucketCount](int i) {
            if (i == 0) return String("-24h");
            if (i == bucketCount - 1) return String("now");
            if (i == bucketCount / 2) return String("-12h");
            return String("");
        },
        "#29d3ff"
    );
}

String modeOption(const char *value, const char *label, LampMode current, LampMode optionMode) {
    String html = "<option value=\"" + String(value) + "\"";
    if (current == optionMode) {
        html += " selected";
    }
    html += ">" + String(label) + "</option>";
    return html;
}

} // namespace

String renderDashboardPage(const DashboardData &data) {
    String html;
    html.reserve(6000);
    html += FPSTR(PAGE_HEAD);

    // Light status card
    html += "<div class=\"card grid-relay\"><p class=\"card-title\">Light status</p>";
    html += data.relayState ? "<div class=\"big-number status-on\">ON</div>" : "<div class=\"big-number status-off\">OFF</div>";
    html += "<p class=\"sub-label\">Light level: " + String(data.lightLevel) + "</p>";
    int percent = constrain(map(data.lightLevel, 0, 1023, 0, 100), 0, 100);
    html += "<div class=\"progress-track\"><div class=\"progress-fill\" style=\"width: " + String(percent) + "%\"></div></div>";
    html += "<div class=\"progress-labels\"><span>" + String(percent) + "%</span><span>Dark threshold: " + String(data.darkLevel) + "</span></div>";
    html += "</div>";

    // 15-minute chart card
    html += "<div class=\"card grid-15min\"><p class=\"card-title\">Light level - last 15 minutes (min/max)</p>";
    html += renderMinuteChart(data);
    html += "</div>";

    // 24-hour chart card
    html += "<div class=\"card grid-24h\"><p class=\"card-title\">Light level - last 24 hours (min/max)</p>";
    html += renderHourlyChart(data);
    html += "</div>";

    // Seconds-in-state card
    html += "<div class=\"card grid-seconds\"><p class=\"card-title\">Seconds in current state</p>";
    html += "<div class=\"big-number\">" + String(data.currLightConditionCycles) + "<span class=\"unit\">s</span></div>";
    html += "<p class=\"sub-label\">of " + String(data.delaySeconds) + "s until switch is allowed</p>";
    html += "</div>";

    // Settings card
    html += "<div class=\"card grid-settings\"><p class=\"card-title\">Settings</p><ul class=\"settings-list\">";
    html += "<li><span>Mode</span><select id=\"modeSelect\" class=\"mode-select\">";
    html += modeOption("auto", "Auto", data.lampMode, LAMP_MODE_AUTO);
    html += modeOption("on", "On", data.lampMode, LAMP_MODE_ON);
    html += modeOption("off", "Off", data.lampMode, LAMP_MODE_OFF);
    html += "</select></li>";
    html += "<li><span>Delay switch seconds</span><span class=\"val\">" + String(data.delaySeconds) + "</span></li>";
    html += "<li><span>Dark level</span><span class=\"val\">" + String(data.darkLevel) + "</span></li>";
    html += "<li><span>NTP server</span><span class=\"val\">" + String(data.ntpServer) + "</span></li>";
    html += "<li><span>Timezone</span><span class=\"val\">" + String(data.timezone) + "</span></li>";
    html += "<li><span>WiFi SSID</span><span class=\"val\">" + String(data.wifiSsid) + "</span></li>";
    html += "<li><span>Admin user</span><span class=\"val\">admin</span></li>";
    html += "</ul><a href=\"config\" class=\"settings-link\">Go to configure page</a></div>";

    html += "</div>"; // close .dashboard

    html += "<footer><div class=\"brand\"><span class=\"dot\"></span> Lamp Control</div><div>" + data.currentTimeString + "</div></footer>";

    html += FPSTR(PAGE_SCRIPT);

    return html;
}
