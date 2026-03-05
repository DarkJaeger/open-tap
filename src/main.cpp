
#include <SPI.h>
#include <FS.h>
#include <SD.h>
#include "Arduino.h"
#include "TFT_eSPI.h"
//#include <XPT2046_Touchscreen.h>
#include <WiFiManager.h>
#include "JPEGDecoder.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>

// Touchscreen pins
//#define XPT2046_IRQ 36
//#define XPT2046_MOSI 32
//#define XPT2046_MISO 39
//#define XPT2046_CLK 25
//#define XPT2046_CS 33

//SD Card Pins
#define SD_MISO 19
#define SD_MOSI 23
#define SD_SCLK 18
#define SD_CS   5   // Chip Select pin for SD card

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320
#define FONT_SIZE 2

const int bootButtonPin = 0;


void drawSdJpeg(const char* filename, int xpos, int ypos);
void jpegRender(int xpos, int ypos);
bool downloadImageToSD(const char* url, const char* filename);

// TFT display
TFT_eSPI tft = TFT_eSPI();

SPIClass sdSPI(VSPI);

// Global variables
int timeout = 60;
unsigned long touchStartTime = 0;
bool isTouching = false;
bool settingsActive = false;

double pnts = 0;

char serverip[17] = "192.168.123.123";
char serverport[7] = "8000";
char kegid[7] = "K2";
//char backcol[] = "0x0000";
//char forecol[] = "0xF800";

bool shouldSaveConfig = false;
bool settingschanged = false;
bool firstrun = true;

String SERV = "192.168.8.123";
String PORT = "8000";
String KGID = "K2";
String BCOL = "0000";
String FCOL = "FD20";

uint16_t bgColor = TFT_BLACK;
uint16_t fgColor = TFT_ORANGE;

String JDBID2 = "";
String JNAME2 = "";
String JDESC2 = "";
String JLOGO2 = "";
String JKCAP2 = "";
String JKEMP2 = "";
String JKCUR2 = "";
String JKGID2 = "";

int centerX, centerY;


//XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);
SPIClass touchscreenSPI = SPIClass(HSPI);

uint16_t parseColor565(String value, uint16_t fallback) {
  value.trim();
  if (value.startsWith("0x") || value.startsWith("0X")) {
    value = value.substring(2);
  } else if (value.startsWith("#")) {
    value = value.substring(1);
  }

  if (value.length() == 0 || value.length() > 4) {
    return fallback;
  }

  char* endPtr = nullptr;
  long parsed = strtol(value.c_str(), &endPtr, 16);
  if (endPtr == value.c_str() || *endPtr != '\0' || parsed < 0 || parsed > 0xFFFF) {
    return fallback;
  }

  return static_cast<uint16_t>(parsed);
}

String formatColor565(uint16_t color) {
  char out[5];
  snprintf(out, sizeof(out), "%04X", color);
  return String(out);
}

void ApplyConfiguredColors() {
  bgColor = parseColor565(BCOL, TFT_BLACK);
  fgColor = parseColor565(FCOL, TFT_ORANGE);
  BCOL = formatColor565(bgColor);
  FCOL = formatColor565(fgColor);
}


double KGtoPints(double empt, double full, double cur){
  double beer = cur - empt;
  double pints = beer / 0.6;
  return pints;
}


double beerRemaining(double Full, double Empt, double Curr){

double fullBeer = Full - Empt;
double currentBeer = Curr - Empt;

    if (fullBeer <= 0) {
        // Avoid divide-by-zero or invalid data
        return 0.0;
    }

    double percent = (currentBeer / fullBeer) * 100.0;

    // Clamp to valid range
    if (percent < 0.0) percent = 0.0;
    if (percent > 100.0) percent = 100.0;

    return percent * 2;
}


