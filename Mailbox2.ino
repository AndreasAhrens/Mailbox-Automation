

#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <PubSubClient.h>


// Create an ESP8266 WiFiClient class to connect to the MQTT server.
const char* mqtt_server = "192.168.31.68";
WiFiClient client;
PubSubClient mqttclient(client);
long lastMsg = 0;
char msg[50];
int value = 0;
// Set hatchLast, indicating that the last state the hatch was in, was closed in this case.
// This will be used to count the number of times the hatch has been opened since the door was last
// opened. Thus giving the total amount of mail deposited since last emptying. If > 0, needs emptying.
// hatchLast and doorLast are also used in order to not spam the MQTT server.
int hatchLast = 0;
int doorLast = 0;

// The number of deposits since last time the door was opened.
int deposits = 0;
String deposits_str;
char deposits_char[50];

// Pin for the outside light
int outside = D2;
// Pin for light inside mailbox
int inside = D1;
// The pin for magnet switch for hatch
const int hatchPin = D3;
// The pin for magnet switch for door
const int doorPin = D4;
// Assumed the D5 pin to be the temperature sensor (input pin)
const int tempSensorInPin = D5;

unsigned long previousMillis = 0;
unsigned long previousMillisDoor = 0;

const long interval = 100;        
const long closedInterval = 100; 

void setup() {
  pinMode(outside, OUTPUT);            // Initialize the outside pin as an output
  pinMode(inside, OUTPUT);             // Initialize the inside pin as input
  pinMode(hatchPin, INPUT);            // Initialize the hatchpin as input
  pinMode(doorPin, INPUT);             // Oh come on, you guessed it by now
  pinMode(tempSensorInPin, INPUT);     // Oh yes I did! Initialize the temperature sensor as input
  digitalWrite(hatchPin, HIGH);        // Set initial status for hatch as high
  digitalWrite(doorPin, HIGH);         // Set initial status for door pin as high
  digitalWrite(tempSensorInPin, HIGH); // Set initial status for temperature sensor as high
  Serial.begin(115200);
  WiFiManager wifiManager;                    // This sets up WiFiManager, connecting to last network
  wifiManager.autoConnect("AutoConnectAP");   // And if that doesn't work, set up AutoConnectAP for config
  mqttclient.setServer(mqtt_server, 1883);    // Connect to MQTT Server
  mqttclient.setCallback(callback);           // And set the callback
  
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic); // For troubleshooting
  Serial.print("] "); 
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  /*
   * I use Mosquitto as MQTT server on the same Raspberry Pi as my Home Assistant server.
   * /outside/mailbox/outside/ is used for status for outside light and /outside/mailbox/outside/set/
   * is used for setting the actual light to on or off. I have configured the On and Off commands
   * to be 1 and 0 respectively. By reporting back the status, I can make sure its actually done.
   */
  // If payload is 1 and topic is Outside (I could go through the whole thing, but this is good enough for me)
  if ((char)payload[0] == '1' && topic[17] == 'o') { 
    digitalWrite(outside, HIGH);                            // Turn on the light for the outside light
    mqttclient.publish("/outside/mailbox/outside/", "ON");  // Notify MQTT that the outside mailbox is on
  } else if ((char)payload[0] == '0' && topic[17] == 'o') { 
    digitalWrite(outside, LOW);                             // Turn it off if 0
    mqttclient.publish("/outside/mailbox/outside/", "OFF"); // And then notify MQTT server
  }
  // If payload is 1 and topic is Inside
  else if ((char)payload[0] == '1' && topic[17] == 'i') {
    digitalWrite(inside, HIGH);                             // Turn on the light for the outside light
    mqttclient.publish("/outside/mailbox/inside/", "ON");   // And notify MQTT server
  } else if ((char)payload[0] == '0' && topic[17] == 'i') {
    digitalWrite(inside, LOW);                              // Same for turning off
    mqttclient.publish("/outside/mailbox/inside/", "OFF");  // Notify MQTT
  }

  mqttclient.publish("/outside/mailbox/temperature/", "42.42"); // Report initial dummy value to the MQTT as temp reading.
}



