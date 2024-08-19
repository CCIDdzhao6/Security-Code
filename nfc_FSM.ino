/*
API calls from the website should trigger an interrupt, as parts of the code block execution
until the relevant event occurs. Thus the interrupt allows the website to trigger parts of the code outside

pull down resistors = 5k

State Hierarchy

overide *most powerful*
   |
disable
   |
standard states *Least powerful*

IO Hierarchy



*/


#include <WiFi.h>
#include <HTTPClient.h>
#include <Base64.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>

#define PN532_SCK  (2)
#define PN532_MOSI (3)
#define PN532_SS   (4)
#define PN532_MISO (5)

// If using the breakout or shield with I2C, define just the pins connected
// to the IRQ and reset lines.  Use the values below (2, 3) for the shield!
#define PN532_IRQ   (2)
#define PN532_RESET (3) 
#define UID_LENGTH  (7)
Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);


//pinout for IO
#define LOCK_BUTTON_PIN    (14)
#define RELAY_ENABLE_PIN   (16)
#define OVERIDE_KEY_PIN    (2)
#define DISABLE_KEY_PIN    (4)





//GLOBAL VARIABLES----------------------------------------------------------
// Replace with relevent network credentials
const char* ssid = "fuse33-equipment";
const char* password = "FuseEquipment";

// Replace with your Formidable Form API endpoint
const char* serverName = "https://fuse33.com/wp-json/frm/v2/entries";

// API key
const char* apiKey = "30HK-EG90-3UEP-7RMM";

// Define the ID array to match
const uint8_t ID[4] = {0x00, 0x00, 0x00, 0x00};
static uint8_t uid[UID_LENGTH];

// Define the IP address and MAC address
IPAddress ip;
byte mac[6];
String macString;

//check for a valid card read
uint8_t success;

//Overide and disable conditions
int overide_command = 0;
int disable_command = 0;
int stop_overide = 0;
int stop_disable = 0;
int manualLock = 0;
int lastManualLockState = LOW; 
int currentManualLockState;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 30;

// Define the state of the system
int state = 0;

// Defines the api return data and the authorization state
char apiReturn[20];
bool authorized = false;



//--------------------------------------------------------------------------

/*
Main code that takes the id of the NFC card being used on the NFC board and saves it in uid
*/
void scan_card() {

  uint8_t uidLength; // Length of the UID (4 or 7 bytes depending on ISO14443A card type)

  // Attempt to read the card's UID using the NFC reader
  uint8_t success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength,100);

  // If reading the UID is successful
  if (success) {
    // Create Basic Authentication header
    String authHeader = "Basic " + base64::encode(String(apiKey) + ":");
    // Add Authorization header
    // Display some basic information about the card
    Serial.println("Found an ISO14443A card");
    Serial.print("  UID Length: "); 
    Serial.print(uidLength, DEC); 
    Serial.println(" bytes");
    Serial.print("  UID Value: ");
    nfc.PrintHex(uid, uidLength); // Print the UID in hexadecimal format
    Serial.println(""); // Print a newline for readability
    
  }
  else
  {
   Serial.println("failed to read card"); 
  }
}

/*
Initialize of the ESP32 board, includes connecting to wifi, finding the ip and mac address of the ESP32
board, and sends the information to the webserver. Also checks for a NFC board to read NFC cards
*/
void setup() {
  // Initialize serial communication for Debugging
  Serial.begin(115200);
  Serial.println("Hello!");

  // Initialize NFC Object
  nfc.begin();

  HTTPClient http;
  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }

  Serial.println("Connected to the WiFi network");

  // Geting the IP Address
  ip = WiFi.localIP();

  // Geting the MAC address
  WiFi.macAddress(mac);

  //Formats mac address into readable string
  for(int j = 5; j >= 0; j--){
    if(j != 0){
      macString += String(mac[j], HEX) + ":";
    }else{
      macString += String(mac[j], HEX);
    }
  }

  // Specify content-type header
  http.begin(serverName); // Specify the URL
  http.addHeader("Content-Type", "application/json");

  // Create Basic Authentication header
  String authHeader = "Basic " + base64::encode(String(apiKey) + ":");
  // Add Authorization header
  http.addHeader("Authorization", authHeader);

  // Create JSON payload to send ip and mac address to the server
  String jsonAddresses = "{\"form_id\":\"114\",\"1307\":\"" + ip.toString() +"\",\"1308\":\"" + macString +"\"}";

  // Debug: Print the payload
  Serial.print("JSON Addresses: ");
  Serial.println(jsonAddresses);

  // Send HTTP POST request
  int httpResponseCode = http.POST(jsonAddresses);

  // Check the returning code
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println(httpResponseCode); // Print return code
    Serial.println(response); // Print request response payload
  } else {
    Serial.print("Error on sending POST: ");
    Serial.println(httpResponseCode);
  }

  //free resources
  http.end();

  
  uint32_t versiondata = nfc.getFirmwareVersion();

  //Checks to see if NFC Board is connected
  if (!versiondata) {
    Serial.print("Didn't find PN53x board");
    while (1); // halt
  }

  // Initialize relay pins
  pinMode(RELAY_ENABLE_PIN, OUTPUT);
  pinMode(LOCK_BUTTON_PIN, INPUT_PULLDOWN);
  pinMode(OVERIDE_KEY_PIN, INPUT_PULLDOWN);
  pinMode(DISABLE_KEY_PIN, INPUT_PULLDOWN);
  digitalWrite(RELAY_ENABLE_PIN, LOW);
}

