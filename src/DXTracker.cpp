// Copyright (c) F4HWN Armel. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "DXTracker.h"
#include "functions.h"
#include "tasks.h"

// Setup
void setup()
{
  // Debug
  Serial.begin(115200);

  // Init screensaver timer
  screensaver = millis();

  // Init M5
  M5.begin(true, false, false, false);

  // Init Power
  power();

  // Init Speaker
  speaker();

  // Bin Loader
  binLoader();

  // Preferences
  preferences.begin(NAME);

  size_t n = sizeof(config) / sizeof(config[0]);
  n = (n / 4) - 1;

  //preferences.putUInt("config", 0);

  configCurrent = preferences.getUInt("config", 0);

  if(configCurrent > n) {
    configCurrent = 0;
    preferences.putUInt("config", configCurrent);
  }

  // LCD
  M5.Lcd.setBrightness(brightnessCurrent);
  M5.Lcd.fillScreen(M5.Lcd.color565(TFT_BACK.r, TFT_BACK.g, TFT_BACK.b));
  screensaver = millis(); // Screensaver update !!!

  // SPIFFS
  if(!SPIFFS.begin())
  {
    Serial.println("SPIFFS Mount Failed");
    return;
  }

  // Title
  M5.Lcd.setFreeFont(&rounded_led_board10pt7b);
  M5.Lcd.setTextColor(TFT_WHITE, M5.Lcd.color565(TFT_BACK.r, TFT_BACK.g, TFT_BACK.b));
  M5.Lcd.setTextDatum(CC_DATUM);
  M5.Lcd.drawString(String(NAME), 160, 20);
  M5.Lcd.setFreeFont(0);
  M5.Lcd.drawString("Version " + String(VERSION) + " by F4HWN", 160, 50);

  // QRCode
  M5.Lcd.qrcode("https://github.com/armel/DXTracker", 90, 80, 140, 6);

  // We start by connecting to the WiFi network
  M5.Lcd.setTextPadding(320);

  while(true)
  {
    uint8_t attempt = 1;
    M5.Lcd.drawString(String(config[(configCurrent * 4)]), 160, 60);
    WiFi.begin(config[(configCurrent * 4)], config[(configCurrent * 4) + 1]);
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      if(attempt % 2 == 0)
      {
        M5.Lcd.drawString("Connexion en cours", 160, 70);
      }
      else 
      {
        M5.Lcd.drawString(" ", 160, 70);
      }
      attempt++;
      if(attempt > 10) {
        break;
      }
    }
    if(WiFi.status() != WL_CONNECTED)
    {
      configCurrent += 1;
      if(configCurrent > n) {
        configCurrent = 0;
        preferences.putUInt("config", configCurrent);
      }
    }
    else {
      break;    
    }
  }

  M5.Lcd.drawString(String(WiFi.localIP().toString().c_str()), 160, 70);

  // Scroll
  posA = M5.Lcd.width();
  imgA.createSprite(M5.Lcd.width(), 20);

  posB = M5.Lcd.width();
  imgB.createSprite(M5.Lcd.width(), 20);

  // Start server (for Web site Screen Capture)
  httpServer.begin();     

  // Multitasking task for retreive propag data
  xTaskCreatePinnedToCore(
      hamdata,        // Function to implement the task
      "hamdata",      // Name of the task
      16384,          // Stack size in words
      NULL,           // Task input parameter
      1,              // Priority of the task
      &hamdataHandle, // Task handle
      0);             // Core where the task should run

  // Multitasking task for retreive button
  xTaskCreatePinnedToCore(
      button,         // Function to implement the task
      "button",       // Name of the task
      8192,           // Stack size in words
      NULL,           // Task input parameter
      1,              // Priority of the task
      &buttonHandle,  // Task handle
      1);             // Core where the task should run

  // Accelelerometer
  M5.IMU.Init();

  // Let's go after temporisation
  delay(1000);

  for (uint8_t i = 0; i <= 120; i++)
  {
    M5.Lcd.drawFastHLine(0, i, 320, TFT_BLACK);
    M5.Lcd.drawFastHLine(0, 240 - i, 320, TFT_BLACK);
    delay(5);
  }

  // Waiting for data
  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Lcd.setTextSize(1);  // Font size scaling is x1
  M5.Lcd.setTextFont(2);  // Font 2 selected
  M5.Lcd.setTextDatum(CC_DATUM);
  M5.Lcd.setTextPadding(320);

  while(greylineData == "" || hamQSLData == "" || hamQTHData == "" || satData == "") 
  {
    M5.Lcd.drawString("Loading data", 160, 120);
    delay(250);
    M5.Lcd.drawString(" ", 160, 120);
    delay(250);

    if(hamQTHData != "")
    {
      M5.Lcd.drawString("Cluster Ok", 160, 160);
    }
    if(greylineData != "")
    {
      M5.Lcd.drawString("Greyline Ok", 160, 180);
    }
    if(hamQSLData != "")
    {
      M5.Lcd.drawString("Solar Ok", 160, 200);
    }
    if(satData != "")
    {
      M5.Lcd.drawString("Sat Ok", 160, 220);
    }
  }

  startup = 1;

  M5.Lcd.drawString("Loading data", 160, 120);
  M5.Lcd.drawString("Cluster Ok", 160, 160);
  M5.Lcd.drawString("Greyline Ok", 160, 180);
  M5.Lcd.drawString("Solar Ok", 160, 200);
  M5.Lcd.drawString("Sat Ok", 160, 220);
  
  delay(500);
}

// Main loop
void loop()
{
  // Manage acceleration
  getAcceleration();

  // Let's clean if necessary
  clear();

  // View propag datas
  propagData();

  // Prepare cluster and sat scroll message
  clusterAndSatMessage();

  // Prepare propag scroll message
  propagMessage();

  // View greyline
  greyline();

  // Manage scroll
  scroll();

  // Manage Web site Screen Capture
  getScreenshot();

  // Manage screensaver
  wakeAndSleep();

  // Manage alternance
  if(screenRefresh == 0 && millis() - temporisation > TIMEOUT_TEMPORISATION) {
    temporisation = millis();
    alternance++;
    alternance = (alternance > 11) ? 0 : alternance;
  }
}