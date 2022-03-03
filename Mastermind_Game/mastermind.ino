/*
  Assignemt 3 - Mastermind
  Diego Bellini - 816602
  Andrea Premate - 829777

  Il codice dello slave è uguale al seguente eccetto per la sezione
  indicata con la keyword "MASTER" all'interno del metodo MqttReceivd
*/



/*=====LIBRARIES=======================================================*/
#include "secrets.h"
#include <IRrecv.h>            // receiver data types
#include <IRremoteESP8266.h>   // library core
#include <IRutils.h>           // print function
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266TelegramBOT.h>
#include <WiFiClientSecure.h>
#include <MQTT.h>
#include <MySQL_Connection.h>
#include <MySQL_Cursor.h>    //MYSQL libraries
#include <LiquidCrystal_I2C.h>
#include <Wire.h>  
#include <EEPROM.h>
/*

AUTO LOCALIZZAZIONE TRAMITE GOOGLE API (non applicata perchè a pagamanto/inserimento dati carta credito)

#include <WifiLocation.h>
const char* googleApiKey = "AIzaSyDdl6__21Wvw0h7RkS9azt7TUHKs37jV20";
WifiLocation location (googleApiKey);
nel setup()
location_t loc = location.getGeoFromWiFi();

*/
/*=====================================================================*/

/*=====PIN=============================================================*/

#define GREEN_LED D0
#define DISPLAY_CHARS 16    // number of characters on a line
#define DISPLAY_LINES 2     // number of display lines
#define DISPLAY_ADDR 0x27   // display address on I2C bus
#define COMB_LEN 4
#define YELLOW_LED D4
#define BUTTON D5 
#define BUTTON_DEBOUNCE_DELAY 20
#define RED_LED D6
#define RECV_PIN D7

/*=====================================================================*/


/*=====GLOBAL VARIABLES================================================*/

static String usrTg[3] = {"", "",""};
static String trycomb[4] = {"####", "####","####","####"};
static String enemyComb[4] = {"####", "####","####","####"};
static String waitingRoom[6] = {"", "", "", "", "", ""};
static String tgRoom[6] = {"", "", "", "", "", ""};
String enemy_usr="";
//String local_usr="dab2097";
String local_usr="";
String topic2="";
String topic3="";
bool play=false;
bool my_comb= false;
bool guess=false;
bool new_match=true;
int count=0;
unsigned long time_temp = 0;
String combinazione="";
String tentativo="";
int n_try;
bool continueloop;
String enemy_res="";
int enemy_try=-1;
bool weather;
static int n_player= 0;
static String topic_match= "default";
static int penalty=0;
long lastInteractionTime;  
static bool afk = false;
String stat="";
String mqtt_user="";
String mqtt_user_change="";
String tg_user="";
String tg_enemy="";
char json300[300];
char json340[340];
char json180[180];

/*=====================================================================*/



/*=====DEFINE==========================================================*/
// WiFi cfg
#ifdef IP
IPAddress ip(IP);
IPAddress subnet(SUBNET);
IPAddress dns(DNS);
IPAddress gateway(GATEWAY);
#endif
WiFiClient client;

// MQTT data
MQTTClient mqttClient;                     // handles the MQTT communication protocol
WiFiClient networkClient;  

// MySQL server cfg
char mysql_user[] = MYSQL_USER;       // MySQL user login username
char mysql_password[] = MYSQL_PASS;   // MySQL user login password
IPAddress server_addr(MYSQL_IP);      // IP of the MySQL *server* here
MySQL_Connection conn((Client *)&client);

//MYSQL query to insert data
char query1[256];
char INSERT_DATA[] = "INSERT INTO `dbellini3`.`Mastermind_example` (`User`, `wins`, `draws`, `lost`) VALUES ('%s', '%d', '%d', '%d')";

char query2[500];
char READ_DATA[] = "SELECT * FROM `dbellini3`.`Mastermind_example`";

char query3[256];
char UPDATE_DATA[] = "UPDATE `dbellini3`.`Mastermind_example` SET `User` = '%s', `wins` = '%d', `draws` = '%d', `lost` = '%d' WHERE `id` = '%d'";

// Telegram bot
TelegramBOT bot(BOT_TOKEN, BOT_NAME, BOT_USERNAME);
#define BOT_MTBS 20000   // mean time between scans for new messages (richiesta aggiornamenti da parte di telegram)
long botLastScanTime;   // last time messages' scan has been done

// weather api (refer to https://openweathermap.org/current)
const char weather_server[] = "api.openweathermap.org";
const char weather_query[] = "GET /data/2.5/weather?q=%s,%s&units=metric&APPID=%s";

// Initialize sensor
IRrecv irrecv(RECV_PIN);
static decode_results results;

LiquidCrystal_I2C lcd(DISPLAY_ADDR, DISPLAY_CHARS, DISPLAY_LINES);

#define MAX_INACTIVITY 300000   // mean time between scans for new messages


/*=====================================================================*/




/*====== TOPICS =============================================================*/

#define MQTT_TOPIC_WELCOME "db3ap/mastermind" 
#define MQTT_TOPIC_WEB "db3ap/mastermind/web" 
//#define MQTT_TOPIC_USER "db3ap/dab2097" 
//#define MQTT_TOPIC_USER_CHANGE "db3ap/dab2097/username" 

/*===========================================================================*/


/*======SETUP==========================================================*/


