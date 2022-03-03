// Use this file to store all of the private credentials and connection details

#define SECRET_SSID "TIM-19330313"                  // SSID
#define SECRET_PASS "phakvkdfsxuxxzu"            // WiFi password

// MQTT access
#define MQTT_BROKERIP "149.132.178.180"           // IP address of the machine running the MQTT broker
#define MQTT_CLIENTID "d.bellini"                 // client identifier
#define MQTT_USERNAME "dbellini3"            // mqtt user's name
#define MQTT_PASSWORD "iot816602" 

// InfluxDB cfg
#define INFLUXDB_URL "http://149.132.178.180:8086"   // IP and port of the InfluxDB server
#define INFLUXDB_TOKEN "o06OTBn2M6UrcIpXj9jfLO_4MOi_1EaKB3Z74V75fzgXp6IinRLwnOTJRawlAlCHecIJxhzdNCRgl04t88ZojA=="   // API authentication token (Use: InfluxDB UI -> Load Data -> Tokens -> <select token>)
#define INFLUXDB_ORG "labiot-org"                 // organization id (Use: InfluxDB UI -> Settings -> Profile -> <name under tile> )
#define INFLUXDB_BUCKET "dbellini3-bucket"                 // bucket name (Use: InfluxDB UI -> Load Data -> Buckets)

// MySQL access
#define MYSQL_IP {149, 132, 178, 180}              // IP address of the machine running MySQL
#define MYSQL_USER "dbellini3"                  // db user
#define MYSQL_PASS "iot816602"              // db user's password

// ONLY if static configuration is needed
/*
#define IP {192, 168, 1, 100}                    // IP address
#define SUBNET {255, 255, 255, 0}                // Subnet mask
#define DNS {149, 132, 2, 3}                     // DNS
#define GATEWAY {149, 132, 182, 1}               // Gateway
*/
