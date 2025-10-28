# clock-of-knowlage-v2
This project is a network-connected clock built on an **ESP32** and a **GP1294AI 256×48 VFD display**.  
It keeps accurate time using NTP, shows Wi-Fi connection status, and displays both local and external temperature/humidity readings.

## Features
- Time synchronization via NTP (`pool.ntp.org`), with UK timezone and automatic daylight savings adjustment  
- Wi-Fi with auto-reconnect and backoff handling  
- External temperature and humidity fetched from the [Open-Meteo API](https://open-meteo.com/)  
- Local temperature and humidity from a DHT22 sensor (planned upgrade to a more accurate sensor)  
- UI made with [lopaka](https://lopaka.app/gallery/16709/35604) and rendered with the [U8g2](https://github.com/olikraus/u8g2) graphics library  
- Error handling and retry logic for NTP and weather fetches  

## Hardware
- ESP32 DevKit board  
- GP1294AI 256×48 Vacuum Fluorescent Display  
- DHT22 sensor (easily swappable for alternatives)  
- Standard jumpers and breadboards

Approximate costs (as of 2025):  
- ESP32 board: 3 for ~£18 [Amazon](https://www.amazon.co.uk/dp/B0D8T5XD3P)
- VFD display: ~£41 [AliExpress](https://www.aliexpress.com/item/1005004465805347.html)
- DHT22 sensor: ~£3 [Amazon](https://www.amazon.co.uk/dp/B0DP3XJSXR)

## Issues and bugs
1. if the wifi accociation fails it waits forever and needs a manual restart