void setup() {
  Serial.begin(115200);
  Serial.println("device is in Wake up mode");
  connectToWiFi();
  local_usr=WiFi.macAddress();
  while (!Serial) { }

  EEPROM.begin(512); //Size can be anywhere between 4 and 4096 bytes.
  delay(100);
  EEPROM_readAnything(0, afk);
  /*
  for (int i =0; i<local_usr.length(); i++){
    EEPROM_readAnything(i+1, local_usr[i]);
  }
  EEPROM_readAnything(local_usr.length()+1, "\0");
  Serial.print(local_usr);
  */
  Serial.print(F("Previous afk state: "));
  Serial.println(afk);
  irrecv.enableIRIn(true);   // enable the receiver (true -> enable internal pull-up)
  
  pinMode(GREEN_LED, OUTPUT);
  digitalWrite(GREEN_LED, HIGH);
  pinMode(YELLOW_LED, OUTPUT);
  digitalWrite(YELLOW_LED, HIGH);
  pinMode(RED_LED, OUTPUT);
  digitalWrite(RED_LED, HIGH);

  pinMode(BUTTON, INPUT_PULLUP);

  // setup MQTT
  mqttClient.begin(MQTT_BROKERIP, 1883, networkClient);   // setup communication with MQTT broker
  mqttClient.onMessage(mqttMessageReceived);

  setupLCD();
  lastInteractionTime = millis();
  
  Serial.println(F("\n\nSetup completed.\n\n"));
}

/*=====================================================================*/



/*====LOOP=============================================================*/


