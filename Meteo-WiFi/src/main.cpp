#include <Wire.h>
#include <DHT.h>
#include <Adafruit_Sensor.h>
#include <LiquidCrystal_I2C.h>

#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>

#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <GyverEncoder.h>

#define debug true                     // вывод отладочных сообщений
#define postingInterval (5 * 60 * 1000) // интервал между отправками данных в миллисекундах (5 минут)
#define DHTTYPE DHT22 
#define DHTPIN D4 // * ПИНЫ: - DHT22
#define I2CSDA 4 // *        - I2C sda
#define I2CSCL 5 // *        - I2C scl
#define EnkCLK D8 // *       - энкодер CLK
#define EnkDT D7 // *                  DT
#define EnkSW D6 // *                  SW

Encoder enc1(CLK, DT, SW); // инициализация энкодера
LiquidCrystal_I2C lcd(0x27,20,4); // инициализация дисплея
DHT dht(DHTPIN, DHTTYPE); // инициализация DHT22

const char* ssid     = "Segma-WiFi";
const char* password = "derq5898587";
boolean firstReadDHT = true, screenLight = true;
float dhtTemp, dhtHum, tempHistHour[24][3];
unsigned long lastConnectionTime = 0; // время последней передачи данных
String Hostname;                      // * имя железки - выглядит как ESPAABBCCDDEEFF т.е. ESP+mac адрес.

void wifiCon()
{ 
  Hostname = "ESP" + WiFi.macAddress();
  Hostname.replace(":", "");
  WiFi.hostname(Hostname);
  if (debug)
  {
    Serial.print("Подключение Wifi");
  }
  lcd.clear();
  lcd.setCursor(1,1);
  lcd.print("Connect to WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
    Serial.print(".");
  }
  if (debug)
  { 
    Serial.println();
    Serial.print("Подключенно к "); Serial.println(ssid);
    Serial.print("IP адрес: ");     Serial.println(WiFi.localIP());
    Serial.print("MAC адрес: ");    Serial.println(WiFi.macAddress());
    Serial.print("Narodmon ID: ");  Serial.println(Hostname);
    Serial.println();
  }
  lcd.setCursor(8,3);
  lcd.print("*OK*");
}

void dhtRead()
{ // чтение датчика DHT
  startDHT:
  do{ // выполняем пока не прочьтем датчик
    dhtTemp = dht.readTemperature();
    dhtHum = dht.readHumidity();
    if (debug && firstReadDHT)
    { // если первый прогон датчика DHT22, и включон дебаг
      Serial.print("Подключение к DHT22");
      int i=0;
      while ((isnan(dhtTemp) || isnan(dhtHum)) && i<25) 
      {
        delay(2000);
        Serial.print(".");
        if (i<25) i++; else { Serial.println(); Serial.println("Ошибка подключения к DHT22!"); delay(15000); ESP.restart(); }
      }
    }
    delay(2000);
  } while ((isnan(dhtTemp) || isnan(dhtHum)));
  if (dhtTemp==0 && dhtHum==0) {goto startDHT;} // если значения нули - перезапускаем (проверка на нули)
  else if (debug && firstReadDHT) {Serial.println(); Serial.println("Данные прочитаны!");Serial.println();firstReadDHT = false;}
}

void displayStart() 
{
  lcd.init(I2CSDA,I2CSCL); // ! До этого была строка lcd.init(4,5); ..незнаю зачем..
  lcd.backlight();
  lcd.setCursor(2,1); 
  lcd.print("-= Meteo WiFi =-");
  lcd.setCursor(8,3); 
  lcd.print("v.0.1");
  for(int i=10; i>0; i--) {Serial.println(i); delay(1000);}
}

void displayFirst()
{

  lcd.clear();
  lcd.setCursor(0,0); lcd.print("Temp:");
  lcd.setCursor(6,0); lcd.print(dhtTemp);
  lcd.setCursor(0,1); lcd.print("Humm:");
  lcd.setCursor(6,1); lcd.print(dhtHum);
}

void displayLoop()
{
  lcd.setCursor(6,0); lcd.print("     ");
  lcd.setCursor(6,0); lcd.print(dhtTemp);
  lcd.setCursor(6,1); lcd.print("     ");
  lcd.setCursor(6,1); lcd.print(dhtHum);
}

void enk() //отключение экрана
{
  enc1.tick();  // отработка в прерывании
  if (screenLight=false) {
    lcd.backlight(); screenLight = true; // эсли отключена подсветка - включаем
  }


  }
  /*switch (screenLight){
      case true:  { lcd.noBacklight(); screenLight = false; break;}  
      case false: { lcd.backlight();   screenLight = true;  break;}
  }*/
}

void setup()
{
  Serial.begin(115200);
  displayStart();
  pinMode(EnkCLK, INPUT);
  Serial.println("Загрузка...");
  // время на запуск
  attachInterrupt(digitalPinToInterrupt(EnkCLK), enk, CHANGE);
  
  wifiCon();
  lastConnectionTime = millis() - postingInterval + 15000; //первая передача на народный мониторинг через 15 сек.
  ArduinoOTA.onStart([]() {
    Serial.println("Start");  //  "Начало OTA-апдейта"
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");  //  "Завершение OTA-апдейта"
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed"); // "Ошибка при аутентификации"
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed"); // "Ошибка при начале OTA-апдейта"
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed"); // "Ошибка при подключении"
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed"); // "Ошибка при получении данных"
    else if (error == OTA_END_ERROR) Serial.println("End Failed"); // "Ошибка при завершении OTA-апдейта"
  });
  ArduinoOTA.begin();
  dhtRead(); // первое чтение датчика
  displayFirst();
}

bool SendToNarodmon()
{ // Собственно формирование пакета и отправка
  WiFiClient client;
  String buf;
  buf = "#" + Hostname + "#Segma-BME280-HOME" + "\r\n"; // заголовок И ИМЯ, которое будет отображаться в народном мониторинге, чтоб не палить мак адрес
  buf = buf + "#T1#" + String(dhtTemp) + "\r\n";        // температура bmp180
  buf = buf + "#H1#" + String(dhtHum) + "\r\n";         // влажность
  buf = buf + "##\r\n"; // закрываем пакет

  if (!client.connect("narodmon.ru", 8283))
  { // попытка подключения
    Serial.println("connection 'narodmon.ru' failed");
    return false; // не удалось;
  }
  else
  {
    client.print(buf); // и отправляем данные
    if (debug)
      Serial.print(buf);
    while (client.available())
    {
      String line = client.readStringUntil('\r'); // если что-то в ответ будет - все в Serial
      Serial.print(line);
    }
  }
  return true; //ушло
}

void loop()
{
  ArduinoOTA.handle();
  delay(1000); // нужна, беж делея у меня не подключался к вайфаю
  //Serial.println(bme.readTemperature());
  dhtRead();
  displayLoop();
  if (millis() - lastConnectionTime > postingInterval)
  { // ждем 5 минут и отправляем
    if (WiFi.status() == WL_CONNECTED)
    { // ну конечно если подключены
      if (SendToNarodmon())
      {
        lastConnectionTime = millis();
      }
      else
      {
        lastConnectionTime = millis() - postingInterval + 15000;
      } //следующая попытка через 15 сек
    }
    else
    {
      lastConnectionTime = millis() - postingInterval + 15000;
      Serial.println("Not connected to WiFi");
    } //следующая попытка через 15 сек
  }
  yield(); // ? что за команда - фиг знает, но ESP работает с ней стабильно и не глючит.
}