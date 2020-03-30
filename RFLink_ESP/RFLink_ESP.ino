// ****************************************************************************
// Version 2.1
//   - in the source you can select the communication mode RS232 / MQTT
// Version 2.2 By Schmurtz
// Cosmetic & functional changes (but the basics of RFlink-ESP still the same)
//   - added : Wifi Manager (Autoconnect)
//   - added : advanced settings Tab (via Autoconnect library example) : allow to configure MQTT with authentication and many other handful things like NTP...
//   - added : home page that allow to see the last RF events received or sent
//   - added : New way to send RFlink raw commands from MQTT : RFlink/send     payload : {"TRANSMIT":"10;EV1527;005DF;2;ON;"}
//   - added : New Template for MQTT to RF message (the payload is in JSON format and the "CMD" is included in payload)
//   - added : Compatibility with Sonoff RF Bridge with direct Hack (in arduino IDE -> choose "ESP8285", 160Mhz, flash size 1MB + FS 64KB )
//             Due too low memory of the Sonoff RF Bridge, please use a "basic OTA" Sketch and then make an OTA from IDE
//             Direct Hack informations : https://github.com/xoseperez/espurna/wiki/Hardware-Itead-Sonoff-RF-Bridge---Direct-Hack
//   - Correction : some modifications for sending KAKU protocol in "RFL_Protocol_KAKU.h"
//
// ****************************************************************************
#define Version  "2.2"
#define Revision  0x01
#define Build     0x01


#ifdef __AVR_ATmega2560__
#define RS232
#else
#define MQTT
#define RS232
#endif

// ****************************************************************************
// used in Raw signal
//
// original: 20=8 bits. Minimal number of bits*2 that need to have been received
//    before we spend CPU time on decoding the signal.
//
// MAX might be a little to low ??
// ****************************************************************************
#define MIN_RAW_PULSES         26   //20  // =8 bits. Minimal number of bits*2 that need to have been received before we spend CPU time on decoding the signal.
#define MAX_RAW_PULSES        150
// ****************************************************************************
#define MIN_PULSE_LENGTH      25   // Pulses shorter than this value in uSec. will be seen as garbage and not taken as actual pulses.
#define SIGNAL_TIMEOUT         7   // Timeout, after this time in mSec. the RF signal will be considered to have stopped.
#define RAW_BUFFER_SIZE      512   // Maximum number of pulses that is received in one go.
#define INPUT_COMMAND_SIZE    60   // Maximum number of characters that a command via serial can be.


// ****************************************************************************
// Global Variables
// ****************************************************************************
byte          PKSequenceNumber = 0 ;  // 1 byte packet counter

char          PreFix [20]              ;
unsigned long Last_Detection_Time = 0L ;
int           Learning_Mode       = 1  ;  // normally always start in production mode 0 //

char          pbuffer  [ INPUT_COMMAND_SIZE ] ;           // Buffer for printing data
char          pbuffer2 [ 30 ] ;
char          InputBuffer_Serial [ INPUT_COMMAND_SIZE ];   // Buffer for Seriel data

struct RawSignalStruct {                 // Raw signal variabelen places in a struct
  int           Number ;                 // Number of pulses, times two as every pulse has a mark and a space.
  int           Min    ;
  int           Max    ;
  bool          Repeats;
  long          Mean   ;
  unsigned long Time   ;                 // Timestamp indicating when the signal was received (millis())
  int Pulses [ RAW_BUFFER_SIZE + 2 ] ;   // Table with the measured pulses in microseconds divided by RawSignal.Multiply. (halves RAM usage)
  // First pulse is located in element 1. Element 0 is used for special purposes, like signalling the use of a specific plugin
} RawSignal = {0, 0, 0, false, 0L};

//} RawSignal= {0,0,0,0,0L };
//} RawSignal= {0,0,0,0,0,0L};

unsigned long Last_BitStream      = 0L    ;  // holds the bitstream value for some plugins to identify RF repeats
bool          Serial_Command      = false ;
int           SerialInByteCounter = 0     ;  // number of bytes counter
byte          SerialInByte                ;  // incoming character value
String        Unknown_Device_ID   = ""    ;


