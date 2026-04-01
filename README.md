# SensESP Mast 1

ESP32-basierter Mastsensor für Segelboote, der Winddaten, Temperatur und Luftfeuchtigkeit über WLAN an einen [Signal K](https://signalk.org/) Server überträgt und vier PWM-Ausgänge für die Mastbeleuchtung steuert.

## Hardware

| Komponente | Beschreibung |
|---|---|
| **Mikrocontroller** | Seeed XIAO ESP32C3 |
| **Windsensor** | Calypso Ultrasonic Portable (Bluetooth Low Energy) |
| **Klimasensor** | SHT85 (Temperatur & Luftfeuchtigkeit, I²C) |
| **Ausgänge** | 4× PWM (D0–D3) für 24V Navigationsbeleuchtung |

## Funktionen

**Winddaten (Calypso BLE)**
Verbindet sich automatisch per BLE mit dem Calypso Ultrasonic Anemometer und liest Windgeschwindigkeit, Windrichtung und Akkustand aus. Die Verbindung wird bei Unterbrechung selbstständig wiederhergestellt.

**Klimadaten (SHT85)**
Der SHT85 misst alle 30 Sekunden Temperatur und relative Luftfeuchtigkeit am Mast.

**Mastbeleuchtung**
Vier PWM-Ausgänge (8-Bit, 8 kHz) an D0–D3 hören auf Steuerwerte vom Signal K Server und geben diese als PWM-Signal aus, um die Helligkeit der Navigationsleuchten zu regeln.

## Signal K Datenpfade

| Pfad | Einheit | Quelle |
|---|---|---|
| `environment.wind.speedApparent` | m/s | Calypso |
| `environment.wind.angleApparent` | rad | Calypso |
| `electrical.batteries.99.capacity.stateOfCharge` | ratio (0–1) | Calypso |
| `environment.outside.temperature` | K | SHT85 |
| `environment.outside.humidity` | ratio (0–1) | SHT85 |
| `electrical.outside.mast.channel.1–4.value` | 0–255 | SK → PWM Ausgang |


## Info

Das Standard-Board ist `xiao_esp32c3`. Weitere unterstützte Boards sind in der `platformio.ini` konfiguriert (u.a. HALMET, HALSER, SHEsp32).

## Konfiguration

Nach dem ersten Start öffnet das Gerät einen WLAN Access Point mit dem Namen `sensesp-mast1` (Passwort: `thisisfine`). Darüber lassen sich WLAN-Zugangsdaten und die Signal K Serveradresse konfigurieren.

OTA-Updates sind aktiviert (Passwort: siehe `main.cpp`).

## Abhängigkeiten

- [SensESP](https://github.com/SignalK/SensESP/) `>=3.0.0-beta.6`
- [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) `^2.3.4`
- [SHT85](https://github.com/RobTillaart/SHT85) `^0.6.2`
- [ESP32 AnalogWrite](https://github.com/dlloydev/ESP32-ESP32S2-AnalogWrite)
