#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Wire.h>
#include "RTClib.h"
#include <EEPROM.h>

// Pin Definitions
#define RELAY_PIN 14
#define EEPROM_SIZE 512

// WiFi credentials
const char* ssid = "ClassBell";
const char* password = "12345678";

// Dashboard password
const String dashboardPassword = "bellpass";

RTC_DS3231 rtc;
AsyncWebServer server(80);

// Bell control
bool bellActive = false;
unsigned long bellStartTime = 0;
unsigned long bellDuration = 2000; // default 2 sec
int lastBellMinute = -1;

// Stop/resume upcoming bell
bool stopUpcoming = false;

// Login state
bool loggedIn = false;

// Schedule
struct BellTime { int hour; int minute; };
const int daysOfWeek = 7; // 0=Sunday
BellTime schedule[daysOfWeek][10]; // max 10 bells per day
int scheduleCount[daysOfWeek] = {0};

// ---------------- FORWARD ----------------
String loginPage();
String dashboardPage();
void ringBell();
void saveScheduleToEEPROM();
void loadScheduleFromEEPROM();

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  EEPROM.begin(EEPROM_SIZE);

  Wire.begin(4,5);
  if(!rtc.begin()) Serial.println("RTC not found!");
  else Serial.println("RTC found!");

  // Load schedule from EEPROM
  loadScheduleFromEEPROM();

  WiFi.softAP(ssid,password);
  Serial.println("AP Started");

  // Dashboard page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", loggedIn ? dashboardPage() : loginPage());
  });

  // Login
  server.on("/login", HTTP_POST, [](AsyncWebServerRequest *request){
    if(request->hasParam("pass", true)){
      String pass = request->getParam("pass", true)->value();
      if(pass == dashboardPassword){
        loggedIn = true;
        request->send(200, "text/plain", "success");
        return;
      }
    }
    request->send(200, "text/plain", "fail");
  });

  // Manual ring
  server.on("/ring", HTTP_GET, [](AsyncWebServerRequest *request){
    ringBell();
    request->send(200, "text/plain", "Bell Ringed");
  });

  // Update bell duration
  server.on("/setDuration", HTTP_POST, [](AsyncWebServerRequest *request){
    if(request->hasParam("duration", true)){
      String d = request->getParam("duration", true)->value();
      unsigned long newDuration = d.toInt();
      if(newDuration >= 100) {
        bellDuration = newDuration;
        request->send(200, "text/plain", "Duration Updated");
        return;
      }
    }
    request->send(200, "text/plain", "Invalid Value");
  });

  // Toggle stop/resume upcoming bell
  server.on("/toggleNext", HTTP_GET, [](AsyncWebServerRequest *request){
    stopUpcoming = !stopUpcoming;
    request->send(200, "text/plain", stopUpcoming?"Upcoming Bell Stopped":"Upcoming Bell Resumed");
  });

  // Update schedule
  server.on("/updateSchedule", HTTP_POST, [](AsyncWebServerRequest *request){
    if(request->hasParam("day", true) && request->hasParam("times", true)){
      int d = request->getParam("day", true)->value().toInt();
      String timesStr = request->getParam("times", true)->value();
      int count = 0;
      int h=0, m=0;
      char* cstr = strdup(timesStr.c_str());
      char* token = strtok(cstr, ",");
      while(token && count<10){
        sscanf(token, "%d:%d", &h, &m);
        if(h>=0 && h<=23 && m>=0 && m<=59){
          schedule[d][count].hour = h;
          schedule[d][count].minute = m;
          count++;
        }
        token = strtok(NULL, ",");
      }
      scheduleCount[d] = count;
      free(cstr);
      saveScheduleToEEPROM();
      request->send(200, "text/plain", "Schedule Updated");
      return;
    }
    request->send(200, "text/plain", "Invalid Input");
  });

  // Serve current RTC datetime
  server.on("/datetime", HTTP_GET, [](AsyncWebServerRequest *request){
    DateTime now = rtc.now();
    char buf[50];
    const char* days[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    sprintf(buf,"%s, %02d-%02d-%04d %02d:%02d:%02d",
            days[now.dayOfTheWeek()],
            now.day(), now.month(), now.year(),
            now.hour(), now.minute(), now.second());
    request->send(200, "text/plain", buf);
  });

  // Set RTC time
  server.on("/setRTC", HTTP_POST, [](AsyncWebServerRequest *request){
    if(request->hasParam("year",true) && request->hasParam("month",true) && request->hasParam("day",true) &&
       request->hasParam("hour",true) && request->hasParam("minute",true) && request->hasParam("second",true)){
      int y = request->getParam("year",true)->value().toInt();
      int m = request->getParam("month",true)->value().toInt();
      int d = request->getParam("day",true)->value().toInt();
      int h = request->getParam("hour",true)->value().toInt();
      int min = request->getParam("minute",true)->value().toInt();
      int sec = request->getParam("second",true)->value().toInt();
      rtc.adjust(DateTime(y,m,d,h,min,sec));
      request->send(200,"text/plain","RTC Updated");
      return;
    }
    request->send(200,"text/plain","Invalid Input");
  });

  server.begin();
}

