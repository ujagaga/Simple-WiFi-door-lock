#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WebServer.h>
#include <ArduinoOTA.h>
#include <pgmspace.h>
#include <EEPROM.h>


/* Define 2 WiFi AP to connect to in case one is not available. 
This is usefull when developing at home and then using elsewhere. */
#define SSID_1              "WiFi SSID 1"
#define PASS_1              "passwd_1"

#define SSID_2              "WiFi SSID 2"
#define PASS_2              "passwd_2"

#define LOCK_RELAY_PIN      (2)
#define LED_PIN             (13)
#define RESET_PIN           (0)

#define UNLOCK_TIMEOUT      (1)
#define RESET_TIMEOUT       (5)
#define OTA_UPDATE_TIMEOUT  (15)

#define KEY_SIZE            (64)

/* Reset button push request type */
#define USER_REQUEST_NONE   (0)
#define USER_REQUEST_RESET  (1)
#define USER_REQUEST_UPDATE (2)

/* HTML response definitions */
const char INDEX_HTML[] PROGMEM = R"(
<!DOCTYPE HTML>
<html>
  <head>
    <meta name = "viewport" content = "width = device-width, initial-scale = 1.0, maximum-scale = 1.0, user-scalable=0">
    <title>OC WiFi Lock</title>
  </head>
  <body>
  <h1>OC WiFi Lock</h1>
  <p>
    Access key is not yet initialized. To configure the access key, append to your url the desired access key as parameter.<br>
    E.g. http://%ip%?key=Some random html safe string
  </p>
  <p>
    Copy the resulting url. It will be your way to unlock the door.<br>To reset the key, hold the reset button for 6s. The LED will start blinking.<br>
    To activate the "Over The Air" update, hold the reset button for at least 15 seconds. The LED will blink faster.
  </p>
  </body>
</html>
)";

const char AUTH_HTML[] PROGMEM = R"(
<!DOCTYPE HTML>
<html>
  <head>
    <meta name = "viewport" content = "width = device-width, initial-scale = 1.0, maximum-scale = 1.0, user-scalable=0">
    <title>OC WiFi Lock</title>
  </head>
  <body>
  <p>You are not authorized to access this device.</p>
  </body>
</html>
)";

const char KEY_SET_HTML[] PROGMEM = R"(
<!DOCTYPE HTML>
<html>
  <head>
    <meta name = "viewport" content = "width = device-width, initial-scale = 1.0, maximum-scale = 1.0, user-scalable=0">
    <title>OC WiFi Lock</title>
  </head>
  <body>
  <h1>OC WiFi Lock</h1>
  <p>New authorization key set. Please use <b><a href="http://%ip%?key=%key%">this</a></b> link to unlock the door.</p>
  </body>
</html>
)";

/* Global variables */
ESP8266WiFiMulti WiFiMulti;
ESP8266WebServer webServer(80);
bool updateStartedFlag = false;
char key[KEY_SIZE] = {0};
uint32_t unlockTimestamp = 0;           
uint32_t userBtnRequestTimestamp = 0;
int userRequest = USER_REQUEST_NONE;
uint32_t btnPressDetectedAt = 0;        // used for software debouncing

/* Software debouncing routine for unstable button press */
bool checkBtnPress(){
  if(digitalRead(RESET_PIN) == LOW){
    btnPressDetectedAt = millis();
  }

  if((millis() - btnPressDetectedAt) < 500){
    return true;
  }
  return false;
}

