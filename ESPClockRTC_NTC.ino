#include <ESP8266WiFi.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <SPI.h>
#include <RTClib.h>
#include <TimeLib.h>
#include <WiFiUdp.h>
#include <NtpClientLib.h>

extern "C" {
  #include "user_interface.h"
}


/*Defines*/
#define NUMPIXELS 60
#define PIN 14
#define SDA 4
#define SCL 5
#define BUILTINLED 2
#define RESYNCBUTTON 13

//#define WIFI_CONNECTIVITY

//#define DS3231_I2C_ADDRESS 0x68
/*Enumerations*/
enum Modes {INIT, SMILEY, TEST, FULLCLOCK, MINIMALCLOCK, TEMP, READNTP};


/*Global*/
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
RTC_DS3231 rtc;

char ssid[] = "your_ssid";  //  your network SSID (name)
char pass[] = "your_Password";       // your network password

const byte interruptPin = 13;
volatile byte stateOfUpdate = 8;
volatile byte stateOfLight = 0;
int LL = 0;
long lastMillisOfUpdate = 0;
int numOfTrials = 0;
long lastTime = 0;

void onSTAGotIP(WiFiEventStationModeGotIP ipInfo) {
  Serial.printf("Got IP: %s\r\n", ipInfo.ip.toString().c_str());
  NTP.begin("pool.ntp.org", 1, true);
  NTP.setInterval(50);
  digitalWrite(2, HIGH);
}

void onSTADisconnected(WiFiEventStationModeDisconnected event_info) {
  Serial.printf("Disconnected from SSID: %s\n", event_info.ssid.c_str());
  Serial.printf("Reason: %d\n", event_info.reason);
  digitalWrite(2, LOW);
}


void setup() {   
  ESP.wdtDisable();
  pinMode(RESYNCBUTTON, INPUT_PULLUP);
  pinMode(BUILTINLED, OUTPUT);
  digitalWrite(2, LOW);
  static WiFiEventHandler e1, e2;
  Serial.begin(115200);
  while (!Serial) // new trick
    delay(100);  
  delay(200);        
  Wire.begin(SDA,SCL);
  delay(100);  


  NTP.onNTPSyncEvent([](NTPSyncEvent_t ntpEvent) {
    if (ntpEvent) {
      Serial.print("Time Sync error: ");
      if (ntpEvent == noResponse)
        Serial.println("NTP server not reachable");
      else if (ntpEvent == invalidAddress)
        Serial.println("Invalid NTP server address");
    }
    else {
      Serial.print("Got NTP time: ");
      Serial.println(NTP.getTimeDateString(NTP.getLastNTPSync()));
      stateOfUpdate = 4;
    }
  });
  WiFi.onEvent([](WiFiEvent_t e) {
    Serial.printf("Event wifi -----> %d\n", e);
  });
  e1 = WiFi.onStationModeGotIP(onSTAGotIP);// As soon WiFi is connected, start NTP Client
  e2 = WiFi.onStationModeDisconnected(onSTADisconnected);

  
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }
  delay(100);
  Serial.println("Clock init...");
  strip.begin();
  strip.show();
  DisplayToRing(rtc.now(), SMILEY);
   
   if (rtc.lostPower()) {
    Serial.println("RTC lost power, lets set the time!");
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    //UpdateRTCTime();
  }
  delay(1000);
  attachInterrupt(digitalPinToInterrupt(RESYNCBUTTON), handleInterrupt, FALLING);
  ESP.wdtEnable(8000);
}

// Button event triggering the Time Sync process
// Setting the State Machine to the start state
void handleInterrupt() {
  if(stateOfUpdate == 8 || stateOfUpdate == 9){
    stateOfUpdate = 0;
  }
}



void loop() {
// Time update State Machine
  UpdateRTC();

  // Check if Clock Display needs to be updated
  if((millis() - lastTime) > 1000) {
    lastTime = millis();
    if(stateOfUpdate < 8) {
      Serial.print("stateOfUpdate: ");
      Serial.println(stateOfUpdate);
      DisplayToRing(rtc.now(), SMILEY);
    }else {
      LL = analogRead(A0);
  // hysteresis
      if( stateOfLight > 0 && LL < 550 )
        stateOfLight = 0;
      if( stateOfLight == 0 && LL > 600 )
        stateOfLight = 1;
      
      DateTime ttt = rtc.now();
      if(ttt.hour() == 3 && ttt.minute() == 1)
        stateOfUpdate = 0;
      DisplayToRing(ttt, (stateOfLight == 0) ? MINIMALCLOCK : FULLCLOCK);    
    }
  }
  delay(5); // Trial instead of yield();
}