void DrawRemaining(double BeerPercentage){
uint32_t barColor;


//Set the bar colour based on the percentage
  if (BeerPercentage < 10){
    barColor = TFT_RED;
    //Serial.println("Beer Red");
  }
  else if (BeerPercentage < 20){
    barColor = TFT_ORANGE;
    //Serial.println("Beer Orange");
  }
  else if (BeerPercentage < 30){
    barColor = TFT_YELLOW;
    //Serial.println("Beer Yellow");
  }
  else{
    barColor = TFT_GREEN;
    //Serial.println("Beer Green");
  }
// First we will fill the whole bar with the colour
// then overlay the black to show the remaining

  //if (firstrun == true || settingschanged == true){
        tft.fillRect(1,32,15,200,barColor);
        tft.fillRect(1,32,15,200 - BeerPercentage,bgColor);
        
        //Right side bar
        // Fill the whole bar with colour
        tft.fillRect(224,32,15,200,barColor);
        // Top X, Y are static. Height is variable by percentage inverse
        tft.fillRect(224,32,15,200 - BeerPercentage,bgColor);
        tft.setTextColor(fgColor, bgColor);
        tft.drawCentreString("[Approx " + String(round(pnts)) + " Pints remaining]", centerX, 236, 2);
 // }
}


void DrawScreen() {
  Serial.println("First Run: " + String(firstrun));
  Serial.println("Settings Changed: " + String(settingschanged));
    if (firstrun == true){
      settingschanged = true;
      firstrun = false;
    }

    if (settingschanged == true){

        String ImageURL = "http://" + SERV + ":" + PORT + JLOGO2;
        Serial.println(ImageURL);

        if (SD.exists(JLOGO2)) {
          Serial.println("Image already exists on SD card.");
        } else {
          Serial.println("Image does not exist. Proceed to download.");
          downloadImageToSD(ImageURL.c_str(), JLOGO2.c_str());
          delay(100);
        }

        tft.fillScreen(bgColor);
        
        //Beer Name at the top
        tft.setTextColor(fgColor, bgColor);
        tft.fillRect(0,0,240,29,bgColor);
        tft.drawRect(0,0,240,29,fgColor);
        tft.drawCentreString(JNAME2, centerX, 10, 2);

        // 200 x 200 beer logo
        tft.fillRect(19,31,202,202,bgColor);
        tft.drawRect(19,31,202,202,fgColor);
        //tft.drawCentreString(JDESC2, centerX, 270, 2);

        //Left side bar
        tft.fillRect(0,31,17,202,bgColor);
        tft.drawRect(0,31,17,202,fgColor);

        //Right side bar
        tft.fillRect(223,31,17,202,bgColor);
        tft.drawRect(223,31,17,202,fgColor);

        //bottom text + Keg id
        tft.fillRect(0,235,240,84,bgColor);
        tft.drawRect(0,235,240,84,fgColor);
        tft.drawRect(0,235,240,20,fgColor);

        tft.drawCentreString("[Approx " + String(round(pnts)) + " Pints remaining]", centerX, 236, 2);
        
        //tft.setTextSize(2); // Set the text size to 2
        
        tft.setViewport(2,256,238,82);
              //tft.setTextWrap(true, false);
              //tft.drawCentreString(JDESC2, centerX, 255, 2);
        tft.setCursor(0, 0);
        tft.print(JDESC2);
        tft.resetViewport();

        

// convert percentage to pints... assume 1012 grams per litre
//


        drawSdJpeg(JLOGO2.c_str(), 20, 32);
        settingschanged = false;
    }

}