/*
default state, locks out machine and waits for card or api authorization from server to be recieved
*/
void lock(){
    scan_card(); // Buffer to store the returned UID
    /* 
    Wait for an ISO14443A type cards (Mifare, etc.).  When one is found
    'uid' will be populated with the UID, and UID_LENGTH will indicate
    if the uid is 4 bytes (Mifare Classic) or 7 bytes (Mifare Ultralight)
    */
    if(success){
      HTTPClient http;
      http.begin(serverName); // Specify the URL
      http.addHeader("Content-Type", "application/json");

      // Create Basic Authentication header
      String authHeader = "Basic " + base64::encode(String(apiKey) + ":");

      // Add Authorization header
      http.addHeader("Authorization", authHeader);

      // Make a HTTP POST request
      if (WiFi.status() == WL_CONNECTED) {

        // variable parts of the JSON payload
        String idNumber = String(uid[0]) + ":"; // Use uid instead of data to avoid shadowing

        String request = "Door"; // Renamed to postData to avoid shadowing

        //building the idNumber
        for(int i = 1; i < UID_LENGTH; ++i){
            if(i !=UID_LENGTH-1){
            idNumber += String(uid[i]) + ":";
            }else{
            idNumber += String(uid[i]);
            }
        }

        // Create JSON payload of the request and the id of the card
        String jsonPayload = "{\"form_id\":\"114\",\"api_test_data\":\"" + request + "\",\"api_test_card_id\":\"" + idNumber + "\"}";

        // Debug: Print the payload
        Serial.print("JSON Payload: ");
        Serial.println(jsonPayload);

        // Send HTTP POST request
        int httpResponseCode = http.POST(jsonPayload);

        // Check the returning code
        if (httpResponseCode > 0) {
            String response = http.getString();
            Serial.println(httpResponseCode); // Print return code
            Serial.println(response); // Print request response payload
        } else {
            Serial.print("Error on sending POST: ");
            Serial.println(httpResponseCode);
        }

        // Free resources
        http.end();
        delay(2000);
     
    } else {
    Serial.println("Error in WiFi connection");
    } 
   
  }
  //debugging to see if FSM is working proporly, replace this with a access granted command from server
  if(uid[1] == 0x74){
      state = 1;
       for(int j = 0; j < UID_LENGTH; j++) uid[j] = 0x0; //resets the uid for the next card
    }
}

//allows the machine to turn on by triggering a relay to open
void unlock(){
    delay(1000);
    digitalWrite(RELAY_ENABLE_PIN, HIGH); // Trigger relay to open
    delay(1000);
}

//manual overide to turn on the machine via key
void overide(){
    digitalWrite(RELAY_ENABLE_PIN, HIGH);
    Serial.println("overide detected");
    stop_overide = digitalRead(OVERIDE_KEY_PIN);
    if(!stop_overide){
      state = 0;
      Serial.println("overide command stop");
    }else{
      state = 2;
    }
}

//maunal api overide that disables the machine
void disable(){
  digitalWrite(RELAY_ENABLE_PIN, LOW);
  Serial.println("disable command detected");
  stop_disable = digitalRead(DISABLE_KEY_PIN);
  if(overide_command){//checks if the overide command is turned on, if it is then switch to overide state
    Serial.println("disable stop, overide occured");
    state = 2;
  }else if(!stop_disable){//stops disable command if the key is turned in the off position
    Serial.println("disable command stop");
    state = 0;
  }else{
    state = 3;
  }
}

//main loop
void loop() {   
    //Physical User IO pins
    manualLock = digitalRead(LOCK_BUTTON_PIN); //Check if the lock button is pressed to end current session
    overide_command = digitalRead(OVERIDE_KEY_PIN); //Check if the overide key is inserted, and overides the system
    disable_command = digitalRead(DISABLE_KEY_PIN); //Check if the disable key is inserted, and disables the system

    //debounce delay from the manual lock button being pressed
    if(manualLock != lastManualLockState){
      lastDebounceTime = millis();
    }
    if((millis() - lastDebounceTime) > debounceDelay){
      if(manualLock != currentManualLockState){
        currentManualLockState = manualLock;
        if(manualLock){
          state = 0;
        }
      }
    }

    //debugging to check if the manual lock, disable, and overide are being detected
    lastManualLockState = manualLock;
    Serial.print("Manual Lock");
    Serial.println(manualLock);
    Serial.print("disable");
    Serial.println(disable_command);
    Serial.print("overide");
    Serial.println(overide_command);

    //Logic for switching to special states in the FSM
    if(overide_command){
      state = 3;
    }else if(disable_command){
      state = 4;
    }

    /*
    FSM Lookup Table
    state 0: lock
    state 1: unlock
    state 2: overide
    state 3: disable\
    */
    switch (state)
    {
    case 0:
        lock();
        break;
    case 1:
        unlock();
        break;
    case 2: 
        overide();
        break;
    case 3: 
        disable();
        break;
    default:
        lock();
        break;
    }
  
}
