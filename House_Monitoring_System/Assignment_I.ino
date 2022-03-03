//display, DHT11
#include <LiquidCrystal_I2C.h>   // display library
#include <Wire.h>                // I2C library
#define DISPLAY_CHARS 16    // number of characters on a line
#define DISPLAY_LINES 2     // number of display lines
#define DISPLAY_ADDR 0x27   // display address on I2C bus
LiquidCrystal_I2C lcd(DISPLAY_ADDR, DISPLAY_CHARS, DISPLAY_LINES);   // display object
#include <DHT.h>
#define DHTPIN D3       
#define DHTTYPE DHT11   // sensor type DHT 11
DHT dht = DHT(DHTPIN, DHTTYPE);

//influxdb, wifi
#include <ESP8266WiFi.h>
#include "secrets.h"
#include <InfluxDbClient.h>
#define LED_ONBOARD LED_BUILTIN_AUX   
#define RSSI_THRESHOLD -78            // WiFi signal strength threshold

// WiFi cfg
char ssid[] = SECRET_SSID;   // your network SSID (name)
char pass[] = SECRET_PASS;   // your network password
#ifdef IP
IPAddress ip(IP);
IPAddress subnet(SUBNET);
IPAddress dns(DNS);
IPAddress gateway(GATEWAY);
#endif
WiFiClient client;

// InfluxDB cfg
InfluxDBClient client_idb(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);
Point connection("connection");
Point temp_hum("temp_hum");
Point photores("photores");
Point tilt("tilt");
Point alarm("alarm");

//photoresistor, TILT, BUZZER
#define LED BUILTIN_LED   // LED pin
#define PHOTORESISTOR A0              // photoresistor pin
#define DANGER_TEMP_HUM 26   
#define TILT D5
#define BUZZER D6

//web page
#include <ESP8266WebServer.h>
ESP8266WebServer server(80);

// global variables(for html page)
static unsigned int lightSensorValue = 0;
static int show = 0;
static float t = 0;
static float h = 0;
static int tilted = 0;
static int alarmOn = 0;
int static init_db = 0;
bool static led_status = HIGH;
static long rssi = 0;
 
void setup() {
  Serial.begin(115200);
  Serial.println(F("\n\nCheck LCD connection..."));
  Wire.begin();
  Wire.beginTransmission(DISPLAY_ADDR);
  byte error = Wire.endTransmission();
  if (error == 0) {
    Serial.println(F("LCD found."));
    lcd.begin(DISPLAY_CHARS, 2);   // initialize the lcd
  } else {
    Serial.print(F("LCD not found. Error "));
    Serial.println(error);
    Serial.println(F("Check connections and configuration. Reset to try again!"));
    while (true)
      delay(1);
  }
  dht.begin();

  WiFi.mode(WIFI_STA);

  server.on("/", handle_root);
  server.onNotFound(handle_NotFound);
  server.begin();
  
  pinMode(LED_ONBOARD, OUTPUT);
  digitalWrite(LED_ONBOARD, HIGH);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);
  pinMode(TILT, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, HIGH);

  Serial.println(F("\n\nSetup completed.\n\n"));
}

