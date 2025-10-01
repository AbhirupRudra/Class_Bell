# Smart Classroom Bell 🔔

**Idea:** Vivek Vats | [GitHub]

**Created by:** Abhirup Rudra | [GitHub](https://github.com/AbhirupRudra)

---

## Overview
The **Smart Classroom Bell** is an ESP8266-based automated bell system designed to manage classroom schedules efficiently. It rings automatically according to a weekly schedule, which is fully configurable through a responsive web dashboard.

---

## Features
- 📅 Weekly schedule with different timings for each day  
- 🔘 Manual bell ringing anytime from the dashboard  
- ⏱️ Adjustable bell duration  
- 🕒 Live RTC display with current date and time  
- ✋▶️ Stop/Resume upcoming alarms 30 minutes prior  
- 💾 Persistent settings saved in memory, even after restart  
- ⚡ Non-blocking operation to ensure smooth web access  

---

## Components Required
- ESP8266 module  
- RTC DS3231 module  
- Relay module for the bell  
- Connecting wires and power supply  

---

## Installation
1. Install **ESP8266 Board** in Arduino IDE.  
2. Install libraries:  
   - [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer)  
   - [AsyncTCP](https://github.com/me-no-dev/AsyncTCP)  
   - [RTClib](https://github.com/adafruit/RTClib)  
3. Connect the RTC and relay as per the circuit diagram.  
4. Upload the code to the ESP8266.  

---

## Usage
1. Connect to the ESP8266 WiFi AP (`ClassBell`) and password: `12345678`.  
2. Open the dashboard via browser at `192.168.4.1`.  
3. Login using the password: `bellpass`.  
4. View and edit the weekly schedule in the table.  
5. Ring the bell manually or adjust the bell duration.  
6. Stop or resume upcoming bells 30 min prior to schedule.  
7. Update RTC date and time if needed.  

---

## Notes
- Ensure the RTC module is properly connected; the bell system relies on accurate time.  
- All schedule changes are stored in **EEPROM**, so they persist after restart.  
- The dashboard is mobile-friendly and fully responsive.  

---

## License
This project is open-source and free to use for educational purposes.  

---

Made with ❤️ by **Abhirup Rudra** | Idea by **Vivek Vats**
