/*********
Bashir Rahmah

AC Control System, Hardware to web API

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files.

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
*********/

#include <Arduino.h>
#include <WebSerial.h>
#include <AsyncElegantOTA.h>
#ifdef ESP32
#include <WiFi.h>
#include <AsyncTCP.h>
#else
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#endif
#include <ESPAsyncWebServer.h>

AsyncWebServer server(80);
void(* resetFunc) (void) = 0;

// REPLACE WITH YOUR NETWORK CREDENTIALS
const char* ssid = "MyWifiName";
const char* password = "MyWifiPassword";

const char* PARAM_INPUT_1 = "fan_speed";
const char* PARAM_INPUT_2 = "mode_type";

// Mills Initial Var Decleration
unsigned long timeSinceReboot;

// Fan Speed Global Vars
int fan_speed = 0;
int existing_fan_speed;
// Mode Global Vars
int ac_mode;
int existing_ac_mode;

// Create Variable That Indicates If The System Is Currently Doing Something As To Not Interfere With Current Operations.
bool isCurrentlyDoingStuff = false;
bool WeJustChangedTheMode = false;
bool DidWeJustTurnOn = true;
bool ShouldWeReboot = false;

// Delay Int For Consecutive Button Presses
int delay_time = 120;

// HTML web page
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
<title>AC Control System</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
</head><body>
<h2>ESP-32 AC</h2>
<form action="/get">Fan Speed (0-100)<input type="text" name="fan_speed">
  <input type="submit" value="Submit">
</form><br>
<form action="/get">Mode Type (0 Off, 1 Fan, 2 AC, 3 Exhaust, 4 Fire Mode)<input type="text" name="mode_type">
  <input type="submit" value="Submit">
</form>
<br>
<a href="/webserial">Serial Monitor</a>
<br>
<a href="/update">Update Firmware</a>
<br>
<a href="/reboot">Reboot Device + AC Controler</a>
</body></html>)rawliteral";

void notFound(AsyncWebServerRequest *request) {
request->send(404, "text/plain", "Not found");
}

void setup() {
Serial.begin(115200);

// Setup The GPIO Pins 
// WIFI LED Pin
pinMode(25, OUTPUT);  
// Reset Pin
pinMode(13, OUTPUT);  
// Cool
pinMode(16, OUTPUT);  
// Fan
pinMode(17, OUTPUT);  
// Exhaust
pinMode(18, OUTPUT);  
// Off
pinMode(19, OUTPUT); 
// Fan Up 
pinMode(21, OUTPUT);  
// Fan Down
pinMode(22, OUTPUT);  

digitalWrite(25, LOW); // Turn WiFi LED Off By Default

// Start WIFI
WiFi.mode(WIFI_STA);
Serial.print("Connecting To WIFI...");
WiFi.begin(ssid, password);
if (WiFi.waitForConnectResult() != WL_CONNECTED) {
  Serial.println("WiFi Failed!");
  // Turn WIFI LED OFF
  digitalWrite(25, LOW);
  // Restart The ESP chip to attempt to reconnect
  ShouldWeReboot = true;
  return;
} else {
  digitalWrite(25, HIGH); // Turn On LED
  }
Serial.println();
Serial.print("IP Address: ");
Serial.println(WiFi.localIP());
// Start WebSerial
  // WebSerial is accessible at "<IP Address>/webserial" in browser
  WebSerial.begin(&server);
  // Print Network Details To WebSerial
    WebSerial.println();
    WebSerial.print("IP Address: ");
    WebSerial.println(WiFi.localIP());
  // Begin Update Server
  AsyncElegantOTA.begin(&server);    // Start ElegantOTA  
     
// Send web page with input fields to client
server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send_P(200, "text/html", index_html);
});

// When <ESP-IP>/reboot is called, Reboot The ESP
server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send_P(200, "text/html", "<b>Thank You For Your Request, I will now momentarily commit Seppuku</b>");
  ShouldWeReboot = true;
}); 

