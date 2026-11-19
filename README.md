# Matrix Clock (SandClock)

Firmware and web UI for an ESP8266 (WeMos D1 mini) clock on a 32x8 WS2812B LED matrix. It is based on the 7-Segment-Digital-Clock-V2 project; the original build notes are kept at the end.

<p>
  <img src="docs/clock-time.jpg" width="49%" alt="The clock showing 8:00 in red through a wood front panel">
  <img src="docs/clock-temperature.jpg" width="49%" alt="The clock showing an outdoor temperature of 0 degrees in cyan">
</p>

- Time from NTP with POSIX timezone and DST rules, shown until synced as `--:--`
- Outdoor temperature from a Home Assistant sensor, with a cycle (time 5 s, temperature 2 s) or combined display
- Scrolling messages from the web UI or the HTTP API
- Hourly and half-hourly chime, optionally muted when the light sensor says it is dark
- Automatic brightness from an LDR on `A0`
- Web UI at `http://sandclock.local` with live previews of the clock face, plus a device info panel
- Optional password, OTA updates, and settings that survive firmware and filesystem flashes
- Status in Home Assistant (light level, WiFi signal, problems, offline alerts) from a ready-made package
- Diagnostics (WiFi name, IP address, firmware) scrolled on the clock after each boot, or on demand by covering the light sensor (or flashing a light at it) three times

## Build and flash