// History of last events
////////////////////////////////////////////////////////
#include <time.h>;
// these GMT settings are now displayed in "advanced settings" tab
// #define TZ              1       // (utc+) TZ in hours
// #define DST_MN          60      // daylight saving time (DST), use 60mn for summer time in some countries
// #define TZ_MN           ((TZ)*60)
// #define TZ_SEC          ((TZ)*3600)
// #define DST_SEC         ((DST_MN)*60)
////////////////////////////////////////////////////////


////////////////////////////////////////////////////////
// To record and display last events on the home page.
struct RFlinkEvent {
  String           History_date ;
  bool             History_isreceived ;
  String           History_message;
};

static const int History_MaxEvents = 20;    // Please avoid to make a too big value here, the past events will be stored in memory and displayed in the html home
bool History_MaxReached = false;
byte History_CurrentEvent = 0;
RFlinkEvent History_EventList[History_MaxEvents];  // To stock in memory the last x events (20 by default)
////////////////////////////////////////////////////////

// ***********************************************************************************
// Hardware pins
// ***********************************************************************************
#ifdef __AVR_ATmega2560__
#define TRANSMIT_PIN    5    // Data to the 433Mhz transmitter on this pin
#define RECEIVE_PIN    19    // On this input, the 433Mhz-RF signal is received. LOW when no signal.    
#elif ESP32
#define TRANSMIT_PIN    5    // Data to the 433Mhz transmitter on this pin
#define RECEIVE_PIN    19    // On this input, the 433Mhz-RF signal is received. LOW when no signal.
#elif ESP8266
//for Sonoff RF bridge : TRANSMIT_PIN =5 &  RECEIVE_PIN = 4
#define TRANSMIT_PIN    5    // Data to the 433Mhz transmitter on this pin (GPIO 5 = D1 on wemos & Lolin)
#define RECEIVE_PIN    4    // On this input, the 433Mhz-RF signal is received. LOW when no signal. (GPIO 4 = D2 on wemos & Lolin)
#endif


// ***********************************************************************************
// File with the device registrations
// ***********************************************************************************
#ifndef __AVR_ATmega2560__
#include "RFLink_File.h"
_RFLink_File  RFLink_File ; // ( "/RFLink.txt" ) ;


// ***********************************************************************************
// ***********************************************************************************
/*

  unsigned long Send_Time_ms   = 10000 ;

  //#define OneWire_Pin  2   // PIN for OneWire, if necessary must be defined before library Sensor_Receiver_2

  //#include "user_interface.h"      //system_get_sdk_version()
  //#define WIFI_TX_POWER  82
  //#define WIFI_MODE_BGN  PHY_MODE_11G

  //#define CAMPER
  #include "Sensor_Receiver_2.h"
  // */



//#include "Wifi_Settings_ESP32.h"   // replaced by Autoconnect library
#ifdef ESP32
#include <WiFi.h>
#elif ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>     // Replace with WebServer.h for ESP32
#include <AutoConnect.h>
#include <FS.h>  // To save MQTT parameters


// for OTA from IDE (not linked to autoconnect)
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#endif

//for MQTT support
#include <WiFiClient.h>
#include "PubSubClient.h"   // for MQTT support
#include <ArduinoJson.h>   // To decrypt MQTT message received

WiFiClient My_WifiClient ;
PubSubClient MQTT_Client ( My_WifiClient ) ;

// -*-*-*-*-*-*-*-- for AutoConnect --*-*-*-*-*-*-*-

#if defined(ARDUINO_ARCH_ESP8266)
typedef ESP8266WebServer  WiFiWebServer;  // Replace with WebServer for ESP32
#elif defined(ARDUINO_ARCH_ESP32)
typedef WebServer WiFiWebServer;
#endif

AutoConnect  portal;
AutoConnectConfig config;

//  -=-=-=-= Adds advanced tab to Autoconnect  =-=-=-=-
String  Adv_HostName;
char  Adv_NTPserver1[25];
char  Adv_NTPserver2[25];
int GMT;
int DST;

//  -=-=-=-= Adds MQTT tab to Autoconnect  =-=-=-=-
#define PARAM_FILE      "/settings.json"
#define AUX_SETTING_URI "/settings"
#define AUX_SAVE_URI    "/settings_save"
//#define AUX_CLEAR_URI   "/settings_clear"

// String MQTT_ID = "RFLink-ESP" ;   // not used anymore
// #define MQTT_USER_ID  "anyone"    // not used anymore
#define GET_CHIPID()  (ESP.getChipId())
String  serverName;
String  Mqtt_Username;
String  Mqtt_Password;
String  Mqtt_Port;
String  Mqtt_Topic;
// String  apid;                      // not used anymore
//String  hostName;
bool  Mqtt_Enabled;
unsigned long lastPub = 0;