// Send a GET request to <ESP_IP>/get?input1=<inputMessage>
server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
  String inputMessage;
  String inputParam;
  // GET Fan Speed value on <ESP_IP>/get?fan_speed=<FanSpeed>
  if (request->hasParam(PARAM_INPUT_1)) {
    inputMessage = request->getParam(PARAM_INPUT_1)->value();
    inputParam = PARAM_INPUT_1;
    int fan_speed_int = inputMessage.toInt();
    fan_speed = fan_speed_int;
    WebSerial.print("Recived Call to Fan Speed API With Data: ");
    WebSerial.println(inputMessage);
    Serial.print("Recived Call to Fan Speed API With Data: ");
    Serial.println(inputMessage);
  }
  // GET Mode Type value on <ESP_IP>/get?mode_type=<mode_type(0 Off, 1 Fan, 2 AC, 3 Exh, 4 Fire Mode)>
  else if (request->hasParam(PARAM_INPUT_2)) {
    inputMessage = request->getParam(PARAM_INPUT_2)->value();
    inputParam = PARAM_INPUT_2;
    int modeType = inputMessage.toInt();
    ac_mode = modeType;
    WebSerial.print("Recived Call to AC Mode API With Data: ");
    WebSerial.println(inputMessage);
    Serial.print("Recived Call to AC Mode API With Data: ");
    Serial.println(inputMessage);
  }
  else {
    inputMessage = "0";
    inputParam = "no_param";
    WebSerial.println("No Parameters Were Passed Through, Sending Http Error");
    Serial.println("No Parameters Were Passed Through, Sending Http Error");
    request->send(404, "text/html", "GET Fan Speed value on (ESP_IP)/get?fan_speed=(FanSpeed[0-100]) <br> GET Mode Type value [Fan,AC,Off,Etc] on (ESP_IP)/get?mode_type=(mode_type[0 Off, 1 Fan, 2 AC, 3 Exh, 4 Fire Mode])");
  }

  WebSerial.println(""+ inputParam +" = " + inputMessage +"");
  Serial.println(""+ inputParam +" = " + inputMessage +"");
  request->send(200, "text/html", "HTTP GET request sent to your ESP on input field (" 
                                   + inputParam + ") with value: " + inputMessage +
                                   "<br><a href=\"/\">Return to Home Page</a>");
});
server.onNotFound(notFound);
server.begin();
}

