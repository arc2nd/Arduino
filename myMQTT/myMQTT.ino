// myMQTT
// James Parks
// I've cobbled this together from multiple examples
// to connect an Arduino MKR1000 (a friend gave me 
// four of them, as he was going out of country) to my 
// wifi network, talk to a DHT sensor, send that data 
// to an MQTT server and receive messages to turn pins 
// on and off

// TODO: web interface to allow you to specify the 
//    SSID/password and the MQTT broker info
// TODO: write SSID/password and MQTT broker info to 
//    EEPROM

// begin wifi includes
#include <SPI.h>
#include <WiFi101.h>
#include <Dns.h>
#include <Dhcp.h>

// begin MQTT includes
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

// begin sensor includes and declaration
#include <SimpleDHT.h>
#define pinDHT11 2
SimpleDHT11 dht11(pinDHT11);

// begin neopixel includes and declarations
#include <Adafruit_NeoPixel.h>
#define ledPin 9
#define ledCount 3
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(ledCount, ledPin, NEO_GRB + NEO_KHZ800);
// Argument 1 = Number of pixels in NeoPixel strip
// Argument 2 = Arduino pin number (most are valid)
// Argument 3 = Pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
//   NEO_RGBW    Pixels are wired for RGBW bitstream (NeoPixel RGBW products)

// activity booleans
bool temp_active = false;
bool mqtt_active = true;
bool lights_active = true;

// arduino_secrets.h is where we're storing our login info
#include "arduino_secrets.h" 
char ssid[] = SECRET_SSID;        // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)
int status = WL_IDLE_STATUS;     // the WiFi radio's status

// global variables
uint32_t prevTime; //a variable to hold a time in milliseconds
uint32_t light_state = 0; //a variable to hold the state of the blinky light we're switching on/off


// neopixel variables
int i = 0;
int p;
int light_mode = 2;
int max_r = 255;
int max_g = 128;
int max_b = 128;
int light_step = 1;
int light_delay = 90;
int tar[ledCount][3]; // target colors
int cur[ledCount][3]; // current colors


/************************* Ethernet Client Setup *****************************/
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};

//Uncomment the following, and set to a valid ip if you don't have dhcp available.
//IPAddress iotIP (192, 168, 0, 42);
//Uncomment the following, and set to your preference if you don't have automatic dns.
//IPAddress dnsIP (8, 8, 8, 8);
//If you uncommented either of the above lines, make sure to change "Ethernet.begin(mac)" to "Ethernet.begin(mac, iotIP)" or "Ethernet.begin(mac, iotIP, dnsIP)"


/************ Global State (you don't need to change this!) ******************/

// Set up the clients
WiFiClient wClient;
Adafruit_MQTT_Client mqtt(&wClient, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

// You don't need to change anything below this line!
#define halt(s) { Serial.println(F( s )); while(1);  }

/****************************** Feeds ***************************************/
// Create the feeds that we will publish/subscribe to
// I'm using io.adafruit.com here, but it has been tested with a local Mosquitto broker
// Notice MQTT paths for AIO follow the form: <username>/feeds/<feedname>


// Subscribes
Adafruit_MQTT_Subscribe onoffbutton = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/onoff_test");
Adafruit_MQTT_Subscribe modeslider = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/mode");

// Publishes
Adafruit_MQTT_Publish onoffstatus = Adafruit_MQTT_Publish(&mqtt,  AIO_USERNAME "/feeds/onoff_status");
Adafruit_MQTT_Publish tempFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/temperature");
Adafruit_MQTT_Publish humFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/humidity");



// things to do when turning the light on
void turn_on() {
  digitalWrite(LED_BUILTIN, HIGH);
  light_state = 1;
  onoffstatus.publish(light_state);
}

// things to do when turning the light off
void turn_off() {
  digitalWrite(LED_BUILTIN, LOW);
  light_state = 0;
  onoffstatus.publish(light_state);
}

// if on, turn off. if off, turn on
void toggle() {
  if (light_state) {
    turn_off();
  }
  else {
    turn_on();
  }
}

// print your WiFi shield's IP address:
void printWiFiData() {
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print your MAC address:
  byte mac[6];
  WiFi.macAddress(mac);
  Serial.print("MAC address: ");
  printMacAddress(mac);
}

// print data about your connected network
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

// print the board's MAC address
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
  //when connection is first made, blink the LED a bunch of times
  for (int i=0; i<5; i++) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(125);
    digitalWrite(LED_BUILTIN, LOW);
    delay(125);
  }
}