void loop() {
  static unsigned long time_temp = 0;
  static int show = 0;
  static int penshow =0;
  connectToWiFi();
  connectToMQTTBroker();     // connect to MQTT broker (if not already connected)
  mqttClient.loop();

  if(afk && penshow==0){
    Serial.println("Disconnected from the last game, you will have a penalty: -2 tries");
    afkLCD();
    penalty = 2;
    penshow = 1;
    
  }

  if (show == 0) {
    welcomeLCD();
    show = 1;
  }

  if (millis() > lastInteractionTime + MAX_INACTIVITY) {
    Serial.println("deep sleep until button pressed");
    if(play){
      Serial.println("Disconnected from the game");
      afk = true;
      EEPROM_writeAnything(0, afk);
      const int capacity9 = JSON_OBJECT_SIZE(4);
    StaticJsonDocument<capacity9> doc4;
    doc4["status"] = "abbandono";
    doc4["n_try"] = 11;
    size_t n9 = serializeJson(doc4, json300);
    String defeat_topic= "db3ap/" + String(topic_match) + "/" + String(enemy_usr) + "/endgame";
    
    const int capacity10 = JSON_OBJECT_SIZE(6);
    StaticJsonDocument<capacity10> JSONbuffer9;
    JSONbuffer9["room"] = topic_match;
    JSONbuffer9["user"] = local_usr;
    JSONbuffer9["comb"] = 11;
    JSONbuffer9["operation"] = "abbandono";
    size_t n10 = serializeJson(JSONbuffer9, json340);
    connectToWiFi();   // connect to WiFi (if not already connected)
    connectToMQTTBroker();     // connect to MQTT broker (if not already connected)
    mqttClient.loop(); 
    mqttClient.publish(MQTT_TOPIC_WEB, json340, n10);
    mqttClient.publish(defeat_topic.c_str(), json300, n9);
    }
    else{
      Serial.println("Disconnected correctly");
      if (penalty == 0)
        afk = false;
      EEPROM_writeAnything(0, afk);
    }
    lcd.clear();
    lcd.noBacklight();
    
    ESP.deepSleep(0);
  }
  
  if (millis() > botLastScanTime + BOT_MTBS) {
    bot.getUpdates(bot.message[0][1]);   // launch API GetUpdates up to xxx message
    botExecMessages();                   // process received messages
    botLastScanTime = millis();
  }


/*============================LOOP - START GAME =======================*/

  if (isButtonPressed()==true && new_match==true){
    printCurrentWeather();
    //weather=false;
    if (weather==false){
      const int capacity8 = JSON_OBJECT_SIZE(4);
      StaticJsonDocument<capacity8> JSONbuffer8;
      JSONbuffer8["username"] = local_usr;
      JSONbuffer8["telegram"] = tg_user;
      size_t n8 = serializeJson(JSONbuffer8, json180);
      connectToWiFi();
      connectToMQTTBroker();     // connect to MQTT broker (if not already connected)
      mqttClient.loop();
      mqttClient.publish(MQTT_TOPIC_WELCOME, json180, n8);
      for (int a=0; a<3; a++){
        connectToWiFi();   // connect to WiFi (if not already connected)
        bot.begin();
        if (bot.message[a][1]!=""){
          connectToWiFi();   // connect to WiFi (if not already connected)
          bot.begin();
          String welcome = "Il giocatore " + String(local_usr) + "si sta connettendo alla stanza" ;
          bot.sendMessage(bot.message[a][4], welcome, "");
        }
      }
    }else{
      conditionLCD();
    }
  }

/*============================LOOP - CHOOSE PSWD =======================*/  
  
  if(play){
      if (my_comb){
        if (time_temp==0){
          chooseLCD();
          time_temp=-1;
        }
        
        if (irrecv.decode(&results) && combinazione.length()<4) {
          lastInteractionTime = millis();
          if(toStringKey(results.value) == "1" || toStringKey(results.value) == "2" || toStringKey(results.value) == "3" || toStringKey(results.value) == "4" || toStringKey(results.value) == "5" || toStringKey(results.value) == "6" || toStringKey(results.value) == "7" || toStringKey(results.value) == "8" || toStringKey(results.value) == "9" || toStringKey(results.value) == "0"){
            combinazione+= toStringKey(results.value);
            lcd.setCursor(0, 1);
            lcd.print(combinazione);
          }
          irrecv.resume(); 
          Serial.println(combinazione);
        }
        if (combinazione.length()==4){
        const int capacity = JSON_OBJECT_SIZE(4);
        StaticJsonDocument<capacity> JSONbuffer;
        JSONbuffer["player"] = n_player;
        String comb= "";
        for(int i=0;i<COMB_LEN;i++)
          comb += String(combinazione[i]);
        JSONbuffer["comb"]= comb;
        size_t n = serializeJson(JSONbuffer, json300);
        String new_topic= "db3ap/" + String(topic_match) + "/" + String(enemy_usr);
        connectToWiFi();   // connect to WiFi (if not already connected)
        connectToMQTTBroker();     // connect to MQTT broker (if not already connected)
        mqttClient.loop(); 
        mqttClient.publish(new_topic.c_str(), json300, n);
        my_comb=false;
        const int capacity4 = JSON_OBJECT_SIZE(6);
        StaticJsonDocument<capacity4> JSONbuffer4;
        JSONbuffer4["room"] = topic_match;
        JSONbuffer4["user"] = local_usr;
        JSONbuffer4["comb"] = comb;
        JSONbuffer4["operation"] = "set";
        size_t n4 = serializeJson(JSONbuffer4, json340);
        connectToWiFi();   // connect to WiFi (if not already connected)
        connectToMQTTBroker();     // connect to MQTT broker (if not already connected)
        mqttClient.loop(); 
        mqttClient.publish(MQTT_TOPIC_WEB, json340, n4);
        for (int a=0; a<3; a++){
        connectToWiFi();   // connect to WiFi (if not already connected)
        bot.begin();
        if (bot.message[a][1]!="" && bot.message[a][1]==tg_enemy){
          connectToWiFi();   // connect to WiFi (if not already connected)
          bot.begin();
          String welcome = "Il giocatore " + String(local_usr) + " ha inserito la chiave nascosta" ;
          bot.sendMessage(bot.message[a][4], welcome, "");
        }
      }
        }

/*============================LOOP - ATTEMPTS =========================*/
        
      } else if(guess){ 
      if (time_temp==-1){
        
        Serial.println("Prova ad indovinare la password di " + String(enemy_usr));
        time_temp=2;        
        continueloop = true;
        n_try= 0 + penalty;
        penalty = 0;
        afk=false;
        EEPROM_writeAnything(0, afk);
      }
      if (irrecv.decode(&results) && tentativo.length()<4) {
        lastInteractionTime = millis();
        if(toStringKey(results.value) == "1" || toStringKey(results.value) == "2" || toStringKey(results.value) == "3" || toStringKey(results.value) == "4" || toStringKey(results.value) == "5" || toStringKey(results.value) == "6" || toStringKey(results.value) == "7" || toStringKey(results.value) == "8" || toStringKey(results.value) == "9" || toStringKey(results.value) == "0"){
          tentativo+= toStringKey(results.value);
          if(tentativo.length() == 1){
            attemptLCD();
            lcd.setCursor(0, 1);
            lcd.print("    ");
          }
          
          lcd.setCursor(0, 1);
          lcd.print(tentativo);
        }
        irrecv.resume(); 
        Serial.println(tentativo);
      }
      if (tentativo.length()==4 && n_try < 10 && continueloop && guess){
        n_try++;
        String try_comb= "";
        stat="";
        for(int i=0;i<COMB_LEN;i++){
          try_comb += String(tentativo[i]);
          trycomb[i] = tentativo[i];
        }
        int* eval = evaluate_comb(trycomb);
        eval_to_led_blink(eval);
        bool found=true;
        for (int k=0; k<4; k++){
          if (eval[k]!=1){
            found=false;
          }
        }
        if (found){
            continueloop = false;
            Serial.println("Complimenti hai indovinato la password in: " + String(n_try) + " tentativi!");
            foundLCD();
            guess=false;
            const int capacity2 = JSON_OBJECT_SIZE(4);
            StaticJsonDocument<capacity2> doc2;
            doc2["status"] = "victory";
            doc2["n_try"] = n_try;
            size_t n2 = serializeJson(doc2, json300);
            String won_topic= "db3ap/" + String(topic_match) + "/" + String(enemy_usr) + "/endgame";
            
            const int capacity7 = JSON_OBJECT_SIZE(7);
            StaticJsonDocument<capacity7> JSONbuffer7;
            JSONbuffer7["room"] = topic_match;
            JSONbuffer7["user"] = local_usr;
            JSONbuffer7["comb"] = try_comb;
            JSONbuffer7["operation"] = String(stat);
            size_t n7 = serializeJson(JSONbuffer7, json340);
            connectToWiFi();   // connect to WiFi (if not already connected)
            connectToMQTTBroker();     // connect to MQTT broker (if not already connected)
            mqttClient.loop(); 
            mqttClient.publish(MQTT_TOPIC_WEB, json340, n7);
            mqttClient.publish(won_topic.c_str(), json300, n2);
            for (int a=0; a<3; a++){
              connectToWiFi();   // connect to WiFi (if not already connected)
              bot.begin();
              if (bot.message[a][1]!="" && bot.message[a][1]==tg_enemy){
                connectToWiFi();   // connect to WiFi (if not already connected)
                bot.begin();
                String welcome = "Il giocatore " + String(local_usr) + "ha indovinato la chiave segreta al " + String(n_try)+ " tentativo";
                bot.sendMessage(bot.message[a][4], welcome, "");
              }
            }
        } else if (n_try==10){
          guess=false;
          Serial.print("Purtroppo hai esaurito tutti i tentativi a disposizione, non hai vinto :(");
          loseLCD(); 
          const int capacity3 = JSON_OBJECT_SIZE(4);
          StaticJsonDocument<capacity3> doc3;
          doc3["status"] = "defeat";
          doc3["n_try"] = n_try;
          size_t n3 = serializeJson(doc3, json300);
          String defeat_topic= "db3ap/" + String(topic_match) + "/" + String(enemy_usr) + "/endgame";
          
          const int capacity6 = JSON_OBJECT_SIZE(7);
          StaticJsonDocument<capacity6> JSONbuffer6;
          JSONbuffer6["room"] = topic_match;
          JSONbuffer6["user"] = local_usr;
          JSONbuffer6["comb"] = try_comb;
          JSONbuffer6["operation"] = String(stat);
          size_t n6 = serializeJson(JSONbuffer6, json340);
          connectToWiFi();   // connect to WiFi (if not already connected)
          connectToMQTTBroker();     // connect to MQTT broker (if not already connected)
          mqttClient.loop(); 
          mqttClient.publish(MQTT_TOPIC_WEB, json340, n6);
          mqttClient.publish(defeat_topic.c_str(), json300, n3);
          for (int a=0; a<3; a++){
              connectToWiFi();   // connect to WiFi (if not already connected)
              bot.begin();
              if (bot.message[a][1]!="" && bot.message[a][1]==tg_enemy){
                connectToWiFi();   // connect to WiFi (if not already connected)
                bot.begin();
                String welcome = "Il giocatore " + String(local_usr) + "ha esaurito l'ultimo tentativo";
                bot.sendMessage(bot.message[a][4], welcome, "");
              }
            }
        } else{
          const int capacity5 = JSON_OBJECT_SIZE(7);
          StaticJsonDocument<capacity5> JSONbuffer5;
          JSONbuffer5["room"] = topic_match;
          JSONbuffer5["user"] = local_usr;
          JSONbuffer5["comb"] = try_comb;
          JSONbuffer5["operation"] = String(stat);
          size_t n5 = serializeJson(JSONbuffer5, json340);
          connectToWiFi();   // connect to WiFi (if not already connected)
          connectToMQTTBroker();     // connect to MQTT broker (if not already connected)
          mqttClient.loop(); 
          mqttClient.publish(MQTT_TOPIC_WEB, json340, n5);
          for (int a=0; a<3; a++){
              connectToWiFi();   // connect to WiFi (if not already connected)
              bot.begin();
              if (bot.message[a][1]!="" && bot.message[a][1]==tg_enemy){
                connectToWiFi();   // connect to WiFi (if not already connected)
                bot.begin();
                String welcome = "Il giocatore " + String(local_usr) + "ha effettuato il suo " + String(n_try)+ " tentativo"; ;
                bot.sendMessage(bot.message[a][4], welcome, "");
              }
            }
        }
      tentativo="";
     }

/*============================LOOP - END GAME =========================*/
     
  } else if (new_match==false && time_temp!=-1){
    if (enemy_try!=-1){
      int vittoria=0;
      int pareggio=0;
      int sconfitta=0;
      Serial.println("tentativi avversario: " + String(enemy_try));
      Serial.println("tentativi miei: " + String(n_try));
      if (enemy_try > n_try){
        Serial.println("Complimenti hai vinto la partita!");
        winLCD();
        digitalWrite(GREEN_LED, LOW);
        delay(1000);
        digitalWrite(GREEN_LED, HIGH);
        vittoria=1;
      } else if (enemy_try == n_try){
        Serial.println("Tu e il tuo avversario siete entrambi molto bravi, la partita è finita in pareggio!");
        drawLCD();
        digitalWrite(YELLOW_LED, LOW);
        delay(1000);
        digitalWrite(YELLOW_LED, HIGH);
        pareggio=1;
      }else{
        Serial.println("Peccato! il tuo avversario è stato più bravo..");
        lose2LCD();
        digitalWrite(RED_LED, LOW);
        delay(1000);
        digitalWrite(RED_LED, HIGH);
        sconfitta=1;
      }
      delay(1000);
      Serial.println("La partita è finita, premi il bottone per iniziarne una nuova.");
      const int capacity11 = JSON_OBJECT_SIZE(6);
      StaticJsonDocument<capacity11> JSONbuffer11;
      JSONbuffer11["room"] = topic_match;
      JSONbuffer11["user"] = local_usr;
      JSONbuffer11["comb"] = 11;
      JSONbuffer11["operation"] = "finish";
      size_t n11 = serializeJson(JSONbuffer11, json340);
      connectToWiFi();   // connect to WiFi (if not already connected)
      connectToMQTTBroker();     // connect to MQTT broker (if not already connected)
      mqttClient.loop(); 
      mqttClient.publish(MQTT_TOPIC_WEB, json340, n11);
      endGameLCD();
      enemy_res= "";
      enemy_try=-1; 
      new_match=true;
      time_temp=0;
      combinazione="";
      tentativo="";
      play=false;
      tg_enemy="";
      for (int i=0; i< 4; i++){
        enemyComb[i]="";
      }
      UpdateDB(vittoria, pareggio, sconfitta);
      
           
    } else{
      if (millis()-time_temp>20000){
        Serial.println("Attendi che il tuo avversario finisca la partita...");
        waitingLCD();
        time_temp=millis();
        lastInteractionTime = millis();
      }
    }
}
}
}

