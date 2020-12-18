/***********************************************************************************
* Project: Lab3
* Class: CIT324 - Networking for IoT
* Author: Vasilije Mehandzic
*
* File: main.cpp
* Description: Exercise #2 - Pollin’, pollin’, pollin’ …
* Date: 12/14/2020
**********************************************************************************/

#include <Arduino.h>
#include "serial-utils.h"
#include "wifi-utils.h"
#include "string-utils.h"
#include "whiskey-bug.h"
#include "gps-html.h"
#include <PubSubClient.h>
#include <pthread.h>
#include <HttpClient.h>
#include <TinyGPS++.h>
//  #include <ArduinoJson.h>
// for disable brownout detector https://github.com/espressif/arduino-esp32/issues/863
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// operation modes enum
enum operation_type { 
    OPERATION_TYPE_NORMAL,
    OPERATION_TYPE_UDP_BROADCAST,
    OPERATION_TYPE_MQTT_MODE,
    OPERATION_TYPE_LED_BLINK_MODE,
    OPERATION_TYPE_GOOGLE_MAPS_MODE,
};
TinyGPSPlus gps;
HardwareSerial SerialGPS(1);   
const char *gpsStream = "";

const unsigned int UDP_PORT = 8888, UDP_PACKET_SIZE = 64, MQTT_BROKER_PORT = 1883;
const char* P_UDP_MESSAGE = "^^^^^^<| DoN'T MoVE DoN'T SToP {<waka waka waka waka>} |>^^^^^^";
const char* MQTT_BROKER_SERVER = "maqiatto.com";
const char* MQTT_BROKER_CLIENT_ID = "WhiskeyBug";
const char* MQTT_TEMPERATURE_TOPIC = "vasske@gmail.com/Temperature";
const char* MQTT_PRESSURE_TOPIC = "vasske@gmail.com/Pressure";
const char* MQTT_ALCOHOL_TOPIC = "vasske@gmail.com/AlcoholContent";
const char MQTT_BROKER_USERNAME[] = "vasske@gmail.com";
const char MQTT_BROKER_PASSWORD[] = "emkjutitiPASS";
const char device_id[] = "00f0e90b-d6e1-48d9-9579-fcf981185f07";
const char* get_req =  "https://1vl7w3fjhc.execute-api.us-west-2.amazonaws.com/default/DevicesLEDs?DeviceId=1";
const char* post_req = "https://1vl7w3fjhc.execute-api.us-west-2.amazonaws.com/default/DevicesLEDs";

WiFiUDP Udp;
IPAddress remote_ip;
bool current_action_button, choice_printed = 0, bulb_should_be = 0, server_started = 0;
int serial_read, incoming_byte, num_ssid, previous_mode_of_operation;
int key_index = 0, current_mode_of_operation = OPERATION_TYPE_NORMAL, BULB_PIN = 22, BUTTON_PIN = 38;
String key_pressed, read_string;
byte mac[6];
wl_status_t status = WL_IDLE_STATUS;  // the Wifi radio's status
time_t gps_seconds, seconds = time(NULL);

char ch_temp[16], ch_pres[16], ch_alco[16], ch_lat[16], ch_lng[16];
WhiskeyBug whiskey_bug;
WiFiClient esp_client, webclient;
PubSubClient pub_sub_client(esp_client);
HTTPClient http_get_client, http_post_client;

IPAddress ap_ip_address(192, 168, 1, 1);
IPAddress ap_subnet_address(255, 255, 255, 0);
WiFiServer server(80);

/***********************************************************************************
* Purpose: Print the main menu content.
* No arguments, no returns
**********************************************************************************/
void printMainMenu() {
  Serial.println("–––––––––––––––––––––––––––––––––––––--–––\n"); 
  Serial.print("A – Display MAC address\n");
  Serial.print("L - List available wifi networks\n");
  Serial.print("C – Connect to a wifi network\n");
  Serial.print("U – Autoconnect on the predefinied network\n");
  Serial.print("D – Disconnect from the network\n");
  Serial.print("I – Display connection info\n");
  Serial.print("M – Display the menu options\n");
  Serial.print("V - UDP_BROADCAST mode\n");
  Serial.print("Q - MQTT mode\n"); 
  Serial.print("B – BLINK mode\n");
  Serial.print("G – GOOGLE MAPS mode\n");
  Serial.println("–––––––––––––––––––––––––––––––––––––--–––\n"); 
}

