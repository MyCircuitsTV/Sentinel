/*  Download/Upload files to your ESP32 + SD datalogger/sensor using wifi
    
 By My Circuits 2022, based on code of David Bird 2018 
   
 Requiered libraries:
 
 ESP32WebServer - https://github.com/Pedroalbuquerque/ESP32WebServer download and place in your Libraries folder
 
 Modified DS3231 - Attached to the code DS3231_max

 Sensor libraries - Downloable from adafruit - Depends on the sensor used
 
 Information from David Birds original code - not from My Circuits contribution (with my contribution/simplification to the code do whatever you wish, just mention us!):
  
 This software, the ideas and concepts is Copyright (c) David Bird 2018. All rights to this software are reserved.
 
 Any redistribution or reproduction of any part or all of the contents in any form is prohibited other than the following:
 1. You may print or download to a local hard disk extracts for your personal and non-commercial use only.
 2. You may copy the content to individual third parties for their personal use, but only if you acknowledge the author David Bird as the source of the material.
 3. You may not, except with my express written permission, distribute or commercially exploit the content.
 4. You may not transmit it or store it in any other website or other form of electronic retrieval system for commercial purposes.

 The above copyright ('as annotated') notice and this permission notice shall be included in all copies or substantial portions of the Software and where the
 software use is visible to an end-user.
 
 THE SOFTWARE IS PROVIDED "AS IS" FOR PRIVATE USE ONLY, IT IS NOT FOR COMMERCIAL USE IN WHOLE OR PART OR CONCEPT. FOR PERSONAL USE IT IS SUPPLIED WITHOUT WARRANTY 
 OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 IN NO EVENT SHALL THE AUTHOR OR COPYRIGHT HOLDER BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 See more at http://www.dsbird.org.uk
 
*/

#include <WiFi.h>              //Built-in
#include <ESP32WebServer.h>    //https://github.com/Pedroalbuquerque/ESP32WebServer download and place in your Libraries folder
#include <ESPmDNS.h>

#include "CSS.h" //Includes headers of the web and de style file
#include <SD.h> 
#include <SPI.h>

/* RTC AUTOPOWER */
#include <DS3231.h> //RTC library: DS3231_max, corrected and ESP32 complatible version of https://sites.google.com/site/wayneholder/low-power-techniques-for-arduino

/* LIBRARIES SENSORS */
#include <Adafruit_Sensor.h> //Sensors Adafruit
#include <Adafruit_AHTX0.h> //Humidity and Temperature
#include <Adafruit_BMP280.h> //Pressure and Temperature

/* SERVER */
ESP32WebServer server(80);

/* VARIABLES */
#define servername "MCserver" //Define the name to your server... 
#define SD_pin 5 //SD pin - sentinel

bool   SD_present = false; //Controls if the SD card is present or not

#define on_pin 13 //When HIGH Sentinel will be ON
#define w_mode 14 //Work Mode: logger or webserver - HIGH when Power is triggered with onborad switch
#define VBATPIN 12 //Battery voltage - analogRead(VBATPIN)*6.6/(1024.00*3.75);

String sensorName = "MC001"; //Sensor name

String dataString = ""; //For saving data

String fileName = "DATALOG.TXT"; //File name to save the data 

int sleepTime = 30; //Default sleep time (s), we will read the one in the configuration file..

/* Sensors */
//Adafruit_AHTX0 aht; //Humidity sensor
//Adafruit_BMP280 bmp; //Pressure sensor

DS3231  rtc;
byte time_1[7];   // Temp buffer used to hold BCD time/date values
char buf[12];   // Buffer used to convert time[] values to ASCII strings

/*********  SETUP  **********/