//Get JSON from the brewserver
void GetJSON(String svr, String prt) {
   
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    // Build URL
    String url = "http://" + svr + ":" + prt + "/get_keg/" + KGID; // Add endpoint if needed
    Serial.println("Sending GET to: " + url);

    http.begin(url); // Initialise HTTP connection
    int httpCode = http.GET(); // Send GET request

    if (httpCode > 0) {
      Serial.printf("HTTP Response code: %d\n", httpCode);
      String payload = http.getString(); // Get response body
      Serial.println("Response: " + payload);

// JSON PROCESSOR HERE ###########
JsonDocument doc;
//StaticJsonDocument<200> doc;
DeserializationError error = deserializeJson(doc, payload);


  if (error) {
    Serial.print("Failed to parse JSON: ");
    Serial.println(error.c_str());
    settingschanged = false;
    http.end();
    return;
  }

  settingschanged = false;

  String JDBID = doc["id"];
    if (JDBID != JDBID2){
    JDBID2 = JDBID;
    settingschanged = true;
    }
  String JNAME = doc["name"];
    if (JNAME != JNAME2){
    JNAME2 = JNAME;
    settingschanged = true;
    }
  String JDESC = doc["description"]; 
    if (JDESC != JDESC2){
    JDESC2 = JDESC;
    settingschanged = true;
    }
  String JLOGO = doc["logo_url"];
    if (JLOGO != JLOGO2){
        JLOGO2 = JLOGO;
        settingschanged = true;
      }
      
  String JKCAP = doc["keg_capacity"];
    if (JKCAP != JKCAP2){
    JKCAP2 = JKCAP;
    settingschanged = true;
    }
  String JKEMP = doc["empty_keg_weight"];
    if (JKEMP != JKEMP2){
    JKEMP2 = JKEMP;
    settingschanged = true;
    }
  String JKCUR = doc["current_weight"];
      if (JKCUR != JKCUR2) {
        JKCUR2 = JKCUR;
        //settingschanged = true;
    }

  String JKGID = doc["keg_id"];
    if (JKGID != JKGID2){
    JKGID2 = JKGID;
    settingschanged = true;
    }


 // Serial.println("Parsed Data:");
  //Serial.println("Beer Name: " + JNAME);
  //Serial.println("Logo URL: " + JLOGO);
  //Serial.println("Keg ID: " + JKGID);

//###############################

    } else {
      Serial.printf("HTTP GET failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end(); // Close connection
  } else {
    Serial.println("WiFi not connected!");
  }
}

bool downloadImageToSD(const char* url, const char* filename) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected!");
    return false;
  }

  HTTPClient http;
  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    File file = SD.open(filename, FILE_WRITE);
    if (!file) {
      Serial.println("Failed to open file for writing");
      http.end();
      return false;
    }

    WiFiClient* stream = http.getStreamPtr();
    uint8_t buffer[512];
    int len = http.getSize();

    while (http.connected() && (len > 0 || len == -1)) {
      size_t size = stream->available();
      if (size) {
        int c = stream->readBytes(buffer, ((size > sizeof(buffer)) ? sizeof(buffer) : size));
        file.write(buffer, c);
        if (len > 0) len -= c;
      }
      delay(1);
    }

    file.close();
    Serial.println("Image downloaded and saved to SD!");
    http.end();
    return true;
  } else {
    Serial.printf("HTTP GET failed: %d\n", httpCode);
    http.end();
    return false;
  }
}

// Load config from NVS Preferences
void LoadConfig() {
  Preferences prefs;
  if (!prefs.begin("open-tap", true)) {
    Serial.println("Failed to open Preferences (read-only)");
    return;
  }

  SERV = prefs.getString("serv", SERV);
  PORT = prefs.getString("port", PORT);
  KGID = prefs.getString("kgid", KGID);
  BCOL = prefs.getString("bcol", BCOL);
  FCOL = prefs.getString("fcol", FCOL);
  prefs.end();

  SERV.trim();
  PORT.trim();
  KGID.trim();
  BCOL.trim();
  FCOL.trim();
  ApplyConfiguredColors();

  Serial.println(F("CONFIG loaded from Preferences"));
}

// Write config to NVS Preferences
void WriteCONFIG() {
  Preferences prefs;
  if (!prefs.begin("open-tap", false)) {
    Serial.println("Failed to open Preferences (read-write)");
    return;
  }

  prefs.putString("serv", SERV);
  prefs.putString("port", PORT);
  prefs.putString("kgid", KGID);
  prefs.putString("bcol", BCOL);
  prefs.putString("fcol", FCOL);
  prefs.end();
  Serial.println("CONFIG saved to Preferences");
}

// Display settings screen
void showsettings() {
  tft.fillScreen(bgColor);
  tft.setTextColor(fgColor, bgColor);
  tft.setTextDatum(MC_DATUM);
  tft.drawCentreString("The screen has been put into", tft.width() / 2, 20, 2);
  tft.drawCentreString("Access point mode.", tft.width() / 2, 40, 2);
  tft.drawCentreString("Look for Beer-Tap-Screen", tft.width() / 2, 60, 2);
  tft.drawCentreString("In Wifi and connect.", tft.width() / 2, 80, 2);
  tft.drawCentreString("Visit http://192.168.4.1", tft.width() / 2, 100, 2);
  delay(7000);
}


