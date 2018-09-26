# Basecamp

Custom version of the [c't Basecamp library](https://github.com/merlinschumacher/Basecamp):

* No support for MQTT
* No support for OTA updates
* Fix VS Code's C/C++ IntelliSense
* Rename AP SSID prefix to `PixelTube_`
* Always enable DNS server for captive portal when in AP mode
* Save IP address assigned by DHCP and use it as a static IP in subsequent connections
* Only configure pixel tube number, set IP address based on that
* Allow WiFi connect and disconnect callbacks