/*=====================================================================*/






/*===========METHODS===================================================*/


/*===========MQTT===================================================*/

void mqttMessageReceived(String &topic, String &payload) {
  String user_topic= "db3ap/" + String(local_usr)+ "/username";

/*=======================MASTER========================================*/
  
  if (topic == MQTT_TOPIC_WELCOME) {
    // deserialize the JSON object
    Serial.println("Incoming MQTT message: " + topic + " - " + payload);
    StaticJsonDocument<300> doc;
    deserializeJson(doc, payload);
    const char *username = doc["username"];  
    const char *telegram = doc["telegram"]; 
    bool avaible=true;
    bool duplicate=false;
    for (int w=0; w<6; w++) {
      if (waitingRoom[w]==username){
        duplicate=true;
      }
    }
    for (int w=0; w<6; w++) {
      if (waitingRoom[w]=="" && avaible==true && duplicate==false){
        avaible=false;
        waitingRoom[w]=username;
        tgRoom[w]=telegram;
        n_player +=1;
      }
    }
    
    if(n_player>1 && avaible==false){
      count +=1;
      int n=0;
      String user1="";
      String user2="";
      String tg1="";
      String tg2="";
      for (int w=0; w<6; w++) {
        if (waitingRoom[w]!="" && n<2){
          if (n==0){
            user1=waitingRoom[w];
            tg1=tgRoom[w];
            n++;
            waitingRoom[w]="";
          }else{
            user2= waitingRoom[w];
            tg2=tgRoom[w];
            n++;
            waitingRoom[w]="";
          }
        }
      }
      const int capacity = JSON_OBJECT_SIZE(6);
      StaticJsonDocument<capacity> JSONbuffer;
      String room= "match" + String(count);
      Serial.println(room);
      JSONbuffer["room"]= room;
      JSONbuffer["username"]= user2;
      JSONbuffer["telegram"]= tg2;
      size_t nn =serializeJson(JSONbuffer, json340);
      String topic_start= "db3ap/" + String(user1);
      Serial.println(json340);
      mqttClient.publish(topic_start.c_str(), json340, nn);
      
      const int capacity2 = JSON_OBJECT_SIZE(6);
      StaticJsonDocument<capacity2> JSONbuffer2;
      String room2= "match" + String(count);
      Serial.println(room2);
      JSONbuffer2["room"]= room2;
      JSONbuffer2["username"]= user1;
      JSONbuffer2["telegram"]= tg1;
      size_t n2 =serializeJson(JSONbuffer2, json340);
      String topic_start2= "db3ap/" + String(user2);
      Serial.println(json340);
      mqttClient.publish(topic_start2.c_str(), json340, n2);
      n_player-=2;
    }else {
      Serial.println("Al momento ci sono solo " + String(n_player) + " giocatori in attesa");
    }

/*===========END MASTER================================================*/


/*===========TOPIC FOR THE PLAYERS=====================================*/
    
  } else if (topic == mqtt_user){
    Serial.println("Incoming MQTT message: " + topic + " - " + payload);
    StaticJsonDocument<300> doc;
    deserializeJson(doc, payload);
    const char *room = doc["room"];
    const char *username = doc["username"];
    const char *telegram = doc["telegram"];
    tg_enemy=telegram;
    enemy_usr=username;
    topic_match=room;
    play=true;
    my_comb=true;
    new_match=false;
    topic2="db3ap/" + String(topic_match)+ "/" + String(local_usr);
    topic3="db3ap/" + String(topic_match)+ "/" + String(local_usr) + "/endgame";
    Serial.println("Mi sto iscrivendo ai topic:");
    Serial.println(topic2);
    Serial.println(topic3);
    connectToWiFi();   // connect to WiFi (if not already connected)
    connectToMQTTBroker();     // connect to MQTT broker (if not already connected)
    mqttClient.loop(); 
    mqttClient.subscribe(topic2.c_str());
    mqttClient.subscribe(topic3.c_str());
  } else if (topic == topic2){
    Serial.println("Incoming MQTT message: " + topic);
    StaticJsonDocument<300> doc;
    deserializeJson(doc, payload);
    const char *comb = doc["comb"];
    String combo= comb;
    for (int i=0; i<4; i++){
      enemyComb[i]=combo[i];
    }
    
    guess=true;
  }else if (topic == topic3){
    Serial.println("Incoming MQTT message: " + topic + " - " + payload);
    StaticJsonDocument<300> doc;
    deserializeJson(doc, payload);
    const char *res = doc["status"];
    enemy_try = doc["n_try"];
    enemy_res= res;
  }else if(topic == user_topic){
    String topic_change_user= "db3ap/" + String(local_usr)+ "/username";
    String topic_change= "db3ap/" + String(local_usr);
    Serial.println("Incoming MQTT message: " + topic + " - " + payload);
    StaticJsonDocument<300> doc;
    deserializeJson(doc, payload);
    const char *res = doc["new"];
    local_usr=res;
    mqtt_user="db3ap/" + String(local_usr);
    mqtt_user_change="db3ap/" + String(local_usr) + "/username";
    mqttClient.subscribe(mqtt_user.c_str());
    mqttClient.subscribe(mqtt_user_change.c_str());
    mqttClient.unsubscribe(topic_change.c_str());
    mqttClient.unsubscribe(topic_change_user.c_str());
  }else {
    Serial.println(F("MQTT Topic not recognized, message skipped"));
  }
}