void drawSdJpeg(const char* filename, int xpos, int ypos) {

  // Open the named file (the Jpeg decoder library will close it)
  File jpegFile = SD.open(filename, FILE_READ);  // or, file handle reference for SD library

  if (!jpegFile) {
    Serial.print("ERROR: File \"");
    Serial.print(filename);
    Serial.println("\" not found!");
    return;
  }

  //Serial.println("===========================");
  //Serial.print("Drawing file: ");
  //Serial.println(filename);
  ///
  //Serial.println("===========================");

  // Use one of the following methods to initialise the decoder:
  bool decoded = JpegDec.decodeSdFile(jpegFile);  // Pass the SD file handle to the decoder,
  //bool decoded = JpegDec.decodeSdFile(filename);  // or pass the filename (String or character array)

  if (decoded) {
    // print information about the image to the serial port
    //jpegInfo();
    // render the image onto the screen at given coordinates
    jpegRender(xpos, ypos);
  } else {
    Serial.println("Jpeg file format not supported!");
  }
}

void jpegRender(int xpos, int ypos) {

  //jpegInfo(); // Print information from the JPEG file (could comment this line out)

  uint16_t* pImg;
  uint16_t mcu_w = JpegDec.MCUWidth;
  uint16_t mcu_h = JpegDec.MCUHeight;
  uint32_t max_x = JpegDec.width;
  uint32_t max_y = JpegDec.height;

  bool swapBytes = tft.getSwapBytes();
  tft.setSwapBytes(true);

  // Jpeg images are draw as a set of image block (tiles) called Minimum Coding Units (MCUs)
  // Typically these MCUs are 16x16 pixel blocks
  // Determine the width and height of the right and bottom edge image blocks
  uint32_t min_w = jpg_min(mcu_w, max_x % mcu_w);
  uint32_t min_h = jpg_min(mcu_h, max_y % mcu_h);

  // save the current image block size
  uint32_t win_w = mcu_w;
  uint32_t win_h = mcu_h;

  // record the current time so we can measure how long it takes to draw an image
  uint32_t drawTime = millis();

  // save the coordinate of the right and bottom edges to assist image cropping
  // to the screen size
  max_x += xpos;
  max_y += ypos;

  // Fetch data from the file, decode and display
  while (JpegDec.read()) {  // While there is more data in the file
    pImg = JpegDec.pImage;  // Decode a MCU (Minimum Coding Unit, typically a 8x8 or 16x16 pixel block)

    // Calculate coordinates of top left corner of current MCU
    int mcu_x = JpegDec.MCUx * mcu_w + xpos;
    int mcu_y = JpegDec.MCUy * mcu_h + ypos;

    // check if the image block size needs to be changed for the right edge
    if (mcu_x + mcu_w <= max_x) win_w = mcu_w;
    else win_w = min_w;

    // check if the image block size needs to be changed for the bottom edge
    if (mcu_y + mcu_h <= max_y) win_h = mcu_h;
    else win_h = min_h;

    // copy pixels into a contiguous block
    if (win_w != mcu_w) {
      uint16_t* cImg;
      int p = 0;
      cImg = pImg + win_w;
      for (int h = 1; h < win_h; h++) {
        p += mcu_w;
        for (int w = 0; w < win_w; w++) {
          *cImg = *(pImg + w + p);
          cImg++;
        }
      }
    }

    // calculate how many pixels must be drawn
    uint32_t mcu_pixels = win_w * win_h;

    // draw image MCU block only if it will fit on the screen
    if ((mcu_x + win_w) <= tft.width() && (mcu_y + win_h) <= tft.height())
      tft.pushImage(mcu_x, mcu_y, win_w, win_h, pImg);
    else if ((mcu_y + win_h) >= tft.height())
      JpegDec.abort();  // Image has run off bottom of screen so abort decoding
  }

  tft.setSwapBytes(swapBytes);

  //showTime(millis() - drawTime); // These lines are for sketch testing only
}

