// Use this file to store all of the private credentials and connection details

// WiFi configuration
#define SECRET_SSID "WiFi_SSID"                  // SSID
#define SECRET_PASS "WiFi_Password"              // WiFi password

// ONLY if static configuration is needed
/*
#define IP {192, 168, 1, 100}                    // IP address
#define SUBNET {255, 255, 255, 0}                // Subnet mask
#define DNS {149, 132, 2, 3}                     // DNS
#define GATEWAY {149, 132, 182, 1}               // Gateway
*/

// InfluxDB cfg
#define INFLUXDB_URL "http://192.168.1.66:8086"   // IP and port of the InfluxDB server
#define INFLUXDB_TOKEN "long-api-auth-token..."   // API authentication token (Use: InfluxDB UI -> Load Data -> Tokens -> <select token>)
#define INFLUXDB_ORG "labiot2021"                 // organization id (Use: InfluxDB UI -> Settings -> Profile -> <name under tile> )
#define INFLUXDB_BUCKET "esp8266"                 // bucket name (Use: InfluxDB UI -> Load Data -> Buckets) 
