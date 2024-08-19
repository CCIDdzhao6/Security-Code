#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>

// If using the breakout with SPI, define the pins for SPI communication.
#define PN532_SCK  (2)
#define PN532_MOSI (3)
#define PN532_SS   (4)
#define PN532_MISO (5)

#define PN532_IRQ   (2)
#define PN532_RESET (3)  // Not connected by default on the NFC Shield

Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);

// Global variable to store the key value
uint8_t key = 64;

void setup(void) {
  Serial.begin(115200);
  while (!Serial) delay(10); // for Leonardo/Micro/Zero

  Serial.println("Hello!");

  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (! versiondata) {
    Serial.println("Didn't find PN53x board");
    while (1); // halt
  }
  // Got ok data, print it out!
  Serial.print("Found chip PN5"); Serial.println((versiondata >> 24) & 0xFF, HEX);
  Serial.print("Firmware ver. "); Serial.print((versiondata >> 16) & 0xFF, DEC);
  Serial.print('.'); Serial.println((versiondata >> 8) & 0xFF, DEC);

  Serial.println("Waiting for an ISO14443A Card ...");
  Serial.println("Please enter the number to write to the NFC tag:");
}

void loop(void) {
  static bool numberEntered = false;

  // Check if a number has been entered in the serial monitor
  if (Serial.available() > 0 && !numberEntered) {
    key = Serial.parseInt(); // Read the number entered
    if (key != 0) { // Ensure a valid number is entered
      Serial.print("Number entered: ");
      Serial.println(key);
      Serial.print("Sending ");
      Serial.println(key);
      numberEntered = true;
    } else {
      Serial.println("Invalid number entered, please enter a valid number:");
    }
  }

  if (numberEntered) {
    uint8_t success;
    uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
    uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
    
    success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);

    if (success) {
      // Display some basic information about the card
      Serial.println("Found an ISO14443A card");
      Serial.print("  UID Length: "); Serial.print(uidLength, DEC); Serial.println(" bytes");
      Serial.print("  UID Value: ");
      nfc.PrintHex(uid, uidLength);
      Serial.println("");

      if (uidLength == 4) {
        // We probably have a Mifare Classic card ...
        Serial.println("Seems to be a Mifare Classic card (4 byte UID)");

        // Now we need to try to authenticate it for read/write access
        // Try with the factory default KeyA: 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF
        Serial.println("Trying to authenticate block 4 with default KEYA value");
        uint8_t keya[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

        // Start with block 4 (the first block of sector 1) since sector 0
        // contains the manufacturer data and it's probably better just
        // to leave it alone unless you know what you're doing
        success = nfc.mifareclassic_AuthenticateBlock(uid, uidLength, 4, 0, keya);

        if (success) {
          Serial.println("Sector 1 (Blocks 4..7) has been authenticated");
          uint8_t data[16];

          // Write the key value to block 4
          memset(data, 0, sizeof(data)); // Clear the buffer
          data[0] = key; // Write the key value to the first byte
          success = nfc.mifareclassic_WriteDataBlock(4, data);

          if (success) {
            Serial.println("Successfully wrote key value to Block 4");

            // Try to read the contents of block 4
            success = nfc.mifareclassic_ReadDataBlock(4, data);

            if (success) {
              // Data seems to have been read ... spit it out
              Serial.println("Reading Block 4:");
              nfc.PrintHexChar(data, 16);
              Serial.println("");

              // Wait a bit before reading the card again
              delay(1000);
            } else {
              Serial.println("Ooops ... unable to read the requested block. Try another key?");
            }
          } else {
            Serial.println("Ooops ... unable to write to the requested block. Try another key?");
          }
        } else {
          Serial.println("Ooops ... authentication failed: Try another key?");
        }
      }

      if (uidLength == 7) {
        // We probably have a Mifare Ultralight card ...
        Serial.println("Seems to be a Mifare Ultralight tag (7 byte UID)");

        // Write the key value to page 4
        Serial.println("Writing key value to page 4");
        uint8_t data[4];
        memset(data, 0, sizeof(data)); // Clear the buffer
        data[0] = key; // Write the key value to the first byte
        success = nfc.mifareultralight_WritePage(4, data);

        if (success) {
          Serial.println("Successfully wrote key value to page 4");

          // Try to read the first general-purpose user page (#4)
          Serial.println("Reading page 4");
          success = nfc.mifareultralight_ReadPage(4, data);
          if (success) {
            // Data seems to have been read ... spit it out
            nfc.PrintHexChar(data, 4);
            Serial.println("");

            // Wait a bit before reading the card again
            delay(1000);
          } else {
            Serial.println("Ooops ... unable to read the requested page!?");
          }
        } else {
          Serial.println("Ooops ... unable to write to the requested page!?");
        }
      }

      // Reset to wait for the next number
      numberEntered = false;
      Serial.println("Please enter the next number to write to the NFC tag:");
    } else {
      Serial.println("Waiting for an ISO14443A Card ...");
      delay(1000); // Add a delay to avoid flooding the serial output
    }
  }
}
