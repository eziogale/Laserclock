/*
  NTP-TZ-DST
  NetWork Time Protocol - Time Zone - Daylight Saving Time

  This example shows how to read and set time,
  and how to use NTP (set NTP0_OR_LOCAL1 to 0 below)
  or an external RTC (set NTP0_OR_LOCAL1 to 1 below)

  TZ and DST below have to be manually set
  according to your local settings.

  This example code is in the public domain.
*/

#include <ESP8266WiFi.h>
#include <time.h>                       // time() ctime()
#include <sys/time.h>                   // struct timeval
#include <coredecls.h>                  // settimeofday_cb()

////////////////////////////////////////////////////////

#ifndef STASSID
#define STASSID "wifi-ssid"
#define STAPSK  "wif-pwd"
#endif

#define CIRCLE 0

#define SSID            STASSID
#define SSIDPWD         STAPSK
#define TZ              0       // (utc+) TZ in hours
#define DST_MN          60      // use 60mn for summer time in some countries

#define NTP0_OR_LOCAL1  0       // 0:use NTP  1:fake external RTC
#define RTC_TEST     1510592825 // 1510592825 = Monday 13 November 2017 17:07:05 UTC

////////////////////////////////////////////////////////

#define TZ_MN           ((TZ)*60)
#define TZ_SEC          ((TZ)*3600)
#define DST_SEC         ((DST_MN)*60)

timeval cbtime;			// time set in callback
bool cbtime_set = false;

unsigned long t0, t1, t2, dt, elp, rot_ms;
int x, y, n, rot;
float t, freq, samplerate, ax, ay, angle, handlen;

void time_is_set(void) {
  gettimeofday(&cbtime, NULL);
  cbtime_set = true;
  Serial.println("------------------ settimeofday() was called ------------------");
}

time_t now;

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; } 
  pinMode(14, OUTPUT);
  settimeofday_cb(time_is_set);

#if NTP0_OR_LOCAL1
  // local

  ESP.eraseConfig();
  time_t rtc = RTC_TEST;
  timeval tv = { rtc, 0 };
  timezone tz = { TZ_MN + DST_MN, 0 };
  settimeofday(&tv, &tz);

#else // ntp
  time_t rtc;
  rtc = 1572137990;//Sun Oct 27 02:59:50 2019, 10sec before daylight saving
  timeval tv = { rtc, 0};
  timezone tz = { 0, 0};
  settimeofday(&tv, &tz);
  
  // set up TZ string to use a POSIX/gnu TZ string for local timezone
  // TZ string information:
  // https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset(); // save the TZ variable
  
//  configTime(TZ_SEC, DST_SEC, "pool.ntp.org");
  configTime(0, 0, "pool.ntp.org");

  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, SSIDPWD);
  // don't wait, observe time changing when ntp timestamp is received

#endif // ntp
  pinMode(LED_BUILTIN, OUTPUT);
  dt = 200;//us
  freq = 70;
  rot_ms = 333;
  t0 = 4289967295;

  now = time(nullptr);
  Serial.print(ctime(&now));

  for (int n = 0; n < 1024; n++) {
    analogWrite(LED_BUILTIN,n);
//    Serial.println(n);
    delay(1);
    }
}

// for testing purpose:
extern "C" int clock_gettime(clockid_t unused, struct timespec *tp);

#define PTM(w) \
  Serial.print(":" #w "="); \
  Serial.print(tm->tm_##w);

void printTm(const char* what, const tm* tm) {
  Serial.print(what);
  PTM(isdst); PTM(yday); PTM(wday);
  PTM(year);  PTM(mon);  PTM(mday);
  PTM(hour);  PTM(min);  PTM(sec);
}

timeval tv;
struct timezone tz;
timespec tp;

uint32_t now_ms, now_us;

void loop() {

  gettimeofday(&tv, &tz);
  clock_gettime(0, &tp);
  now = time(nullptr);
  now_ms = millis();
  now_us = micros();

/************* laser *********************/

/* fast loop */
  elp = micros() - t0;
  if (elp > 1000000) elp = 4294967295 - elp;
  if (elp >= dt) {
    t0 = micros();
    t=fmod(t+elp*freq/1000000.0, 1);

#if CIRCLE
    x=(cos(M_PI/2+2*M_PI*t)+1)/2 * 1023;
    y=(sin(M_PI/2+2*M_PI*t)+1)/2 * 1023;
    analogWrite(4,x);//wave x
    analogWrite(5,y);//wave y
    digitalWrite(14,1);// laser on
#else
    ax=cos(M_PI/2+angle*2*M_PI/360);
    ay=sin(M_PI/2+angle*2*M_PI/360);
    x=(ax*sin(3*M_PI/2+2*M_PI*t)+1)/2 * 1023;
    y=(ay*sin(3*M_PI/2+2*M_PI*(t+0.08))+1)/2 * 1023;
    analogWrite(4,x);//wave x
    analogWrite(5,y);//wave y
//    digitalWrite(14,t<0.5);// laser on
    digitalWrite(14,fabs(t-0.25)<handlen/4);// laser on
#endif
  analogWrite(13,1023-x);//wave x
  analogWrite(15,1023-y);//wave y
    
  }
  // slow loop
  if (millis() - t1 > rot_ms) {
    t1=millis();
    rot=(rot+1)%3;
    switch (rot) {
      case 0:
//        angle=360/12*(localtime(&now)->tm_hour % 12);
//        angle=360*(now % 43200)/43200.0;
        angle=360/12*(localtime(&now)->tm_hour % 12 + localtime(&now)->tm_min / 60.0);
        handlen=0.4;
        break;
      case 1:
        angle=360/60*localtime(&now)->tm_min;
        handlen=0.7;
        break;
      case 2:
        angle=360/60*localtime(&now)->tm_sec;
        handlen=1;
/*        Serial.println((now % 43200)*12/43200.0);
        Serial.print(localtime(&now)->tm_hour % 12 + localtime(&now)->tm_min / 60.0);
        Serial.printf("tz_minuteswest: %d, tz_dsttime: %d\n", tz.tz_minuteswest, tz.tz_dsttime);
        Serial.print("\t");*/
        Serial.print(localtime(&now)->tm_hour);
        Serial.print(":");
        Serial.print(localtime(&now)->tm_min);
        Serial.print(":");
        Serial.println(localtime(&now)->tm_sec);

/*        #if 1
        printf("tz_minuteswest: %d, tz_dsttime: %d\n", tz.tz_minuteswest, tz.tz_dsttime);
        printf("gettimeofday() tv.tv_sec : %ld\n", tv.tv_sec);
        printf("time()            time_t : %ld\n", now);
        Serial.println();
        #endif
        
        printf("         ctime: %s", ctime(&now)); // print formated local time
        printf(" local asctime: %s", asctime(localtime(&now))); // print formated local time
        printf("gmtime asctime: %s", asctime(gmtime(&now))); // print formated gm time
        
        // print gmtime and localtime tm members
        printTm("      gmtime", gmtime(&now));
        Serial.println();
        printTm("   localtime", localtime(&now));
        Serial.println();
        
        Serial.print(gmtime(&now)->tm_hour);
        Serial.print(localtime(&now)->tm_min);
        Serial.print(localtime(&now)->tm_sec);
        
        Serial.println();
 */   
        break;
    }
  }

  if (millis() - t2 > 5000) {
    t2=millis();
    digitalWrite(LED_BUILTIN, WiFi.status() != WL_CONNECTED);
  }
  if (millis() - t2 > 10) {
      digitalWrite(LED_BUILTIN, HIGH);
  }

}
