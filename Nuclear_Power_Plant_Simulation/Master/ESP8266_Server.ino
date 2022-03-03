/*=====LIBRARIES=======================================================*/
#include "secrets.h"
#include <ESP8266WiFi.h> //WIFI LIBRARIES
#include <ArduinoJson.h>
#include <MQTT.h>
#include <NTPClient.h>
#include <WiFiUdp.h>     //NTP LIBRERIES TO GET TIME
#include <LiquidCrystal_I2C.h>   // display library
#include <Wire.h>                // I2C library
#include <DHT.h>        // DHT SENSOR LIBRARY
#include <MySQL_Connection.h>
#include <MySQL_Cursor.h>    //MYSQL libraries
#include <InfluxDbClient.h>  //InfluxDB library
#include <iostream>
#include <string>
/*=====================================================================*/



/*=====PIN=============================================================*/
#define DISPLAY_CHARS 16    // number of characters on a line
#define DISPLAY_LINES 2     // number of display lines
#define DISPLAY_ADDR 0x27 // display address on I2C bus
#define GREEN_LED D0       // sensor I/O pin, eg. D3 (DO NOT USE D0 or D4! see above notes)
#define YELLOW_LED D7
#define BUTTON D5 
#define RED_LED D6
#define BUTTON_DEBOUNCE_DELAY 20
#define DHTPIN D4       // sensor I/O pin, eg. D3 (DO NOT USE D0 or D4! see above notes)
#define DHTTYPE DHT11   // sensor type DHT 
#define POTENTIOMETER A0
/*=====================================================================*/



/*=====GLOBAL VARIABLES================================================*/
bool show= false;
long rssi;
float h, t, hic;
String rssi_t= "-100";
String temperature_t= "2000";
String pressure_t = "55";
String radioactivity_t= "15";
String MAC;
String macs[5];
bool start_session= false;
int count=0;
int radioactivity_g = 5;
int temperature_g = 0;
int pressure_g = 0;
long signal_g = 0;
bool global_alert= false;
bool t_alert=false;
bool l_alert=false;
static float moltiplicazione=1;
unsigned int potentiometerValue;
bool stopAlert=false;
String Time="";
/*=====================================================================*/



/*=====DEFINE==========================================================*/
// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

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
Point pointDevice("master");

// MySQL server cfg
char mysql_user[] = MYSQL_USER;       // MySQL user login username
char mysql_password[] = MYSQL_PASS;   // MySQL user login password
IPAddress server_addr(MYSQL_IP);      // IP of the MySQL *server* here
MySQL_Connection conn((Client *)&client);

//MYSQL query to insert data
char query[256];
char INSERT_DATA[] = "INSERT INTO `dbellini3`.`Nuclear_Power_Plant` (`DateTime`, `AlertType`, `Node`, `Role`, `CurrentValue`, `Threshold`) VALUES ('%s', '%s', '%s', '%s', '%d', '%d')";

// MQTT data
MQTTClient mqttClient;                     // handles the MQTT communication protocol
WiFiClient networkClient;                  // handles the network connection to the MQTT broker

// DHT sensor
DHT dht = DHT(DHTPIN, DHTTYPE);

LiquidCrystal_I2C lcd(DISPLAY_ADDR, DISPLAY_CHARS, DISPLAY_LINES);   // display object

/*=====================================================================*/



/*====== TOPICS =============================================================*/

const char *MQTT_TOPIC_WEB = "db3ap/web/parameters";
const char *MQTT_WEB_THRESHOLD = "db3ap/web/threshold";
const char *MQTT_CONFIG = "db3ap/access/config";
const char *MQTT_CONFIG_SLAVE = "db3ap/access/";
const char *MQTT_SLAVE = "db3ap/slave/+";
const char *MQTT_WEB_START= "db3ap/web/start";
const char *MQTT_WEB_ALERT = "db3ap/web/stopAlert";

/*===========================================================================*/



/*======SETUP==========================================================*/