void setup(void)
{  
  Serial.begin(115200);
  
  pinMode(on_pin, OUTPUT); //On Pin
  pinMode(w_mode, INPUT); //working mode Pin
  
  analogReadResolution(12);  
  
  //SD
  Serial.print(F("Initializing SD card..."));
  
  //see if the card is present and can be initialised.
  //Note: Using the ESP32 and SD_Card readers requires a 1K to 4K7 pull-up to 3v3 on the MISO line, otherwise may not work
  //Sentinel already includes it
  if (!SD.begin(SD_pin))
  { 
    Serial.println(F("Card failed or not present, no SD Card data logging possible..."));
    SD_present = false; 
  } 
  else
  {
    Serial.println(F("Card initialised... file access enabled..."));
    SD_present = true; 
  } 

  digitalWrite(on_pin, HIGH); //it just need to be triggered

  //RTC - It can be checked
  rtc.begin(); //RTC start
  rtc.clearAlarms(); // when on pin is on the alarms can be cleared
  //rtc.enableAlarmInt(3, false);

  //SENSORS
//  aht.begin(); //Humidity
//
//  bmp.begin(0x77,0x58); //Pressure, I2C scanner to check adresses...
//  //Default settings from datasheet:
//  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
//                  Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
//                  Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
//                  Adafruit_BMP280::FILTER_X16,      /* Filtering. */
//                  Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */

  Serial.println("Init Finish");

  //If w_mode pin (WEB_pin) HIGH - triggers webserver
  if (digitalRead(w_mode)==1)
  {
    digitalWrite(on_pin, HIGH); //it just need to be triggered

    //Server    
    WiFi.softAP("MyCircuits", "12345678"); //Network and password for the access point genereted by ESP32
    
    //Set your preferred server name, if you use "mcserver" the address would be http://mcserver.local/
    if (!MDNS.begin(servername)) 
    {          
      Serial.println(F("Error setting up MDNS responder!")); 
      ESP.restart(); 
    } 
  
    /*********  Server Commands  **********/
    server.on("/",         SD_dir);
    server.on("/upload",   File_Upload);
    server.on("/fupload",  HTTP_POST,[](){ server.send(200);}, handleFileUpload);
  
    server.begin();
    
    Serial.println("HTTP server started");
  }
}

/*********  LOOP  **********/

void loop(void)
{
  //Server
  while(digitalRead(w_mode)==1)//While WEB_pin HIGH
  {
    server.handleClient(); //Listen for client connections
    Serial.println(digitalRead(w_mode));
  }

  //When magnet is removed or not present
  //1. Read configuration File, TIP: we are only including the sampling time but we can add as many configuration variables as we need
  File dataF = SD.open("/CONFIG.TXT");
  String row = dataF.readStringUntil('\n');
  
  row.replace("Sampling rate (sec): ","");
  row.replace("\"","");
  row.replace("\n","");

  sleepTime = row.toInt();
  Serial.println(sleepTime);//just for debuging
  dataF.close();

  //2. We get data from sensor
  //Humidity:
//  sensors_event_t humid, temp; //Adafruit sensors library
//  aht.getEvent(&humid, &temp);//Populate temp and humidity objects with fresh data

  //Pressure:
//  float pres = bmp.readPressure();

  //Time:
  rtc.getDateTime(time_1);

  dataString = "";
  dataString += rtc.dateToString(time_1, buf);
  dataString += ",";
  dataString += rtc.timeToString(time_1, buf);
  dataString += ",";
  dataString += analogRead(VBATPIN)*6.6/(1024.00*3.75);
//  dataString += ",";
//  dataString += sensorName;
//  dataString += ",";
//  dataString += temp.temperature; //Celcius
//  dataString += ",";
//  dataString += humid.relative_humidity;//%
//  dataString += ",";
//  dataString += pres; //Pa

  Serial.println(dataString); //just for debuging

  //3. We save data into a file, TIP: it is possible to generate a new file automatically every time we open the webserver, 
  //and to add a row with column names when we generate the new file
  dataF = SD.open("/DATALOG.TXT", FILE_APPEND);
  dataF.println(dataString);
  dataF.close();
  
  //4. We set the new alarm - This can be easily improved (e.g., including update time in the server, consideirng previous alarm time...)
  rtc.addTime(time_1, sleepTime); //Add sleepTime (s) to current time
  
  //rtc.clearAlarms(); //Clear alarms
  rtc.setAlarm(time_1, ALARM1_HOUR_MIN_SEC_MATCH);//Type of alarm stablished (check library for definitions!)
  rtc.enableAlarmInt(1, true); //Activation of the alarm 1 
  //rtc.clearAlarms(); //Clear alarms - Why can control the ON/OFF just with the alarm. When active ON, when clear OFF
  
  digitalWrite(on_pin, LOW); //it just need to be triggered
  
  delay(200);
  
}

/*********  FUNCTIONS  **********/

//Download a file from the SD, it is called in void SD_dir()
void SD_file_download(String filename)
{
  if (SD_present) 
  { 
    File download = SD.open("/"+filename);
    if (download) 
    {
      server.sendHeader("Content-Type", "text/text");
      server.sendHeader("Content-Disposition", "attachment; filename="+filename);
      server.sendHeader("Connection", "close");
      server.streamFile(download, "application/octet-stream");
      download.close();
    } else ReportFileNotPresent("download"); 
  } else ReportSDNotPresent();
}
//Upload a file to the SD
void File_Upload()
{
  append_page_header();
  webpage += F("<h3>Select File to Upload</h3>"); 
  webpage += F("<FORM action='/fupload' method='post' enctype='multipart/form-data'>");
  webpage += F("<input class='buttons' style='width:25%' type='file' name='fupload' id = 'fupload' value=''>");
  webpage += F("<button class='buttons' style='width:10%' type='submit'>Upload File</button><br><br>");
  webpage += F("<a href='/'>[Back]</a><br><br>");
  append_page_footer();
  server.send(200, "text/html",webpage);
}