// ---------------- LOOP ----------------
void loop() {
  DateTime now = rtc.now();
  int today = now.dayOfTheWeek();

  // Check schedule for today
  for(int i=0;i<scheduleCount[today];i++){
    int bellHour = schedule[today][i].hour;
    int bellMinute = schedule[today][i].minute;
    int preAlarmMinutes = 30;
    // Stop/resume upcoming bell logic
    if(!stopUpcoming && lastBellMinute != now.minute()){
      if(now.hour()==bellHour && now.minute()==bellMinute){
        ringBell();
        lastBellMinute = now.minute();
      }
    }
  }

  // Non-blocking bell turn-off
  if(bellActive && millis()-bellStartTime>=bellDuration){
    digitalWrite(RELAY_PIN, LOW);
    bellActive=false;
  }

  delay(10);
}

// ---------------- BELL ----------------
void ringBell(){
  digitalWrite(RELAY_PIN,HIGH);
  bellStartTime=millis();
  bellActive=true;
}

// ---------------- EEPROM ----------------
void saveScheduleToEEPROM(){
  int addr = 0;
  for(int d=0; d<7; d++){
    int count = (scheduleCount[d]>10)?10:scheduleCount[d];
    EEPROM.write(addr++, count);
    for(int i=0; i<count; i++){
      EEPROM.write(addr++, schedule[d][i].hour);
      EEPROM.write(addr++, schedule[d][i].minute);
    }
  }
  EEPROM.commit();
}

void loadScheduleFromEEPROM(){
  int addr = 0;
  for(int d=0; d<7; d++){
    int count = EEPROM.read(addr++);
    if(count>10) count=10;
    scheduleCount[d]=count;
    for(int i=0;i<count;i++){
      schedule[d][i].hour = EEPROM.read(addr++);
      schedule[d][i].minute = EEPROM.read(addr++);
      if(schedule[d][i].hour<0 || schedule[d][i].hour>23) schedule[d][i].hour=0;
      if(schedule[d][i].minute<0 || schedule[d][i].minute>59) schedule[d][i].minute=0;
    }
  }
}

// ---------------- HTML ----------------
String loginPage(){
  return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>Login</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body{background:black;color:white;font-family:sans-serif;text-align:center;padding-top:50px}
input{padding:10px;margin:10px;border-radius:5px;border:none}
button{padding:10px;border:none;border-radius:5px;background:#444;color:white;}
</style>
</head>
<body>
<h2>Dashboard Login</h2>
<input type="password" id="pass" placeholder="Password"><br>
<button onclick="login()">Login</button>
<p id="msg" style="color:red;"></p>
<script>
function login(){
  fetch('/login',{method:'POST',body:new URLSearchParams({pass:document.getElementById('pass').value})})
  .then(r=>r.text()).then(res=>{
    if(res=='success'){ location.reload(); }
    else{ document.getElementById('msg').innerText='Wrong Password!'; }
  });
}
</script>
</body>
</html>
)rawliteral";
}