// read the DHT sensor, print it to Serial, publish it
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
  Serial.print((float)temperature); Serial.print(" *C, "); 
  Serial.print((float)humidity); Serial.println(" H");
  float tempPub = ((float)temperature * (9.0/5.0)) + 32.0;
  Serial.println(tempPub);
  tempFeed.publish(tempPub);
  uint32_t humPub = (uint32_t)humidity;
  humFeed.publish(humPub);
}

void rand_color(int (&color)[3]) {
  int remainder_one = 255 - (int)color[0];
  int this_green = random(remainder_one);
  int this_blue = remainder_one - this_green;
  color[1] = this_green;
  color[2] = this_blue;
}

void move_closer(int p, int (&cur)[ledCount][3], int (&tar)[ledCount][3], int light_step) {
  // move this pixel's red closer to target
  if (cur[p][0] > tar[p][0]) {
    cur[p][0]--;
  }
  else if (cur[p][0] < tar[p][0]) {
    cur[p][0] = cur[p][0] + light_step;
  }
  else {
    tar[p][0] = random(max_r);
  }

  // move this pixel's green closer to target
  if (cur[p][1] > tar[p][1]) {
    cur[p][1]--;
  }
  else if (cur[p][1] < tar[p][1]) {
    cur[p][1] = cur[p][1] + light_step;
  }
  else {
    tar[p][1] = random(max_g);
  }

  // move this pixel's blue closer to target
  if (cur[p][2] > tar[p][2]) {
    cur[p][2]--;
  }
  else if (cur[p][2] < tar[p][2]) {
    cur[p][2] = cur[p][2] + light_step;
  }
  else {
    tar[p][2] = random(max_b);
  }
}

void random_color_indiv(int (&current)[ledCount][3], int (&target)[ledCount][3], int light_step) {
  for (p=0; p<ledCount; p++) {
    pixels.setPixelColor(p, current[p][0], current[p][1], current[p][2]);
    move_closer(p, current, target, light_step);
  }
}


void random_color_all(int (&current)[ledCount][3], int (&target)[ledCount][3], int light_step) {
  for (p=0; p<ledCount; p++) {
    pixels.setPixelColor(p, current[0][0], current[0][1], current[0][2]);
  }
  move_closer(0, current, target, light_step);
}


void cylon_chase(int i) {
  for (p=0; p<ledCount; p++) 
  {
    if (p == i) {
      pixels.setPixelColor(p, max_r, 0, 0);
    }
    else {
      pixels.setPixelColor(p, 0, 0, 0);
    }
  }
}


void candle() {
  //  Regular (orange) flame:
    int r = 226, g = 121, b = 35;
  //  Purple flame:
  //  int r = 158, g = 8, b = 148;
  //  Green flame:
  //  int r = 74, g = 150, b = 12;

  //  Flicker, based on our initial RGB values
  for(int i=0; i<pixels.numPixels(); i++) {
    int flicker = random(0,100);
    int r1 = r-flicker;
    int g1 = g-flicker;
    int b1 = b-flicker;
    if(g1<0) g1=0;
    if(r1<0) r1=0;
    if(b1<0) b1=0;
    pixels.setPixelColor(i,r1,g1, b1);
  }
  pixels.show();

  //  Adjust the delay here, if you'd like.  Right now, it randomizes the 
  //  color switch delay to give a sense of realism
  // delay(random(10,113));
  delay(random(1, 15));
}