void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttclient.connect("ESP8266Client")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      mqttclient.publish("/outside/mailbox/outside/", "OFF");
      // ... and resubscribe
      mqttclient.subscribe("/outside/mailbox/outside/set");
      // Same for inside light
      mqttclient.publish("/outside/mailbox/inside/", "OFF");
      mqttclient.subscribe("/outside/mailbox/inside/set");

      /* A-a-and same goes for the temp sensor
       * Report a dummy value as a temp reading. The actual temp reading
       * will occur in our main loop.
       */
      mqttclient.publish("/outside/mailbox/temperature/", "42.42");
      mqttclient.subscribe("/outside/mailbox/temperature/set");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttclient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
void loop() {
  
  unsigned long currentMillis = millis(); // Get current time to compare to last time we checked.
  if (!client.connected()) {
    reconnect();
  }
  mqttclient.loop(); // Do the MQTT loop
  // If the hatch is poen and it's been longer than 500 ms
  if(digitalRead(hatchPin) == HIGH && currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis; // set current time, to compare next time.
    // So before it was closed, now it's open. This can probably be refactored to include all in one if.
    if(hatchLast == 0) {
      deposits++;
      Serial.print("Number of deposits since last open: ");
      Serial.println(deposits);
      // Now let's let the MQTT server know
      deposits_str = String(deposits); //convert to string
      deposits_str.toCharArray(deposits_char, deposits_str.length() + 1); //packaging up the string in order to publish to MQTT
      Serial.println(deposits_char);
      mqttclient.publish("/outside/mailbox/deposits/", deposits_char);
      hatchLast = 1; // NOW the last status was open. We don't want to continue increase.
      mqttclient.publish("/outside/mailbox/hatch/", "open");
    }
    
  }
  else if(digitalRead(hatchPin) == LOW && currentMillis - previousMillis >= closedInterval) {
    previousMillis = currentMillis;
    if(hatchLast == 1) {
      hatchLast = 0; // Before it was open, now it's closed. 
      mqttclient.publish("/outside/mailbox/hatch/", "closed");
    }
  }

  // Door open detection
  if(digitalRead(doorPin) == HIGH && currentMillis - previousMillisDoor >= interval) {
    previousMillisDoor = currentMillis;
    if(doorLast == 0) {
      mqttclient.publish("/outside/mailbox/door/", "open");
      doorLast = 1;  
    }
    
    // At least one deposit
    if(deposits >= 1) {
      deposits = 0;
      Serial.print("Number of deposits since last open: ");
      Serial.println(deposits);
      deposits_str = String(deposits); //converting to string
      deposits_str.toCharArray(deposits_char, deposits_str.length() + 1); //packaging up the data in order to publish to MQTT
      Serial.println(deposits_char);
      mqttclient.publish("/outside/mailbox/deposits/", deposits_char);
    }
  }
  else if(digitalRead(doorPin) == LOW && currentMillis - previousMillisDoor >= closedInterval) {
    previousMillisDoor = currentMillis;
    if(doorLast == 1) {
      mqttclient.publish("/outside/mailbox/door/", "closed");
      doorLast = 0;  // Set last status to closed. Only gets run if last was open.
    }
  }
 
  // TODO - add temperatur sensor and humitidy sensor, send data via MQTT.
  // Because everybody needs to know what the temperature is in their mailbox.

  // Temperature reading
  const float temp = float(digitalRead(tempSensorInPin)); // interpret input bits as float
  String temp_str  = String(temp);                        // convert the temp reading to a string

  temp_str.toCharArray(temp_str, temp_str.length() + 1);  // packaging up the data in order to publish to MQTT
  Serial.println(temp_str);                               // for troubleshooting
  mqttclient.publish("/outside/mailbox/temperature/", temp_str);
}