/***********************************************************************************
* Purpose: Print on the serial port the mac address in use.
* No arguments, no returns 
**********************************************************************************/
void printMacAddresses() {  
    WiFi.macAddress(mac);  // get your MAC address
    Serial.println(macAddressToString(mac));  // and print  your MAC address
}

/***********************************************************************************
* Purpose: Scan and detailed serial port print of the Network APs found
* No arguments, no returns
**********************************************************************************/
void networksList() {
  int num_of_ssid = WiFi.scanNetworks();   
  if (num_of_ssid > -1) {
    for (int this_net = 0; this_net < num_of_ssid; this_net++) {     
      Serial.print(this_net + 1);  // print the network number      
      Serial.print(". " + WiFi.SSID(this_net) + " [" );  // print the ssid
      Serial.print(wifiAuthModeToString(WiFi.encryptionType(this_net)).c_str());  // print the authentication mode
      Serial.print("]  (");
      Serial.print(WiFi.RSSI(this_net));  // print the ssid, encryption type and rssi for each network found
      Serial.print(" dBm)\n");
    }
  }
  else
    Serial.print("Couldn't get, or there is not a wifi connection!\n");
}

/***********************************************************************************
* Purpose: Connect to the chosen network from the list 
* Arguments String ssid ,network_password. If empty, re-enter.
**********************************************************************************/
void connectTo(String ssid = "", String network_password = "") {  
  if (ssid == "")
    ssid = WiFi.SSID(std::atoi(serialPrompt("\nChoose Network: ", 3).c_str()) - 1);

  if (network_password == "")
    network_password = serialPrompt("Password: ", 42);  // that's it

  Serial.print("Connecting to "); Serial.print(ssid.c_str()); Serial.print("...\n\n");
  WiFi.begin(ssid.c_str(), network_password.c_str());
  delay(2000);
  Serial.println(wifiStatusToString(WiFi.status()).c_str()); 
}

/***********************************************************************************
* Purpose: Disconnect WiFi and print the current status
* No arguments, no returns
**********************************************************************************/
void disconnect() {
  Serial.print("Disonnecting...");
  WiFi.disconnect();
  delay(2000);
  status = WiFi.status();
  Serial.println(wifiStatusToString(status).c_str());   
}

/***********************************************************************************
* Purpose: Print the connection info
* No arguments, no returns
**********************************************************************************/
void connectionInfo() {
  Serial.print("Status:\t\t");  Serial.println(wifiStatusToString(WiFi.status()).c_str());
  Serial.print("Network:\t");  Serial.println(WiFi.SSID());
  Serial.print("IP Address:\t");  Serial.println(WiFi.localIP());
  Serial.print("Subnet Mask:\t");  Serial.println(WiFi.subnetMask());
  Serial.print("Gateway:\t");  Serial.println(WiFi.gatewayIP());
} 

void autoConnect() {
  int num_of_ssid = WiFi.scanNetworks();
  for (int this_net = 0; this_net < num_of_ssid; this_net++) {
    if (WiFi.SSID(this_net) == "link_2.4")
      WiFi.begin(WiFi.SSID(this_net).c_str(), ((String)"rewrewrew").c_str());
  }
  Serial.println("\nAUTO-CONNECTED!");
}

/***********************************************************************************
* Purpose: Change the operation mode to normal
* No arguments, no returns 
**********************************************************************************/
void changeModeToNormal() {
  current_mode_of_operation = OPERATION_TYPE_NORMAL;
  Serial.println("NORMAL MODE");
}

/***********************************************************************************
* Purpose: Change the operation mode to udp broadcast
* No arguments, no returns
**********************************************************************************/
void changeModeToUDP() {
  current_mode_of_operation = OPERATION_TYPE_UDP_BROADCAST;
  Serial.println("UDP_BROADCAST MODE\nESC - change the current mode to NORMAL");
}