void setup() {
  WiFi.mode(WIFI_STA);  
  printWifiStatus();
  timeClient.begin();
  timeClient.setTimeOffset(7200);
  dht.begin();
  setupLCD();
  //pointDevice.addTag("device", "ESP8266");
  //pointDevice.addTag("SSID", WiFi.SSID());
  pinMode(GREEN_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BUTTON, INPUT_PULLUP);
  digitalWrite(GREEN_LED, HIGH);
  digitalWrite(YELLOW_LED, HIGH);
  digitalWrite(RED_LED, HIGH);
  
  // setup MQTT
  mqttClient.begin(MQTT_BROKERIP, 1883, networkClient);   // setup communication with MQTT broker
  mqttClient.onMessage(mqttMessageReceived);
  rssi= connectToWiFi();
  connectToMQTTBroker();   // connect to MQTT broker (if not already connected)
  mqttClient.loop();

  Serial.begin(115200);
  Serial.println(F("\n\nSetup completed."));
}


/*=====================================================================*/





/*====LOOP=============================================================*/


void loop() {
  static byte green_led = HIGH;
  static byte yellow_led = HIGH;
  static byte red_led = HIGH;
  static unsigned long time_temp = 0;
  static unsigned long alert_temp = 0;

  rssi= connectToWiFi();
  connectToMQTTBroker();   // connect to MQTT broker (if not already connected)
  mqttClient.loop();

  if (start_session && (!global_alert) && (!t_alert)){
    if ( millis() - time_temp > 40000){
      startLCD();
      retrieveDHT();
      
      temperature_g+=t*20;
      pressure_g+=h;
      signal_g+=rssi;
      Serial.print(F("Temperature"));
      Serial.println((temperature_g/(count+1)));
      Serial.print(F("Pressure"));
      Serial.println((pressure_g/(count+1)));
      Serial.print(F("Rssi"));
      Serial.println((signal_g/(count+1)));
      Serial.print(F("Radioactivity"));
      Serial.println((radioactivity_g/(count+1)));
      if((temperature_g/(count+1))>temperature_t.toInt()){
        t_alert=true;
        digitalWrite(GREEN_LED, HIGH);
        digitalWrite(YELLOW_LED, HIGH);
        digitalWrite(RED_LED, LOW);
        tempLCD();
        Time="";
        get_Time ();
        WriteMultiToMySqlDB(Time, "Temperature", MAC, "master", (int)temperature_g/(count+1), temperature_t.toInt());
        potentiometerValue = analogRead(POTENTIOMETER);
        Serial.print(F("COOLING POWER: "));
        Serial.println((potentiometerValue*100)/10230);
        const int capacity = JSON_OBJECT_SIZE(2);
        StaticJsonDocument<capacity> JSONbuffer;
        JSONbuffer["start"]= start_session;
        int num= (potentiometerValue*100)/10230;
        if (num > 0){
          JSONbuffer["cooling"]= num;
        }else{
          JSONbuffer["cooling"]= 1;
        }
        char json[600];
        size_t n =serializeJson(JSONbuffer, json);
        for (int i=0; i <5; i++){
          if(macs[i]!="" && macs[i]!=MAC){
            String topic_start= "db3ap/slave/" + String(macs[i])+ "/start";
            mqttClient.publish(topic_start.c_str(), json, n);
          }
        }
      }else if (millis()-alert_temp>50000 && (((pressure_g/(count+1))>pressure_t.toInt())||((signal_g/(count+1))<rssi_t.toInt())||((radioactivity_g/(count+1))>radioactivity_t.toInt()))){
        if ((pressure_g/(count+1))>pressure_t.toInt()){
          Time="";
          get_Time();
          WriteMultiToMySqlDB(Time, "Pressure", MAC, "master", (int)pressure_g/(count+1), pressure_t.toInt());
          pressureLCD();
        }else if ((signal_g/(count+1))<rssi_t.toInt()){
          
          Time="";
          get_Time();
          WriteMultiToMySqlDB(Time, "WiFi Strength", MAC, "master", (int)signal_g/(count+1), rssi_t.toInt());
          signalLCD();
        }else if ((radioactivity_g/(count+1))>radioactivity_t.toInt()){
          Time="";
          get_Time();
          String mac_error="";
          bool adds=true;
          for (int i=0; i <5; i++){
            if(macs[i]!="" && macs[i]!=MAC && adds){
              mac_error= macs[i];
              adds=false;       
            }
          }
          WriteMultiToMySqlDB(Time, "Radioactivity", mac_error, "slave", (int)radioactivity_g/(count+1), radioactivity_t.toInt());
          radioactivityLCD();
        }
        l_alert=true;
        digitalWrite(GREEN_LED, HIGH);
        digitalWrite(YELLOW_LED, LOW);
        digitalWrite(RED_LED, HIGH);
      }else{
        digitalWrite(GREEN_LED, LOW);
        digitalWrite(YELLOW_LED, HIGH);
        digitalWrite(RED_LED, HIGH);
      }
      
      const int capacity = JSON_OBJECT_SIZE(6);
      StaticJsonDocument<capacity> JSONbuffer;
      bool adds=true;
      for (int i=0; i <5; i++){
        if(macs[i]!="" && adds){
          JsonArray ports = JSONbuffer.createNestedArray(macs[i]);
          if (l_alert){
            ports.add("yellow");
          }else if (t_alert){
            ports.add("orange");
          }else if (global_alert){
            ports.add("red");
          }else{
            ports.add("green");
          }
          
          if (macs[i]==MAC){
            ports.add("master");
          }else{
            ports.add("slave");
          }
        }else {
          adds=false;
        }        
      }
      
      
      char json[600];
      size_t n =serializeJson(JSONbuffer, json);
      mqttClient.publish(MQTT_TOPIC_WEB, json, n);
      check_influxdb();
      WriteMultiToInfluxDB((int)signal_g/(count+1), pressure_g/(count+1), temperature_g/(count+1), radioactivity_g/(count+1), (int)rssi, (int)t*20, (int)h);
      time_temp= millis();
      temperature_g=0;
      pressure_g=0;
      signal_g=0;
      radioactivity_g=5;
      count=0;
    }
    if ((isButtonPressed()==true && l_alert==true) || stopAlert){
      l_alert = false;
      stopAlert=false;
      alert_temp=millis();
      startLCD();
      digitalWrite(GREEN_LED, LOW);
      digitalWrite(YELLOW_LED, HIGH);
      digitalWrite(RED_LED, HIGH);
    } 
  }else{
    welcomeLCD();
  }
}

