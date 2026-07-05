# lamp-control-esp8266

A small ESP8266 (NodeMCU) firmware that automatically switches a lamp on or off based on ambient light, with WiFi configuration, a live web dashboard, manual override, and NTP-synced local time.

## Features

- **Automatic light-based switching** — turns the relay on when the light level drops below a configurable "dark" threshold, and off again once it's bright, with a configurable delay to avoid rapid toggling around the threshold.
- **Manual override (Auto / On / Off)** — switch the lamp manually from the web dashboard. Not persisted: the device always starts in `Auto` mode after every reboot.
- **Live web dashboard** — current light level, relay state, time until the next allowed switch, and rolling light-level history charts (last 15 minutes and last 24 hours, min/max per bucket), rendered as inline SVG — no external JS libraries or CDNs.
- **Auto-refreshing UI** — configurable refresh interval (5 / 15 / 30 / 60 s), remembered in the browser between reloads.
- **WiFi configuration via [IotWebConf](https://github.com/prampec/IotWebConf)** — falls back to its own access-point/captive-portal for setup if no WiFi is configured or reachable, and reconnects automatically afterwards.
- **NTP time sync** — configurable NTP server (default `pool.ntp.org`) and POSIX timezone string (default Central European Time), synced once at boot and once daily.
- **Interactive serial configuration menu** — reachable only via a physical USB/serial connection (never over WiFi): change WiFi credentials or the config portal password, or factory-reset all settings.

## Hardware

| NodeMCU pin | GPIO | Connected to |
|---|---|---|
| `A0` | ADC0 | Light sensor (LDR) voltage divider |
| `D5` | GPIO14 | Relay module `IN` (active low: `LOW` = relay on) |
| `D8` | GPIO15 | Status LED (active high, through a resistor) |

### Wiring diagram

```
NodeMCU (ESP8266)
-----------------
  A0            <---- midpoint of the LDR voltage divider (see below)
  D5  (GPIO14)  ----> Relay module IN            (active low: LOW = relay on)
  D8  (GPIO15)  ----> [resistor] ---> LED ---> GND
  3V3 / 5V      ----> Relay module VCC
  GND           ----> Relay module GND, LED cathode, divider GND

Light sensor (voltage divider):

  3V3 ---[ LDR ]---+---[ 10k resistor ]--- GND
                    |
                    +---> A0

Relay module:

  IN ---- from D5        Relay module switches the lamp's mains/DC supply.
  VCC --- from 3V3/5V     (Check your specific module's voltage requirement.)
  GND --- from NodeMCU GND
  OUT ---- to Lamp
```

The light sensor is wired as a voltage divider (LDR + fixed resistor, e.g. 10 kΩ) between `3V3` and `GND`, with the midpoint fed into `A0`. The relay module and LED share `GND` with the NodeMCU; the relay module's `VCC` is powered from `3V3` or `5V` depending on the module.

## Usage

### 1. Build and flash

```
pio run --target upload
```

### 2. First boot — connect to the access point

On first boot (or after a factory reset / config reset), the device has no WiFi configured and opens its own access point:

- SSID: `LampControl`
- No password by default

Connect to it with a phone or laptop; a captive portal should open automatically (or browse to `192.168.4.1`).

### 3. Configure values

On the config page, set:

- WiFi SSID / password (to join your network)
- Config portal (AP) password (protects this config page itself)
- **Delay switch seconds** — how long the light level must stay in a new state before the relay switches
- **Dark level** — light level (0–1023) below which it's considered "dark"
- **NTP server** — default `pool.ntp.org`
- **Timezone** — POSIX TZ string, default `CET-1CEST,M3.5.0,M10.5.0/3` (Europe/Berlin)

After saving, the device restarts and joins your WiFi. Its dashboard is then reachable at its IP address (shown on the serial console on connect) on port 80.

### Alternative: configure via serial port

Instead of (or in addition to) the web config page, you can configure WiFi credentials and the config portal password over a **physical USB/serial connection only** — this path is intentionally never exposed over WiFi:

1. Connect via USB and open a serial terminal at **115200 baud**.
2. Within 30 seconds of boot, press `s` + Enter to open the settings menu.
3. Follow the on-screen menu to set the WiFi SSID, WiFi password, or config portal password, or to reset all settings to defaults.

## License

MIT — see [LICENSE](LICENSE).
