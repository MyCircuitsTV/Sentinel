
/***************************************************************************
 This is a library for the DS3231 Real Time Clock and supports setting and
 reading time, parsing and formatting ASCII representations of date and time
 values, setting alarms and adding a time interval expressed in seconds to
 a current time, or to an alarm value (supports reissuing alarms periodically.)
 
 The library communicates with the DS3231 use I2C via the Arduini Wire lib.
 
 Written by Wayne Holder https://sites.google.com/site/wayneholder/
 BSD license, all text above must be included in any redistribution
 ***************************************************************************/


/* #if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif */
#include "Arduino.h"

#include <Wire.h>
#include <inttypes.h>
#include <DS3231.h>
#ifdef __AVR__
 #include <avr/pgmspace.h>
#elif defined(ESP8266)
 #include <pgmspace.h>
#elif defined(ARDUINO_ARCH_SAMD)
// nothing special needed
#elif defined(ARDUINO_SAM_DUE)
 #define PROGMEM
 #define pgm_read_byte(addr) (*(const unsigned char *)(addr))
 #define Wire Wire1
#endif

static const char sun[] PROGMEM = "Sun";
static const char mon[] PROGMEM = "Mon";
static const char tue[] PROGMEM = "Tue";
static const char wed[] PROGMEM = "Wed";
static const char thr[] PROGMEM = "Thr";
static const char fri[] PROGMEM = "Fri";
static const char sat[] PROGMEM = "Sat";
static const char* const dow[] PROGMEM = {sun, mon, tue,wed, thr, fri, sat};

DS3231::DS3231 () {
}

/*
 * * * * * Public Functions * * * * 8
 */

void DS3231::begin (void) {
  Wire.begin();
  _ctrlReg = _getRegister(0x0E);
}

/*
 *  Parse ASCII reprentation of a time value in either 23 hour format "h:m:s"
 *  or AM/PM format "h:m:sP" (suffix of 'P' or 'A' identifies format) and
 *  convert into BCD values placed into the array 'time'.
 *
 *  Note: leading zeroes are not required on h,m or s values, but are allowed.
 *  Also: time[] array must be of length 7.
 */
boolean DS3231::parseTime (char* src, uint8_t* time) {
  return _parseVals(src, time, 3) == 3;
}

/*
 *  Parse ASCII reprentation of a date and time value in either 23 hour format or
 *  AM/PM format (see parseTime() function) and convert into BCD values placed into
 *  the array 'time'.  Note: time array ahould be of length 7.
 *
 *  Uses Zeller's Congruence to compute and set the 'day of week' value in time[].
 *
 *  Input format: "m/d/y h:m:s", "m/d/y hh:mm:ssA" (AM) or "m/d/y hh:mm:ssP" (PM)
 *  Note: leading zeroes are not required on m,d,y,h,m or s values, but are allowed.
 */
boolean DS3231::parseDateTime (char* src, uint8_t* time) {
  uint8_t len = _parseVals(src, time, 6);
  time[3] = _dayOfWeek(_bcdToDec(time[5]), _bcdToDec(time[4]), _bcdToDec(time[6]));
  return len == 6;
}

/*
 *  Set current time copying values in time[] array to DS3231's registers
 *  Note: time[] array must be of length 7.
 */
void DS3231::setDateTime (uint8_t* time) {
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0);
  for (uint8_t ii = 0; ii < 7; ii++) {
    Wire.write(time[ii]);
  }
  Wire.endTransmission();
}

/*
 *  If 'amPm' is true and current format is 24 hour, change value in time[]
 *  array to AM/PM format else, if false and current format is AM/PM, change to
 *  24 hour format.  Does nothing if value in time[] is aready requested format.
 *  The hours value in time[] is adjusted to convert to new mode.
 *
 *  Note: time[] array must be of length 7.
 */
void DS3231::setTimeMode (uint8_t* time, boolean amPm) {
  // Do nothing if correct mode already set
  if (amPm != (time[2] & 0x40)) {
    if (amPm) {
      // Convert hours from 24 hour to AM/PM format
      uint8_t hours24 = _bcdToDec(time[2]);
      uint8_t hoursAmPm = hours24 >= 12 ? hours24 - 12 : hours24;
      time[2] = _decToBcd(hoursAmPm == 0 ? 12 : hoursAmPm) | (hours24 >= 12 ? 0x20 : 0) | 0x40;
    } else {
      // Convert hours from AM/PM to 24 hour format
      uint8_t hoursDec = _bcdToDec(time[2] & 0x1F);
      time[2] = time[2] & 0x20 ? _decToBcd((hoursDec % 12) + 12) : _decToBcd(hoursDec % 12);
    }
  }
}

/*
 *  Get current time by reading DS3231's registers into time[] array
 *  Note: time[] array must be of length 7.
 */
void DS3231::getDateTime (uint8_t* time) {
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0);
  Wire.endTransmission();
  Wire.requestFrom(DS3231_I2C_ADDRESS, (uint8_t) 7);
  for (uint8_t ii = 0; ii < 7; ii++) {
    *time++ = Wire.read() & 0x7F;
  }
}