/*=================================SUBSCRIBES================================*/

void connectToMQTTBroker() {
  if (!mqttClient.connected()) {   // not connected
    Serial.print(F("\nConnecting to MQTT broker..."));
    while (!mqttClient.connect(MQTT_CLIENTID, MQTT_USERNAME, MQTT_PASSWORD)) {
      Serial.print(F("."));
      delay(1000);
    }
    Serial.println(F("\nConnected!"));

    // connected to broker, subscribe topics
    mqttClient.subscribe(MQTT_TOPIC_WELCOME);
    mqtt_user="db3ap/" + String(local_usr);
    mqtt_user_change="db3ap/" + String(local_usr) + "/username";
    mqttClient.subscribe(mqtt_user.c_str());
    mqttClient.subscribe(mqtt_user_change.c_str());
    Serial.println(F("\nSubscribed to control topic!"));
  }
}


/*==============================END MQTT===============================*/




/*==============================PSWD METHODS===========================*/

void eval_to_led_blink(int* eval){
  int green=0;
  int yellow=0;
  int red=0;
  for(int i=0; i<COMB_LEN; i++){
    if (eval[i] == 1){
      digitalWrite(GREEN_LED, LOW);
      delay(500);
      digitalWrite(GREEN_LED, HIGH);
      green++;
    }
    else if (eval[i] == 0){
      digitalWrite(YELLOW_LED, LOW);
      delay(500);
      digitalWrite(YELLOW_LED, HIGH);
      yellow++;
    }
    else if (eval[i] == -1){
      digitalWrite(RED_LED, LOW);
      delay(500);
      digitalWrite(RED_LED, HIGH);
      red++;
    }
    delay(500);
    stat= String(green) + String(yellow) + String(red);
  }
  
}