Needs [PlatformIO](https://platformio.org/).

```
cp src/secrets.h.example src/secrets.h   # WiFi used on first boot, before it is set in the web UI
pio run -t upload                        # firmware, over USB (first time only)
pio run -t uploadfs                      # web UI files, over USB (first time only)
```

After that, flash over WiFi with the deploy script. It builds, flashes the firmware and the filesystem image, waits for each reboot, then checks the running revision, that the UI is served, and that your settings did not change:

```
tools/deploy.sh                # firmware, then filesystem
tools/deploy.sh --fw           # firmware only
tools/deploy.sh --fs           # web UI only
tools/deploy.sh --dry-run      # read the clock, change nothing
CLOCK_PASSWORD='...' tools/deploy.sh   # if a password is set
```

`src/vars.h` has the semantic `FW_VERSION`. The exact commit (`git describe`, with `-dirty` for uncommitted changes) is embedded at build time and shown in `/info` and the Device info panel.

## Settings and persistence

Settings (WiFi, Home Assistant, colors, timezone, display, buzzer, password) live in the flash sector reserved for EEPROM, which sits after the filesystem. Flashing a firmware or filesystem image does not touch it. A full chip erase does, after which the clock starts with defaults and the WiFi credentials compiled in from `src/secrets.h`.

If the clock cannot join WiFi within 20 seconds of boot it opens a WPA2 access point named `SandClock` (web UI at `http://192.168.4.1`) while it keeps retrying your network every 30 seconds. The access point goes away once WiFi connects.

The access point password is random (10 characters, no look-alikes such as `0`/`O` or `1`/`I`), created the first time it is needed and stored with the other settings. While nobody has joined, the clock scrolls `AP PW <password>` across the matrix once a minute at a slow, readable pace. It is also shown in the web UI's Security card, where you can generate a new one. While a device is joined to the access point, the clock pauses its WiFi retries (one radio cannot serve both reliably), resuming about ten seconds after the last device leaves. Saving WiFi credentials from the setup page reboots the clock, which then tries the new network before opening the access point again.

Joining phones get a captive-portal redirect: unknown URLs are pointed at `http://192.168.4.1/`, so opening the setup page is usually automatic. `http://sandclock.local` also works while the access point is up.

## Safe mode

A firmware that crashes while starting cannot be fixed over WiFi, because it never stays up long enough to receive an update. The clock counts crashed starts (exception or watchdog resets) in RTC memory. After three in a row it starts in safe mode: only WiFi (or the setup access point), a status page and OTA run. The matrix stays dark and the board's LED blinks fast. The status page at `http://sandclock.local/` shows the reset details; `/info` reports `safeMode`, `crashCount` and `resetInfo`.

Upload a working firmware (`tools/deploy.sh`, or the `/update` page) and the clock returns to normal. "Try a normal start" on the status page also resets the count. Two minutes of stable running clear the count, and a power cycle clears it too (RTC memory does not survive power loss).

A different failure mode is the clock running fine but its network stack wedged, which neither the watchdog nor safe mode catches. If the clock is unreachable and unsynced for 30 minutes it reboots itself.

To try it, crash the clock on purpose three times, waiting for each restart:

```
for i in 1 2 3; do curl -u admin:PASSWORD -X POST http://sandclock.local/debug/crash -d type=exception; sleep 25; done
```

`type=wdt` hangs the clock until the software watchdog resets it instead.

## Security

By default the web UI and API are open to anyone on your network. Set a password in the Security card to turn on HTTP Basic auth for every API call, the index page and the OTA page. The user name is always `admin`; the password is 8 to 31 characters. Setting or clearing it reboots the clock.

Basic auth over plain HTTP sends the password unencrypted and there is no brute-force throttling, so this keeps casual and accidental access out, not an attacker on your LAN.

If you forget the password, erase the flash over USB and set the clock up again (this also erases all other settings):

```
pio run -t erase
pio run -t upload && pio run -t uploadfs
```

## HTTP API

Use `curl -u admin:PASSWORD` when a password is set. POST bodies are form-encoded.

| Method | Path | Parameters |
| --- | --- | --- |
| GET | `/info` | Runtime status: network, time, NTP, Home Assistant, light, memory, flash |
| GET | `/config` | Settings (the WiFi password and HA token are masked) |
| POST | `/config/clockcolor`, `/config/tempcolor`, `/config/othercolor` | `r`, `g`, `b` (0-255). `othercolor` is the scrolling message color |
| POST | `/config/brightness` | `brightness` 0-255 |
| POST | `/config/autobrightness` | `autoBrightness` `true`/`false` |
| POST | `/config/hourformat` | `hourFormat` 12 or 24 |
| POST | `/config/displaystyle` | `displayStyle` 1 (cycle) or 2 (combined) |
| POST | `/config/chime` | any of `chimeEnabled`, `chimeMuteDark` (`true`/`false`), `chimeDarkThreshold` (0-1023) |
| POST | `/config/timezones` | `tzName`, `tzValue` (POSIX TZ string, for example `MDT6`) |
| POST | `/config/hassConfig` | `hassBaseUrl`, `hassEntityId`, `hassAT` |
| POST | `/config/wifiConfig` | `wifi_ssid`, `wifi_password` |
| POST | `/config/auth` | `authPassword` (8-31 characters, empty turns auth off) |
| POST | `/config/ap` | Generate a new setup access point password |
| POST | `/debug/crash` | `type` `exception` or `wdt`: crash on purpose to test safe mode |
| POST | `/showmessage` | `messageText`, optional `repeatCount` 1-100, `scrollDelay` 10-1000 ms |
| POST | `/sound` | `type` `single` or `double` |
| GET | `/syncTime` | Restart NTP and sync now |
| GET | `/reboot` | Restart |
| POST | `/forcetemp`, `/forcetime` | Debug: `forceTemp` degrees, `forceTime` epoch in milliseconds. SNTP puts the real time back |
| GET/POST | `/update` | OTA page. Upload field `firmware` or `filesystem` |

```
curl -u admin:PASSWORD -X POST http://sandclock.local/showmessage \
     -d messageText="HELLO" -d repeatCount=2
```

## Buzzer and the light sensor

The chime can be switched off, and "mute when dark" silences all automatic beeps (the chime and the message start/end beeps) while the smoothed light reading is below the threshold. The Buzzer card shows the live sensor reading to help you pick the threshold; lower readings mean darker. The Sound buttons in the web UI always beep.

## Diagnostics on the clock

For when the web UI is out of reach (new network, unknown IP address), the clock can show its status on the matrix:

- **After every boot** it scrolls `WIFI <name>  IP <address>  FW <version>` once, quietly. Without WiFi it shows `NO WIFI <name>` and, while the setup access point is up, its name, password and address.
- **On demand**, cover the light sensor with your hand three times in a row, or in a dark room flash a light at it (a phone flashlight) three times. Each cover or flash and each gap should last about a quarter to one second, all within a few seconds. The clock beeps and scrolls the same details plus its uptime, and anything that needs attention (`HA ERR <code>`, `NO TIME`).

Both gestures are measured against the recent light level: a cover has to drop the reading below half, a flash has to at least double it. So covers work wherever there is some light (not below a level of 60), and flashes work in dim and dark rooms. Lights switching on or off, people walking past, headlights, a single shadow and lightning do not trigger them, and mixing covers and flashes in one sequence does not count. After a gesture the sensor is ignored for 10 seconds. `/info` reports `ldrNow` (the live 20 Hz reading) and `gestureBaseline` (the learned light level) for tuning.

## Home Assistant

The clock reads its outdoor temperature from Home Assistant (see the Temperature card). In the other direction, Home Assistant can show the clock's status: [docs/home-assistant/sandclock.yaml](docs/home-assistant/sandclock.yaml) is a ready-made package that polls `/info` every 30 seconds and creates:

| Entity | Meaning |
| --- | --- |
| `sensor.sandclock_light_level` | Room light from the LDR, 0 (dark) to 1023 (bright), smoothed over about 20 s |
| `sensor.sandclock_wifi_signal` | WiFi signal in dBm |
| `sensor.sandclock_uptime` | Seconds since the last boot |
| `sensor.sandclock_firmware` | Running firmware version |
| `binary_sensor.sandclock_temperature_fetch` | On when the temperature fetch from Home Assistant is failing |
| `binary_sensor.sandclock_time_sync` | On when the clock has no time from NTP |
| `binary_sensor.sandclock_safe_mode` | On when the clock is in safe mode |

Installation steps are at the top of the file: enable packages, copy the file, put in the clock's IP address (give the clock a DHCP reservation; `.local` names do not always resolve in Home Assistant) and add the clock's admin password to `secrets.yaml`. No firmware setting is involved.

When the clock stops answering, all of these turn `unavailable`. An offline alert, for example:

```yaml
automation:
  - alias: SandClock offline
    triggers:
      - trigger: state
        entity_id: sensor.sandclock_uptime
        to: unavailable
        for: "00:05:00"
    actions:
      - action: notify.notify
        data:
          message: The SandClock has not answered for 5 minutes.
```

## Tools

| File | Purpose |
| --- | --- |
| `tools/deploy.sh` | Build, OTA flash, verify (see above) |
| `tools/render.py` | Terminal mock of the matrix using the real glyphs from `src/digits.h`, for example `python3 tools/render.py combined --time 14:08 --temp -4` |
| `tools/gen_fonts_js.py` | Regenerates `data/js/fonts.js` (used by the web UI preview) after editing `src/digits.h` |
| `tools/git_rev.py` | PlatformIO pre-build script that embeds `git describe` |

## Tests and CI

`tests/run.sh` runs the host-side checks: JavaScript and shell syntax, `fonts.js` matches `digits.h`, the web UI preview draws the same frames as `tools/render.py` (which mirrors `src/display.h`), the Home Assistant reply parser (`src/ha_parse.h`) handles odd replies, the safe mode decision logic (`src/safe_mode_logic.h`), and the light-sensor gesture (`src/light_gesture_logic.h`). Run `pio run` once first so ArduinoJson is available. GitHub Actions (`.github/workflows/ci.yml`) builds both images and runs these checks on every push.

When you change a layout in `src/display.h`, update `tools/render.py` and `data/js/preview.js` too; the preview test fails if they drift.

## License

<a rel="license" href="http://creativecommons.org/licenses/by-nc-sa/4.0/"><img alt="Creative Commons License" style="border-width:0" src="https://i.creativecommons.org/l/by-nc-sa/4.0/88x31.png" /></a><br />This work is licensed under a <a rel="license" href="http://creativecommons.org/licenses/by-nc-sa/4.0/">Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License</a>.

## Upstream hardware notes (7-Segment-Digital-Clock-V2)

These come from the original project. This matrix build uses the pins defined in `src/vars.h` (LDR on `A0`, buzzer on `D7`) and the LED setup in `src/main.cpp`.

### Components

| Components                    | x     |
| -------------                 | ----- |
|ESP8266 WeMos Mini D1          | 1x    |
|DS3231 RTC                     | 1x    |
|Micro USB Breakout board       | 1x    |
|Micro USB cable                | 1x    |
|5V 2.5A power supply           | 1x    |
|WS2812B LED Strip 60 LED's per meter     | 2x    |

### 3D Printing

Most files can be printed with 0.3 setting and low infill.

### Schema

Connect D6 from the ESP8266 through a 10 ohm resistor to the data pin of the LED strip. Connect the RTC to gnd, vcc and connect SDA and SCLK to the ESP8266 (check the pin layout of the ESP board you are using). Don't power the LED strip from the ESP board but use a Micro USB breakout board en connect the 5V to the LED strip.

### YouTube video

<a href="https://youtu.be/tGhew6kTAwc" target="_blank"><img src="https://img.youtube.com/vi/tGhew6kTAwc/0.jpg" 
alt="Click to view: 7 Segment Digital Clock V2" width="500" border="1" /></a>