/*
 *  Convert time value in time[] array into ASCII string copied into dst[] array.
 *  Format of ACSII output is either "hh:mm:ss PM" or "hh:mm:ss" depending on how
 *  time was originally set via parseTime() or parseDateTime().  All values have a
 *  leading zero to maintain a fixed length format.  Returns pointer to dst[] array
 *  passed a parameter to support chain calls.
 *  
 *  Note: time[] array must be of length 7 and dst[] array must support expected
 *  output format (either length 9 (24 hour), or length 12 (AM/PM)
 */
char* DS3231::timeToString (uint8_t* time, char* dst) {
  if (time[2] & 0x40) {
    _bcdToDecmialString(time[2] & 0x1F, &dst[0]);  // hour (1-12)
  } else {
    _bcdToDecmialString(time[2] & 0x3F, &dst[0]);  // hour (0-23)
  }
  dst[2] = ':';
  _bcdToDecmialString(time[1], &dst[3]);           // minutes
  dst[5] = ':';
  _bcdToDecmialString(time[0], &dst[6]);           // seconds
  if (time[2] & 0x40) {
    dst[8] = ' ';
    dst[9] = time[2] & 0x20 ? 'P' : 'A';
    dst[10] = 'M';
    dst[11] = 0;
  } else {
    dst[8] = 0;
  }
  return dst;
}

/*
 *  Convert date value in time[] array into ASCII string copied into dst[] array.
 *  ACSII output is "mm/dd/yy" with leading zeroes for a fixed length format.
 *  Returns pointer to dst array passed a parameter to support chain calls.
 *  
 *  Note: time[] array must be of length 7 and dst[] array must be at least 9.
 */
char* DS3231::dateToString (uint8_t* time, char* dst) {
  _bcdToDecmialString(time[5], &dst[0]);
  dst[2] = '/';
  _bcdToDecmialString(time[4], &dst[3]);
  dst[5] = '/';
  _bcdToDecmialString(time[6], &dst[6]); //Changed by JF... 2018
  dst[8] = 0;
  return dst;
}

/*
 *  Convert the day of week value in time[] array into ASCII string copied into dst[] array.
 *  ACSII output is either "Sun", "Mon", "Tue", "Wed", "Thr", "Fri" or "Sat".
 *  Returns pointer to dst[] array passed a parameter to support chain calls.
 *  
 *  Note: time[] array must be of length 7 and dst[] array must be at least 4.
 */
char* DS3231::dayOfWeekToString (uint8_t* time, char* dst) {
  strcpy_P(dst, (char*) pgm_read_word(&(dow[time[3] - 1])));
  return dst;
}

/*
 *  Convert time value in time[] array into an uint32_t (long) value that represents
 *  the number of seconds since midnight (0 - 86399).  Used with addTime() function
 *  to compute a future time.
 */
uint32_t DS3231::timeToSeconds (uint8_t* time) {
  uint32_t val;
  if (time[2] & 0x40) {
    val = _bcdToDec(time[2] & 0x1F) % 12;
    if (time[2] & 0x20) {
      val += 12;
    }
  } else {
    val = _bcdToDec(time[2] & 0x3F);
  }
  val *= 3600;
  val += (long) _bcdToDec(time[1]) * 60;
  val += (long) _bcdToDec(time[0]);
  return val;
}

/*
 *  Add the number of seconds to the time value in the time[] array.  Used
 *  to reissue an alarm by computing a new time advanced from the current time.
 *
 *  Returns 'true' if addition caused wrap around to new day.
 *
 *  Note: time[] array must be of length 7.
 */
boolean DS3231::addTime (uint8_t* time, uint32_t seconds) {
  seconds += timeToSeconds(time);
  boolean wrap = seconds >= 86400;
  if (wrap) {
    seconds = seconds % 86400;
  }
  time[0] = _decToBcd((uint8_t) (seconds % 60));
  time[1] = _decToBcd((uint8_t) ((seconds % 3600) / 60));
  uint8_t hours =  (uint8_t) (seconds / 3600);
  if (time[2] & 0x40) {
    if (hours >= 12) {
      hours -= 12;
      time[2] = _decToBcd(hours == 0 ? 12 : hours) | 0x60;
    } else {
      time[2] = _decToBcd(hours == 0 ? 12 : hours) | 0x40;
    }
  } else {
    time[2] = _decToBcd(hours);
  }
  return wrap;
}

/*
 *  Set the alarm selected via the 'type' value (see defined values in DS3231.h)
 *  See example program to see how this is used.
 *
 *  Important: the time format (24 hr, AM/PM) used to define the Alarm must match
 *  the time format of the DS3231's clock or the Alarm will not trip.
 */