/*=====================================================================*/






/*===========METHODS===================================================*/


void mqttMessageReceived(String &topic, String &payload) {
  // this function handles a message from the MQTT broker
  Serial.println("Incoming MQTT message: " + topic + " - " + payload);


/*==============================CONFIGURAZIONE===============================*/

  if (topic == MQTT_CONFIG) {
    // deserialize the JSON object
    StaticJsonDocument<128> doc;
    deserializeJson(doc, payload);
    const char *new_mac = doc["MAC"];
    String topic= String(MQTT_CONFIG_SLAVE) + String(new_mac);
    String topic2= "db3ap/slave/" + String(new_mac);
    String topic3= "db3ap/slave/" + String(new_mac) + "/start";
    bool addMac=true;
    for (int i=0; i <5; i++){
      if(macs[i]!="" && addMac){
        addMac=false;
        macs[i]= String(new_mac);
      }      
    }
    const int capacity = JSON_OBJECT_SIZE(7);
    StaticJsonDocument<capacity> JSONbuffer;
    JSONbuffer["topic1"]= topic2;   
    JSONbuffer["topic2"]= topic3;
    
    
    //JSONbuffer["welcome_msg"]= "Per poter connettersi con successi alla rete un sensore deve fornire i seguenti parametri: radioactivity, temperature, pressure, signal e danger_port. In mancanza di strumenti mancanti settare i campi al valore di default di -1.";  
    const char *publish_topic= topic.c_str();
    char json[600];
    size_t n =serializeJson(JSONbuffer, json);
    mqttClient.publish(publish_topic, json, n);
  }


/*==================================CAMBIO SOGLIE ===========================*/
  
  if (topic == MQTT_WEB_THRESHOLD){
    StaticJsonDocument<300> doc;
    deserializeJson(doc, payload);
    const char *rssi_char = doc["rssi"];
    const char *temperature_char = doc["temperature"];
    const char *moisture_char = doc["pressure"];
    const char *radioactivity_char = doc["radioactivity"];
    
    if (String(temperature_char) !=""){
      temperature_t = String(temperature_char);
    }
    if (String(rssi_char) !=""){
      rssi_t = String(rssi_char);
    }
    if (String(moisture_char) !=""){
      pressure_t = String(moisture_char);
    }
    if (String(radioactivity_char) !=""){
      radioactivity_t= String(radioactivity_char);
    }
  }


/*=================================START / STOP SESSION =====================*/

  
  if (topic == MQTT_WEB_START){
      StaticJsonDocument<300> doc;
      deserializeJson(doc, payload);
      bool session_char = doc["start"];
      if (session_char){
        Serial.println("Start Session");
        start_session= true;
        const int capacity = JSON_OBJECT_SIZE(2);
        
        StaticJsonDocument<capacity> JSONbuffer;
        JSONbuffer["start"]= start_session;
        JSONbuffer["cooling"]= 0;
        char json[600];
        size_t n =serializeJson(JSONbuffer, json);
        for (int i=0; i <5; i++){
          if(macs[i]!="" && macs[i]!=MAC){
            String topic_start= "db3ap/slave/" + String(macs[i])+ "/start";
            mqttClient.publish(topic_start.c_str(), json, n);
          }
        }
      }else{
        Serial.println("Stop Session");
        start_session= false;
        digitalWrite(GREEN_LED, HIGH);
        digitalWrite(YELLOW_LED, HIGH);
        digitalWrite(RED_LED, HIGH);
        const int capacity = JSON_OBJECT_SIZE(2);
        StaticJsonDocument<capacity> JSONbuffer;
        JSONbuffer["start"]= start_session;
        JSONbuffer["cooling"]= 0;
        char json[600];
        size_t n =serializeJson(JSONbuffer, json);
        for (int i=0; i <5; i++){
          if(macs[i]!="" && macs[i]!=MAC){
            String topic_start= "db3ap/slave/" + String(macs[i])+ "/start";
            mqttClient.publish(topic_start.c_str(), json, n);
          }
        }
      }
   }

/*============================== HANDLE REMOTE ALERT=========================*/
   
   if (topic == MQTT_WEB_ALERT){
      stopAlert=true;
   }


/*============================ SLAVE PARAMETERS =============================*/

   
   if (!(topic == MQTT_WEB_START || topic == MQTT_WEB_THRESHOLD || topic == MQTT_CONFIG || topic == MQTT_WEB_ALERT)){
      StaticJsonDocument<300> doc;
      deserializeJson(doc, payload);
      int radioactivity = doc["radioactivity"];
      int temperature = doc["temperature"];
      int pressure = doc["pressure"];
      long signal_c = doc["signal"];
      bool alert = doc["danger_port"];
      count+=1;
      if (alert){
        Serial.println("Debug red");
        global_alert=true;
        globalLCD();
        Time="";
        get_Time();
        WriteMultiToMySqlDB(Time, "Global Error", topic, "slave", 0, 0);
        digitalWrite(GREEN_LED, HIGH);
        digitalWrite(YELLOW_LED, HIGH);
        digitalWrite(RED_LED, LOW);
        const int capacity = JSON_OBJECT_SIZE(6);
        StaticJsonDocument<capacity> JSONbuffer;
        bool adds=true;
        for (int i=0; i <5; i++){
          if(macs[i]!="" && adds){
            JsonArray ports = JSONbuffer.createNestedArray(macs[i]);
            if (l_alert || t_alert){
              ports.add("yellow");
            }else if (global_alert){
              ports.add("red");
            }else{
              ports.add("green");
            }
            
            if (macs[i]==MAC){
              ports.add("master");
              Serial.println(i);
            }else{
              ports.add("slave");
            }
          }else {
            adds=false;
          }        
        }
        char json[600];
        size_t n =serializeJson(JSONbuffer, json);
        mqttClient.publish(MQTT_TOPIC_WEB, json, n);
      }else{
        global_alert=false;
      }
      moltiplicazione= moltiplicazione * 1.15;
      Serial.print(F("Moltiplicazione: "));
      Serial.println(moltiplicazione);
      radioactivity_g+=radioactivity;
      
      pressure_g+=pressure;
      signal_g+=signal_c;
      if (t_alert==true){
        moltiplicazione=1;
        temperature_g=0;
        radioactivity_g=5;
        pressure_g=0;
        signal_g=0;
        count=0;
        t_alert=false;
        digitalWrite(GREEN_LED, LOW);
        digitalWrite(YELLOW_LED, HIGH);
        digitalWrite(RED_LED, HIGH);
      }
      temperature_g+=temperature*moltiplicazione;
      
   }
}