void setup() {
  pinMode(bootButtonPin, INPUT_PULLUP);
  Serial.begin(115200);
//#######################################################################
  // Initialise TFT
  tft.init();
  tft.setRotation(0);

  tft.fillScreen(bgColor);
  tft.setTextColor(fgColor, bgColor);//
   Serial.println("TFT On.");
//########################################################################

//initialise the SD CARD
  sdSPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
  delay(1000);
  

const int MAX_RETRIES = 5;
int attempt = 0;
bool sdMounted = false;

 Serial.println("Firing up SD.");

while (attempt < MAX_RETRIES && !sdMounted) {
    sdSPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
    delay(1000);
    if (!SD.begin(SD_CS, sdSPI)) {
        Serial.println("SD Card Mount Failed! Retrying..." + String(attempt));
        attempt++;
        delay(100); // Small delay before retrying
    } else {
        Serial.println("SD Card initialised.");
        sdMounted = true;
        
    }
}


if (!sdMounted) {
    Serial.println("Failed to mount SD card after 5 attempts.");
    ESP.restart();
}
//####################################################################

  Serial.println("SETUP - LOADING CONFIG....");

  LoadConfig();
  tft.fillScreen(bgColor);
  tft.setTextColor(fgColor, bgColor);

  centerX = SCREEN_WIDTH / 2;
  centerY = SCREEN_HEIGHT / 2;

  WiFiManager wm;
  WiFiManagerParameter custom_server("serverip", "Server IP:", SERV.c_str(), 16);
  WiFiManagerParameter custom__port("serverport", "Server Port:", PORT.c_str(), 6);
  WiFiManagerParameter custom__kgid("kegid", "Keg ID:", KGID.c_str(), 6);
  WiFiManagerParameter custom__bg("backcol", "Background Colour (hex 0000-FFFF):", BCOL.c_str(), 8);
  WiFiManagerParameter custom__fg("forecol", "Foreground Colour (hex 0000-FFFF):", FCOL.c_str(), 8);


  wm.addParameter(&custom_server);
  wm.addParameter(&custom__port);
  wm.addParameter(&custom__kgid);
  wm.addParameter(&custom__bg);
  wm.addParameter(&custom__fg);

  tft.drawCentreString("No Wifi. Connect to", centerX, 30, FONT_SIZE);
  tft.drawCentreString("Wifi: Beer-Tap-Screen ", centerX, 60, FONT_SIZE);
  tft.drawCentreString("Via mobile to configure.", centerX, 90, FONT_SIZE);

   Serial.println("SETUP  - STARTING WIFI...");

  bool res = wm.autoConnect("Beer-Tap-Screen", "ilovebeer");
  if (!res) {
    Serial.println("Failed to connect to wifi");
    tft.fillScreen(bgColor);
    tft.drawCentreString("Failed to connect.", centerX, 30, FONT_SIZE);
    tft.drawCentreString("Device will restart.", centerX, 60, FONT_SIZE);
    delay(5000);
    ESP.restart();
  } else {
    Serial.println("Connected to wifi! :)");
    tft.fillScreen(bgColor);
    tft.drawCentreString("Connected to Wifi!", centerX, 30, FONT_SIZE);

//Just incase any settings have been modified - if not it should just resvae the settings it has already loaded.

    strncpy(serverip, custom_server.getValue(), sizeof(serverip) - 1);
    serverip[sizeof(serverip) - 1] = '\0';
    strncpy(serverport, custom__port.getValue(), sizeof(serverport) - 1);
    serverport[sizeof(serverport) - 1] = '\0';
    strncpy(kegid, custom__kgid.getValue(), sizeof(kegid) - 1);
    kegid[sizeof(kegid) - 1] = '\0';
    SERV = custom_server.getValue();
    PORT = custom__port.getValue();
    KGID = custom__kgid.getValue();
    BCOL = custom__bg.getValue();
    FCOL = custom__fg.getValue();
    BCOL.trim();
    FCOL.trim();
    ApplyConfiguredColors();

    Serial.println ("S=" + SERV);
    Serial.println ("P=" + PORT);
    Serial.println ("K=" + KGID);
    Serial.println ("B=" + BCOL);
    Serial.println ("F=" + FCOL);

  // write them to config so they arent forgotten
  if (SERV == "" || PORT == "" || KGID == "" || BCOL == "" || FCOL == ""){
      Serial.println("At least one of the values returned from wifi config was blank");
      Serial.println("Not saving CONFIG.");
      LoadConfig();
  } else {
      Serial.println("All of the values returned from wifi config were NON BLANK");
      Serial.println("SAVING CONFIG.");
      WriteCONFIG();
  }
    delay(6000);
  }
Serial.println("SETUP - REQUESTING JSON.");
GetJSON(SERV, PORT);
Serial.println("SETUP - DRAW SCREEN.");
DrawScreen();
firstrun = false;
}

