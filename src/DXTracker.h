// Copyright (c) F4HWN Armel. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#define VERSION "1.1.0"
#define AUTHOR "F4HWN"
#define NAME "DXTracker"

#define TIMEOUT_BIN_LOADER    3               // 3 sec
#define TIMEOUT_SCREENSAVER   5 * 60 * 1000   // 5 min
#define TIMEOUT_MAP           5 * 1000        // 5 sec
#define TIMEOUT_TEMPORISATION 10 * 1000       // 10 sec

#undef min

#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <JPEGDecoder.h>
#include <FS.h>
#include <SPIFFS.h>
#include <M5Unified.h>
#include <M5StackUpdater.h>


// Wifi
WiFiClient clientHamQSL, clientSat, clientGreyline, clientHamQTH;
WiFiClient httpClient;
WiFiServer httpServer(80);

// Web site Screen Capture stuff
#define GET_unknown 0
#define GET_index_page  1
#define GET_screenshot  2

// Flags for button presses via Web site Screen Capture
bool buttonLeftPressed = false;
bool buttonCenterPressed = false;
bool buttonRightPressed = false;

// Preferences
Preferences preferences;

// Color
typedef struct __attribute__((__packed__))
{
  uint8_t r;
  uint8_t g;
  uint8_t b;
} colorType;

colorType TFT_BACK = {48, 48, 48};
colorType TFT_GRAY = {128, 128, 128};

// Timezone
const char* ntpServer = "pool.ntp.org";
const char* ntpTimeZone = "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00"; // For Europe/Paris
//const char* ntpTimeZone = "CET-1CEST,M3.5.0,M10.5.0/3"; // For Europe/Brussels
//const char* ntpTimeZone = "EET-2EEST,M3.5.0/3,M10.5.0/4"; // For Europe/Sofia 
//const char* ntpTimeZone = "EST5EDT,M3.2.0,M11.1.0"; // For America/Montreal
//const char* ntpTimeZone = "AST4"; // For America/Martinique
//const char* ntpTimeZone = "AST4"; // For America/Guadeloupe
//const char* ntpTimeZone = "NCT-11"; // For Pacific/Noumea

int utc = 1;

// HTTP endpoint
String endpointHamQSL = "http://www.hamqsl.com/solarxml.php";
String endpointSat = "http://rrf2.f5nlg.ovh:8080/cgi-bin/DXSat.py";
String endpointHamQTH = "http://rrf2.f5nlg.ovh:8080/cgi-bin/DXCluster.py";

String endpointGreyline[2] = {
  "http://rrf2.f5nlg.ovh:8080/greyline.jpg",
  "http://rrf2.f5nlg.ovh:8080/sunmap.jpg"
};

// Scroll
LGFX_Sprite imgA(&M5.Lcd); // Create Sprite object "img" with pointer to "tft" object
String messageA = "";
int16_t posA;

LGFX_Sprite imgB(&M5.Lcd); // Create Sprite object "img" with pointer to "tft" object
String messageB = "";
int16_t posB;

// Bin loader
File root;
String binFilename[128];
uint8_t binIndex = 0;

// Propag data
String solarData[] = {
  "SFI", "Sunspots", "A-Index", "K-Index", 
  "X-Ray", "Helium Line", "Proton Flux", "Electron Flux", 
  "Aurora", "Solar Wind", "Magnetic Field", "Signal Noise"
};

String solarKey[] = {
  "solarflux", "sunspots", "aindex", "kindex", 
  "xray", "heliumline", "protonflux", "electonflux", 
  "aurora", "solarwind", "magneticfield", "signalnoise"
};

String skipData[] = {
  "E-Skip North America",
  "E-Skip Europe",
  "E-Skip Europe 4m",
  "E-Skip Europe 6m",
};

String skipKey[] = {
  "location=\"north_america\">", 
  "location=\"europe\">", 
  "location=\"europe_4m\">",  
  "location=\"europe_6m\">" 
};

String propagKey[] = {
  "80m-40m\" time=\"day\">", 
  "30m-20m\" time=\"day\">", 
  "17m-15m\" time=\"day\">", 
  "12m-10m\" time=\"day\">", 
  "80m-40m\" time=\"night\">",
  "30m-20m\" time=\"night\">",
  "17m-15m\" time=\"night\">",
  "12m-10m\" time=\"night\">"    
};

String cluster[50], call[50], frequency[50], band[50], country[50];

// Task Handle
TaskHandle_t hamdataHandle;
TaskHandle_t buttonHandle;

// Miscellaneous
String tmpString;
String dateString;
String greylineData = "", hamQSLData = "", hamQTHData = "", satData = "";
String greylineUrl = "";
String reloadState = "";

boolean decoded = 0;
boolean startup = 0;
boolean screensaverMode = 0;
boolean greylineRefresh = 0;
boolean greylineSelect = 0;

uint8_t screenRefresh = 1;
uint8_t htmlGetRequest;
uint8_t alternance = 0;
uint8_t configCurrent = 0;
uint8_t brightnessCurrent = 64;
uint8_t messageCurrent = 0;

int16_t parenthesisBegin = 0;
int16_t parenthesisLast = 0;

uint32_t temporisation;
uint32_t screensaver;
uint32_t frequencyExclude[] = {
  1840, 1842, 3573, 5357,	
  7056, 7071, 7074, 7078,
  10130, 10132, 10133, 10136, 
  14071, 14074, 14078, 14090,
  18100, 18104, 21074, 21078,
  21091, 24915, 28074, 28078,
  50310, 50313, 50328, 50323,
  70100, 144174, 222065, 432065
};

#undef SPI_READ_FREQUENCY
#define SPI_READ_FREQUENCY 40000000