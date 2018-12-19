#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <WebServer.h>

String ssid = "";
String password = "";
const char* ap_ssid = "ESP32";
const char* www_username = "admin";
const char* www_password = "esp32";
const int NUMBER_DIGITAL_PORTS = 6;
enum system_status{
  STOPPED = -1,
  IDLE = 0,
  CONNECTING = 1,
  WAITING_FOR_CONFIG = 2,
  DOING_SOMETHING = 3,
  FAILURE = 4};
String authFailResponse = "Authentication Failed";
int digital_ports[NUMBER_DIGITAL_PORTS] = {LED_BUILTIN,27,26,25,33,32};
int digital_port_types[NUMBER_DIGITAL_PORTS] = {OUTPUT,OUTPUT,OUTPUT,OUTPUT,OUTPUT,OUTPUT};
int digital_port_defaults[NUMBER_DIGITAL_PORTS] = {LOW,HIGH,HIGH,HIGH,HIGH,HIGH};
int wifi_connect_attempts = 0;
int max_wifi_connect_attempts = 1;
int current_status = IDLE;
bool is_in_ap_mode = false;
bool do_blink = false;
bool ota_enabled = false;

WebServer server(80);
TaskHandle_t TaskPointer;

void setup() {
  Serial.begin(115200);
  xTaskCreatePinnedToCore(DoStatusBlinkTaskCode, "Blink Status", 1000, NULL, 1, &TaskPointer, 0);

  // Init all digital ports
  current_status = DOING_SOMETHING;
  for(int i = 0; i<(NUMBER_DIGITAL_PORTS-1); i++){
    pinMode(digital_ports[i], digital_port_types[i]);
    digitalWrite(digital_ports[i], digital_port_defaults[i]);
  }

  current_status = CONNECTING;
  WiFi.begin();
  while(WiFi.waitForConnectResult() != WL_CONNECTED && wifi_connect_attempts <= max_wifi_connect_attempts-1) {
    wifi_connect_attempts++;
    Serial.print("WiFi Connect Failed! attempt (");
    Serial.print(wifi_connect_attempts);
    Serial.print("/");
    Serial.print(max_wifi_connect_attempts);
    Serial.print(")\n");
  }

  if(WiFi.status() != WL_CONNECTED)
  //Go into smart config mode
  {
    is_in_ap_mode = true;
    current_status = WAITING_FOR_CONFIG;
    Serial.println("Going to smart config mode");
    WiFi.softAP(ap_ssid);
    server.on("/settings/network/setup", function_network_setup);
    server.begin();
    while(password == "" && ssid == ""){
      server.handleClient();
    }
    WiFi.begin(ssid.c_str(), password.c_str());
    current_status = CONNECTING;
    wifi_connect_attempts = 0;
    while(WiFi.waitForConnectResult() != WL_CONNECTED && wifi_connect_attempts <= max_wifi_connect_attempts-1) {
      wifi_connect_attempts++;
      Serial.print("WiFi Connect Failed! attempt (");
      Serial.print(wifi_connect_attempts);
      Serial.print("/");
      Serial.print(max_wifi_connect_attempts);
      Serial.print(")\n");
    }
    
    // If the smartconfig isn't done and the max attempts has been made then reboot to signify failed attempt
    if(WiFi.status() != WL_CONNECTED) ESP.restart();
    is_in_ap_mode = false;
    current_status = IDLE;
  }
  current_status = DOING_SOMETHING;

  ArduinoOTA.setPassword("admin");
  
  // If is in smart_config mode only expose setup endpoint
  server.on("/function/blink/toggle", function_blink_toggle);
  server.on("/settings/ota", function_ota_enable);

  current_status = STOPPED;
  server.begin();
  ArduinoOTA.begin();

  Serial.print("Open http://");
  Serial.print(WiFi.localIP());
  Serial.println("/ in your browser to see it working");
  vTaskDelete(TaskPointer);
  xTaskCreatePinnedToCore(DoGPIOTaskCode, "GPIO", 1000, NULL, 1, &TaskPointer, 0);
}

// Webserver endpoints

void function_blink_toggle() {
  do_blink = !do_blink;
  server.send(200, "text/plain", "");
}

void function_network_setup(){
  String network = server.arg("network");
  String pass = server.arg("password");
  if(network != "" && pass != ""){
    ssid = network;
    password = pass;
  }else{
    server.send(500, "text/plain", "");
  }
  server.send(200, "text/plain", "");
}

void function_ota_enable(){
  String enStringArg = server.arg("en");
  bool doEnableOTA = false;
  if(enStringArg != ""){
    doEnableOTA = ( 1 <= enStringArg.toInt() );
  }
  ota_enabled = doEnableOTA;
  server.send(200, "text/plain", "");
}

// End of webserver endpoints

// Second core tasks

void DoGPIOTaskCode(void *pvParameters) {
  Serial.println("Starting gpio task on core 2");
  for (;;) {
    doGPIO();
    delay(5);
  }
}

void DoStatusBlinkTaskCode(void *pvParameters){
  Serial.println("Starting blinking task on core 2");
  while(current_status != STOPPED){
    if(current_status == IDLE){
      delay(5000);
      digitalWrite(LED_BUILTIN, HIGH);
      delay(500);
      digitalWrite(LED_BUILTIN, LOW);
    }else if(current_status == CONNECTING){
      delay(1000);
      digitalWrite(LED_BUILTIN, HIGH);
      delay(100);
      digitalWrite(LED_BUILTIN, LOW);
      delay(100);
      digitalWrite(LED_BUILTIN, HIGH);
      delay(100);
      digitalWrite(LED_BUILTIN, LOW);
      delay(100);
      digitalWrite(LED_BUILTIN, HIGH);
      delay(100);
      digitalWrite(LED_BUILTIN, LOW);
    }else if(current_status == WAITING_FOR_CONFIG){
      delay(1000);
      digitalWrite(LED_BUILTIN, HIGH);
      delay(500);
      digitalWrite(LED_BUILTIN, LOW);
    }else if(current_status == DOING_SOMETHING){
      delay(100);
      digitalWrite(LED_BUILTIN, HIGH);
      delay(50);
      digitalWrite(LED_BUILTIN, LOW);
    }else if(current_status == FAILURE){
      delay(250);
      digitalWrite(LED_BUILTIN, HIGH);
      delay(250);
      digitalWrite(LED_BUILTIN, LOW);
    }
  }
  Serial.println("Status blinking stopped");
}
// End of second core tasks

void doAuth() {
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication(DIGEST_AUTH, "", authFailResponse);
  }
}

void doBlink() {
  if (do_blink) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(1000);
    digitalWrite(LED_BUILTIN, LOW);
    delay(1000);
  } else {
    digitalWrite(LED_BUILTIN, LOW);
  }
}

void doGPIO() {
  doBlink();
}

void loop() {
  if(ota_enabled){
    ArduinoOTA.handle();
  }
  server.handleClient();
}
