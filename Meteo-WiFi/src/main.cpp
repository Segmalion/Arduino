//ардуино 181, либы схоронил
#include <Wire.h>
#include <DHT.h>
#include <Adafruit_Sensor.h>
#include <LiquidCrystal_I2C.h>

#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h> 

#define debug true // вывод отладочных сообщений
#define postingInterval  (5*60*1000) // интервал между отправками данных в миллисекундах (5 минут)
#define DHTPIN D4 
#define DHTTYPE DHT22
#define I2CSDA D2
#define I2CSCL D1

LiquidCrystal_I2C lcd(0x27,20,4); // инициализация дисплея
DHT dht(DHTPIN, DHTTYPE); // инициализация DHT22

float dhtTemp, dhtHum;
/**/unsigned long lastConnectionTime = 0;           // время последней передачи данных
/**/String Hostname; //имя железки - выглядит как ESPAABBCCDDEEFF т.е. ESP+mac адрес.

void wifimanstart() { // Волшебная процедура начального подключения к Wifi.
                      // Если не знает к чему подцепить - создает точку доступа ESP8266 и настроечную таблицу http://192.168.4.1
                      // Подробнее: https://github.com/tzapu/WiFiManager
  WiFiManager wifiManager;
  wifiManager.setDebugOutput(debug);
  wifiManager.setMinimumSignalQuality();
  if (!wifiManager.autoConnect("ESP8266")) {
  if (debug) Serial.println("*WM: failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000); }
if (debug) Serial.println("*WM: connected...");
}

void displayStart() {
  lcd.init(4,5);
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("OK");
}

void setup() {
  Hostname = "ESP"+WiFi.macAddress();
  Hostname.replace(":","");
  Serial.begin(115200);
  displayStart();

  /*if (isnan(dht.readHumidity()) || isnan(dht.readTemperature())) {
    Serial.println("Failed to read from DHT sensor!");
    while (1) {}
  } else {Serial.println("Find DHT sensor");}*/
  
  WiFi.hostname(Hostname);
  wifimanstart();
  Serial.println(WiFi.localIP()); Serial.println(WiFi.macAddress()); Serial.print("Narodmon ID: "); Serial.println(Hostname); // выдаем в компорт мак и айпишник железки, так же выводим на дисплей
  lastConnectionTime = millis() - postingInterval + 15000; //первая передача на народный мониторинг через 15 сек.
}

void displayLoop() {
  lcd.setCursor(0,1);
  lcd.print(dhtTemp);
}

void dhtRead(){

  if (isnan(dht.readTemperature())) dhtTemp = dhtTemp; else dhtTemp = dht.readTemperature();
  if (isnan(dht.readHumidity())) dhtHum = dhtHum; else dhtHum = dht.readHumidity();
}

bool SendToNarodmon() { // Собственно формирование пакета и отправка.
    WiFiClient client;
    String buf;
    buf = "#" + Hostname + "#Segma-BME280-HOME" + "\r\n"; // заголовок И ИМЯ, которое будет отображаться в народном мониторинге, чтоб не палить мак адрес
    buf = buf + "#T1#" + String(dhtTemp) + "\r\n"; //температура bmp180
    buf = buf + "#H1#" + String(dhtHum) + "\r\n"; //влажность
    //buf = buf + "#P1#" + String(...) + "\r\n"; //давление
    buf = buf + "##\r\n"; // закрываем пакет
 
    if (!client.connect("narodmon.ru", 8283)) { // попытка подключения
      Serial.println("connection 'narodmon.ru' failed");
      return false; // не удалось;
    } else
    {
      client.print(buf); // и отправляем данные
      if (debug) Serial.print(buf);
      while (client.available()) {
        String line = client.readStringUntil('\r'); // если что-то в ответ будет - все в Serial
        Serial.print(line);
        
        }
    }
      return true; //ушло
      
  }


void loop() 
{
  delay(1000);// нужна, беж делея у меня не подключался к вайфаю
  //Serial.println(bme.readTemperature());
  dhtRead();
  displayLoop();
  if (millis() - lastConnectionTime > postingInterval) {// ждем 5 минут и отправляем
      if (WiFi.status() == WL_CONNECTED) { // ну конечно если подключены
      if (SendToNarodmon()) {
      lastConnectionTime = millis();
      }else{  lastConnectionTime = millis() - postingInterval + 15000; }//следующая попытка через 15 сек    
      }else{  lastConnectionTime = millis() - postingInterval + 15000; Serial.println("Not connected to WiFi");}//следующая попытка через 15 сек
    } yield(); // что за команда - фиг знает, но ESP работает с ней стабильно и не глючит.
}