/***********************************************************************************
* Purpose: Switch between two main operation modes
* No arguments, no returns 
**********************************************************************************/
void changeMode() {
  if (current_mode_of_operation == OPERATION_TYPE_NORMAL)
    changeModeToUDP();
  else 
    changeModeToNormal();
}

/***********************************************************************************
* Purpose: Change the operation mode to mqtt
* No arguments, no returns 
**********************************************************************************/
void changeModeToMQTT() {
  current_mode_of_operation = OPERATION_TYPE_MQTT_MODE;
  Serial.print("\nMQTT MODE\nX - change the mode to NORMAL\n");
}

/***********************************************************************************
* Purpose: Check if the key "x" is pressed
* No arguments, no returns, affects some globals
**********************************************************************************/
void checkForXPressed() {
  if (Serial.available() > 0)
    serial_read = Serial.read();
  if ((serial_read == 88)||(serial_read == 120))  //  if X or x is pressed
    changeModeToNormal();
}

/***********************************************************************************
* Purpose: Check if the key "Esc" is pressed
* No arguments, no returns, affects some globals
**********************************************************************************/
void checkForESCPressed() {
  if (Serial.available() > 0)
      serial_read = Serial.read();
  if (serial_read == 27)
    changeModeToNormal();
}

/***********************************************************************************
* Purpose: Send earlies specified UDP packet with possible loop break with ESC key
* No arguments, no returns
**********************************************************************************/
void sendUDP()
{
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("You need to connect first! Switching back to the normal mode.\n");
    changeModeToNormal();
  }
  else {
    remote_ip = WiFi.gatewayIP();
    remote_ip[3] = 255;
    checkForESCPressed();
    
    // exit from loop every 10 seconds
    while (time(NULL) - seconds < 10 && current_mode_of_operation == OPERATION_TYPE_UDP_BROADCAST)
      checkForESCPressed();

    Udp.begin(UDP_PORT);
    Udp.beginPacket(remote_ip, UDP_PORT);

    for (int i = 0; i < UDP_PACKET_SIZE; i++)
      Udp.write(P_UDP_MESSAGE[i]);

    Udp.endPacket();
    Udp.stop();    
    seconds = time(NULL);
  }
}

/***********************************************************************************
* Purpose: Send the payload to topic
* Arguments: topic - ; payload
* No returns
**********************************************************************************/
void myMQTT(const char* topic, const char* payload) {
  pub_sub_client.setServer(MQTT_BROKER_SERVER, MQTT_BROKER_PORT);

  if (pub_sub_client.connect(MQTT_BROKER_CLIENT_ID, MQTT_BROKER_USERNAME, MQTT_BROKER_PASSWORD)) {
      if (pub_sub_client.publish(topic, payload)) {}
      else {
        Serial.print("ERROR: publishing failed with state ");
        Serial.print(pub_sub_client.state());
        delay(2000);
      }
  } else {
    Serial.print("ERROR: connect failed with state ");
    Serial.print(pub_sub_client.state());
    delay(2000);
  }
}

/***********************************************************************************
* Purpose: Change blink type mode
* No arguments, no returns
**********************************************************************************/
void changeBlinkMode() {  
  if (current_mode_of_operation == OPERATION_TYPE_NORMAL) {
    previous_mode_of_operation = current_mode_of_operation;
    current_mode_of_operation = OPERATION_TYPE_LED_BLINK_MODE;
  }
  else
    current_mode_of_operation = OPERATION_TYPE_NORMAL;
}

/***********************************************************************************
* Purpose: Turn the bulb on by setting PIN to high
* No arguments, no returns
**********************************************************************************/
void bulbOn(int ms=0) {
  if (ms > 0)
    delay(ms);

  digitalWrite(BULB_PIN, HIGH);
}

/***********************************************************************************
* Purpose: Turn the bulb off by setting PIN to low
* No arguments, no returns
**********************************************************************************/
void bulbOff(int ms=0) {
  if (ms > 0)
    delay(ms);

  digitalWrite(BULB_PIN, LOW);
}

