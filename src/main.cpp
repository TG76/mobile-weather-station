/*
  Pinbelegung:
  Display:      ESP32 Lolin32:
  BUSY          4
  RST           16
  DC            17
  CS            5   (SS)
  CLK           18  (SCK)
  DIN           23  (MOSI)
  GND           GND
  3.3V          3V

  BME280:       ESP32 Lolin32:
  VCC           3V
  GND           GND
  SCL           22
  SDA           21
*/


#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>
#include <GxEPD.h>
#include <GxGDEP015OC1/GxGDEP015OC1.cpp>
#include <GxIO/GxIO_SPI/GxIO_SPI.cpp>
#include <GxIO/GxIO.cpp>
#include <Fonts/FreeSansBoldOblique18pt7b.h>


const char* ssid     = "Im Bachgrund 11";
const char* password = "grunge1105";
const char* mqtt_server = "192.168.1.14";

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

IPAddress local_IP(192, 168, 1, 205);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(192, 168, 1, 1); //optional
IPAddress secondaryDNS(8, 8, 4, 4); //optional

//uncomment the following lines if you're using SPI
/*#include <SPI.h>
#define BME_SCK 18
#define BME_MISO 19
#define BME_MOSI 23
#define BME_CS 5*/


Adafruit_BME280 bme; // I2C SDA_PIN 21, SCL_PIN 22
//Adafruit_BME280 bme(BME_CS); // hardware SPI
//Adafruit_BME280 bme(BME_CS, BME_MOSI, BME_MISO, BME_SCK); // software SPI
float temperature = 0;
float humidity = 0;
float pressure = 0;
float pressurePa = 0;

// LED Pin
const int ledPin = 2;

GxIO_Class io(SPI, SS, 17, 16);  //SPI,SS,DC,RST
GxEPD_Class display(io, 16, 4);  //io,RST,BUSY


/*
Simple Deep Sleep with Timer Wake Up
=====================================
ESP32 offers a deep sleep mode for effective power
saving as power is an important factor for IoT
applications. In this mode CPUs, most of the RAM,
and all the digital peripherals which are clocked
from APB_CLK are powered off. The only parts of
the chip which can still be powered on are:
RTC controller, RTC peripherals ,and RTC memories
This code displays the most basic deep sleep with
a timer to wake it up and how to store data in
RTC memory to use it over reboots
This code is under Public Domain License.
Author:
Pranav Cherukupalli <cherukupallip@gmail.com>
*/

#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  300        /* Time ESP32 will go to sleep (in seconds) */

RTC_DATA_ATTR int bootCount = 0;

/*
Method to print the reason by which ESP32
has been awaken from sleep
*/
void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}

void setup_deepsleep(){
   //Increment boot number and print it every reboot
  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));

  //Print the wakeup reason for ESP32
  print_wakeup_reason();

  /*
  First we configure the wake up source
  We set our ESP32 to wake up every 5 seconds
  */
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) +
  " Seconds");

  /*
  Next we decide what all peripherals to shut down/keep on
  By default, ESP32 will automatically power down the peripherals
  not needed by the wakeup source, but if you want to be a poweruser
  this is for you. Read in detail at the API docs
  http://esp-idf.readthedocs.io/en/latest/api-reference/system/deep_sleep.html
  Left the line commented as an example of how to configure peripherals.
  The line below turns off all RTC peripherals in deep sleep.
  */
  //esp_deep_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
  //Serial.println("Configured all RTC Peripherals to be powered down in sleep");

  /*
  Now that we have setup a wake cause and if needed setup the
  peripherals state in deep sleep, we can now start going to
  deep sleep.
  In the case that no wake up sources were provided but deep
  sleep was started, it will sleep forever unless hardware
  reset occurs.
  */
}

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure");
    }
    
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  // Feel free to add more if statements to control more GPIOs with MQTT

  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off". 
  // Changes the output state according to the message
  if (String(topic) == "esp32/output") {
    Serial.print("Changing output to ");
    if(messageTemp == "on"){
      Serial.println("on");
      digitalWrite(ledPin, HIGH);
    }
    else if(messageTemp == "off"){
      Serial.println("off");
      digitalWrite(ledPin, LOW);
    }
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266Client")) {
      Serial.println("connected");
      // Subscribe
      client.subscribe("esp32/output");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  // default settings
  // (you can also pass in a Wire library object like &Wire2)
  //status = bme.begin();  
  if (!bme.begin(0x76)) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  
  pinMode(ledPin, OUTPUT);

  setup_deepsleep();

}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

    temperature = bme.readTemperature();   
    // Convert the value to a char array
    char tempString[5];
    dtostrf(temperature, 1, 2, tempString);
    Serial.print("Temperature: ");
    Serial.println(tempString);
    client.publish("esp32/temperature", tempString);

    humidity = bme.readHumidity();
    // Convert the value to a char array
    char humString[5];
    dtostrf(humidity, 1, 2, humString);
    Serial.print("Humidity: ");
    Serial.println(humString);
    client.publish("esp32/humidity", humString);

    pressurePa = bme.readPressure();
    pressure = pressurePa / 100;
    // Convert the value to a char array
    char preString[8];
    dtostrf(pressure, 1, 2, preString);
    Serial.print("Pressure: ");
    Serial.println(preString);
    client.publish("esp32/pressure", preString);

    // Teil refresh vom  Display
    display.updateWindow(0, 0, GxEPD_WIDTH, GxEPD_HEIGHT, false);
    display.init();                   // e-Ink Display initialisieren
//    display.fillScreen(GxEPD_WHITE);  // Display Weiss füllen
    display.setRotation(1);           // Display um 90° drehen
//    display.update();                 // Display aktualisieren
    display.setTextColor(GxEPD_BLACK);     // Schriftfarbe Schwarz
    display.setFont(&FreeSansBoldOblique18pt7b);  // Schrift definieren

    // Rechteck mit weissem Hintergrund erstellen
    //X-Position, Y-Position, Breite, Höhe, Farbe
    display.fillRect(0, 0, 200, 200, GxEPD_WHITE); //Xpos,Ypos,box-w,box-h


    // Temperatur schreiben
    display.setCursor(0, 50);
    //if (temperature >= 20) display.print("+");
    //if (temperature < 0 & temperature> -10) display.print("-");
    //if (temperature >= 0 & temperature < 20) display.print(" +");
    display.print(temperature, 1);
    display.setCursor(70, 50);
    display.print(" C");

    // Da bei der Schrift kein Grad Zeichen vorhanden ist selber eins mit Kreisen erstellen
    display.fillCircle(80, 20, 5, GxEPD_BLACK);  //Xpos,Ypos,r,Farbe
    display.fillCircle(80, 20, 2, GxEPD_WHITE);  //Xpos,Ypos,r,Farbe

    // Luftfeuchtigkeit schreiben
    display.setCursor(0, 100);
    //if (humidity >= 20) display.print(" ");
    //if (humidity >= 0 & humidity < 20) display.print("  ");
    display.print(humidity, 1);
    display.print(" %");


    // Luftdruck schreiben
    display.setCursor(0, 150);
    display.print(pressure, 1);
    //if (pressure < 1000) display.print(" ");
    display.print(" hPa");

    // Teil refresh vom  Display
    display.updateWindow(0, 0, GxEPD_WIDTH, GxEPD_HEIGHT, false);


    delay(500);

    Serial.println("Going to sleep now");
    Serial.flush(); 
    esp_deep_sleep_start();
    Serial.println("This will never be printed");

//  }
}
