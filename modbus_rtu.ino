#include "src/Artila-Matrix310.h"

#include <arpa/inet.h>  //htons
#include "src/crc16.h"
#include "src/rtu.h"
#include "AsyncJson.h"
// #include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>//?
#include <NTPClient.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
StaticJsonDocument<128> jsonDocument;
WiFiUDP ntpUDP;
//原本是格林威治時間，台灣+8(28800sec)
NTPClient timeClient(ntpUDP, "tw.pool.ntp.org", 28800);
const char *ssid = "Artila";
const char *password = "CF25B34315";
AsyncWebServer server(80);

bool printFin = true, writeFin = false;
int readLen = 0;
unsigned char slave_id, func;  //6 byte
unsigned short reg_addr, read_count;
struct modbus_write {
  u_int8_t slave_id;
  u_int8_t func;
  u_int8_t reg_addr[2];
  u_int8_t read_count[2];
  u_int8_t CRC[2];
} mod_write;  // struct length: 8
//  = {0x02, 0x03, 0x00, 0x44, 0x00, 0x03, 0x00, 0x00};
struct modbus_read {
  u_int8_t slave_id;  // unsigned int 8bit type
  u_int8_t func;
  u_int8_t length;
  u_int8_t co2[2];
  u_int8_t temp[2];
  u_int8_t rh[2];
  u_int8_t CRC[2];
} mod_read;  // struct length: 11

void wifiConnect() {
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}
void setupRouting(){
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
    String s = "CO2 Meter!";  // Read HTML contents
    req->send(200, "text/plain", s);
  });

  // server.on("/co2meter", HTTP_POST, [](AsyncWebServerRequest *req) {
    
  //   req->send(200, "application/json", jsonStr);//co2不能大於1000
  // });

  server.on("/json", HTTP_POST, [](AsyncWebServerRequest * request) {

    AsyncJsonResponse * response = new AsyncJsonResponse();
    JsonObject& root = response->getRoot();
    root["key1"] = "key number one";
    JsonObject& nested = root.createNestedObject("nested");
    nested["key1"] = "key number one";

    response->setLength();
    request->send(response);
  });

  server.on("/time", HTTP_GET, [](AsyncWebServerRequest *req) {
    timeClient.update();  //NTP
    // Serial.print("TIME: ");
    String nowTime = timeClient.getFormattedTime();
    // Serial.println(nowTime);
    req->send(200, "text/plain", nowTime);
  });

  server.begin();
}
void rtuWrite() {
  if (Serial2.availableForWrite() >= 8) {
    unsigned short crc;
    crc = do_crc_table(&mod_write.slave_id, sizeof(mod_write) - 2);
    Serial.print("writeCRC: ");
    Serial.println(crc, HEX);
    *(u_int16_t *)mod_write.CRC = crc;
    Serial.print("modWrite: ");
    for (int i = 0; i < sizeof(mod_write); i++) {
      Serial.print(*(byte *)(&mod_write.slave_id + i), HEX);
      Serial.print(" ");
    }
    Serial.println("");
    int writeLen = Serial2.write((byte *)&mod_write, sizeof(mod_write));
    // delay(10); //?送出去or到buf
    Serial2.flush();
    digitalWrite(COM1_RTS, LOW);  // read
    delay(1);
    Serial.print("data send: ");
    Serial.println(writeLen);
    printFin = false;
    writeFin = true;
  }
}
void rtuRead() {
  unsigned long RS485Timeout = millis();
  while (1) {
    if (Serial2.available() > 0) {
      readLen = Serial2.readBytes((byte *)&mod_read, sizeof(mod_read));
      Serial2.flush();
      if (readLen > 0) {
        writeFin = false;
      }
      digitalWrite(COM1_RTS, HIGH);  // write
      delay(1);
      break;
    } 
    if (millis() - RS485Timeout > 1000) {
      Serial.println("read nothing!");
      break;
    }
  }
}
void create_json(char *tag, float value, char *unit) {  
  jsonDocument.clear();  
  jsonDocument["type"] = tag;
  jsonDocument["value"] = value;
  jsonDocument["unit"] = unit;
  serializeJson(jsonDocument, buffer);
}
void addJsonObject(char *tag, float value, char *unit) {
  JsonObject obj = jsonDocument.createNestedObject();
  obj["type"] = tag;
  obj["value"] = value;
  obj["unit"] = unit; 
}
void serialPrint() {
  Serial.print("data receive: ");
  Serial.println(readLen);
  for (int i = 0; i < readLen; i++) {
    Serial.print(*(byte *)(&mod_read.slave_id + i), HEX);
    Serial.print(" ");
  }
  Serial.println("");
  Serial.print("CO2: ");
  Serial.print(htons(*(u_int16_t *)(&mod_read.slave_id + 3)), DEC);
  Serial.println(" ppm");

  Serial.print("TEMP: ");
  Serial.print(htons(*(u_int16_t *)(&mod_read.slave_id + 5)) / 100.0, 1);
  Serial.println(" °C");

  Serial.print("RH: ");
  Serial.print(htons(*(u_int16_t *)(&mod_read.slave_id + 7)) / 100.0, 1);
  Serial.println(" %");

  Serial.println("");
  Serial.println("do it again~");
  Serial.println("");
  printFin = true;
}
void setup() {
  // Modbus communication runs at 115200 baud
  Serial.begin(115200);
  Serial.setTimeout(100);
  //Serial2.begin(9600);//Serial2.begin(9600,SERIAL_8N1, COM1_RX,COM1_TX);
  Serial2.begin(9600, SERIAL_8N1, COM1_RX, COM1_TX);
  Serial2.setTimeout(100);
  pinMode(COM1_RTS, OUTPUT);
  digitalWrite(COM1_RTS, HIGH);  // write
  delay(1);
  memset(&mod_write, 0, sizeof(mod_write));
  memset(&mod_read, 0, sizeof(mod_read));
  wifiConnect();
  timeClient.begin();
  setupRouting();

}
void loop() {
}