void DisplayToRing(DateTime dt, Modes modes)
{
     ClearRing();
  // INIT, SMILEY, TEST,FULLCLOCK, MINIMALCLOCK, TEMP
    switch(modes)
    {
      case INIT:
        //ClearRing();
        break;
      case SMILEY:
      //:-)
        //ClearRing();
        for(int k= 0; k<NUMPIXELS;k++ )
        {
          if(k==8 || k == 52)
          {
            strip.setPixelColor(k, strip.Color(10,0,0)  ); 
          }
          if (k > 21 && k < 39)
          {
             strip.setPixelColor(k, strip.Color(0,10,0)  ); 
          }
        }
      
      break;
      case TEST:
      /*Ring Test Code needs to be added here*/
      break;
      case FULLCLOCK:
        DrawHours();
      case MINIMALCLOCK:
        int h,m,s;       
        h = (dt.hour() > 11) ? (dt.hour()-12)*5 : dt.hour()*5;
        m = dt.minute();
        s = dt.second();
        strip.setPixelColor(h, strip.Color(10,0,0)  ); 
        strip.setPixelColor(m, strip.Color(0,10,0)  ); 
        strip.setPixelColor(s, strip.Color(0,0,10)  ); 
        break;
      case TEMP:
        break;
      case READNTP:
        break;
      default:
        //Serial.println("Wrong Mode Called..");
        break;
    };
    strip.show();
  }


void ClearRing(){
  for (int i = 0; i< NUMPIXELS; i++){
    strip.setPixelColor(i, strip.Color(0,0,0)  ); 
  }
  return;      
}

//Lets Draw the 12 hours signs with "White" Colour
void DrawHours(){
  for(int i=0; i < 12; i++){
    strip.setPixelColor(i*5, strip.Color(10,10,10)); 
  }
  return;
}

// State Machine of the Time Sync 
void UpdateRTC()
{
   // Starting point - no Wifi, no IP, nothing
   if(stateOfUpdate == 0){
     ESP.wdtDisable();
     WiFi.persistent(false);
     WiFi.mode(WIFI_OFF);
     WiFi.mode(WIFI_STA);
     WiFi.begin(ssid, pass);
     delay(100);
     ESP.wdtEnable(8000);
     Serial.print("Wifi Started");
     numOfTrials = 0;
     if (WiFi.status() != WL_CONNECTED){
        stateOfUpdate = 1;
        lastMillisOfUpdate = millis();
        return;
     }
     else {       
        stateOfUpdate = 2;
        return;
     }
   }
   if ( stateOfUpdate == 1)
   {
     // Failed attempt, wait 1000 [ms]
     if ( (millis() - lastMillisOfUpdate) < 1000) {
        yield();
        return; 
     } else {
        // Time is over, 
        
        //less than 30 trials
        if ((WiFi.status() != WL_CONNECTED) && numOfTrials < 30 ) {
          numOfTrials++;
          Serial.print(".");
          lastMillisOfUpdate = millis();
          yield();
          return;
        }
        //Time is over, 3 trials done
        if ((WiFi.status() != WL_CONNECTED) && numOfTrials == 30 ) {
          numOfTrials = 0;
          stateOfUpdate = 5;
          Serial.print("No connection");
          yield();
          return;
        }
        if (WiFi.status() == WL_CONNECTED) {
          numOfTrials = 0;
          stateOfUpdate = 2;
          yield();
          return;
        }
        // This Path is not possible...
     }
   }
   // Wifi connected and udp port needs to be opened, NTP packet needs to be sent out
   if( stateOfUpdate == 2 ) {
      // Starting text
       if(numOfTrials == 0){
        Serial.println("");
        Serial.println("Connected");
       }
      // the sync is not happened yet

      // Buggy ===> 30 time ask for time 
      if ( numOfTrials < 30 ) {
        // state change will be triggered by the event.
        lastMillisOfUpdate = millis();
       
        stateOfUpdate = 3;
        NTP.getTime();
        numOfTrials++;
        Serial.print("#");
       
        return;
      }
      else {
        if (numOfTrials == 30)
        {
          
          Serial.println("");
          Serial.println("Failed to get NTP....");
          numOfTrials = 0;
          stateOfUpdate = 5; 
          return;    
        }

      stateOfUpdate = 4;
      }
   }
   if( stateOfUpdate == 3)
   {
      if((millis() - lastMillisOfUpdate) > 1000)
      {
        Serial.print(".");
        stateOfUpdate = 2;
        return;
      }
      
      return;
      
    }
   
   if ( stateOfUpdate == 4  || stateOfUpdate == 5 )
   {
      if(stateOfUpdate == 4){
        
      Serial.println("#SyncDone...");
      Serial.println("");
      Serial.println(NTP.getTimeDateString(NTP.getLastNTPSync()));
      rtc.adjust(DateTime(NTP.getDateStr(NTP.getLastNTPSync()).c_str() ,NTP.getTimeStr(NTP.getLastNTPSync()).c_str()));
      }
    
      // NTP.stop();
       
       WiFi.disconnect(); 
       yield();      
       WiFi.mode(WIFI_OFF);
       numOfTrials = 0;
       yield();
      if(stateOfUpdate == 4)
        stateOfUpdate = 8;
      if(stateOfUpdate == 5)
        stateOfUpdate = 9;
     
      // end of the state machine
     
      return;
   }

   // Do nothing.... Waiting for an event....
  if(stateOfUpdate == 8  || stateOfUpdate == 9){
    return;    
  }
  return;
}