void loop() {
  static unsigned long pressStartTime = 0;
  static bool longPressTriggered = false;

  //TS_Point p = touchscreen.getPoint();

//Serial.println(p.x);
//Serial.println(p.y);
//Serial.println(p.z);
  // Check if touch is valid (pressure and coordinates)
int buttonState = digitalRead(bootButtonPin);

  if (buttonState == LOW) { // Button is pressed
      if (pressStartTime == 0) {
        pressStartTime = millis();
      }

      if (!longPressTriggered && (millis() - pressStartTime >= 3000)) {
        longPressTriggered = true;
        Serial.println("Long press detected! Opening settings...");
        showsettings();

        WiFiManager wm;
        WiFiManagerParameter custom_server("serverip", "Server IP:", SERV.c_str(), 16);
        WiFiManagerParameter custom__port("serverport", "Server Port:", PORT.c_str(), 6);
        WiFiManagerParameter custom__kgid("kegid", "Keg ID:", KGID.c_str(), 6);
        WiFiManagerParameter custom__bg("backcol", "Background Colour (hex 0000-FFFF):", BCOL.c_str(), 8);
        WiFiManagerParameter custom__fg("forecol", "Foreground Colour (hex 0000-FFFF):", FCOL.c_str(), 8);

        wm.addParameter(&custom_server);
        wm.addParameter(&custom__port);
        wm.addParameter(&custom__kgid);
        wm.addParameter(&custom__bg);
        wm.addParameter(&custom__fg);
        wm.setConfigPortalTimeout(timeout);

          if (!wm.startConfigPortal("Beer-Tap-Screen","ilovebeer")) {
            Serial.println("failed to connect and hit timeout");
            delay(3000);
            //reset and try again, or maybe put it to deep sleep
            ESP.restart();
          }
            else
          {
            // only need to write the config if the settings were changes.
            // if it times out then no change


            SERV = custom_server.getValue();
            PORT = custom__port.getValue();
            KGID = custom__kgid.getValue();
            BCOL = custom__bg.getValue();
            FCOL = custom__fg.getValue();
            BCOL.trim();
            FCOL.trim();
            ApplyConfiguredColors();

            Serial.println ("New values from config page:");
            Serial.println ("S=" + SERV);
            Serial.println ("P=" + PORT);
            Serial.println ("K=" + KGID);
            Serial.println ("B=" + BCOL);
            Serial.println ("F=" + FCOL);

            //WriteCONFIG();

              if (SERV == "" || PORT == "" || KGID == "" || BCOL == "" || FCOL == ""){
                  Serial.println("LOOP - At least one of the values returned from wifi config was blank");
                  Serial.println("LOOP - Not saving CONFIG.");
                  LoadConfig();
              } else {
                  Serial.println("LOOP - All of the values returned from wifi config were NON BLANK");
                  Serial.println("LOOP - SAVING CONFIG.");
                  WriteCONFIG();
              }
          delay(200); // Simple debounce
          }


            settingschanged = true;
            firstrun = true;
          }
      }
 

    else {
        // No valid touch -> reset timer and flag
        pressStartTime = 0;
        longPressTriggered = false;
    }


  // Normal operations
  GetJSON(SERV, PORT);

double br = beerRemaining(JKCAP2.toDouble(),JKEMP2.toDouble(),JKCUR2.toDouble());

pnts = KGtoPints(JKEMP2.toDouble(),JKCAP2.toDouble(),JKCUR2.toDouble());


////Serial.println("FULL: " + JKCAP2);
//Serial.println("EMPT: " + JKEMP2);
//Serial.println("CUR: " + JKCUR2);
//Serial.println("Percentage Remaining: " + String(br));
//test
  
  DrawScreen();
  DrawRemaining(br);
  delay(10000);
}
  