// JSON definition of AutoConnectAux.
// Multiple AutoConnectAux can be defined in the JSON array.
// In this example, JSON is hard-coded to make it easier to understand
// the AutoConnectAux API. In practice, it will be an external content
// which separated from the sketch, as the mqtt_RSSI_FS example shows.
static const char AUX_settings[] PROGMEM = R"raw(
[
  {
    "title": "RFlink-ESP Settings",
    "uri": "/settings",
    "menu": true,
    "element": [
      {
        "name": "style",
        "type": "ACStyle",
        "value": "label+input,label+select{position:sticky;left:180px;width:230px!important;box-sizing:border-box;}"
      },
      {
        "name": "newline0",
        "type": "ACElement",
        "value": "<hr>"
      },
      {
        "name": "header",
        "type": "ACText",
        "value": "<h2>MQTT broker settings</h2>",
        "style": "text-align:center;color:#2f4f4f;padding:10px;"
      },
      {
        "name": "caption",
        "type": "ACText",
        "value": "MQTT Broker settings",
        "style": "font-family:serif;color:#4682b4;"
      },
      {
        "name": "newline1",
        "type": "ACElement",
        "value": "<hr>"
      },
      {
        "name": "Mqtt_Enabled",
        "type": "ACCheckbox",
        "value": "unique",
        "label": "MQTT Enabled",
        "checked": false
      },
      {
        "name": "mqttserver",
        "type": "ACInput",
        "value": "",
        "label": "Server",
        "pattern": "^(([a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9\\-]*[a-zA-Z0-9])\\.)*([A-Za-z0-9]|[A-Za-z0-9][A-Za-z0-9\\-]*[A-Za-z0-9])$",
        "placeholder": "MQTT broker server"
      },
      {
        "name": "Mqtt_Port",
        "type": "ACInput",
        "value": "",
        "label": "port",
        "placeholder": "MQTT port (default 1883)"
      },
      {
        "name": "Mqtt_Username",
        "type": "ACInput",
        "label": "Username",
        "pattern": "^[0-9]{6}$"
      },
      {
        "name": "Mqtt_Password",
        "type": "ACInput",
        "label": "Password"
      },
      {
        "name": "Mqtt_Topic",
        "type": "ACInput",
        "label": "Topic",
        "pattern": "(.*)[^\/]$",
        "placeholder": "example : RFlink"
      },
      {
        "name": "newline2",
        "type": "ACElement",
        "value": "<hr>"
      },
     {
        "name": "style2",
        "type": "ACStyle",
        "value": "label+input,label+select{position:sticky;left:180px;width:230px!important;box-sizing:border-box;}"
      },
      {
        "name": "header2",
        "type": "ACText",
        "value": "<h2>Advanced Settings</h2>",
        "style": "text-align:center;color:#2f4f4f;padding:10px;"
      },
      {
        "name": "caption2",
        "type": "ACText",
        "value": "Other settings",
        "style": "font-family:serif;color:#4682b4;"
      },
      {
        "name": "newline3",
        "type": "ACElement",
        "value": "<hr>"
      },
      {
        "name": "Adv_HostName",
        "type": "ACInput",
        "value": "RFlink-ESP",
        "label": "Hostname",
        "pattern": "^\\d*[a-z][a-z0-9!^(){}\\-_~]*$"
        
      },
      {
        "name": "Adv_NTPserver1",
        "type": "ACInput",
        "value": "",
        "label": "@NTP server 1",
        "pattern": "^(([a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9\\-]*[a-zA-Z0-9])\\.)*([A-Za-z0-9]|[A-Za-z0-9][A-Za-z0-9\\-]*[A-Za-z0-9])$",
        "placeholder": "pool.ntp.org if empty"
      },
      {
        "name": "Adv_NTPserver2",
        "type": "ACInput",
        "value": "",
        "label": "@NTP server 2",
        "pattern": "^(([a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9\\-]*[a-zA-Z0-9])\\.)*([A-Za-z0-9]|[A-Za-z0-9][A-Za-z0-9\\-]*[A-Za-z0-9])$",
        "placeholder": "time.nist.gov if empty"
      },
      {
        "name": "GMT",
        "type": "ACInput",
        "value": "",
        "label": "Timezone (GMT)",
        "pattern": "^[-+]?[1-9]\\d*\\.?[0]*$",
        "placeholder": "between -12 and +14"
      },
      {
        "name": "DST",
        "type": "ACInput",
        "value": "",
        "label": "DST",
        "pattern": "^[-+]?[1-9]\\d*\\.?[0]*$",
        "placeholder": "Daylight Saving Time in minutes"
      },
      {
        "name": "save",
        "type": "ACSubmit",
        "value": "Save",
        "uri": "/settings_save"
      },
      {
        "name": "discard",
        "type": "ACSubmit",
        "value": "Discard",
        "uri": "/"
      }
    ]
  },
  {
    "title": "RFlink-ESP Settings",
    "uri": "/settings_save",
    "menu": false,
    "element": [
      {
        "name": "caption",
        "type": "ACText",
        "value": "<h4>Parameters saved as:</h4>",
        "style": "text-align:center;color:#2f4f4f;padding:10px;"
      },
      {
        "name": "parameters",
        "type": "ACText"
      },
      {
        "name": "clear",
        "type": "ACSubmit",
        "value": "OK",
        "uri": "/settings"
      }
    ]
  }
]
)raw";

