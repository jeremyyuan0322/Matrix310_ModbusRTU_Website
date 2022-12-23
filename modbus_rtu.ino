#include <WiFi.h>      //server.onRequestBody
#include <arpa/inet.h> //classWebServer.on
#include "src/crc16.h"
#include "src/rtu.h"
#include "src/index.h"
#include <ESPAsyncWebServer.h>
#include <string.h>
#include<stdlib.h>
// RS-485
#define COM1_RX 16 // out
#define COM1_TX 17 // in
#define COM1_RTS 4 // request to send

const char *ssid = "Artila";
const char *password = "CF25B34315";
AsyncWebServer server(80);
// AsyncEventSource events("/events");

bool printFin = true, writeFin = false;
int readLen = 0;
unsigned char slave_id, func;//6 byte
unsigned short reg_addr, read_count;
struct modbus_write
{
  u_int8_t slave_id;
  u_int8_t func;
  u_int8_t reg_addr[2];
  u_int8_t read_count[2];
  u_int8_t CRC[2];
} mod_write; // struct length: 8
//  = {0x02, 0x03, 0x00, 0x44, 0x00, 0x03, 0x00, 0x00};
struct modbus_read
{
  u_int8_t slave_id; // unsigned int 8bit type
  u_int8_t func;
  u_int8_t length;
  u_int8_t co2[2];
  u_int8_t temp[2];
  u_int8_t rh[2];
  u_int8_t CRC[2];
} mod_read; // struct length: 11

void wifiConnect()
{
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req)
  {
    int paramsNr = req->params();
    Serial.print("paramsNr: ");
    Serial.println(paramsNr);
    if(paramsNr==4){
      // AsyncWebParameter* p= req->getParam(0);
      // writeStr=p->value();
      Serial.println(sizeof(atoi((req->getParam(0)->value()).c_str())));
      Serial.println(atoi((req->getParam(0)->value()).c_str()),HEX);
      Serial.println(atoi((req->getParam(1)->value()).c_str()),HEX);
      Serial.println(atoi((req->getParam(2)->value()).c_str()),HEX);
      Serial.println(atoi((req->getParam(3)->value()).c_str()),HEX);

      // slave_id=*((u_int8_t *)(atoi((req->getParam(0)->value()).c_str())));
      // func=*((u_int8_t *)(atoi((req->getParam(0)->value()).c_str())));
      // *(u_int16_t *)reg_addr=*((u_int16_t *)(atoi((req->getParam(0)->value()).c_str())));
      // *(u_int16_t *)read_count=*((u_int16_t *)(atoi((req->getParam(0)->value()).c_str())));
      // *(u_int8_t *)mod_write.slave_id=*(u_int8_t *)(atoi((req->getParam(0)->value()).c_str()));//1U=1byte
      // *(u_int8_t *)mod_write.func=*(u_int8_t *)(atoi((req->getParam(1)->value()).c_str()));
      // *(u_int16_t *)mod_write.reg_addr=*(u_int16_t *)(atoi((req->getParam(2)->value()).c_str()));//2U
      // *(u_int16_t *)mod_write.read_count=*(u_int16_t *)(atoi((req->getParam(3)->value()).c_str()));//atoi(int): 4UL

      mod_write.slave_id=strtol(((req->getParam(0)->value()).c_str()),NULL,16);//1U=1byte
      mod_write.func=strtol(((req->getParam(1)->value()).c_str()),NULL,16);
      *(u_int16_t*)mod_write.reg_addr=htons(strtol(((req->getParam(2)->value()).c_str()),NULL,16));//2U
      *(u_int16_t*)mod_write.read_count=htons(strtol(((req->getParam(3)->value()).c_str()),NULL,16));//atoi(int): 4UL

      // *(u_int8_t *)mod_write.slave_id=slave_id;
      // *(u_int8_t *)mod_write.func=func;
      // *(u_int16_t *)mod_write.reg_addr=reg_addr;
      // *(u_int16_t *)mod_write.read_count=read_count;
    }
    String s = MAIN_page;             // Read HTML contents
    req->send(200, "text/html", s); });

  server.on("/modbus", HTTP_GET, [](AsyncWebServerRequest *req){
    if (printFin = true)
    {
      rtuWrite();
    }
    if (writeFin = true)
    {
      rtuRead();
    }
    if (readLen > 0)
    {
      serialPrint();
    }
    u_int16_t co2 = htons(*(u_int16_t *)(&mod_read.slave_id + 3));
    u_int16_t temp = htons(*(u_int16_t *)(&mod_read.slave_id + 5));
    u_int16_t rh = htons(*(u_int16_t *)(&mod_read.slave_id + 7));
    req->send(200, "text/plain", String(co2) + String(" ") + String(temp / 100.0) + String(" ") + String(rh / 100.0) + String(" ")); });

  server.begin();
}
void rtuWrite()
{
  if (Serial2.availableForWrite() >= 8)
  {
    unsigned short crc;
    crc = do_crc_table(&mod_write.slave_id, sizeof(mod_write) - 2);
    Serial.print("writeCRC: ");
    Serial.println(crc, HEX);
    *(u_int16_t *)mod_write.CRC = crc;
    Serial.print("modWrite: ");
    for (int i = 0; i < sizeof(mod_write); i++)
    {
      Serial.print(*(byte *)(&mod_write.slave_id + i), HEX);
      Serial.print(" ");
    }
    Serial.println("");
    int writeLen = Serial2.write((byte *)&mod_write, sizeof(mod_write));
    // delay(10); //?送出去or到buf
    Serial2.flush();
    digitalWrite(COM1_RTS, LOW); // read
    delay(0.01);
    Serial.print("data send: ");
    Serial.println(writeLen);
    printFin = false;
    writeFin = true;
  }
}
void rtuRead()
{
  delay(32);
  if (Serial2.available() > 0)
  {
    readLen = Serial2.readBytes((byte *)&mod_read, sizeof(mod_read));
    Serial2.flush();
    if (readLen > 0)
    {
      writeFin = false;
    }
    digitalWrite(COM1_RTS, HIGH); // write
    delay(0.01);
  }
  else
  {
    Serial.println("nothing!");
  }
}
void serialPrint()
{
  Serial.print("data receive: ");
  Serial.println(readLen);
  for (int i = 0; i < readLen; i++)
  {
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
void setup()
{
  // Modbus communication runs at 115200 baud
  Serial.begin(115200);
  Serial.setTimeout(100);
  Serial2.begin(9600);
  Serial2.setTimeout(100);
  pinMode(COM1_RTS, OUTPUT);
  digitalWrite(COM1_RTS, HIGH); // write
  delay(0.01);
  memset(&mod_write,0,sizeof(mod_write));
  memset(&mod_read,0,sizeof(mod_read));
  wifiConnect();
}
void loop()
{
}