/*=================================SUBSCRIBES================================*/


void connectToMQTTBroker() {
  if (!mqttClient.connected()) {   // not connected
    //Serial.print(F("\nConnecting to MQTT broker..."));
    while (!mqttClient.connect(MQTT_CLIENTID, MQTT_USERNAME, MQTT_PASSWORD)) {
      //Serial.print(F("."));
      delay(1000);
    }
    //Serial.println(F("\nConnected!"));

    // connected to broker, subscribe topics
    mqttClient.subscribe(MQTT_CONFIG);
    mqttClient.subscribe(MQTT_WEB_THRESHOLD);
    mqttClient.subscribe(MQTT_WEB_ALERT);
    mqttClient.subscribe(MQTT_WEB_START);
    mqttClient.subscribe(MQTT_SLAVE);
  }
}


/*================================= WIFI METHODS ============================*/


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

void printWifiStatus() {
  Serial.println(F("\n=== WiFi connection status ==="));

  // SSID
  Serial.print(F("SSID: "));
  Serial.println(WiFi.SSID());

  // signal strength
  Serial.print(F("Signal strength (RSSI): "));
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");

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

  //MAC ADDRESS
  Serial.print(F("MAC address: "));
  Serial.println(WiFi.macAddress());
  MAC = WiFi.macAddress();
  macs[0]= MAC;

  Serial.println(F("==============================\n"));
}