String dashboardPage(){
  // Create HTML table for schedule editing
  String scheduleHTML = "<table border='1' style='margin:auto;color:white;border-collapse:collapse;'>";
  scheduleHTML += "<tr><th>Day</th><th>Bell Times</th><th>Action</th></tr>";
  const char* dayNames[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
  for(int d=0; d<7; d++){
    String timesStr="";
    for(int i=0;i<scheduleCount[d];i++){
      timesStr += String(schedule[d][i].hour)+":"+(schedule[d][i].minute<10?"0":"")+String(schedule[d][i].minute);
      if(i<scheduleCount[d]-1) timesStr+=", ";
    }
    scheduleHTML+="<tr><td>"+String(dayNames[d])+"</td>"
                  "<td><input type='text' id='times"+String(d)+"' value='"+timesStr+"'></td>"
                  "<td><button onclick='updateDay("+String(d)+")'>Update</button></td></tr>";
  }
  scheduleHTML += "</table>";

  return "<!DOCTYPE html><html><head><title>Class Bell</title>"
         "<meta name='viewport' content='width=device-width, initial-scale=1'>"
         "<style>"
         "body{background:black;color:white;font-family:sans-serif;text-align:center;padding-top:20px}"
         "button{padding:10px;margin:10px;border:none;border-radius:5px;background:#444;color:white;font-size:18px}"
         "input{padding:5px;margin:5px;width:120px;}"
         "table, th, td {border:1px solid white; border-collapse:collapse; padding:5px;}"
         "th{background:#333;}"
         "</style></head>"
         "<body>"
         "<h2>Class Bell Dashboard</h2>"
         "<p>Current Date & Time: <span id='currentDateTime'>--:--:--</span></p>"
         "<button onclick='ring()'>Ring Bell Now</button>"
         "<button id='toggleBtn' onclick='toggleNext()'>Stop Upcoming Bell</button><br><br>"
         "<label>Bell Duration (ms):</label>"
         "<input type='number' id='duration' value='"+String(bellDuration)+"'>"
         "<button onclick='setDuration()'>Set</button>"
         "<p id='msg'></p>"

         "<h3>Set RTC Time</h3>"
         "Year:<input type='number' id='year' value='2025'> "
         "Month:<input type='number' id='month' value='10'> "
         "Day:<input type='number' id='day' value='01'><br>"
         "Hour:<input type='number' id='hour' value='00'> "
         "Minute:<input type='number' id='minute' value='05'> "
         "Second:<input type='number' id='second' value='00'> "
         "<button onclick='setRTC()'>Update RTC</button><br><br>"

         "<h3>Weekly Schedule:</h3>"
         +scheduleHTML+

         "<script>"
         "function updateDateTime(){"
         "fetch('/datetime').then(r=>r.text()).then(t=>{document.getElementById('currentDateTime').innerText=t;});"
         "}setInterval(updateDateTime,1000);updateDateTime();"

         "function ring(){ fetch('/ring').then(r=>r.text()).then(res=>{document.getElementById('msg').innerText=res;}); }"
         "function toggleNext(){ fetch('/toggleNext').then(r=>r.text()).then(res=>{document.getElementById('msg').innerText=res;"
         "let btn=document.getElementById('toggleBtn'); btn.innerText = btn.innerText.includes('Stop')?'Resume Upcoming Bell':'Stop Upcoming Bell';}); }"
         "function setDuration(){let val=document.getElementById('duration').value;"
         "fetch('/setDuration',{method:'POST',body:new URLSearchParams({duration:val})}).then(r=>r.text()).then(res=>{document.getElementById('msg').innerText=res;}); }"
         "function setRTC(){let params={year: document.getElementById('year').value,"
         "month: document.getElementById('month').value, day: document.getElementById('day').value,"
         "hour: document.getElementById('hour').value, minute: document.getElementById('minute').value,"
         "second: document.getElementById('second').value};"
         "fetch('/setRTC',{method:'POST',body:new URLSearchParams(params)}).then(r=>r.text()).then(res=>{document.getElementById('msg').innerText=res;}); }"
         "function updateDay(day){"
         "let val=document.getElementById('times'+day).value;"
         "fetch('/updateSchedule',{method:'POST',body:new URLSearchParams({day:day,times:val})})"
         ".then(r=>r.text()).then(res=>{document.getElementById('msg').innerText=res; location.reload();});"
         "}"
         "</script>"
         "</body></html>";
}