//Handles the file upload a file to the SD
File UploadFile;
//Upload a new file to the Filing system
void handleFileUpload()
{ 
  HTTPUpload& uploadfile = server.upload(); //See https://github.com/esp8266/Arduino/tree/master/libraries/ESP8266WebServer/srcv
                                            //For further information on 'status' structure, there are other reasons such as a failed transfer that could be used
  if(uploadfile.status == UPLOAD_FILE_START)
  {
    String filename = uploadfile.filename;
    if(!filename.startsWith("/")) filename = "/"+filename;
    Serial.print("Upload File Name: "); Serial.println(filename);
    SD.remove(filename);                         //Remove a previous version, otherwise data is appended the file again
    UploadFile = SD.open(filename, FILE_WRITE);  //Open the file for writing in SD (create it, if doesn't exist)
    filename = String();
  }
  else if (uploadfile.status == UPLOAD_FILE_WRITE)
  {
    if(UploadFile) UploadFile.write(uploadfile.buf, uploadfile.currentSize); // Write the received bytes to the file
  } 
  else if (uploadfile.status == UPLOAD_FILE_END)
  {
    if(UploadFile)          //If the file was successfully created
    {                                    
      UploadFile.close();   //Close the file again
      Serial.print("Upload Size: "); Serial.println(uploadfile.totalSize);
      webpage = "";
      append_page_header();
      webpage += F("<h3>File was successfully uploaded</h3>"); 
      webpage += F("<h2>Uploaded File Name: "); webpage += uploadfile.filename+"</h2>";
      webpage += F("<h2>File Size: "); webpage += file_size(uploadfile.totalSize) + "</h2><br><br>"; 
      webpage += F("<a href='/'>[Back]</a><br><br>");
      append_page_footer();
      server.send(200,"text/html",webpage);
    } 
    else
    {
      ReportCouldNotCreateFile("upload");
    }
  }
}

//Initial page of the server web, list directory and give you the chance of deleting and uploading
void SD_dir()
{
  if (SD_present) 
  {
    //Action acording to post, dowload or delete, by MC 2022
    if (server.args() > 0 ) //Arguments were received, ignored if there are not arguments
    { 
      Serial.println(server.arg(0));
  
      String Order = server.arg(0);
      Serial.println(Order);
      
      if (Order.indexOf("download_")>=0)
      {
        Order.remove(0,9);
        SD_file_download(Order);
        Serial.println(Order);
      }
  
      if ((server.arg(0)).indexOf("delete_")>=0)
      {
        Order.remove(0,7);
        SD_file_delete(Order);
        Serial.println(Order);
      }
    }

    File root = SD.open("/");
    if (root) {
      root.rewindDirectory();
      SendHTML_Header();    
      webpage += F("<table align='center'>");
      webpage += F("<tr><th>Name/Type</th><th style='width:20%'>Type File/Dir</th><th>File Size</th></tr>");
      printDirectory("/",0);
      webpage += F("</table>");
      SendHTML_Content();
      root.close();
    }
    else 
    {
      SendHTML_Header();
      webpage += F("<h3>No Files Found</h3>");
    }
    append_page_footer();
    SendHTML_Content();
    SendHTML_Stop();   //Stop is needed because no content length was sent
  } else ReportSDNotPresent();
}

//Prints the directory, it is called in void SD_dir() 
void printDirectory(const char * dirname, uint8_t levels)
{
  
  File root = SD.open(dirname);

  if(!root){
    return;
  }
  if(!root.isDirectory()){
    return;
  }
  File file = root.openNextFile();

  int i = 0;
  while(file){
    if (webpage.length() > 1000) {
      SendHTML_Content();
    }
    if(file.isDirectory()){
      webpage += "<tr><td>"+String(file.isDirectory()?"Dir":"File")+"</td><td>"+String(file.name())+"</td><td></td></tr>";
      printDirectory(file.name(), levels-1);
    }
    else
    {
      webpage += "<tr><td>"+String(file.name())+"</td>";
      webpage += "<td>"+String(file.isDirectory()?"Dir":"File")+"</td>";
      int bytes = file.size();
      String fsize = "";
      if (bytes < 1024)                     fsize = String(bytes)+" B";
      else if(bytes < (1024 * 1024))        fsize = String(bytes/1024.0,3)+" KB";
      else if(bytes < (1024 * 1024 * 1024)) fsize = String(bytes/1024.0/1024.0,3)+" MB";
      else                                  fsize = String(bytes/1024.0/1024.0/1024.0,3)+" GB";
      webpage += "<td>"+fsize+"</td>";
      webpage += "<td>";
      webpage += F("<FORM action='/' method='post'>"); 
      webpage += F("<button type='submit' name='download'"); 
      webpage += F("' value='"); webpage +="download_"+String(file.name()); webpage +=F("'>Download</button>");
      webpage += "</td>";
      webpage += "<td>";
      webpage += F("<FORM action='/' method='post'>"); 
      webpage += F("<button type='submit' name='delete'"); 
      webpage += F("' value='"); webpage +="delete_"+String(file.name()); webpage +=F("'>Delete</button>");
      webpage += "</td>";
      webpage += "</tr>";

    }
    file = root.openNextFile();
    i++;
  }
  file.close();

 
}