void setup() {
  // Initialize serial and wait for port to open:
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(ledPin, OUTPUT);
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
    status = WiFi.begin(ssid, pass); //if no password needed, get rid of pass, don't leave it empty

    // wait 10 seconds for connection:
    delay(10000);
  }
  
  // you're connected now, so print out the data:
  Serial.print("You're connected to the network");
  printCurrentNet();
  printWiFiData();

  // MQTT section
  if (mqtt_active) {
    Serial.println("MQTT active");
    Serial.print(F("\nInit the Client as ..."));
    Serial.println(AIO_USERNAME);
    //Ethernet.begin(mac);
    //delay(1000); //give the ethernet a second to initialize
    mqtt.subscribe(&onoffbutton);
    mqtt.subscribe(&modeslider);
  }

  if (lights_active) {
    int tmp_col[3] = {random(max_r), random(max_g), random(max_b)};
    // load up the target and current colors array
    for (p=0; p<ledCount; p++) {
      rand_color(tmp_col);
      tar[p][0] = tmp_col[0];
      tar[p][1] = tmp_col[1];
      tar[p][2] = tmp_col[2];
      //tar[p][0] = random(max_r);
      //tar[p][1] = random(max_g);
      //tar[p][2] = random(max_b);
    }
    tmp_col[0] = random(max_r);
    tmp_col[1] = 0;
    tmp_col[2] = 0;
    for (p=0; p<ledCount; p++) {
      rand_color(tmp_col);
      cur[p][0] = tmp_col[0];
      cur[p][1] = tmp_col[1];
      cur[p][2] = tmp_col[2];
      //cur[p][0] = random(max_r);
      //cur[p][1] = random(max_g);
      //cur[p][2] = random(max_b);
    }

    pixels.begin();
    pixels.show();
    Serial.println("lights active...");
    delay(1000);
    pixels.setBrightness(255); // brightness 0-255
    pixels.show();
  }

  prevTime = millis();
}


void loop() {
  uint32_t t = 0;
  
  // Do the temperature reading
  t = millis();
  if (temp_active) {
    if ((t - prevTime) > 600000) { // Every 10 minutes
      read_temp();
      prevTime = t;
    }
  }
  
  // this is our 'wait for incoming subscription packets' busy subloop
  if (mqtt_active) {
    MQTT_connect();
    Adafruit_MQTT_Subscribe *subscription;
    while ((subscription = mqtt.readSubscription(125))) {
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
        //Serial.println(light_state);
        onoffstatus.publish(light_state);
      }
      if (subscription == &modeslider) {
        Serial.println((char *)modeslider.lastread);
        if (!strcmp((char *)modeslider.lastread, "0")) {
          light_mode = 0;
        }
        if (!strcmp((char *)modeslider.lastread, "1")) {
          light_mode = 1;
        }
        if (!strcmp((char *)modeslider.lastread, "2")) {
          light_mode = 2;
        }
        if (!strcmp((char *)modeslider.lastread, "3")) {
          light_mode = 3;
        }
        if (!strcmp((char *)modeslider.lastread, "4")) {
          light_mode=4;
        }
        Serial.print("Current mode: ");
        Serial.println(light_mode);
      }
    }
  }

  if (lights_active) {
    int p; // current pixel
    delay(light_delay);


    if (light_mode == 0) { // off
      for (p=0; p<ledCount; p++) {
        pixels.setPixelColor(p, 0, 0, 0);
      }
    }
    if (light_mode == 1) { // each pixel random colors
      light_delay = 0;
      random_color_indiv(cur, tar, light_step);
    }
    if (light_mode == 2) { // all pixels same random colors
      light_delay = 0;
      random_color_all(cur, tar, light_step);
    }
    if (light_mode == 3) { // cylon chase
      light_delay = 125;
      cylon_chase(i);
    }
    if (light_mode == 4) { // candle
      light_delay=0;
      candle();
    }
  }

  /*for (p=0; p<ledCount; p++) {
    pixels.setPixelColor(p, 0, 0, 0);
    //pixels.show();
  }
  pixels.setPixelColor(i, random(255), random(255), random(255));
  */
  pixels.show();

  if (i>=(ledCount-1)) {
    i = 0;
    //Serial.println("resetting i");
  }
  else {
    i++;
  }
}