/*================================== LCD METHODS ============================*/


void setupLCD(){
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
}

void welcomeLCD(){
  lcd.setBacklight(255);              // clear text
  lcd.setCursor(0, 0);
  lcd.print("2nd  Assignment:");
  lcd.setCursor(0, 1);
  lcd.print("use remote cntrl");
}

void startLCD(){
  lcd.setBacklight(255);   
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("   connected   ");
  lcd.setCursor(0, 1);
  lcd.print("nodes: ");
  int num=0;
  for (int i=0; i<5; i++){
    if(macs[i]!=""){
      num+=1;
    }
  }
  lcd.print(num+1);
}

void tempLCD(){
  lcd.setBacklight(255);   
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Global Error");
  lcd.print(temperature_t);
  lcd.setCursor(0, 1);
  lcd.print(" fix it now!");
}

void globalLCD(){
  lcd.setBacklight(255);   
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Reactor C°>");
  lcd.print(temperature_t);
  lcd.setCursor(0, 1);
  lcd.print("need cooling");
}

void pressureLCD(){
  lcd.setBacklight(255);   
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("pressure >");
  lcd.print(pressure_t);
  lcd.setCursor(0, 1);
  lcd.print("use fix-button");
}

void signalLCD(){
  lcd.setBacklight(255);   
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("wifi signal <");
  lcd.print(rssi_t);
  lcd.setCursor(0, 1);
  lcd.print("use fix-button");
}

