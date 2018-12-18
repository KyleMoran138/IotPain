#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <WebServer.h>

const char* ssid = "I0Tthing";
const char* password = "esp32";
const char* www_username = "admin";
const char* www_password = "esp32";
const int NUMBER_DIGITAL_PORTS = 6;
String authFailResponse = "Authentication Failed";
int digital_ports[NUMBER_DIGITAL_PORTS] = {LED_BUILTIN,27,26,25,33,32};
int digital_port_types[NUMBER_DIGITAL_PORTS] = {OUTPUT,OUTPUT,OUTPUT,OUTPUT,OUTPUT,OUTPUT};
int digital_port_defaults[NUMBER_DIGITAL_PORTS] = {LOW,HIGH,HIGH,HIGH,HIGH,HIGH};
int wifi_connect_attempts = 0;
int max_wifi_connect_attempts = 5;
int smart_config_connect_attempts = 0;
int max_smart_config_connect_attempts = 20;
bool is_in_soft_ap_mode = false;
bool do_blink = false;

WebServer server(80);
TaskHandle_t TaskPointer;

void setup() {
  Serial.begin(115200);

  // Init all digital ports
  for(int i = 0; i<(NUMBER_DIGITAL_PORTS-1); i++){
    pinMode(digital_ports[i], digital_port_types[i]);
    digitalWrite(digital_ports[i], digital_port_defaults[i]);
  }

  // If the max connect attempts have been made then open a soft AP to configure
  if(wifi_connect_attempts < max_wifi_connect_attempts){
    WiFi.mode(WIFI_STA);
    WiFi.begin();
    while(WiFi.waitForConnectResult() != WL_CONNECTED && wifi_connect_attempts < max_wifi_connect_attempts) {
      wifi_connect_attempts++;
      Serial.print("WiFi Connect Failed! attempt (");
      Serial.print(wifi_connect_attempts);
      Serial.print("/");
      Serial.print(max_wifi_connect_attempts);
      Serial.print(")\n");
    }
  }else{
    //Go into smart config mode
    is_in_soft_ap_mode = true;
    Serial.println("Goingo to smart config mode");
    WiFi.mode(WIFI_AP_STA);
    WiFi.beginSmartConfig();
    while(!WiFi.smartConfigDone() && smart_config_connect_attempts < max_smart_config_connect_attempts){
      delay(500);
      Serial.print("Smart connect failed! attempt (");
      Serial.print(smart_config_connect_attempts);
      Serial.print("/");
      Serial.print(max_smart_config_connect_attempts);
      Serial.print(")\n");
      smart_config_connect_attempts++;
    }
    delay(1000);
    // If the smartconfig isn't done and the max attempts has been made then reboot to signify failed attempt
    if(!WiFi.smartConfigDone() && WiFi.status() != WL_CONNECTED && smart_config_connect_attempts >= max_smart_config_connect_attempts) ESP.restart();
    is_in_soft_ap_mode = false;
  }

  // TODO: Only allow OTA if soft
  ArduinoOTA.setPassword("admin");
  
  // If is in soft_ap mode only expose setup endpoint
  if(is_in_soft_ap_mode){
    server.on("/setup/network", function_network_setup);
  }else{
    server.on("/function/blink/toggle", function_blink_toggle);
  }
  
  server.begin();
  ArduinoOTA.begin();

  Serial.print("Open http://");
  Serial.print(WiFi.localIP());
  Serial.println("/ in your browser to see it working");
  xTaskCreatePinnedToCore(DoGPIOTaskCode, "GPIO", 1000, NULL, 1, &TaskPointer, 0);
}

// Webserver endpoints

void function_blink_toggle() {
  do_blink = !do_blink;
  server.send(200, "text/plain", "");
}

void function_network_setup(){
  String network = server.arg("network");
  String password = server.arg("password");
  if(network != "" && password != ""){
    WiFi.begin(network.c_str(), password.c_str());
    ESP.restart();
    delay(1000);
  }else{
    server.send(500, "text/plain", "");
  }
  server.send(200, "text/plain", "");
}
// End of webserver endpoints

// Second core tasks

void DoGPIOTaskCode(void *pvParameters) {
  Serial.print("Starting gpio task on core 2");
  for (;;) {
    doGPIO();
    delay(5);
  }
}

void DoStatusBlinkTaskCode(){
  Serial.print("Blinking status led");
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
  ArduinoOTA.handle();
  server.handleClient();
}
