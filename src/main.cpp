#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <WiFi.h>

#define BME_SCK 18
#define BME_MISO 19
#define BME_MOSI 23
#define BME_CS 5

#define SEALEVELPRESSURE_HPA (1013.25)

Adafruit_BME280 bme(BME_CS); // hardware SPI

const int PACKET_LEN = 9;
const uint8_t measure_cmd[PACKET_LEN] = {0xff, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};
const uint8_t range10k_cmd[PACKET_LEN] = {0xff, 0x01, 0x99, 0x00, 0x00, 0x00, 0x27, 0x10, 0x96};
const uint8_t range5k_cmd[PACKET_LEN] = {0xff, 0x01, 0x99, 0x00, 0x00, 0x00, 0x13, 0x88, 0x2f};
const uint8_t range2k_cmd[PACKET_LEN] = {0xff, 0x01, 0x99, 0x00, 0x00, 0x00, 0x07, 0xd0, 0x8f};

// wifi credentials
#include "wifi-credentials.h"

WiFiServer server(80);

uint8_t get_checksum(const uint8_t *buffer) {
  uint8_t checksum = 0;
  for (int i = 1; i < 8; ++i) {
    checksum += buffer[i];
  }
  checksum = 0xff - checksum;
  checksum += 1;
  return checksum;
}

void write_packet(const uint8_t *buffer) {
  for (int i = 0; i < PACKET_LEN; ++i) {
    Serial2.write(buffer[i]);
  }
}

void read_packet(uint8_t *buffer) {
  for (int i = 0; i < PACKET_LEN; ++i) {
    buffer[i] = (uint8_t)Serial2.read();
  }
}

int read_co2() {
  uint8_t response[PACKET_LEN];

  write_packet(measure_cmd);
  read_packet(response);
  if (response[1] != 0x86) {
    return -1;
  }
  uint8_t checksum = get_checksum(response);
  if (checksum != response[8]) {
    return -1;
  }
  return response[2] * 256 + response[3];
}

float temperature, pressure, altitude, humidity;
int co2;

void update_values() {
    temperature = bme.readTemperature();
    pressure = bme.readPressure() / 100.0F;
    altitude = bme.readAltitude(SEALEVELPRESSURE_HPA);
    humidity = bme.readHumidity();
    co2 = read_co2();
}

void setup() {
    //Serial.begin(9600);
    // connect to wifi
    WiFi.begin(ssid, passwd);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
    }
    //Serial.println(WiFi.localIP());
    server.begin();

    // initialize MH-Z19B
    Serial2.begin(9600);
    while(!Serial2);
    // set CO2 range to 10k ppm
    write_packet(range10k_cmd);
    //write_packet(range5k_cmd);

    // initialize BME280
    if (!bme.begin()) {
      // BME280 initialization failed
      while (1) delay(10);
    }

    // wait until value is ready
    while (co2 == -1) {
      update_values();
      delay(1000);
    }
}

String header;

const char *location = "Bedroom";

void send_metrics(WiFiClient &client) {
  client.printf("# HELP environ_temp Environment temperature (in C).\n");
  client.printf("# TYPE environ_temp gauge\n");
  client.printf("environ_temp{location=\"%s\"} %.2f\n", location, temperature);
  
  client.printf("# HELP environ_pressure Environment atmospheric pressure (in hPa).\n");
  client.printf("# TYPE environ_pressure gauge\n");
  client.printf("environ_pressure{location=\"%s\"} %.2f\n", location, pressure);
  
  client.printf("# HELP environ_altitude Environment altitude from sea level (in m).\n");
  client.printf("# TYPE environ_altitude gauge\n");
  client.printf("environ_altitude{location=\"%s\"} %.2f\n", location, altitude);
  
  client.printf("# HELP environ_humidity Environment relative humidity (in percentage).\n");
  client.printf("# TYPE environ_humidity gauge\n");
  client.printf("environ_humidity{location=\"%s\"} %.2f\n", location, humidity);
  
  client.printf("# HELP environ_co2 Environment CO2 concentration (in ppm).\n");
  client.printf("# TYPE environ_co2 gauge\n");
  client.printf("environ_co2{location=\"%s\"} %d\n", location, co2);
}

void loop() {
  WiFiClient client = server.available();

  if (client) {
    //Serial.print("New client from ");
    //Serial.print(client.remoteIP());
    //Serial.printf(":%d\n", client.remotePort());

    String currentLine = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        header += c;
        if (c == '\n') {
          if (currentLine.length() == 0) {
            // end of client request
            if (header.indexOf("GET /metrics") >= 0) {
              update_values();
              // write metric
              client.println("HTTP/1.1 200 OK");
              client.println("Content-Type: text/html; charset=utf-8");
              client.println("Connection: close");
              client.println(); // end headers
              send_metrics(client);
            } else if (header.indexOf("GET /") >= 0) {
              // something else - link to metrics
              client.println("HTTP/1.1 200 OK");
              client.println("Content-Type: text/html; charset=utf-8");
              client.println("Connection: close");
              client.println(); // end headers
              client.println("<html><head><title>Environment Sensor @ ESP32</title></head>"
              "<body><h1>Environment Sensor @ ESP32</h1>"
              "<p><a href=\"/metrics\">Metrics</a></p></body></html>");
            } else {
              // terminate with 400 bad request
              client.println("HTTP/1.1 400 Bad Request");
              client.println("Content-Type: text/html; charset=utf-8");
              client.println("Connection: close");
              client.println(); // end headers
            }
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }
    header = "";

    client.stop();
    //Serial.println("Client disconnected");
  }
}