int* evaluate_comb(String enemyTryComb[]){
  static int evaluation[COMB_LEN];
  int used_enemy_comb[COMB_LEN];
  int used_myComb[COMB_LEN];
  //init
  for (int i=0; i<COMB_LEN; i++){
    evaluation[i] = -1;
    used_enemy_comb[i] = -1;
    used_myComb[i] = -1;
  }
  int pos = 0;
  // position and number ok
  for(int i=0; i<COMB_LEN; i++){
    if (enemyComb[i] == trycomb[i]){
      evaluation[pos] = 1;    
      used_enemy_comb[i] = 1;
      used_myComb[i] = 1;
      pos++;
    } 
  }

  for(int i=0; i<COMB_LEN; i++)
    for(int j=0; j<COMB_LEN; j++)
      if(enemyComb[i] == trycomb[j] && used_enemy_comb[j] != 1 && used_myComb[i] != 1){
        evaluation[pos] = 0;    
        used_enemy_comb[j] = 1;
        used_myComb[i] = 1;
        pos++;
      }
  return evaluation;
}

  void get_comb(String comb[]){
  int i=0;
  
  while(i<COMB_LEN){
    if (irrecv.decode(&results)) {   // if new value received, value is stored in 'results'
      Serial.print(toStringKey(results.value)+"\n");
      comb[i] = toStringKey(results.value);
      if(!(toStringKey(results.value) == "1" || toStringKey(results.value) == "2" || toStringKey(results.value) == "3" || toStringKey(results.value) == "4" || toStringKey(results.value) == "5" || toStringKey(results.value) == "6" || toStringKey(results.value) == "7" || toStringKey(results.value) == "8" || toStringKey(results.value) == "9" || toStringKey(results.value) == "0"))
        i--;
      irrecv.resume();   // resume ir reception
    i++;
    }
    
  }
  for(int i=0;i<COMB_LEN;i++){
    Serial.print(i);
    Serial.print(" Valore: ");
    Serial.print(comb[i]+"\n");
  }
}

String toStringKey(unsigned long value) {
  switch (value & 0xFFFFFF) {
    case 0xFFA25D:
      return "CH-";
    case 0xFF629D:
      return "CH";
    case 0xFFE21D:
      return "CH+";
    case 0xFF22DD:
      return "|<<";
    case 0xFF02FD:
      return ">>|";
    case 0xFFC23D:
      return ">||";
    case 0xFFE01F:
      return "-";
    case 0xFFA857:
      return "+";
    case 0xFF906F:
      return "EQ";
    case 0xFF6897:
      return "0";
    case 0xFF9867:
      return "100+";
    case 0xFFB04F:
      return "200+";
    case 0xFF30CF:
      return "1";
    case 0xFF18E7:
      return "2";
    case 0xFF7A85:
      return "3";
    case 0xFF10EF:
      return "4";
    case 0xFF38C7:
      return "5";
    case 0xFF5AA5:
      return "6";
    case 0xFF42BD:
      return "7";
    case 0xFF4AB5:
      return "8";
    case 0xFF52AD:
      return "9";
    case 0xFFFFFF:   // key hold
      return "HOLD";
    default:
      return "UNKNOWN";
  }
}


/*================================= WIFI METHODS ============================*/

void printWifiStatus() {

  // print the SSID of the network you're attached to
  Serial.print(F("SSID: "));
  Serial.println(WiFi.SSID());

  // print your board's IP address
  IPAddress ip = WiFi.localIP();
  Serial.print(F("IP Address: "));
  Serial.println(ip);

  // print the received signal strength
  long rssi = WiFi.RSSI();
  Serial.print(F("Signal strength (RSSI):"));
  Serial.print(rssi);
  Serial.println(F(" dBm"));
  Serial.println(WiFi.macAddress());
}

void connectToWiFi() {
  if (WiFi.status() != WL_CONNECTED) {   // not connected
    Serial.print(F("Attempting to connect to SSID: "));
    Serial.println(SECRET_SSID);

    while (WiFi.status() != WL_CONNECTED) {
#ifdef IP
      WiFi.config(ip, dns, gateway, subnet);
#endif
      WiFi.mode(WIFI_STA);
      WiFi.begin(SECRET_SSID, SECRET_PASS);   // Connect to WPA/WPA2 network. Change this line if using open or WEP network
      Serial.print(F("."));
      delay(5000);
    }
    Serial.println(F("\nConnected"));
    bot.begin();
    printWifiStatus();
  }
  
}


/*=============================== SENSOR METHODS ============================*/

boolean isButtonPressed() {
  static byte lastState = digitalRead(BUTTON);   // the previous reading from the input pin

  for (byte count = 0; count < BUTTON_DEBOUNCE_DELAY; count++) {
    if (digitalRead(BUTTON) == lastState) return false;
    delay(1);
  }

  lastState = !lastState;
  lastInteractionTime = millis();
  return lastState == HIGH ? false : true;
}


/*================================== DB METHODS =============================*/