//  -=-=-=-= END - Adds MQTT tab to Autoconnect  =-=-=-=-


bool mqttConnect() {   // MQTT connection (documented way from AutoConnect : https://github.com/Hieromon/AutoConnect/tree/master/examples/mqttRSSI_NA)
  // static const char alphanum[] = "0123456789"
  //   "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
  //   "abcdefghijklmnopqrstuvwxyz";  // For random generation of client ID.
  // char    clientId[9];
  // for (uint8_t i = 0; i < 8; i++) {
  //   clientId[i] = alphanum[random(62)];
  // }
  // clientId[8] = '\0';

  uint8_t retry = 3;
  while (!MQTT_Client.connected()) {
    if (serverName.length() <= 0)
      break;
    if (Mqtt_Port == "" ) Mqtt_Port = "1883";   // just in case ....
    MQTT_Client.setServer(serverName.c_str(), Mqtt_Port.toInt());
    Serial.println(String("\nAttempting MQTT broker: ") + serverName);

  //  if (MQTT_Client.connect(clientId, MQTT_USER_ID, Mqtt_Password.c_str())) {
    //We generate a unique name for this device to avoid mqtt problems in case if you have multiple RFlink-ESP devices
      String clientId = Adv_HostName + "-" + String(GET_CHIPID(), HEX);
      if (MQTT_Client.connect(clientId.c_str() , Mqtt_Username.c_str(), Mqtt_Password.c_str())) {
      Serial.print(F("MQTT connection Established with ID : ")); //+ String(clientId));
      Serial.println(clientId);
     // ... and subscribe, send = MQTTtoRF
     MQTT_Client.subscribe ( ( Mqtt_Topic + "/send/#").c_str() ) ;   // 

      return true;
    }
    else {
      Serial.println("Connection mqttserver:" + String(serverName));
      Serial.println("Connection Mqtt_Username:" + String(Mqtt_Username.c_str()));
      Serial.println("Connection Mqtt_Password:" + String(Mqtt_Password.c_str()));
      Serial.println("Connection failed:" + String(MQTT_Client.state()));
      if (!--retry)
        break;
      delay(500);
    }
  }
  return false;
}


void getParams(AutoConnectAux& aux) {

  //////  MQTT  settings //////

  Mqtt_Enabled = aux["Mqtt_Enabled"].as<AutoConnectCheckbox>().checked;
  serverName = aux["mqttserver"].value;
  serverName.trim();
  Mqtt_Port = aux["Mqtt_Port"].value;
  Mqtt_Port.trim();
  Mqtt_Username = aux["Mqtt_Username"].value;
  Mqtt_Username.trim();
  Mqtt_Password = aux["Mqtt_Password"].value;
  Mqtt_Password.trim();
  Mqtt_Topic = aux["Mqtt_Topic"].value;
  Mqtt_Topic.trim();

  ////// advanced settings //////
 
  Adv_HostName = aux["Adv_HostName"].value;
  Adv_HostName.trim();
  String Temp_Adv_NTPserver1= aux["Adv_NTPserver1"].value;
  String Temp_Adv_NTPserver2= aux["Adv_NTPserver2"].value;
  Temp_Adv_NTPserver1.toCharArray(Adv_NTPserver1,25);
  Temp_Adv_NTPserver2.toCharArray(Adv_NTPserver2,25);
  GMT = aux["GMT"].value.toInt();
  DST = aux["DST"].value.toInt();

}