void loop() {
  
  //
  //            temperature-humidity and DANGER_TEMP_HUM alert -> BUILTIN_LED(sx) & RED LED
  //
  
  h = dht.readHumidity();
  t = dht.readTemperature();
  if (dht.computeHeatIndex(t, h, false) >= DANGER_TEMP_HUM)    // high temperature perceived
    digitalWrite(LED, LOW);                           
   else                                              
    digitalWrite(LED, HIGH);                         

  
  //
  //            brightness
  //
  
  lightSensorValue = analogRead(PHOTORESISTOR);   // read analog value (range 0-1023)

 
  //
  //            tilt
  //
  
  byte val = digitalRead(TILT);
  if (val == HIGH) {   
    tilted = 1; //TILTED
  } else {                  
    tilted = 0; //NOT TILTED
  }


  //
  //            set alarm (based on tilt-state and brightness)
  //
 
  if(tilted == 1 && lightSensorValue >= 600){
    digitalWrite(BUZZER, LOW);
    alarmOn = 1;
  }
  else{
    digitalWrite(BUZZER, HIGH);
    alarmOn = 0;
  }


  //
  //            rssi and bad signal alert -> LED_BUILTIN_AUX(dx) & YELLOW LED
  //
  
  rssi = connectToWiFi();  // WiFi connect if not established and if connected get wifi signal strength

  if ((rssi > RSSI_THRESHOLD) && (!led_status)) {   // good signal strength -> keep led off
    led_status = HIGH;
    digitalWrite(LED_ONBOARD, led_status);
  } else if ((rssi <= RSSI_THRESHOLD) && (led_status)) {   // bad signal strength -> keep led on
    led_status = LOW;
    digitalWrite(LED_ONBOARD, led_status);
  }
  
  // ---------------------------------------- LCD: HELLO
  if (show == 0) {
    lcd.setBacklight(255);    // set backlight to maximum
    lcd.home();               // move cursor to 0,0
    lcd.clear();              // clear text
    lcd.print("Hello LCD");   // show text
    delay(1000);

    // blink backlight
    lcd.setBacklight(0);   // backlight off
    delay(400);
    lcd.setBacklight(255);

    // ---------------------------------------- LCD: TEMPERATURE AND HUMIDITY
  } else if (show == 1){
    lcd.clear();
    
    if (isnan(h) || isnan(t)) {   // readings failed, skip
      Serial.println(F("Failed to read(show=1)"));
      return;
    }

    // compute heat index in Celsius (isFahreheit = false)
    float hic = dht.computeHeatIndex(t, h, false);
    lcd.setCursor(0, 0);
    lcd.print("H: ");
    lcd.print(int(h));
    lcd.print("% T:");
    lcd.print(int(t));
    lcd.print(" C");
    lcd.setCursor(0, 1);
    lcd.print("A.temp: ");   // the temperature perceived by humans (takes into account humidity)
    lcd.print(hic);
    lcd.print(" C");

    // ---------------------------------------- LCD: RSSI, BRIGHTNESS, TILT
  }

  else if (show == 2){
    lcd.clear();

    if (isnan(rssi) || isnan(lightSensorValue) || isnan(tilted)) {   // readings failed, skip
      Serial.println(F("Failed to read(show=2)"));
      return;
    }

    lcd.setCursor(0, 0);
    lcd.print("RSSI: ");
    lcd.print(int(rssi));
    lcd.print("dBm");
    lcd.setCursor(0, 1);
    lcd.print("L:");
    lcd.print(int(lightSensorValue*100/1023));
    lcd.print("% ");
    if(tilted == 0)
      lcd.print("Tilt: OFF");   // the temperature perceived by humans (takes into account humidity)
     else
      lcd.print("Tilt: ON");

    // ----------------------------------------
  }

  show += 1;
  if(show == 3)
    show = 1;             //alternates LCD views

  check_influxdb();
  if (init_db == 0) {   // set tags
    connection.addTag("source", "ESP8266_WifiModule");
    temp_hum.addTag("source", "DHT11");
    photores.addTag("source", "Photoresistor");
    tilt.addTag("source", "Tilt_sensor");
    alarm.addTag("source", "Buzzer");
    init_db = 1;
  }
  WriteMultiToDB(ssid, (int)rssi, !led_status, (int)t, int(h), lightSensorValue, tilted, alarmOn);
  server.handleClient();

  delay(1000);
}

void printWifiStatus() {
  Serial.println(F("\n=== WiFi connection status ==="));

  // SSID
  Serial.print(F("SSID: "));
  Serial.println(WiFi.SSID());

  // signal strength
  Serial.print(F("Signal strength (RSSI): "));
  Serial.print(WiFi.RSSI());
  Serial.println(F(" dBm"));

  // current IP
  Serial.print(F("IP Address: "));
  Serial.println(WiFi.localIP());

  // subnet mask
  Serial.print(F("Subnet mask: "));
  Serial.println(WiFi.subnetMask());

  // gateway
  Serial.print(F("Gateway IP: "));
  Serial.println(WiFi.gatewayIP());

  // DNS
  Serial.print(F("DNS IP: "));
  Serial.println(WiFi.dnsIP());

  Serial.println(F("==============================\n"));
}

long connectToWiFi() {
  long rssi_strength;
  // connect to WiFi (if not already connected)
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print(F("Connecting to SSID: "));
    Serial.println(ssid);

#ifdef IP
    WiFi.config(ip, dns, gateway, subnet);   // by default network is configured using DHCP
#endif

    WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED) {
      Serial.print(F("."));
      delay(250);
    }
    Serial.println(F("\nConnected!"));
    rssi_strength = WiFi.RSSI();   // get wifi signal strength
    printWifiStatus();
  } else {
    rssi_strength = WiFi.RSSI();   // get wifi signal strength
  }

  return rssi_strength;
}