int UpdateDB(int vittoria, int pareggio, int sconfitta) {
  int error;
  if (conn.connect(server_addr, 3306, mysql_user, mysql_password)) {
    Serial.println(F("MySQL connection established."));

    MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);

    sprintf(query2, READ_DATA);
    Serial.println(query2);
    // execute the query
    cur_mem->execute(query2);  
    column_names *cols = cur_mem->get_columns();
    for (int f = 0; f < cols->num_fields; f++) {
      Serial.print(cols->fields[f]->name);
      if (f < cols->num_fields-1) {
        Serial.print(", ");
      }
    }
    Serial.println();
    // Read the rows and print them
    row_values *row = NULL;
    bool valore=false;
    String riga;
    String riga1;
    String riga2;
    String riga3;
    String riga4;
    do {
      row = cur_mem->get_next_row();
      if (row != NULL && valore==false) {
          riga=row->values[0];
          riga1=row->values[1];
          riga2=row->values[2];
          riga3=row->values[3];
          riga4=row->values[4];
        }
      if (riga1 == local_usr && row != NULL)
        valore=true;
    } while (row != NULL && valore==false);

    int id=riga.toInt();
    int primo= riga2.toInt();
    int secondo= riga3.toInt();
    int terzo= riga4.toInt();
    primo+=vittoria;
    secondo+=pareggio;
    terzo+=sconfitta;

    if (valore == false){
      int n = local_usr.length();
      char field_1[n+1];
      strcpy(field_1, local_usr.c_str());
      sprintf(query1, INSERT_DATA, field_1, vittoria, pareggio, sconfitta);
      cur_mem->execute(query1);
    } else {
      Serial.println("l'utente è già nel db.");
      int n = local_usr.length();
      char field_1[n+1];
      strcpy(field_1, local_usr.c_str());
      sprintf(query3, UPDATE_DATA, field_1, primo, secondo, terzo, id);
      cur_mem->execute(query3);
    }
    
    // Deleting the cursor also frees up memory used
    delete cur_mem;

    conn.close();
  } else {
    Serial.println(F("MySQL connection failed."));
    error = -1;
  }

  return error;
}


int ReadDB(String user) {
  int error;
  if (conn.connect(server_addr, 3306, mysql_user, mysql_password)) {
    Serial.println(F("MySQL connection established."));

    MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);
    char READ_USER[]="SELECT * FROM `dbellini3`.`Mastermind_example` WHERE `User`='%s'";
    int n = user.length();
    char field_1[n+1];
    strcpy(field_1, user.c_str());
    sprintf(query2, READ_USER, field_1);
    Serial.println(query2);
    // execute the query
    cur_mem->execute(query2);
    column_names *cols = cur_mem->get_columns();
    for (int f = 0; f < cols->num_fields; f++) {
      Serial.print(cols->fields[f]->name);
      if (f < cols->num_fields-1) {
        Serial.print(", ");
      }
    }
    Serial.println();
    // Read the rows and print them
    row_values *row = NULL;
    bool valore=false;
    String riga;
    String riga1;
    String riga2;
    String riga3;
    String riga4;
    Serial.println(user);
    do {
      row = cur_mem->get_next_row();
      if (row != NULL && valore==false) {
          riga=row->values[0];
          riga1=row->values[1];
          riga2=row->values[2];
          riga3=row->values[3];
          riga4=row->values[4];
        }
      if (riga1 == user && row != NULL)
        valore=true;
    } while (row != NULL && valore==false);
    
    if (valore == false){
      Serial.println("L'utente non è presente nel db.");
      usrTg[0]= "";
      usrTg[1]= "";
      usrTg[2]= ""; 
    } else {
      Serial.println("l'utente è già nel db.");
      usrTg[0]= riga2.toInt();
      usrTg[1]= riga3.toInt();
      usrTg[2]= riga4.toInt(); 
    }
    delete cur_mem;

    conn.close();
  } else {
    Serial.println(F("MySQL connection failed."));
    error = -1;
  }

  return error;
}


/*============================API METHODS==============================*/


void botExecMessages() {
  for (int i = 1; i < bot.message[0][0].toInt() + 1; i++) {
    String messageRcvd = bot.message[i][5];
    String usrRcvd = bot.message[i][4];
    Serial.println(messageRcvd);
    String prefix= String(messageRcvd[0]) + String(messageRcvd[1]);
    String us="";
    String prefix2= String(messageRcvd[0]) + String(messageRcvd[1]) + String(messageRcvd[2]) + String(messageRcvd[3]);
    String sub="";
    if (messageRcvd.equals("/subscribe")) {
      bot.sendMessage(bot.message[i][4], "Sottoscrizione avvenuta con successo!", "");
    }else if(prefix2=="/sub"){
      for (int i=4; i<messageRcvd.length(); i++){
        sub+=messageRcvd[i];
      }
      if (sub==local_usr){
        tg_user=bot.message[i][4];
        String iscrizione= "L'utente " + String(local_usr) + " si è iscritto con successo!";
        bot.sendMessage(bot.message[i][4], iscrizione, "");
        Serial.println(tg_user);
      }
    }else if (prefix== "/U") {
      for (int i=2; i<messageRcvd.length(); i++){
        us+=messageRcvd[i];
      }
      ReadDB(us);
      if(usrTg[0]==""){
        String msg= "L'utente " + String(us) + " non risulta iscritto";
        bot.sendMessage(bot.message[i][4], msg, "");
      }else{
        String msg= "Statistiche dell'utente: " + String(us);
        String msg1= "N° vittorie totali: " + String(usrTg[0]);
        String msg2= "N° pareggi totali: " + String(usrTg[1]);
        String msg3= "N° sconfitte totali: " + String(usrTg[2]);
        bot.sendMessage(bot.message[i][4], msg, "");
        bot.sendMessage(bot.message[i][4], msg1, "");
        bot.sendMessage(bot.message[i][4], msg2, "");
        bot.sendMessage(bot.message[i][4], msg3, "");
      }
      
    }
    else if (messageRcvd.equals("/help")) {
      String welcome = "Welcome from MastermindBot!";
      String welcomeCmd1 = "/subscribe : to reiceved msg from mastermind-bot";
      String welcomeCmd2 = "/sub<username> : to reiceved msg from a specific user";
      String welcomeCmd3 = "/U<username> : to retrieve info about <username>";
      bot.sendMessage(bot.message[i][4], welcome, "");
      bot.sendMessage(bot.message[i][4], welcomeCmd1, "");
      bot.sendMessage(bot.message[i][4], welcomeCmd2, "");
      bot.sendMessage(bot.message[i][4], welcomeCmd3, "");
    }else {
      bot.sendMessage(bot.message[i][4], F("Unknown command! Use /help to see all the available commands"), "");
    }
  }
  bot.message[0][0] = "";   // all messages have been replied, reset new messages
  yield();
}