//Delete a file from the SD, it is called in void SD_dir()
void SD_file_delete(String filename) 
{ 
  if (SD_present) { 
    SendHTML_Header();
    File dataFile = SD.open("/"+filename, FILE_READ); //Now read data from SD Card 
    if (dataFile)
    {
      if (SD.remove("/"+filename)) {
        Serial.println(F("File deleted successfully"));
        webpage += "<h3>File '"+filename+"' has been erased</h3>"; 
        webpage += F("<a href='/'>[Back]</a><br><br>");
      }
      else
      { 
        webpage += F("<h3>File was not deleted - error</h3>");
        webpage += F("<a href='/'>[Back]</a><br><br>");
      }
    } else ReportFileNotPresent("delete");
    append_page_footer(); 
    SendHTML_Content();
    SendHTML_Stop();
  } else ReportSDNotPresent();
} 

//SendHTML_Header
void SendHTML_Header()
{
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate"); 
  server.sendHeader("Pragma", "no-cache"); 
  server.sendHeader("Expires", "-1"); 
  server.setContentLength(CONTENT_LENGTH_UNKNOWN); 
  server.send(200, "text/html", ""); //Empty content inhibits Content-length header so we have to close the socket ourselves. 
  append_page_header();
  server.sendContent(webpage);
  webpage = "";
}

//SendHTML_Content
void SendHTML_Content()
{
  server.sendContent(webpage);
  webpage = "";
}

//SendHTML_Stop
void SendHTML_Stop()
{
  server.sendContent("");
  server.client().stop(); //Stop is needed because no content length was sent
}

/* //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SelectInput(String heading1, String command, String arg_calling_name)
{
  SendHTML_Header();
  webpage += F("<h3>"); webpage += heading1 + "</h3>"; 
  webpage += F("<FORM action='/"); webpage += command + "' method='post'>"; // Must match the calling argument e.g. '/chart' calls '/chart' after selection but with arguments!
  webpage += F("<input type='text' name='"); webpage += arg_calling_name; webpage += F("' value=''><br>");
  webpage += F("<type='submit' name='"); webpage += arg_calling_name; webpage += F("' value=''><br><br>");
  append_page_footer();
  SendHTML_Content();
  SendHTML_Stop();
} */

//ReportSDNotPresent
void ReportSDNotPresent()
{
  SendHTML_Header();
  webpage += F("<h3>No SD Card present</h3>"); 
  webpage += F("<a href='/'>[Back]</a><br><br>");
  append_page_footer();
  SendHTML_Content();
  SendHTML_Stop();
}

//ReportFileNotPresent
void ReportFileNotPresent(String target)
{
  SendHTML_Header();
  webpage += F("<h3>File does not exist</h3>"); 
  webpage += F("<a href='/"); webpage += target + "'>[Back]</a><br><br>";
  append_page_footer();
  SendHTML_Content();
  SendHTML_Stop();
}

//ReportCouldNotCreateFile
void ReportCouldNotCreateFile(String target)
{
  SendHTML_Header();
  webpage += F("<h3>Could Not Create Uploaded File (write-protected?)</h3>"); 
  webpage += F("<a href='/"); webpage += target + "'>[Back]</a><br><br>";
  append_page_footer();
  SendHTML_Content();
  SendHTML_Stop();
}

//File size conversion
String file_size(int bytes)
{
  String fsize = "";
  if (bytes < 1024)                 fsize = String(bytes)+" B";
  else if(bytes < (1024*1024))      fsize = String(bytes/1024.0,3)+" KB";
  else if(bytes < (1024*1024*1024)) fsize = String(bytes/1024.0/1024.0,3)+" MB";
  else                              fsize = String(bytes/1024.0/1024.0/1024.0,3)+" GB";
  return fsize;
}
