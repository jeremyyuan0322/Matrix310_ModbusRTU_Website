#include "src/Artila-Matrix310.h"
#include "src/crc16.h"
#include "src/rtu.h"
#include <arpa/inet.h>  //htons
#include <WiFi.h>
#include <NTPClient.h>
#include <ArduinoJson.h>
#include <WebServer.h>

WiFiUDP ntpUDP;
//原本是格林威治時間，台灣+8(28800sec)
NTPClient timeClient(ntpUDP, "tw.pool.ntp.org", 28800);
const char *ssid = "Artila";
const char *password = "CF25B34315";
WebServer server(80);
StaticJsonDocument<512> jsonDocument;
bool printFin = true, writeFin = false;//flag不要亂動 就是這樣！！
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
void wifiConnect();
void addJsonObject(char *tag, float value, char *unit);

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

void addJsonObject(JsonArray modbusToJson, char *tag, char *value, char *unit) {
  JsonObject modbus = modbusToJson.createNestedObject();
  modbus["type"] = tag;
  modbus["value"] = value;
  modbus["unit"] = unit;
}

void setupRouting() {
  server.on("/", handleRoot);
  server.on("/data", HTTP_POST, handleData);
  server.begin();
}
void handleRoot() {
  String s = "co2 meter";             // Read HTML contents
  server.send(200, "text/plain", s);  // Send web page
}
void handleData() {
  Serial.println("get request with JSON");
  if (server.hasArg("plain") == false) {
    server.send(400, "text/plain", "Invalid JSON");
    return;
  }
  String body = server.arg("plain");
  //Before reading the input using deserializeJson(), this function resets the document, so you don’t need to call JsonDocument::clear().
  deserializeJson(jsonDocument, body);
  
  mod_write.slave_id = jsonDocument["slave_id"].as<u_int8_t>();  //1U=1byte
  mod_write.func = jsonDocument["func"].as<u_int8_t>();
  *(u_int16_t *)mod_write.reg_addr = htons(strtol((jsonDocument["reg_addr"].as<String>()).c_str(), NULL, 16));  //2U, use strtol() to transfer to HEX
  *(u_int16_t *)mod_write.read_count = htons(strtol((jsonDocument["read_count"].as<String>()).c_str(), NULL, 16));

  if (printFin = true) {
    rtuWrite();
    jsonDocument.clear();//write完先清空給read用
  }
  if (writeFin = true) {
    rtuRead();
  }
  if (readLen > 0) {
    serialPrint();
  }
  // char *co2ToStr = malloc(10 * sizeof(char));//用完記得free(co2ToStr);
  char co2ToStr[10];
  char tempToStr[10];
  char rhToStr[10];
  u_int16_t co2 = htons(*(u_int16_t *)(&mod_read.slave_id + 3));
  float temp = htons(*(u_int16_t *)(&mod_read.slave_id + 5)) / 100.0;
  float rh = htons(*(u_int16_t *)(&mod_read.slave_id + 7)) / 100.0;
  sprintf(co2ToStr, "%d", co2);
  sprintf(tempToStr, "%.2f", temp);
  sprintf(rhToStr, "%.2f", rh);

  jsonDocument["Device"] = "co2Meter";
  jsonDocument["timeStamp"] = getTime();
  JsonArray modbusToJson = jsonDocument.createNestedArray("modbusRead");

  addJsonObject(modbusToJson ,(char *)"CO2", co2ToStr, (char *)"ppm");
  addJsonObject(modbusToJson ,(char *)"TEMP", tempToStr, (char *)"°C");
  addJsonObject(modbusToJson ,(char *)"RH", rhToStr, (char *)"%");

  String json;
  serializeJson(jsonDocument, json);
  server.send(200, "application/json", json);
}

String getTime() {
  timeClient.update();  //NTP
  // Serial.print("TIME: ");
  String nowTime = timeClient.getFormattedTime();
  // Serial.println(nowTime);
  return nowTime;
}
void rtuWrite() {
  if (Serial2.availableForWrite() >= 8) {
    unsigned short crc;
    crc = do_crc(&mod_write.slave_id, sizeof(mod_write) - 2);
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
    printFin = false;//在全部read完之後才會改為ture然後print
    writeFin = true;//wrtie結束，接下來一直read
  }
}
void rtuRead() {
  unsigned long RS485Timeout = millis();
  while (1) {
    if (Serial2.available() > 0) {
      readLen = Serial2.readBytes((byte *)&mod_read, sizeof(mod_read));
      Serial2.flush();
      if (readLen > 0) {
        writeFin = false;//有收到東西之後可以write
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

void serialPrint() {
  Serial.print("data receive: ");
  Serial.println(readLen);
  for (int i = 0; i < readLen; i++) {
    Serial.print(*(byte *)(&mod_read.slave_id + i), DEC);
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
  server.handleClient();
  delay(1);
}