void radioactivityLCD(){
  lcd.setBacklight(255);   
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("R. value >");
  lcd.print(radioactivity_t);
  lcd.setCursor(0, 1);
  lcd.print("use fix-button");
}


/*=============================== SENSOR METHODS ============================*/


void retrieveDHT(){
  h = dht.readHumidity();      // humidity percentage, range 20-80% (±5% accuracy)
  t = dht.readTemperature();   // temperature Celsius, range 0-50°C (±2°C accuracy)
  // compute heat index in Celsius (isFahreheit = false)
  hic = dht.computeHeatIndex(t, h, false);
  if (isnan(h) || isnan(t)) {   // readings failed, skip
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }
}

boolean isButtonPressed() {
  static byte lastState = digitalRead(BUTTON);   // the previous reading from the input pin

  for (byte count = 0; count < BUTTON_DEBOUNCE_DELAY; count++) {
    if (digitalRead(BUTTON) == lastState) return false;
    delay(1);
  }

  lastState = !lastState;
  return lastState == HIGH ? false : true;
}



/*================================== DB METHODS =============================*/

int WriteMultiToMySqlDB(String field1, String field2, String field3, String field4, int field5, int field6) {
  int error;
  if (conn.connect(server_addr, 3306, mysql_user, mysql_password)) {
    Serial.println(F("MySQL connection established."));
    int n = field1.length();
    char field_1[n+1];
    strcpy(field_1, field1.c_str());
    n = field2.length();
    char field_2[n+1];
    strcpy(field_2, field2.c_str());
    n = field3.length();
    char field_3[n+1];
    strcpy(field_3, field3.c_str());
    n = field4.length();
    char field_4[n+1];
    strcpy(field_4, field4.c_str());
    MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);
    sprintf(query, INSERT_DATA, field_1, field_2, field_3, field_4, field5, field6);
    Serial.println(query);
    // execute the query
    cur_mem->execute(query);
    // Note: since there are no results, we do not need to read any data
    // deleting the cursor also frees up memory used
    delete cur_mem;
    error = 1;
    Serial.println(F("Data recorded on MySQL"));

    conn.close();
  } else {
    Serial.println(F("MySQL connection failed."));
    error = -1;
  }

  return error;
}

void get_Time(){
  timeClient.update();
  unsigned long epochTime = timeClient.getEpochTime();
  Serial.println(String(epochTime));
  struct tm *ptm = gmtime ((time_t *)&epochTime);
  Time += String(ptm->tm_mday) + "-";
  Time += String(ptm->tm_mon+1) + "-";
  Time += String(ptm->tm_year+1900) + " ";
  Time += timeClient.getFormattedTime();
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

int WriteMultiToInfluxDB(int rssiG, int pressureG, int temperatureG, int radioactivityG, int rssiL, int tL, int hL) {

  // Store measured value into point
  pointDevice.clearFields();
  // Report RSSI of currently connected network
  pointDevice.addField("rssi_global", rssiG);
  pointDevice.addField("radio_global", radioactivityG);
  pointDevice.addField("press_global", pressureG);
  pointDevice.addField("temp_global", temperatureG);
  pointDevice.addField("rssi", rssiL);
  pointDevice.addField("temperature_m", tL);
  pointDevice.addField("pressure_m", hL);
  Serial.print(F("Writing: "));
  Serial.println(pointDevice.toLineProtocol());
  if (!client_idb.writePoint(pointDevice)) {
    Serial.print(F("InfluxDB write failed: "));
    Serial.println(client_idb.getLastErrorMessage());
  }
}
