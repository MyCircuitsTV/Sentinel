
/***************************************************************************
 This is a library for the DS3231 Real Time Clock and supports setting and
 reading time, parsing and formatting ASCII representations of date and time
 values, setting alarms and adding a time interval expressed in seconds to
 a current time, or to an alarm value (supports reissuing alarms periodically.)
 
 The library communicates with the DS3231 use I2C via the Arduini Wire lib.
 
 Written by Wayne Holder https://sites.google.com/site/wayneholder/
 BSD license, all text above must be included in any redistribution
 ***************************************************************************/

#ifndef DS3231_h
#define DS3231_h

/* #if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif */

#include "Arduino.h"

#include <Wire.h>
#include <inttypes.h>

/*
 *   DS3231 Register Order
 *    0x00 = seconds
 *    0x01 = minutes
 *    0x02 = hour
 *    0x03 = day of week (not used)
 *    0x04 = day of month (1-31)
 *    0x05 = month
 *    0x06 = year
 *    
 *    0x07 = alarm 1 seconds
 *    0x08 = alarm 1 minutes
 *    0x09 = alarm 1 hour
 *    
 *    0x0A = alarm 1 day (1-7) and day of month (1-31)
 *    0x0B = alarm 2 minutes
 *    0x0C = alarm 2 hour
 *    0x0D = alarm 2 day (1-7) and day of month (1-31)
 */

#define DS3231_I2C_ADDRESS   ((uint8_t) 0x68)

#define ALARM1_EVERY_SECOND             0x0F
#define ALARM1_SEC_MATCH                0x0E
#define ALARM1_MIN_SEC_MATCH            0x0C
#define ALARM1_HOUR_MIN_SEC_MATCH       0x08
#define ALARM1_DATE_HOUR_MIN_SEC_MATCH  0x00
#define ALARM1_DAY_HOUR_MIN_SEC_MATCH   0x10

#define ALARM2_EVERY_MINUTE             0x8E
#define ALARM2_MIN_MATCH                0x8C
#define ALARM2_HOUR_MIN_MATCH           0x88
#define ALARM2_DATE_HOUR_MIN_MATCH      0x80
#define ALARM2_DAY_HOUR_MIN_MATCH       0x90

class DS3231 {
public:
  DS3231 (void);
  void begin (void);
  void enableAlarmInt (uint8_t alarm, boolean enable);
  void setAlarm (uint8_t* time, uint8_t type);
  void getAlarmTime (uint8_t alarm, uint8_t* time);//Chanded from void to uint8_t* for ESP32
  void clearAlarms (void);
  uint8_t getAlarms (void);
  boolean parseTime (char* src, uint8_t* time);
  boolean parseDateTime (char* src, uint8_t* time);
  void setDateTime (uint8_t* time);
  void getDateTime (uint8_t* time);
  void setTimeMode (uint8_t* time, boolean amPm);
  char* timeToString (uint8_t* time, char* dst);
  char* dateToString (uint8_t* time, char* dst);
  char* dayOfWeekToString (uint8_t* time, char* dst);
  uint32_t timeToSeconds (uint8_t* time);
  boolean addTime (uint8_t* time, uint32_t seconds);
  void enable32kHzPin (boolean enable);
private:
  uint8_t _ctrlReg;
  uint8_t _bcdToDec (uint8_t val);
  uint8_t _decToBcd (uint8_t val);
  uint8_t _parseVals (char* time, uint8_t* vals, uint8_t len);
  uint8_t _dayOfWeek (uint8_t mm, uint8_t dd, uint8_t yy);
  uint8_t _getRegister (uint8_t reg);
  void _setRegister (uint8_t reg, uint8_t val);
  void _bcdToDecmialString (uint8_t val, char* dst);
};

#endif