void DS3231::setAlarm (uint8_t* time, uint8_t type) {
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  boolean alarm1 = (type & 0x80) == 0;
  Wire.write(alarm1 ? 0x07 : 0x0B);
  if (alarm1) {
    Wire.write(time[0] | ((type & 1) << 7));          // secondss
  }
  Wire.write(time[1] | ((type & 2) << 6));            // minutes
  Wire.write(time[2] | ((type & 4) << 5));            // hours
  if (type & 0x10) {
    Wire.write(time[3] | (((type & 8) << 4) | 0x40)); // day of week
  } else {
    Wire.write(time[4] | ((type & 8) << 4));          // date
  }
  Wire.endTransmission();
}

/*
 *  Enable or disable alarm defined by 'alarm" value (1 = alarm 1, 2 = alarm 2,
 *  3 = both alarms 1 & 2)
 */
void DS3231::enableAlarmInt (uint8_t alarm, boolean enable) {
  uint8_t mask = alarm & 0x03;
  _setRegister(0x0E, _ctrlReg = (enable ? (_ctrlReg | mask) : (_ctrlReg & ~mask)) | 0x04);
}

/*
 *  Clear internal alarm status and IRQ signal for both alarms (if set)
 */
void DS3231::clearAlarms (void) {
  _setRegister(0x0F, _getRegister(0x0F) & 0xFC);
}

/*
 *  Returns value indicating which alarms are active (1 = alarm1, 2 = alarm2,
 *  3 = both alarms)
 */
uint8_t DS3231::getAlarms (void) {
  return _getRegister(0x0F) & _ctrlReg & 0x03;
}

/*
 *  Retreive a previously set alarm time and copy into the time[] array.  Copy
 *  alarm1 value if 'alarm' == 1, or alarm2 if 'alarm' == 2;
 *
 *  Note: time[] array must be of length 7.
 */
void DS3231::getAlarmTime (uint8_t alarm, uint8_t* time) { //Chanded from void to uint8_t* for ESP32
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  boolean alarm1 = alarm == 1;
  Wire.write(alarm1 ? 0x07 : 0x0B);
  Wire.endTransmission();
  uint8_t idx = 0;
  Wire.requestFrom(DS3231_I2C_ADDRESS, (uint8_t) (alarm1 ? 3 : 2));
  time[idx++] = alarm1 ? Wire.read() : 0;
  time[idx++] = Wire.read();
  time[idx++] = Wire.read();
}

/*
 *  Enable or disable the DS3231's 32 kHz output pin
 */
void DS3231::enable32kHzPin (boolean enable) {
  uint8_t sReg = _getRegister(0x0F);
  _setRegister(0x0F, (enable ? sReg | 0x80 : sReg & ~0x80));
}

/*
 * * * * * Private Functions * * * * 8
 */

/*
 *  Parse 'len" groups of 2 digit decimal values delimited by any non numeric
 *  character and copy these into slots 5, 5. 6. 2, 1 and 0 of the time[] array.
 *  Returns number of groups read, which can be less than 'len' if the end of the
 *  src[] string is encountered first.
 */
uint8_t DS3231::_parseVals (char* src, uint8_t* time, uint8_t len) {
  uint8_t regOrder[] = {5, 4, 6, 2, 1, 0};
  uint8_t tmp = 0, sx = 0, dx = 0;
  char cc;
  do {
    cc = src[sx++];
    if (cc >= '0' && cc <= '9') {
      tmp = (tmp << 4) + (cc - '0');
    } else {
      time[regOrder[(6 - len) + dx++]] = tmp;
      if (dx == len && (cc == 'A' || cc == 'P')) {
        time[2] |= (cc == 'P') ? 0x60 : 0x40;
      }
      tmp = 0;
    }
  } while (cc != 0 && dx < len);
  return dx;
}

/*
 *  Use Zeller's Congruence to calculate day of week from month, day, year values
 *  Note: hard coded to only work for years 2000-2099.
 */
uint8_t DS3231::_dayOfWeek (uint8_t mm, uint8_t dd, uint8_t yy) {
  uint8_t cen = 20;
  return (uint8_t) (((int) dd + 13 * (mm + 1) / 5 + yy + yy / 4 + cen / 4 + 5 * cen) % 7 + 1);
}

/*
 *  Read the current value of register 'reg' from the DS3231
 */
uint8_t DS3231::_getRegister (uint8_t reg) {
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(reg);
  Wire.endTransmission();
  Wire.requestFrom(DS3231_I2C_ADDRESS, (uint8_t) 7);
  return Wire.read();
}

/*
 *  Write the value 'val' into register 'reg' in the DS3231.
 */
void DS3231::_setRegister (uint8_t reg, uint8_t val) {
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

uint8_t DS3231::_bcdToDec (uint8_t val) {
  return (((val >> 4) * 10) + (val & 0x0F));
}

uint8_t DS3231::_decToBcd (uint8_t val) {
  return (((val / 10) << 4) + (val % 10));
}

void DS3231::_bcdToDecmialString (uint8_t val, char* dst) {
  dst[0] = (val >> 4) + '0';
  dst[1] = (val & 0x0F) + '0';
}