void OTA_init() {
  Serial.println("Starting OTA update");
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname("OC_WIFI_LOCK");

  // No authentication by default
  //ArduinoOTA.setPassword((const char *)"pass123");
   
  ArduinoOTA.onStart([]() {});
  ArduinoOTA.onEnd([]() {
    updateStartedFlag = false;
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {});
  ArduinoOTA.onError([](ota_error_t error) {});
  ArduinoOTA.begin();

  updateStartedFlag = true;
}

void saveKeyToEEPROM(String customKey){ 
  EEPROM.begin(KEY_SIZE);
  uint16_t addr;
  
  for(addr = 0; addr < customKey.length(); addr++){
    EEPROM.write(addr, customKey[addr]);   
  }

  EEPROM.write(customKey.length(), 0);

  EEPROM.commit(); 
  EEPROM.end();
}

void deleteKeyFromEEPROM(){
  Serial.println("RESET");
  EEPROM.begin(KEY_SIZE);
  uint16_t addr;
  
  for(addr = 0; addr < KEY_SIZE; addr++){
    EEPROM.write(addr, 0);   
  }

  EEPROM.commit();
  EEPROM.end();
}

void loadKeyFromEEPROM(){
  EEPROM.begin(KEY_SIZE);
  uint16_t i = 0;

  do{
    key[i] = EEPROM.read(i);    
        
    if((key[i] < 32) || (key[i] > 126)){
      /* Non printable character */
      break;
    }
    i++;
  }while(i < KEY_SIZE);
  key[i] = 0;  

  EEPROM.end();
  Serial.print("LOADED key:");
  Serial.println(key);
}

/* HTTP response callback */
void handleRoot() {
  String response = "";
  int i;

  if(webServer.args() == 0){
    // No args, so no key supplied
    if(int(key[0]) == 0){
      // Key not initialized. Display instructions
      response = FPSTR(INDEX_HTML);
      response.replace("%ip%", WiFi.localIP().toString());
    }else{
      response = FPSTR(AUTH_HTML);
    } 
    
    webServer.send(200, "text/html", response); 
  }else{
    response = "Unauthorized!"; 
   
    String paramName = webServer.argName(0);
    String paramVal = webServer.arg(0);

    String unlockKey(key); 

    if(int(key[0]) == 0){
      
      saveKeyToEEPROM(paramVal);
      loadKeyFromEEPROM();
      
      response = FPSTR(KEY_SET_HTML);
      response.replace("%ip%", WiFi.localIP().toString());
      response.replace("%key%", key);   
    
      webServer.send(200, "text/html", response);
      
    }else if(unlockKey.equals(paramVal)){
      unlockTimestamp = millis();
      response = "ok"; 
    
      webServer.send(200, "text/plain", response);
    }else{

      webServer.send(200, "text/plain", response);   
    }
  }
}

static void showNotFound(void){
  webServer.send(404, "text/plain", "Not found!");      
}

void HTTP_init(){ 
  webServer.on("/favicon.ico", showNotFound);
  webServer.on("/", handleRoot); 
  webServer.begin();
}

void setup() {
  delay(100);
  pinMode(LOCK_RELAY_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(RESET_PIN, INPUT_PULLUP); 
  digitalWrite(LOCK_RELAY_PIN, LOW); 
  digitalWrite(LED_PIN, HIGH);  
  
  Serial.begin(115200,SERIAL_8N1,SERIAL_TX_ONLY);
  
  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(SSID_1, PASS_1);
  WiFiMulti.addAP(SSID_2, PASS_2);
  
  while ( WiFiMulti.run() != WL_CONNECTED ) {
    delay ( 500 );
    ESP.wdtFeed();
    Serial.print ( "." );
  }
  String IP =  WiFi.localIP().toString();
  String wifi_statusMessage = "\nConnected to: " + WiFi.SSID() + String(". IP address: ") + IP;   
  Serial.println(wifi_statusMessage); 

  loadKeyFromEEPROM();
  
  HTTP_init();
}

void loop() { 
  if(checkBtnPress()){
    // Reset requested  
    if(userBtnRequestTimestamp == 0){
      userBtnRequestTimestamp = millis();
    }

    uint32_t timeDelta = millis() - userBtnRequestTimestamp;
    uint32_t secondsDelta = timeDelta / 1000;
    uint32_t counter = (timeDelta / 200);     
      
    if(secondsDelta > OTA_UPDATE_TIMEOUT){
      if((counter % 2) == 0){
        digitalWrite(LED_PIN, LOW);
      }else{
        digitalWrite(LED_PIN, HIGH);
      }

      userRequest = USER_REQUEST_UPDATE;
    }else if(secondsDelta > RESET_TIMEOUT){
      if((counter % 5) == 0){
        digitalWrite(LED_PIN, LOW); 
      }else{
        digitalWrite(LED_PIN, HIGH); 
      }

      userRequest = USER_REQUEST_RESET;
    }else{
      digitalWrite(LED_PIN, HIGH);       
      userRequest = USER_REQUEST_NONE;
    }  
    
  }else{    
    if(userRequest == USER_REQUEST_UPDATE){
      OTA_init();
    }else if(userRequest == USER_REQUEST_RESET){
      // Do the reset
      deleteKeyFromEEPROM();
      loadKeyFromEEPROM();
    }else{      
      digitalWrite(LED_PIN, HIGH); 
    }

    userBtnRequestTimestamp = 0;
    userRequest = USER_REQUEST_NONE;
  } 
 
  if(updateStartedFlag){
    ArduinoOTA.handle();
  }else{
    webServer.handleClient(); 
    
    if((millis() - unlockTimestamp) < UNLOCK_TIMEOUT){
      digitalWrite(LOCK_RELAY_PIN, HIGH); 
    }else{
      digitalWrite(LOCK_RELAY_PIN, LOW); 
    }
  }  
}