void printCurrentWeather() {
  // Current weather api documentation at: https://openweathermap.org/current
  Serial.println(F("\n=== Current weather ==="));

  // call API for current weather
  if (client.connect(weather_server, 80)) {
    char request[100];
    sprintf(request, weather_query, WEATHER_CITY, WEATHER_COUNTRY, WEATHER_API_KEY);
    client.println(request);
    client.println(F("Host: api.openweathermap.org"));
    client.println(F("User-Agent: ArduinoWiFi/1.1"));
    client.println(F("Connection: close"));
    client.println();
  } else {
    Serial.println(F("Connection to api.openweathermap.org failed!\n"));
  }

  while (client.connected() && !client.available()) delay(1);   // wait for data
  String result;
  while (client.connected() || client.available()) {   // read data
    char c = client.read();
    result = result + c;
  }

  client.stop();   // end communication
  //Serial.println(result);  // print JSON

  char jsonArray[result.length() + 1];
  result.toCharArray(jsonArray, sizeof(jsonArray));
  jsonArray[result.length() + 1] = '\0';
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, jsonArray);

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return;
  }
  Serial.print(F("Location: "));
  Serial.println(doc["name"].as<String>());
  Serial.print(F("Country: "));
  Serial.println(doc["sys"]["country"].as<String>());
  Serial.print(F("Temperature (°C): "));
  Serial.println((float)doc["main"]["temp"]);
  Serial.print(F("Weather: "));
  Serial.println(doc["weather"][0]["main"].as<String>());
  Serial.print(F("Weather description: "));
  Serial.println(doc["weather"][0]["description"].as<String>());

  if ((float)doc["main"]["temp"]>15 &&(float)doc["main"]["temp"]<30 && (doc["weather"][0]["main"].as<String>()!="Clear") || (doc["weather"][0]["description"].as<String>()== "few clouds")){
    weather=true;
  }else{
    weather=false;
  }
}

void writeEEPROM(int addr, long rssi) {

  // write to EEPROM.
  EEPROM.write(addr, rssi);

  EEPROM.commit();    //Store data to EEPROM
}

long readEEPROM(int addr) {

  long rssi = EEPROM.read(addr);

  return rssi;
}


template <class T> int EEPROM_writeAnything(int ee, const T& value)
{

  const byte* p = (const byte*)(const void*)&value;
  unsigned int i;
  for (i = 0; i < sizeof(value); i++)
    EEPROM.write(ee++, *p++); //does not write to flash immediately, instead you must call EEPROM.commit()

  EEPROM.commit();

  return i;
}

template <class T> int EEPROM_readAnything(int ee, T& value)
{

  byte* p = (byte*)(void*)&value;
  unsigned int i;

  for (i = 0; i < sizeof(value); i++)
    *p++ = EEPROM.read(ee++);


  return i;
}


/*================================== LCD METHODS=======================*/


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
  lcd.setBacklight(255);    // set backlight to maximum
  lcd.home();               // move cursor to 0,0
  lcd.clear();              // clear text
  lcd.print("Press red button");   // show text
  lcd.setCursor(0, 1);
  lcd.print("to play");
  delay(1000);

  // blink backlight
  lcd.setBacklight(0);   // backlight off
  delay(400);
  lcd.setBacklight(255);
}

void conditionLCD(){
  Serial.println("Allora vai a giocare al parco");
  lcd.home();
  lcd.clear();
  lcd.print("Perfect weather");
  lcd.setCursor(0, 1);
  lcd.print("Go out there!!!");
}

void chooseLCD(){
  lcd.home();               // move cursor to 0,0
  lcd.clear();              // clear text
  lcd.print("Insert your comb");   // show text
  Serial.println("Inserisci la tua combinazione");
}

void attemptLCD(){
  lcd.home();               // move cursor to 0,0
  lcd.clear();              // clear text
  lcd.print(String(n_try+1) + " attempt:");   // show text
}

void foundLCD(){
  lcd.home();               // move cursor to 0,0
  lcd.clear();              // clear text
  lcd.print("Finished in");   // show text
  lcd.setCursor(0, 1);
  lcd.print(String(n_try) + " tries!"); 
}

void loseLCD(){
  lcd.home();               // move cursor to 0,0
  lcd.clear();              // clear text
  lcd.print("No more tries");   // show text
  lcd.setCursor(0, 1);
  lcd.print("No win");
}

void winLCD(){
  lcd.home();               // move cursor to 0,0
  lcd.clear();              // clear text
  lcd.print("Congrats, you");   // show text
  lcd.setCursor(0, 1);
  lcd.print("won the game");
}

void drawLCD(){
  lcd.home();               // move cursor to 0,0
  lcd.clear();              // clear text
  lcd.print("The game ends");   // show text
  lcd.setCursor(0, 1);
  lcd.print("in a draw");
}

void lose2LCD(){
  lcd.home();               // move cursor to 0,0
  lcd.clear();              // clear text
  lcd.print("Unfortunately ");   // show text
  lcd.setCursor(0, 1);
  lcd.print("you lost the game");  
}

void endGameLCD(){
  lcd.home();               // move cursor to 0,0
  lcd.clear();              // clear text
  lcd.print("Match finished");   // show text
  lcd.setCursor(0, 1);
  lcd.print("press the button");
}

void waitingLCD(){
  lcd.home();               // move cursor to 0,0
  lcd.clear();              // clear text
  lcd.print("Waiting for ");   // show text
  lcd.setCursor(0, 1);
  lcd.print("your opponent");
}

void afkLCD(){
  lcd.setBacklight(255);
  lcd.home();               // move cursor to 0,0
  lcd.clear();              // clear text
  lcd.print("Inactivity. Game");   // show text
  lcd.setCursor(0, 1);
  lcd.print("lost, -2 tries");
  delay(1500);
}
