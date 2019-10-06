#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_BME280.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <GyverEncoder.h>

// Оказывается, они не обязателены....
// #include <ESP8266WiFi.h>
// #include <DNSServer.h>
// #include <ESP8266WebServer.h>
// #include <ESP8266mDNS.h>
// #include <SPI.h>
// #include <Adafruit_Sensor.h>

#define sendNM true
#define debug false                     // вывод отладочных сообщений
#define postingInterval (5 * 60 * 1000) // интервал между отправками данных в миллисекундах (5 минут)
#define SEALEVELPRESSURE_HPA (1013.25)

#define I2CSDA 4  // I2C sda
#define I2CSCL 5  // I2C scl
#define EnkCLK 13 // энкодер CLK
#define EnkDT 12  // DT
#define EnkSW 14  // SW

Encoder enc1(EnkCLK, EnkDT, EnkSW); // инициализация энкодера
LiquidCrystal_I2C lcd(0x27, 20, 4); // инициализация дисплея
Adafruit_BME280 bme;

const char *ssid = "Segma-WiFi";
const char *password = "derq5898587";
boolean firstRead = true, screenLight = true;
float bmeTemp, bmeHum, bmePres, tempHistHour[24][3];
unsigned long lastConnectionTime = 0; // время последней передачи данных
String Hostname;                      // * имя железки - выглядит как ESPAABBCCDDEEFF т.е. ESP+mac адрес.

void wifiCon()
{
  Hostname = "ESP" + WiFi.macAddress();
  Hostname.replace(":", "");
  WiFi.hostname(Hostname);
  lcd.clear();
  lcd.setCursor(1, 1);
  lcd.print("Connect to WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  lcd.setCursor(8, 3);
  lcd.print("*OK*");
}

void bmeRead()
{ // чтение датчика BME280
  if (firstRead)
  {
  start:
    if (!bme.begin(0X76))
    {
      lcd.setCursor(0, 3);
      lcd.print("...Sensor EROR...");
      delay(1000 * 60 * 5);
      goto start;
    }
    else
    {
      lcd.setCursor(0, 3);
      lcd.print("...Sensor GOOD...");
      firstRead = false;
    }
  }
  bmeTemp = bme.readTemperature();
  bmeHum = bme.readHumidity();
  bmePres = bme.readPressure() / 133.3224;
}

void displayStart()
{
  lcd.init(I2CSDA, I2CSCL); // ! До этого была строка lcd.init(4,5); ..незнаю зачем..
  lcd.backlight();
  lcd.setCursor(2, 1);
  lcd.print("-= Meteo WiFi =-");
  lcd.setCursor(7, 2);
  lcd.print("v.0.4.1");
  delay(4000);
}

void displayFirst()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Temp:");
  lcd.setCursor(6, 0);
  lcd.print(bmeTemp);
  lcd.setCursor(0, 1);
  lcd.print("Humm:");
  lcd.setCursor(6, 1);
  lcd.print(bmeHum);
  lcd.setCursor(0, 2);
  lcd.print("Pres:");
  lcd.setCursor(6, 2);
  lcd.print(bmePres);
}

void displayLoop()
{
  lcd.setCursor(6, 0);
  lcd.print("     ");
  lcd.setCursor(6, 0);
  lcd.print(bmeTemp);
  lcd.setCursor(6, 1);
  lcd.print("     ");
  lcd.setCursor(6, 1);
  lcd.print(bmeHum);
  lcd.setCursor(6, 2);
  lcd.print("     ");
  lcd.setCursor(6, 2);
  lcd.print(bmePres);
}

void enk() //отключение экрана
{
  enc1.tick(); // отработка в прерывании
  if (!screenLight)
  {
    lcd.backlight();
    screenLight = true;
  }
  else
  {
    lcd.noBacklight();
    screenLight = false;
  }
}

void ota()
{
  ArduinoOTA.onStart([]() {
    Serial.println("Start"); //  "Начало OTA-апдейта"
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd"); //  "Завершение OTA-апдейта"
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR)
      Serial.println("Auth Failed"); // "Ошибка при аутентификации"
    else if (error == OTA_BEGIN_ERROR)
      Serial.println("Begin Failed"); // "Ошибка при начале OTA-апдейта"
    else if (error == OTA_CONNECT_ERROR)
      Serial.println("Connect Failed"); // "Ошибка при подключении"
    else if (error == OTA_RECEIVE_ERROR)
      Serial.println("Receive Failed"); // "Ошибка при получении данных"
    else if (error == OTA_END_ERROR)
      Serial.println("End Failed"); // "Ошибка при завершении OTA-апдейта"
  });
  ArduinoOTA.begin();
}

void setup()
{
  Serial.begin(115200);
  displayStart();
  //pinMode(EnkCLK, INPUT);
  enc1.setType(TYPE2);
  attachInterrupt(digitalPinToInterrupt(EnkCLK), enk, CHANGE);
  enc1.tick();
  bmeRead(); // первое чтение датчика
  wifiCon();
  lastConnectionTime = millis() - postingInterval + 15000; //первая передача на народный мониторинг через 15 сек.
  ota();
  displayFirst();
}

bool sendToNarodmon()
{ // Собственно формирование пакета и отправка
  WiFiClient client;
  String buf;
  buf = "#" + Hostname + "#Segma-BME280-HOME" + "\r\n"; // заголовок И ИМЯ, которое будет отображаться в народном мониторинге, чтоб не палить мак адрес
  buf = buf + "#T1#" + String(bmeTemp) + "\r\n";        // температура bmp180
  buf = buf + "#H1#" + String(bmeHum) + "\r\n";         // влажность
  buf = buf + "#P1#" + String(bmePres) + "\r\n";        // Давление
  buf = buf + "##\r\n";                                 // закрываем пакет

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

void narodmonStart()
{
  if (millis() - lastConnectionTime > postingInterval && sendNM)
  { // ! Отправка на нар.мон.
    if (WiFi.status() == WL_CONNECTED)
    { // ну конечно если подключены
      if (sendToNarodmon())
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
}

void loop()
{
  ArduinoOTA.handle();
  enc1.tick();
  delay(1000 * 10);
  bmeRead();
  displayLoop();
  sendToNarodmon();
  yield(); // ? что за команда - фиг знает, но ESP работает с ней стабильно и не глючит.
}