int WriteMultiToDB(char ssid[], int rssi, int led_status, int temperature, int humidity, unsigned int lightSensorValue, int tilt_value, int alarmOn) { 
  // Store measured value into points
  connection.clearFields();
  temp_hum.clearFields();
  photores.clearFields();
  tilt.clearFields();
  alarm.clearFields();
  
  connection.addField("rssi", rssi);
  connection.addField("bad_signal", led_status);
  temp_hum.addField("temperature", temperature);
  temp_hum.addField("humidity", humidity);
  temp_hum.addField("perceived_temperature", dht.computeHeatIndex(temperature, humidity, false));
  int danger_temp;
  if(dht.computeHeatIndex(temperature, humidity, false) >= DANGER_TEMP_HUM)
    danger_temp = 1;
  else
    danger_temp = 0;
  temp_hum.addField("danger_temp", danger_temp);
  photores.addField("brightness", lightSensorValue);
  tilt.addField("tilted", tilt_value);
  alarm.addField("Alarm_on", alarmOn);
  
  Serial.print(F("Writing: "));
  Serial.println(connection.toLineProtocol());
  if (!client_idb.writePoint(connection)) {
    Serial.print(F("InfluxDB write failed: "));
    Serial.println(client_idb.getLastErrorMessage());
  }
  Serial.println(temp_hum.toLineProtocol());
  if (!client_idb.writePoint(temp_hum)) {
    Serial.print(F("InfluxDB write failed: "));
    Serial.println(client_idb.getLastErrorMessage());
  }
  Serial.println(photores.toLineProtocol());
  if (!client_idb.writePoint(photores)) {
    Serial.print(F("InfluxDB write failed: "));
    Serial.println(client_idb.getLastErrorMessage());
  }
  Serial.println(tilt.toLineProtocol());
  if (!client_idb.writePoint(tilt)) {
    Serial.print(F("InfluxDB write failed: "));
    Serial.println(client_idb.getLastErrorMessage());
  }
  Serial.println(alarm.toLineProtocol());
  if (!client_idb.writePoint(alarm)) {
    Serial.print(F("InfluxDB write failed: "));
    Serial.println(client_idb.getLastErrorMessage());
  }

  Serial.println(F("Wait 2s"));
  delay(1000);
}

void check_influxdb() {
  // Check server connection
  if (client_idb.validateConnection()) {
    Serial.print(F("Connected to InfluxDB: "));
    Serial.println(client_idb.getServerUrl());
  } else {
    Serial.print(F("InfluxDB connection failed: "));
    Serial.println(client_idb.getLastErrorMessage());
  }
}

void handle_root() {
  Serial.print(F("New Client with IP: "));
  Serial.println(server.client().remoteIP().toString());
  server.send(200, F("text/html"), SendHTML());
}

void handle_NotFound() {
  server.send(404, F("text/plain"), F("Not found"));
}

String SendHTML() { 
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr += "<head><meta http-equiv=\"refresh\" content=\"30\" name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr += "<title>Home monitoring system</title>\n";
  ptr += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  ptr += "body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;} h3 {color: #444444;margin-bottom: 50px;}\n";
  ptr += "p {font-size: 14px;color: #888;margin-bottom: 10px;}\n";
  ptr += "</style>\n";
  ptr += "</head>\n";
  ptr += "<body>\n";
  ptr += "<h1> Home monitoring system </h1>\n";

  ptr += "<center><table style=\"width:20%\"><tr><th>Field</th><th>Value</th></tr>";
  ptr += "<tr><td>RSSI</td><td>" + String(int(rssi)) + " dBm</td></tr>";
  String a;
  if(!led_status == 0)
    a = "No";
   else
    a = "Yes";
  ptr += "<tr><td>Bad wifi signal</td><td>" + a + "</td></tr>"; //String(!led_status)
  ptr += "<tr><td>Temperature</td><td>" + String(int(t)) + " C</td></tr>";
  ptr += "<tr><td>Humidity</td><td>" + String(int(h)) + "%</td></tr>";
  ptr += "<tr><td>Perceived temperature</td><td>" + String(dht.computeHeatIndex(t, h, false)) + " C</td></tr>";
  if (dht.computeHeatIndex(t, h, false) >= DANGER_TEMP_HUM)
     ptr += "<tr><td>Too high temperature</td><td> Yes </td></tr>";
   else
     ptr += "<tr><td>Too high temperature</td><td> No </td></tr>";
  ptr += "<tr><td>Brightness</td><td>" + String(int(lightSensorValue*100/1023)) + "%</td></tr>";
  if(tilted == 0)
    a = "No";
   else
    a = "Yes";
  ptr += "<tr><td>Tilted</td><td>" + a + "</td></tr>"; //String(tilted)
  if(alarmOn == 0)
    a = "No";
   else
    a = "Yes";
  ptr += "<tr><td>Alarm on</td><td>" + a + "</td></tr></table></center>"; // String(alarmOn)

  ptr += "</body>\n";
  ptr += "</html>\n";
  return ptr;
}