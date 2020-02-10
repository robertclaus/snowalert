# SnowAlert
An Arduino project to notify you when it's snowed over night.

# Dependencies
SnowAlert requires these libraries to be installed before compiling:

WifiManager by tzapu (version 0.14.0)
ArduinoJson by Benoit Blanchon (version 6.14.1)
TFminiArduino by hideakitai (version 0.1.1)
NTPClient by Fabrice Weinberg (version 3.2.0)
ESP_DoubleResetDetector by Khoi Hoang (version 1.0.1)

# Configuration
1. Decide what time you want to receive the call in the morning. This is your End Time.
2. Decide what time to start measuring the previous evening. This is your Start Time.
3. Plug in your ESP and open the Serial Monitor in the Arduino IDE like we did previously.
4. Connect to the SnowMeasure wifi network on your computer. You should see some activity in the Serial Monitor as you connect.
5. You should be directed to a setup page in your browser automatically after a few seconds.
6. Click Configure Wifi
7. Enter the following values:
    1. SSID - The wifi network the ESP should connect to for internet.
    2. Password - The password to connect to that wifi network.
    3. Start Hour - The hour you want it to measure the snow height in the evening.
    4. Start Minutes - The minute component to the time you want it to measure in the evening.
    5. End Hour - The hour you want it to measure the snow height in the morning (and potentially call you)
    6. End Minutes - The minute component to the time you want it to measure in the morning.
    7. The Alert Webhook URL - This should be the url you saved in the previous step that looks something like this: https://maker.ifttt.com/trigger/snow_alert/with/key/d-Y8rXge5kibp0dkdrCgxu
    8. The Measurement Webhook URL - This should be the same url as above, but replace snow_alert with snow_measurement