/***********************************************************************************
* Purpose: Check the button pressed state (gpio 38)
* No arguments, no returns
**********************************************************************************/
void* checkGPIO38(void* thead_id) {
  void* check_38_ret;
  while (1) {
    delay(500);
    current_action_button = digitalRead(BUTTON_PIN);

    if (current_action_button) {
      bulb_should_be = !bulb_should_be; 
      if (current_mode_of_operation != previous_mode_of_operation) 
        current_mode_of_operation = previous_mode_of_operation; 
    }
  }
  return check_38_ret;
}

/***********************************************************************************
* Purpose: Checks state by get request
* No arguments, no returns
**********************************************************************************/
void  checkAWS() { 
    if (http_get_client.begin(get_req)) {
      if (http_get_client.GET() == HTTP_CODE_OK) { 
        if (http_get_client.getString() != "0")
          bulbOn();
        else
          bulbOff();    
      } 
      http_get_client.end();
    }
}

/***********************************************************************************
* Purpose: Sends html post request as needed
* No arguments, no returns
**********************************************************************************/
void  sendPost() {
  if(http_post_client.begin(post_req)) {
    http_post_client.addHeader("Content-Type", "text/plain");
    http_post_client.sendRequest("POST", String("") + "{\"DeviceId\":\"1\", \"ledState\": " + String(bulb_should_be) + "}");
    http_post_client.end();
  }
}

/***********************************************************************************
* Purpose: Sends html post request as needed
* No arguments, no returns
**********************************************************************************/   
void startAPAndWebServer() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP("vvvAP", "88888888");    //Serial.println("Wait 100 ms for AP_START...");
  delay(100);    

  WiFi.softAPConfig(ap_ip_address, ap_ip_address, ap_subnet_address);    
  Serial.print("AP IP address: "); Serial.print(WiFi.softAPIP()); Serial.print("      Press ESC to exit\n");
  server.begin();
  server_started = 1;
}

/***********************************************************************************
* Purpose: GPS & Web Server, router, etc.
* No arguments, no returns
**********************************************************************************/
void serverGPSWebRouter() {
  gps_seconds = time(NULL);

  if (!server_started)
    startAPAndWebServer();

  while (SerialGPS.available())
    gps.encode(SerialGPS.read());

  while ((time(NULL) - gps_seconds < 5) && (current_mode_of_operation == OPERATION_TYPE_GOOGLE_MAPS_MODE)) {
    webclient = server.available();
    read_string = "";

    // If a new client connects, loop while the client's connected & available
    if (webclient) {        
      if (webclient.connected() && webclient.available()) {             
          read_string = webclient.readString();     

          // Route: /location          
          if (read_string.indexOf("GET /location HTTP") == 0) {
/*             const int capacity = JSON_OBJECT_SIZE(2);
            StaticJsonDocument<capacity> doc;      
            doc["lat"] = gps.location.lat();
            doc["lon"] = gps.location.lng();
            serializeJsonPretty(doc, webclient);   */          
            webclient.print("{ 'gps': ");
            webclient.print("{ 'lat': ");
            webclient.print(gps.location.lat());
            webclient.print(", 'long': ");
            webclient.print(gps.location.lng());
            webclient.print(" } }");
          }

		  // Route: /   
          else if (read_string.indexOf("GET / HTTP") == 0) { 
            std::string HTML;
            HTML += GPS_APP_HTML;
            size_t pos;
            sprintf(ch_lat, "%f", gps.location.lat());
            sprintf(ch_lng, "%f", gps.location.lng());
            while ((pos = HTML.find("latitude_Unknown")) != -1)
              HTML.replace(pos, 16, ch_lat);
            while ((pos = HTML.find("longitude_Unknown")) != -1)
              HTML.replace(pos, 17, ch_lng); 
            webclient.print(HTML.c_str());  
          }

		  // Route: *           
          else  
            webclient.print("404");   
        }
      
      webclient.stop();     
    }
 
    if (Serial.available() > 0) {      
      serial_read = Serial.read();
  
      if (serial_read == 27) {
        current_mode_of_operation = previous_mode_of_operation;
        Serial.println("EXIT FROM GOOGLE MAPS MODE");  // Serial.print(current_mode_of_operation);
        server.end();
        server_started = 0;
      }
    }
  }
}

