// Use this file to store all of the private credentials and connection details

#define SECRET_SSID ""                  // SSID
#define SECRET_PASS ""            // WiFi password

// MQTT access
#define MQTT_BROKERIP "149.132.178.180"           // IP address of the machine running the MQTT broker
#define MQTT_CLIENTID "d.bellini"                 // client identifier
#define MQTT_USERNAME "dbellini3"            // mqtt user's name
#define MQTT_PASSWORD "iot816602" 

// MySQL access
#define MYSQL_IP {149, 132, 178, 180}              // IP address of the machine running MySQL
#define MYSQL_USER "dbellini3"                  // db user
#define MYSQL_PASS "iot816602"              // db user's password

// Telegram bot configuration (data obtained form @BotFather, see https://core.telegram.org/bots#6-botfather)
#define BOT_NAME "db3ap_MASTERMIND" // name displayed in contact list
#define BOT_USERNAME "db3ap_MASTERMIND_bot" // short bot id
#define BOT_TOKEN "1867231080:AAHIE6UBohm5-nsVTMWkJsY9vlOcNJrbQOY" // authorization token

// openweathermap.org configuration
#define WEATHER_API_KEY "ae4e48d789fc8783ecf0140b50e75b78"           // api key form https://home.openweathermap.org/api_keys
#define WEATHER_CITY "Milan"                     // city
#define WEATHER_COUNTRY "it"                     // ISO3166 country code 


// ONLY if static configuration is needed
/*
#define IP {192, 168, 1, 100}                    // IP address
#define SUBNET {255, 255, 255, 0}                // Subnet mask
#define DNS {149, 132, 2, 3}                     // DNS
#define GATEWAY {149, 132, 182, 1}               // Gateway
*/
