#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Wire.h>
#include "RTClib.h"

// Pin Definitions
#define RELAY_PIN 14

// WiFi credentials (ESP as AP)
const char* ssid = "ClassBell";
const char* password = "12345678"; // ESP AP password

// Dashboard password
const String dashboardPassword = "bellpass";

RTC_DS3231 rtc;
AsyncWebServer server(80);

// Bell schedule (24hr format)
struct BellTime { int hour; int minute; };
BellTime schedule[] = {
  {9, 0},   // 1st period end
  {10, 0},  // 2nd period end
  {23,40}   // 3rd period end
};
const int scheduleCount = sizeof(schedule)/sizeof(schedule[0]);

// Non-blocking bell globals
bool bellActive = false;
unsigned long bellStartTime = 0;
const unsigned long bellDuration = 2000; // 2 seconds
int lastBellMinute = -1; // prevent multiple triggers

// Forward declarations
String loginPage(String msg);
String dashboardPage(String msg);
void ringBell();

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  Wire.begin(4, 5); // SDA=D2, SCL=D1
  if (!rtc.begin()) {
    Serial.println("RTC not found! Check wiring.");
  } else {
    Serial.println("RTC found!");
  }

  WiFi.softAP(ssid, password);
  Serial.println("AP Started");

  // Dashboard
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!request->hasParam("pass")) {
      request->send(200, "text/html", loginPage(""));
      return;
    }
    String pass = request->getParam("pass")->value();
    if(pass != dashboardPassword){
      request->send(200, "text/html", loginPage("Wrong Password!"));
      return;
    }
    request->send(200, "text/html", dashboardPage(""));
  });

  // Manual bell
  server.on("/ring", HTTP_GET, [](AsyncWebServerRequest *request){
    ringBell();
    request->send(200, "text/html", dashboardPage("Bell Ringed Manually"));
  });

  server.begin();
}

void loop() {
  DateTime now = rtc.now();

  // Automatic bell from RTC schedule
  for(int i = 0; i < scheduleCount; i++){
    if(now.hour() == schedule[i].hour &&
       now.minute() == schedule[i].minute &&
       lastBellMinute != now.minute()){
         ringBell();
         lastBellMinute = now.minute(); // prevent multiple triggers in same minute
    }
  }

  // Non-blocking bell stop
  if(bellActive && millis() - bellStartTime >= bellDuration){
    digitalWrite(RELAY_PIN, LOW);
    bellActive = false;
  }

  delay(10); // small delay to yield CPU
}

// Start bell (non-blocking)
void ringBell() {
  digitalWrite(RELAY_PIN, HIGH);
  bellStartTime = millis();
  bellActive = true;
}

// HTML Pages
String loginPage(String msg){
  return "<!DOCTYPE html><html><head><title>Login</title>"
         "<meta name='viewport' content='width=device-width, initial-scale=1'>"
         "<style>body{background:black;color:white;font-family:sans-serif;text-align:center;padding-top:50px}"
         "input{padding:10px;margin:10px;border-radius:5px;border:none}"
         "button{padding:10px;border:none;border-radius:5px;background:#444;color:white;}</style></head>"
         "<body><h2>Dashboard Login</h2>"
         "<p style='color:red'>"+msg+"</p>"
         "<form><input type='password' name='pass' placeholder='Password'><br>"
         "<button type='submit'>Login</button></form></body></html>";
}

String dashboardPage(String msg){
  return "<!DOCTYPE html><html><head><title>Class Bell</title>"
         "<meta name='viewport' content='width=device-width, initial-scale=1'>"
         "<style>body{background:black;color:white;font-family:sans-serif;text-align:center;padding-top:50px}"
         "button{padding:10px;margin:10px;border:none;border-radius:5px;background:#444;color:white;font-size:18px}</style></head>"
         "<body>"
         "<h2>Class Bell Dashboard</h2>"
         "<p>"+msg+"</p>"
         "<button onclick=\"location.href='/ring'\">Ring Bell Now</button>"
         "</body></html>";
}