/***********************************************************************************
* Purpose: Change the mod to Google Maps
* No arguments, no returns
**********************************************************************************/
void changeToGoogleMapsMode() {
  if (current_mode_of_operation != OPERATION_TYPE_GOOGLE_MAPS_MODE) {
    previous_mode_of_operation = current_mode_of_operation;
    current_mode_of_operation = OPERATION_TYPE_GOOGLE_MAPS_MODE;
    }
}

void setup() {  
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);  // disable brownout detector  
  Serial.begin(115200,  SERIAL_8N1);
  SerialGPS.begin(9600, SERIAL_8N1, 12, 15);   //17-TX 18-RX
  pinMode(BULB_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT);
  printMainMenu();
  autoConnect();
  pthread_t t1;
  int a1 = 0; 
  pthread_create(&t1, NULL, checkGPIO38, (void *)a1);  
}

void loop() {
  switch (current_mode_of_operation)
  {
    case OPERATION_TYPE_GOOGLE_MAPS_MODE:
      serverGPSWebRouter();
      break;

    case OPERATION_TYPE_LED_BLINK_MODE:
      bulbOn(500);
      bulbOff(500);
      break;

    case OPERATION_TYPE_MQTT_MODE:
      if (WiFi.status() == WL_CONNECTED) {       
        checkForXPressed();  
        // exit from loop more than every second after previous
        while (time(NULL) - seconds < 1 && current_mode_of_operation == OPERATION_TYPE_MQTT_MODE)
          checkForXPressed();

        // Read the sensors from the whiskey bug, convert it to char[] and send it
        sprintf(ch_temp, "%f", whiskey_bug.getTemp());
        sprintf(ch_pres, "%f", whiskey_bug.getPressure());
        sprintf(ch_alco, "%f", whiskey_bug.getAlcoholContent());
        myMQTT(MQTT_TEMPERATURE_TOPIC, ch_temp);
        myMQTT(MQTT_PRESSURE_TOPIC, ch_pres);
        myMQTT(MQTT_ALCOHOL_TOPIC, ch_alco);
        seconds = time(NULL);
      }
      else
      {
        Serial.println("\nYou need to connect first! Will not try mqtt operations.\nSwitching back to NORMAL MODE\n");
        changeModeToNormal();        
      }      
      break;

    case OPERATION_TYPE_UDP_BROADCAST:
      sendUDP();
      break;

    case OPERATION_TYPE_NORMAL:
      key_pressed = "";

      if (!choice_printed) {
        Serial.print("\nChoice: ");
        choice_printed = 1;
      }

      if (Serial.available() > 0) {      
        key_pressed = static_cast<char>(Serial.read());
        Serial.print(key_pressed);
        Serial.print("\n\n");
        choice_printed = 0;
      }
      
      if (key_pressed != "")
        switch (key_pressed[0])
        {
          // if the key m is pressed print the main menu again.
          case 'M':
          case 'm':
            printMainMenu();
            break;

          // if the key a is pressed print the mac address.
          case 'A':
          case 'a':
            printMacAddresses();
            break;

          // if the key l is pressed scan and list the networks.
          case 'L':
          case 'l':
            networksList();
            break;       

          // if the key c is pressed list the networks and connect.
          case 'C':
          case 'c':
            networksList();
            connectTo();
            break;                

          // if the key d is pressed disconnect from network.
          case 'D':
          case 'd':
            disconnect();
            break;

          // if the key i is pressed print the connection info.
          case 'I':
          case 'i':
            connectionInfo();
            break; 

          // if the key v is pressed change the operational mode.
          case 'V':
          case 'v':
            changeMode();
            break; 

          // if the key q is pressed change the operational mode to mqtt.
          case 'Q':
          case 'q':
            changeModeToMQTT();
            break;

          // if the key b is pressed change the operational mode to blink.
          case 'B':
          case 'b':
            changeBlinkMode();
            break;  

          // if the key u is pressed autoconnect to the predefined network.
          case 'U':
          case 'u':
            autoConnect();   

          // if the key g is pressed switch to Google Maps Mode
          case 'G':
          case 'g':
            changeToGoogleMapsMode();                                
        }
      
      break;
  }

  // whole idea is that action button is main command
  if (WiFi.status() == WL_CONNECTED) {
    if (current_action_button)
      sendPost();
    else    
      checkAWS();
    }
}