// Load parameters saved with  saveParams from SPIFFS into the
// elements defined in /settings JSON.
String loadParams(AutoConnectAux& aux, PageArgument& args) {
  (void)(args);
  File param = SPIFFS.open(PARAM_FILE, "r");
  if (param) {
    if (aux.loadElement(param)) {
      getParams(aux);
      Serial.println(PARAM_FILE " loaded");
    }
    else
      Serial.println(PARAM_FILE " failed to load");
    param.close();
  }
  else {
    Serial.println(PARAM_FILE " open failed");
#ifdef ARDUINO_ARCH_ESP32
    Serial.println("If you get error as 'SPIFFS: mount failed, -10025', Please modify with 'SPIFFS.begin(true)'.");
#endif
  }
  return String("");
}

// Save the value of each element entered by '/settings' to the
// parameter file. The saveParams as below is a callback function of
// /settings_save. When invoking this handler, the input value of each
// element is already stored in '/settings'.
// In Sketch, you can output to stream its elements specified by name.
String saveParams(AutoConnectAux& aux, PageArgument& args) {
  // The 'where()' function returns the AutoConnectAux that caused
  // the transition to this page.
  AutoConnectAux&   my_settings = *portal.aux(portal.where());
  getParams(my_settings);
 // AutoConnectInput& mqttserver = my_settings["mqttserver"].as<AutoConnectInput>();  //-> BUG
  // The entered value is owned by AutoConnectAux of /settings.
  // To retrieve the elements of /settings, it is necessary to get
  // the AutoConnectAux object of /settings.
  File param = SPIFFS.open(PARAM_FILE, "w");
  my_settings.saveElement(param, { "mqttserver", "Mqtt_Port", "Mqtt_Username", "Mqtt_Password", "Mqtt_Topic", 
  "Mqtt_Enabled", "period", "Adv_HostName" , "Adv_NTPserver1" ,"Adv_NTPserver2", "GMT" , "DST" });
  param.close();
  
  // Echo back saved parameters to AutoConnectAux page.
  AutoConnectText&  echo = aux["parameters"].as<AutoConnectText>();
  echo.value += "MQTT Enabled: " + String(Mqtt_Enabled == true ? "true" : "false") + "<br>";
  echo.value = "Server: " + serverName;
  echo.value = " Port: " + Mqtt_Port;
  echo.value += "<br>USername: " + Mqtt_Username + "<br>";
  echo.value += "Password: " + Mqtt_Password + "<br>";
  echo.value += "Topic: " + Mqtt_Topic + "<br><br>";
   // echo.value += mqttserver.isValid() ? String(" (OK)") : String(" (ERR)"); //-> BUG
  //echo.value += "Update period: " + String(updateInterval / 1000) + " sec.<br>";
  
  echo.value += "hostname: " + Adv_HostName + "<br>";
  echo.value += "NTP server1: " + String(Adv_NTPserver1) + "<br>";
  echo.value += "NTP server2: " + String(Adv_NTPserver2) + "<br>";
  echo.value += "GMT: " + String(GMT) + "<br>";
  echo.value += "DST: " + String(DST) + "<br>";

  configTime(GMT * 3600, DST * 60, Adv_NTPserver1, Adv_NTPserver2); 
  return String("");
}

