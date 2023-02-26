#include <DS3231.h>

DS3231  rtc;
byte time[7];   // Temp buffer used to hold BCD time/date values
char buf[12];   // Buffer used to convert time[] values to ASCII strings

void setup () {
  Serial.begin(115200);
  Serial.println();
  rtc.begin();
  // Set DS3231 to 5/16/17 11:59:55 PM using AM/PM format
  if (rtc.parseDateTime("5/16/17 11:59:55P", time)) {
    rtc.setDateTime(time);
    Serial.print(F("DS3231 set to: "));
    Serial.println(rtc.timeToString(time, buf));
  } else {
    Serial.println(F("Unable to parse time"));
  }
  rtc.clearAlarms();
  // Set Alarm 1 to trigger at 12:00:05 AM
  rtc.parseTime("12:00:05A", time);
  Serial.print(F("Alarm1 set to: "));
  Serial.println(rtc.timeToString(time, buf));
  rtc.setAlarm(time, ALARM1_HOUR_MIN_SEC_MATCH);
  // Set Alarm 2 to trigger every minute
  Serial.println(F("Alarm2 set to: Once a Minute"));
  rtc.setAlarm(time, ALARM2_EVERY_MINUTE);
  // Enable both Alarms 1 & 2
  rtc.enableAlarmInt(3, true);
}

void loop () {
  rtc.getDateTime(time);
  Serial.print(rtc.dayOfWeekToString(time, buf));
  Serial.print(" - ");
  Serial.print(rtc.dateToString(time, buf));
  Serial.print(" ");
  Serial.print(rtc.timeToString(time, buf));
#if 0
  Serial.print(" : ");
  Serial.print(rtc.timeToSeconds(time), DEC);
#endif
  byte alarms = rtc.getAlarms();
  if (alarms != 0) {
    rtc.clearAlarms();
    if (alarms & 1) {
      Serial.print(F(" - Alarm1"));
      // Set alarm 1 to trigger again in 5 seconds
      rtc.getAlarmTime(1, time);
      rtc.addTime(time, 5);
      Serial.print(F(" - Alarm1 advanced to: "));
      Serial.print(rtc.timeToString(time, buf));
      rtc.setAlarm(time, ALARM1_HOUR_MIN_SEC_MATCH);
    }
    if (alarms & 2) {
      Serial.print(F(" - Alarm2"));
    }
  }
  Serial.println();
  delay(1000);
}
