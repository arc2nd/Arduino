//begin wifi includes
#include <SPI.h>
#include <WiFi101.h>

//begin MQTT includes
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

#include <Dns.h>
#include <Dhcp.h>

#include <SimpleDHT.h>
int pinDHT11 = 2;
SimpleDHT11 dht11(pinDHT11);

#include "arduino_secrets.h" 
///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = SECRET_SSID;        // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)
int status = WL_IDLE_STATUS;     // the WiFi radio's status

/************************* Ethernet Client Setup *****************************/
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};

//Uncomment the following, and set to a valid ip if you don't have dhcp available.
//IPAddress iotIP (192, 168, 0, 42);
//Uncomment the following, and set to your preference if you don't have automatic dns.
//IPAddress dnsIP (8, 8, 8, 8);
//If you uncommented either of the above lines, make sure to change "Ethernet.begin(mac)" to "Ethernet.begin(mac, iotIP)" or "Ethernet.begin(mac, iotIP, dnsIP)"

/************ Global State (you don't need to change this!) ******************/

//Set up the ethernet client
//EthernetClient etherClient;
WiFiClient wClient;
Adafruit_MQTT_Client mqtt(&wClient, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

// You don't need to change anything below this line!
#define halt(s) { Serial.println(F( s )); while(1);  }

/****************************** Feeds ***************************************/

// Setup a feed called 'photocell' for publishing.
// Notice MQTT paths for AIO follow the form: <username>/feeds/<feedname>
Adafruit_MQTT_Publish onoffstatus = Adafruit_MQTT_Publish(&mqtt,  AIO_USERNAME "/feeds/onoff_status");
// Setup a feed called 'onoff' for subscribing to changes.
Adafruit_MQTT_Subscribe onoffbutton = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/onoff_test");

Adafruit_MQTT_Publish tempFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/temperature");
Adafruit_MQTT_Publish humFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/humidity");
uint32_t prevTime;

void setup() {
  //Initialize serial and wait for port to open:
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(9600);
  /*while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }*/

  // check for the presence of the shield:
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi shield not present");
    // don't continue:
    while (true);
  }

  // attempt to connect to WiFi network:
  while ( status != WL_CONNECTED) {
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network:
    status = WiFi.begin(ssid, pass);

    // wait 10 seconds for connection:
    delay(10000);
  }
  
  // you're connected now, so print out the data:
  Serial.print("You're connected to the network");
  printCurrentNet();
  printWiFiData();

  //MQTT section
  Serial.print(F("\nInit the Client as ..."));
  Serial.println(AIO_USERNAME);
  //Ethernet.begin(mac);
  //delay(1000); //give the ethernet a second to initialize
  
  mqtt.subscribe(&onoffbutton);
  prevTime = millis();
}

uint32_t light_state = 0;

void loop() {
  // check the network connection once every 10 seconds:
  //delay(10000);
  //printCurrentNet();
  uint32_t t = 0;
  MQTT_connect();
  
  // Do the temperature reading
  t = millis();
  if ((t - prevTime) > 60000) { // Every 1 minutes
    read_temp();
    prevTime = t;
  }

  // this is our 'wait for incoming subscription packets' busy subloop
  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(1000))) {
    if (subscription == &onoffbutton) {
      Serial.print(F("Got: "));
      Serial.println((char *)onoffbutton.lastread);
      if (strcmp((char *)onoffbutton.lastread, "ON") == 0) {
        turn_on();
      }
      if (strcmp((char *)onoffbutton.lastread, "OFF") == 0) {
        turn_off();
      }
      if (strcmp((char *)onoffbutton.lastread, "TGL") == 0) {
        toggle();
      }
      Serial.println(light_state);
      onoffstatus.publish(light_state);
    }
  }
}

void turn_on() {
  digitalWrite(LED_BUILTIN, HIGH);
  light_state = 1;
  onoffstatus.publish(light_state);
}

void turn_off() {
  digitalWrite(LED_BUILTIN, LOW);
  light_state = 0;
  onoffstatus.publish(light_state);
}

void toggle() {
  if (light_state) {
    turn_off();
  }
  else {
    turn_on();
  }
}

void printWiFiData() {
  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print your MAC address:
  byte mac[6];
  WiFi.macAddress(mac);
  Serial.print("MAC address: ");
  printMacAddress(mac);
}

void printCurrentNet() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print the MAC address of the router you're attached to:
  byte bssid[6];
  WiFi.BSSID(bssid);
  Serial.print("BSSID: ");
  printMacAddress(bssid);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.println(rssi);

  // print the encryption type:
  byte encryption = WiFi.encryptionType();
  Serial.print("Encryption Type:");
  Serial.println(encryption, HEX);
  Serial.println();
}

void printMacAddress(byte mac[]) {
  for (int i = 5; i >= 0; i--) {
    if (mac[i] < 16) {
      Serial.print("0");
    }
    Serial.print(mac[i], HEX);
    if (i > 0) {
      Serial.print(":");
    }
  }
  Serial.println();
}

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care of connecting.
void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println("Retrying MQTT connection in 5 seconds...");
       mqtt.disconnect();
       delay(5000);  // wait 5 seconds
  }
  Serial.println("MQTT Connected!");
  for (int i=0; i<5; i++) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(125);
    digitalWrite(LED_BUILTIN, LOW);
    delay(125);
  }
}

void read_temp() {
  // read without samples.
  byte temperature = 0;
  byte humidity = 0;
  int err = SimpleDHTErrSuccess;
  if ((err = dht11.read(&temperature, &humidity, NULL)) != SimpleDHTErrSuccess) {
    Serial.print("Read DHT11 failed, err="); Serial.println(err);delay(1000);
    return;
  }
  
  Serial.print("Sample OK: ");
  Serial.print((int)temperature); Serial.print(" *C, "); 
  Serial.print((int)humidity); Serial.println(" H");
  uint32_t tempPub = (uint32_t)temperature;
  tempFeed.publish(tempPub);
  uint32_t humPub = (uint32_t)humidity;
  humFeed.publish(humPub);
}
