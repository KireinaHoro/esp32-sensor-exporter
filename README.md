# ESP32 Prometheus Exporter for Environment Sensing

## The exporter

The service runs on port 80.  Fill in WiFi credentials at `include/wifi-credentials.h` as follows:

```cpp
const char *ssid = "<your wifi ssid>";
const char *passwd = "<your wifi password>";
```

This project uses NodeMCU32-S devkit.  BME280 communicates with ESP32 over hardware SPI, with CS connected to GPIO5.  MH-Z19B communicates with ESP32 over UART, with RX/TX using GPIO17 and GPIO16.

## Grafana + Prometheus

A template can be found [here](https://gist.github.com/KireinaHoro/8e5ba83f08b57d948b229f10d3426db6).