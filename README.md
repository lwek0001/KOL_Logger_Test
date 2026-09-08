
## Current Functionality
* ESP32 acts as a wifi access point (AP)
* Task that creates dummy current (A) data
* ESP32 hosts dynamic website displaying dummy current (A) data

## TO-DO
* Display data as a graph
* Display data on a screen (ili9341)
* Store data in SD card
* Obtain real current readings

## Configure the project (just wifi settings)

Open the project configuration menu (`idf.py menuconfig`).

In the `Example Configuration` menu:

* Set the Wi-Fi configuration.
    * Set `WiFi SSID`.
    * Set `WiFi Password`.

## Built from ESP32 softap wifi example