void loop() {
  // Update Server
  AsyncElegantOTA.loop();

// Reset The Fan Controler If We Just Turned On  
if (DidWeJustTurnOn == true) {
DidWeJustTurnOn = false;
// Reset The Fan Controler
Serial.println("Pressing The Reset Button");
WebSerial.println("Pressing The Reset Button");
digitalWrite(13, HIGH);
delay(delay_time);
digitalWrite(13, LOW);
}

// Reboot Function
if (ShouldWeReboot == true){
  ShouldWeReboot = false;
  Serial.println("Rebooting...");
  WebSerial.println("Rebooting...");
  delay(500);
  resetFunc();
  }

// Update Mills
timeSinceReboot = millis();
if (timeSinceReboot > 21600000){
  Serial.println("Hmm, Seems like i've been up for a while now, I'm gonna restart");
  WebSerial.println("Hmm, Seems like i've been up for a while now, I'm gonna restart");
  delay(1000);
  resetFunc();
  }

// Only Consider Changing the mode if the system isnt currently doing stuff
if (isCurrentlyDoingStuff == false) {
// Modify The Mode If The Mode Is Different To What It Was Before
if (ac_mode != existing_ac_mode) {
    existing_ac_mode = ac_mode;
/////////////////////////////////////////////
//         Check For The Mode Type         //
/////////////////////////////////////////////
    // Off
    if (ac_mode == 0) {
      isCurrentlyDoingStuff = true;
      WebSerial.println("Pressing The Off Button");  
      Serial.println("Pressing The Off Button");  
      digitalWrite(19, HIGH);
      delay(delay_time);
      digitalWrite(19, LOW);
      delay(2000);
      WebSerial.println("Pressing The Reset Button");
      Serial.println("Pressing The Reset Button");
      delay(delay_time);
      digitalWrite(13, HIGH);
      delay(delay_time);
      digitalWrite(13, LOW);
      isCurrentlyDoingStuff = false;
     }
    // Fan
    else if (ac_mode == 1) {
      isCurrentlyDoingStuff = true;
      WebSerial.println("Pressing The RESET Button");  
      Serial.println("Pressing The Reset Button");
      digitalWrite(13, HIGH);
      delay(delay_time);
      digitalWrite(13, LOW);
      delay(2000);
      WebSerial.println("Pressing The Fan Button");
      Serial.println("Pressing The Fan Button");
      delay(delay_time);
      digitalWrite(17, HIGH);
      delay(delay_time);
      digitalWrite(17, LOW);  
      isCurrentlyDoingStuff = false;    
     }
    // Cool      
    else if (ac_mode == 2) {
      isCurrentlyDoingStuff = true;
      WebSerial.println("Pressing The RESET Button");  
      Serial.println("Pressing The Reset Button");
      digitalWrite(13, HIGH);
      delay(delay_time);
      digitalWrite(13, LOW);
      delay(2000);
      WebSerial.println("Pressing The FAN Button");
      Serial.println("Pressing The FAN Button");
      digitalWrite(17, HIGH);
      delay(delay_time);
      digitalWrite(17, LOW);  
      delay(3000); // Give The Fan Some Time To Initiate
      WebSerial.println("Pressing The COOL Button");
      Serial.println("Pressing The FAN Button");
      digitalWrite(16, HIGH);
      delay(delay_time);
      digitalWrite(16, LOW);   
      isCurrentlyDoingStuff = false;
     }
    // Exhaust
    else if (ac_mode == 3) {
      isCurrentlyDoingStuff = true;
      WebSerial.println("Pressing The RESET Button");  
      Serial.println("Pressing The Reset Button");
      digitalWrite(13, HIGH);
      delay(delay_time);
      digitalWrite(13, LOW);
      delay(2000);
      WebSerial.println("Pressing The Exhaust Button");
      Serial.println("Pressing The Exhaust Button");
      delay(delay_time);
      digitalWrite(18, HIGH);
      delay(delay_time);
      digitalWrite(18, LOW);      
      isCurrentlyDoingStuff = false;   
      }
    // Fire Mode 
    else if (ac_mode == 4) {
      isCurrentlyDoingStuff = true;
      WebSerial.println("Pressing The RESET Button");  
      Serial.println("Pressing The Reset Button");
      digitalWrite(13, HIGH);
      delay(delay_time);
      digitalWrite(13, LOW);
      delay(2000);
      WebSerial.println("Pressing The Cool Button");
      Serial.println("Pressing The Cool Button");
      delay(delay_time);
      digitalWrite(16, HIGH);
      delay(delay_time);
      digitalWrite(16, LOW);     
      delay(2000);
      WebSerial.println("Pressing The Cool Button");
      Serial.println("Pressing The Cool Button");
      delay(300);
      digitalWrite(16, HIGH);
      delay(delay_time);
      digitalWrite(16, LOW);    
      isCurrentlyDoingStuff = false; 
     }
     WeJustChangedTheMode = true;      
     }
}

// Only Consider Changing the fan speed if the system isnt currently doing stuff
if (isCurrentlyDoingStuff == false) {  
// Only consider changing the fan speed if the mode is not 0 [Off]
if (ac_mode != 0) {
// Modify The Fan Speed If The Speed Is Different To What It Was Before or the mode was just changed 
  if (fan_speed != existing_fan_speed || WeJustChangedTheMode == true) {   
    int previous_fan_speed = existing_fan_speed;
    existing_fan_speed = fan_speed;
    isCurrentlyDoingStuff = true;
    // Set Default Values If Nothing Has Been Imported
    if (WeJustChangedTheMode == true) {
        WeJustChangedTheMode = false;
        previous_fan_speed = 50;
        if (fan_speed == 0) {
          delay(1000);
          if (fan_speed == 0) {
            Serial.println("Didn't Recieve Fan Speed Value, Resorting to 50 for the moment...");
            WebSerial.println("Didn't Recieve Fan Speed Value, Resorting to 50 for the moment...");
            fan_speed = 50;
          }
          }
      } 
      // Change The Fan Speed Depending On the Conditions
        if (previous_fan_speed > fan_speed) {
        // Press Fan Down The Desired Amount Of Times
          int WeNeedToHitFanDown = previous_fan_speed - fan_speed;
          for (int x = 0; x < WeNeedToHitFanDown; x++) {
          delay(delay_time);
          Serial.println("Hit Fan Down");
          digitalWrite(22, HIGH);
          delay(delay_time);
          digitalWrite(22, LOW);
          } 
          isCurrentlyDoingStuff = false;
        } else if (previous_fan_speed == fan_speed) {
          // Do Nothing
          WebSerial.println("Fan Speed Is Already Correct, Doing Nothing");
          Serial.println("Fan Speed Is Already Correct, Doing Nothing");
          isCurrentlyDoingStuff = false;
          } else {
            // Press Fan Up The Desired Amount Of Times
            int WeNeedToHitFanUp = fan_speed - previous_fan_speed;
            for (int x = 0; x < WeNeedToHitFanUp; x++) {
            delay(delay_time);
            Serial.println("Hit Fan Up");
            digitalWrite(21, HIGH);
            delay(delay_time);
            digitalWrite(21, LOW);
            } 
            isCurrentlyDoingStuff = false;
            }  

}
}


// END LOOP
}
}