void handleRoot() {
  time_t now = time(nullptr);
  byte History_Count = 0;
  History_MaxReached ? History_Count = History_MaxEvents : History_Count = History_CurrentEvent;  // determine if the max number of event is already reached
  //sizeof (History_EventList) / sizeof (History_EventList[0]);   // we can't have more than History_MaxEvents records 
Serial.println(History_MaxReached);
Serial.println(History_CurrentEvent);
Serial.println(History_Count);

  // This is the Home Page
  String  content =
    "<html>"
    "<title>RFLink-ESP</title>"
    "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
    "<link rel='stylesheet' href='https://stackpath.bootstrapcdn.com/bootswatch/4.4.1/darkly/bootstrap.min.css'><script src='https://stackpath.bootstrapcdn.com/bootstrap/4.4.1/js/bootstrap.min.js'></script>"
    "</head>"
    "<body>"
    "<h1>RFLink-ESP   " AUTOCONNECT_LINK(COG_32) "</h1><Br>";
    //"<iframe width=\"450\" height=\"260\" style=\"transform:scale(0.79);-o-transform:scale(0.79);-webkit-transform:scale(0.79);-moz-transform:scale(0.79);-ms-transform:scale(0.79);transform-origin:0 0;-o-transform-origin:0 0;-webkit-transform-origin:0 0;-moz-transform-origin:0 0;-ms-transform-origin:0 0;border: 1px solid #cccccc;\" src=\"https://thingspeak.com/channels/454951/charts/1?bgcolor=%23ffffff&color=%23d62020&dynamic=true&type=line\"></iframe>"
    //"<p style=\"text-align:center\">"  "</p></h1>";
  content +=         "Last refresh : "; 
  content +=          ctime(&now); 
  content +=         "<Br>";
  //content +=         "<form action='/' method='POST'><button type='button submit' name='BtnRefreshRain' value='1'>Refresh</button></form><Br>";
  content += "<form action='/' method='POST'><button type='button submit' name='BtnTimeBeforeSWoff' value='0' class='btn btn-secondary'>Refresh</button></form><Br>";
  content +=       "<table class='table'>";
  content +=         "<thead><tr><th>Date</th><th>Type</th><th>Command</th></tr></thead>"; // Table Header    // é = &eacute; 
  content +=         "<tbody>";  // Table content



  int j = History_CurrentEvent;
  for ( int i = 1; i <= History_Count; i++ ) { 
  j--; //  we get the index of the last record in the array
  if (j ==-1) j = History_MaxEvents - 1; // if we are under the first index of the array ,we go to the last
  
  ////////////////// One table line ///////////////////
  content +=             "<tr><td>";
  content +=             History_EventList[j].History_date;
  content +=             "</td><td>";
  History_EventList[j].History_isreceived ? content +=   "received (RF to MQTT)" :content +=   "send (MQTT to RF)";
  content +=             "</td><td>"; // Première ligne : température"
  content +=             History_EventList[j].History_message;
  content +=             "</td>";
////////////////// One table line ///////////////////

 }
  
  content +=             "</tr><tr><td></td><td></td><td></td></tr>";   // we add a last line to bottom of the table
  content +=       "</tbody></table>";
  content +=       "</body>";
  content +=       "</html>";


  WiFiWebServer&  webServer = portal.host();
  webServer.send(200, "text/html", content);
}

  // -*-*-*-*-*-*-- END - for AutoConnect --*-*-*-*-*-*-


// ***********************************************************************************
// ***********************************************************************************

void MQTT_Receive_Callback(char *topic, byte *payload, unsigned int length)
{
    Serial.println(F("MQTT_Receive_Callback"));
    String SerialRFcmd = "";

    String Payload = "";     // convert the payload in String...
    for (int i = 0; i < length; i++)
    {
        Payload += (char)payload[i];
    }
   
    StaticJsonDocument<100> doc;       // We use json library to parse the payload                         
    DeserializationError error = deserializeJson(doc, Payload); // Deserialize the JSON document
    if (error)            // Test if parsing succeeds.
    {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        return;
    }

    bool hasTransmitCmd = doc.containsKey("TRANSMIT");    // We check the kind of command format received with MQTT

        time_t now = time(nullptr);
        Serial.println(ctime(&now));


    if (hasTransmitCmd == true)
    {
        // ***************************************************************************
        // A RF command to transmit. Same sending format than in serial mode
        // from:   RFlink/send     payload : {"TRANSMIT":"10;EV1527;005DF;2;ON;"}
        // to:     10;EV1527;005DF;2;ON;
        // ***************************************************************************
        Serial.print(F("Transmitting..."));
        String PayloadExec = doc["TRANSMIT"];
        SerialRFcmd = PayloadExec;
        PayloadExec.toCharArray(InputBuffer_Serial, sizeof(InputBuffer_Serial));
        Process_Serial();
    }
    else
    {
        // ***************************************************************************
        // A RF command to transmit. Personnalised format for RFlink-ESP
        // from:   RFlink/send/ev1527/005df  payload {"SWITCH":“2","CMD":"ON"}
        // to:     10;EV1527;005DF;2;ON;
        // ***************************************************************************
        unsigned long PayloadSwitch = doc["SWITCH"];
        const char *PayloadCmd = doc["CMD"];

        String Topic = String(topic);
        Topic.toUpperCase();
        Topic = Topic.substring(Mqtt_Topic.length() + 6); //  EV1527/005DF   (we cut Mqtt_Topic + "/received/")
        int x1 = Topic.indexOf("/");

        SerialRFcmd += "10;"; 
        SerialRFcmd += Topic.substring(0, x1) + ";" + Topic.substring(x1 + 1) + ";";
        SerialRFcmd += String(PayloadSwitch) + ";" + String(PayloadCmd) + ";";

        //if ( Learning_Mode > 0 ) {
        Serial.print("MQTT Received Topic: ");
        Serial.print(topic);
        Serial.print("  Payload: " + Payload);
        Serial.println("  ===>  Converted: " + SerialRFcmd);
        //}

        SerialRFcmd.toCharArray(InputBuffer_Serial, sizeof(InputBuffer_Serial));
        Process_Serial();
    }

        // Recording this RF sending to display it on home page
        History_EventList[History_CurrentEvent].History_date = ctime(&now);
        History_EventList[History_CurrentEvent].History_isreceived = false;
        History_EventList[History_CurrentEvent].History_message = SerialRFcmd;
        History_CurrentEvent++;
        if (History_CurrentEvent >= History_MaxEvents)
        {
            History_CurrentEvent = 0;
            History_MaxReached = true;
        }

}


/* ***********************************************************************
*********************************************************************** */

#endif



#include "RFL_Protocols.h"

// ***********************************************************************************
// ***********************************************************************************
void setup() {
  delay(500);
  Serial.begin ( 57600 ) ;
  Serial.println();
  SPIFFS.begin();

  pinMode      ( RECEIVE_PIN,  INPUT        ) ;
  pinMode      ( TRANSMIT_PIN, OUTPUT       ) ;
  digitalWrite ( RECEIVE_PIN,  INPUT_PULLUP ) ;  // pull-up resister on (to prevent garbage)
#ifdef LED_BUILTIN
  pinMode      ( LED_BUILTIN,  OUTPUT);
  digitalWrite ( LED_BUILTIN,  LOW);
#endif


  // -*-*-*-*-*-*-*-- for AutoConnect --*-*-*-*-*-*-*-

  if (portal.load(FPSTR(AUX_settings))) {   // we load all the settings from "/settings" uri
    AutoConnectAux& my_settings = *portal.aux(AUX_SETTING_URI);
    PageArgument  args;
    loadParams(my_settings, args);

//    if (uniqueid) {
     config.apid = String("RFLink-ESP-") + String(GET_CHIPID(), HEX);
     Serial.println("AP name set to " + config.apid);
//    }

    // config.psk = "RFlinkPwd1";   // if not defined, default Wifi AP is 12345678, you can change it here

    if (Adv_HostName.length()) {
      config.hostName = Adv_HostName;
      Serial.println("hostname set to " + config.hostName);
    }
    config.bootUri = AC_ONBOOTURI_HOME;
    config.homeUri = "/";
    config.title = "RFlink-ESP";
    config.autoReconnect = true; 
    portal.config(config);

    portal.on(AUX_SETTING_URI, loadParams);
    portal.on(AUX_SAVE_URI, saveParams);
  }
  else
    Serial.println("load error");

    //-------------------------------------
  
  if (portal.begin()) {
    if (MDNS.begin("RFLink-ESP")) {
      MDNS.addService("http", "tcp", 80);
    }
    Serial.println("connected:" + WiFi.SSID());
    Serial.println("IP:" + WiFi.localIP().toString());
    //configTime(TZ_SEC, DST_SEC, "pool.ntp.org", "time.nist.gov");  // get current time from ntp servers

     if(Adv_NTPserver1[0] == '\0') {
       String Temp_Adv_NTPserver= "pool.ntp.org";
       Temp_Adv_NTPserver.toCharArray(Adv_NTPserver1,25);
     };
     if(Adv_NTPserver2[0] == '\0') {
       String Temp_Adv_NTPserver= "time.nist.gov";
       Temp_Adv_NTPserver.toCharArray(Adv_NTPserver2,25);
     };

    Serial.println("-------------------------------------");
    Serial.println(Adv_NTPserver1);
    Serial.println(Adv_NTPserver2);
    Serial.println("-------------------------------------");
    configTime(GMT * 3600, DST * 60 , Adv_NTPserver1, Adv_NTPserver2);    // We find the correct time thanks to NTP servers

  } else {
    Serial.println("connection failed:" + String(WiFi.status()));
    while (1) {
      delay(100);
      yield();
    }
  }

  WiFiWebServer&  webServer = portal.host();
  webServer.on("/", handleRoot);


  

  // -*-*-*-*-*-*-- END - for AutoConnect --*-*-*-*-*-*-

  



#ifdef MQTT

  // ***************************************************************************
  // Configure OTA
  // ***************************************************************************

  Serial.println("Arduino OTA activated.");

  // Port defaults to 8266
  ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  String clientId = "RFlinkESP-" + String(GET_CHIPID(), HEX); // usefull to create an unique name in case of multiple RFlink on network
  ArduinoOTA.setHostname(clientId.c_str());
  Serial.print(F("OTA name : "));
  Serial.println(clientId);
  

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    Serial.end();
    Serial.begin(57600, SERIAL_8N1, SERIAL_TX_ONLY, 1);  // We disable  RX on the ESP for more OTA stability (bug OTA arduino :  https://github.com/esp8266/Arduino/issues/2576 ou https://github.com/esp8266/Arduino/issues/3881)
    Serial.println(F("Arduino OTA: Start updating"));
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("");
    Serial.println("Arduino OTA: End");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
   // Serial.printf("Arduino OTA Progress: %u%%\r", (progress / (total / 100)));
    Serial.printf("%u%%-", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Arduino OTA Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Arduino OTA: Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Arduino OTA: Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Arduino OTA: Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Arduino OTA: Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("Arduino OTA: End Failed");
  });

  ArduinoOTA.begin();

  // ***************************************************************************
  // END - Configure OTA
  // ***************************************************************************


  RFLink_File.Begin () ;

  MQTT_Client.setCallback ( MQTT_Receive_Callback ) ;

#endif

  // *********   PROTOCOL CLASSES, available and in this order   ************
  RFL_Protocols.Add ( new _RFL_Protocol_KAKU             () ) ;
  RFL_Protocols.Add ( new _RFL_Protocol_EV1527           () ) ;
  RFL_Protocols.Add ( new _RFL_Protocol_Paget_Door_Chime () ) ;
  RFL_Protocols.Add ( new _RFL_Protocol_DUMMY            () ) ;
  RFL_Protocols.Add ( new _RFL_Protocol_Oregon           () ) ;
  RFL_Protocols.setup () ;
  // ************************************************************************


  delay ( 200 ) ;


  sprintf ( pbuffer, "20;%02X;Nodo RadioFrequencyLink - MiRa V%s - R%02x\r\n", PKSequenceNumber++, Version, Revision );
  Serial.print ( pbuffer ) ;

  RawSignal.Time = millis() ;
}


// ***********************************************************************************
// ***********************************************************************************
void loop () {

#ifdef MQTT

  if (WiFi.status() == WL_CONNECTED) {
        MQTT_Client.loop();
        if (!MQTT_Client.connected())  mqttConnect();
  } else Serial.print("not connected");


#endif

  if ( FetchSignal () ) {
    RFL_Protocols.Decode ();
  }
  Collect_Serial () ;

  // -*-*-*-*-*-*-- for AutoConnect --*-*-*-*-*-*-
   MDNS.update();  
   portal.handleClient();
  // -*-*-*-*-*-*-- END - for AutoConnect --*-*-*-*-*-*-
    ArduinoOTA.handle();

  /*
    if ( FetchSignal () ) {
      //RFL_Protocols.Decode ();
      int Time ;
      sprintf ( pbuffer, "20;%02X;", PKSequenceNumber++ ) ;
      Serial.print ( pbuffer ) ;
      Serial.print ( F( "DEBUG_Start;Pulses=" ) ) ;
      Serial.print ( RawSignal.Number - 3 ) ;
          Serial.print ( F ( ";Pulses(uSec)=" )) ;
          for ( int x=0; x<RawSignal.Number+1; x++ ) {
            Time = RawSignal.Pulses[x] ;
              //Time = 30 * ( Time / 30 ) ;
            Serial.print ( Time ) ;
            if (x < RawSignal.Number) Serial.print ( "," );
          }
          Serial.println ( ";" ) ;
    }
  */



}
