/*
Steering Wheel Control Bridge

Car Support:
- Ford Fusion 2010 - 2012

Radio Support:
Pioneer Stereo DMH-W2770NEX

Huge thanks to https://github.com/bigevtaylor for the
original code and for replying to my issue on the original
repo with diagrams!
For original code, see: https://github.com/bigevtaylor/arduino-swc

Thank you to Drew Hinds for helping me with reverse engineering
the Ford Fusion's SWC, finding documentation, and identifying
certain wire functions.
*/

#include <SPI.h>
#include <Arduino.h>
#include <AceButton.h>
using ace_button::AceButton;
using ace_button::ButtonConfig;


//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

// The ADC pin used by the resistor ladder, coming from the SWC.
static const uint8_t SWC_ANALOG_PIN = A0;

// Digipot constants
int digipotWiperPin = 10; // The digital pin used to control the digipot 
int digipotWiperTip = B00000000;
int digipotWiperRing = B00010000;
int digipotDefaultValue = 0; // 0 is ground, 255 is float

static const uint8_t NUM_BUTTONS = 5;
static const bool ALLOW_LONG_PRESS = true;

static AceButton b0(nullptr, 0);
static AceButton b1(nullptr, 1);
static AceButton b2(nullptr, 2);
static AceButton b3(nullptr, 3);
static AceButton b4(nullptr, 4);

// button 5 (&b4) cannot be used because it represents "no button pressed"
static AceButton* const BUTTONS[NUM_BUTTONS] = {
    &b0, &b1, &b2, &b3, &b4,
};

static const uint8_t NUM_LEVELS = NUM_BUTTONS + 1;
static const uint16_t LEVELS[NUM_LEVELS] = {
  68.83,   // Volume -
  128.59,  // Volume +
  219.56,  // Seek +
  343.43,  // Seek -
  513.35,  // Source
  1118.35,  // Open
};


//-----------------------------------------------------------------------------
// Extend LadderButtonConfig to override the getVirtualPin method
//-----------------------------------------------------------------------------

class LadderButtonConfig : public ace_button::LadderButtonConfig {
  public:
    // Inheriting the constructor from the base class
    using ace_button::LadderButtonConfig::LadderButtonConfig;

    // Override the getVirtualPin method
    uint8_t getVirtualPin() const override {
        uint16_t raw = analogRead(SWC_ANALOG_PIN);
        uint16_t level = calculateLevel(raw);

        // Serial.print(F("Analog Level: "));
        // Serial.print(raw);
        // Serial.print(F("; AdjLevel: "));
        // Serial.println(R2);

        return altExtractIndex(NUM_LEVELS, LEVELS, level);
    }

    uint16_t calculateLevel(uint16_t raw) const {
        int vin = 5; // Using 5V through the known 10k resistor
        float vOut = 0;
        float r1 = 10000; // Using a known 10k resistor
        float buffer = 0;

        buffer = raw * vin;
        vOut = (buffer)/1024.0;
        buffer = (vin/vOut) - 1;
        return r1 * buffer;
    }

    altExtractIndex(uint8_t numLevels, uint16_t const levels[], uint16_t level) {
      uint8_t i;
      for (i = 0; i < numLevels - 1; i++) {

        // NOTE(brian): This will overflow a 16-bit ADC. If we need to support that,
        // a possible formula that avoids uint32_t instructions might be something
        // like:
        //    threshold = levels[i]/2 + levels[i+1]/2
        //        + (((levels[i] & 0x1) + (levels[i+1] & 0x1)) / 2)
        uint16_t threshold = (levels[i] + levels[i+1]) / 2;

        if (level < threshold) return i;
      }

      // Return the index of the last level (i.e. numLevels - 1), to indicate
      // "nothing found".
      return i;
    }
};


//-----------------------------------------------------------------------------
// Digipot wiper functions
//-----------------------------------------------------------------------------

void wrTip(int digiValue, int delayMs) {
  digitalWrite(digipotWiperPin, LOW);
  SPI.transfer(digipotWiperTip);
  SPI.transfer(digiValue);
  Serial.println(" Button Press");
  delay(delayMs);
  SPI.transfer(digipotWiperTip);
  SPI.transfer(digipotDefaultValue);
  Serial.println(" Button Release TIP");
  digitalWrite(digipotWiperPin, HIGH);
}

void wrTipHold(int digiValue) {
  digitalWrite(digipotWiperPin, LOW);
  SPI.transfer(digipotWiperTip);
  SPI.transfer(digiValue);
  Serial.println(" Hold TIP");
  digitalWrite(digipotWiperPin, HIGH);
}

void wrTipRelease() {
  digitalWrite(digipotWiperPin, LOW);
  SPI.transfer(digipotWiperTip);
  SPI.transfer(digipotDefaultValue);
  Serial.println(" Button Release TIP");
  digitalWrite(digipotWiperPin, HIGH);
}

void wrRing(int digiValue, int delayMs) {
  digitalWrite(digipotWiperPin, LOW);
  SPI.transfer(digipotWiperRing);
  SPI.transfer(digipotDefaultValue);
  Serial.println(" Select Ring, ground");
  SPI.transfer(digipotWiperTip);
  SPI.transfer(digiValue);
  Serial.println(" Select Tip, button");
  delay(delayMs);
  SPI.transfer(digipotWiperTip);
  SPI.transfer(digipotDefaultValue);
  SPI.transfer(digipotWiperRing);
  SPI.transfer(255); // float
  Serial.println(" Button Release Ring");
  digitalWrite(digipotWiperPin, HIGH);
}


//-----------------------------------------------------------------------------
// Configure AceButton
//-----------------------------------------------------------------------------

// The LadderButtonConfig constructor binds the AceButton objects in the BUTTONS
// array to the LadderButtonConfig.
static LadderButtonConfig buttonConfig(
  SWC_ANALOG_PIN, NUM_LEVELS, LEVELS, NUM_BUTTONS, BUTTONS
);

// The event handler for the buttons.
void handleEvent(AceButton* button, uint8_t eventType, uint8_t buttonState) {

  // Print out a message for all events.
  Serial.print(F("handleEvent(): "));
  Serial.print(F("virtualPin: "));
  Serial.print(button->getPin());
  Serial.print(F("; eventType: "));
  Serial.print(AceButton::eventName(eventType));
  Serial.print(F("; buttonState: "));
  Serial.println(buttonState);

//   switch (eventType) {
//     case AceButton::kEventPressed:
//       Serial.println("BIG BIG");
//       break;
//     case AceButton::kEventReleased:
//       Serial.println("SMALL SMALL");
//       break;
//   }

    switch (button->getPin()) {
      case 0:
        if (eventType == AceButton::kEventPressed) {
          Serial.println("Volume - (Press)");
          wrTip(55, 50); // 24kOhm
        } else if (ALLOW_LONG_PRESS && eventType == AceButton::kEventLongPressed) {
          Serial.println("Volume - (Hold)");
          wrTipHold(55); // 24kOhm
        } else if (ALLOW_LONG_PRESS && eventType == AceButton::kEventReleased) {
          Serial.println("Volume - (Release)");
          wrTipRelease();
        }
        break;
      case 1:
        if (eventType == AceButton::kEventPressed) {
          Serial.println("Volume + (Press)");
          wrTip(42, 50); // 16kOhm
        } else if (ALLOW_LONG_PRESS && eventType == AceButton::kEventLongPressed) {
          Serial.println("Volume + (Hold)");
          wrTipHold(42); // 16kOhm
        } else if (ALLOW_LONG_PRESS && eventType == AceButton::kEventReleased) {
          Serial.println("Volume + (Release)");
          wrTipRelease();
        }
        break;
      case 2:
        if (eventType == AceButton::kEventPressed) {
            Serial.println("Seek + (Press)");
            wrTip(19, 50); // 8kOhm
        } else if (ALLOW_LONG_PRESS && eventType == AceButton::kEventLongPressed) {
            Serial.println("Seek + (Hold)");
            wrTipHold(19); // 8kOhm
        } else if (ALLOW_LONG_PRESS && eventType == AceButton::kEventReleased) {
            Serial.println("Seek + (Release)");
            wrTipRelease();
        }
        break;
      case 3:
        if (eventType == AceButton::kEventPressed) {
            Serial.println("Seek - (Press)");
            wrTip(27, 50); // 11,25kOhm
        } else if (ALLOW_LONG_PRESS && eventType == AceButton::kEventLongPressed) {
            Serial.println("Seek - (Hold)");
            wrTipHold(27); // 11,25kOhm
        } else if (ALLOW_LONG_PRESS && eventType == AceButton::kEventReleased) {
            Serial.println("Seek - (Release)");
            wrTipRelease();
        }
        break;
      case 4:
        if (eventType == AceButton::kEventPressed) {
            Serial.println("Source");
            wrTip(2, 50); // 1.2kOhm
        } else if (ALLOW_LONG_PRESS && eventType == AceButton::kEventLongPressed) {
            Serial.println("Source (Hold)");
            wrTipHold(2); // 1.2kOhm
        } else if (ALLOW_LONG_PRESS && eventType == AceButton::kEventReleased) {
            Serial.println("Source (Release)");
            wrTipRelease();
        }
        break;
    }
}

// On most processors, this should be called every 4-5ms or faster, if the
// default debouncing time is ~20ms. On a ESP8266, we must sample *no* faster
// than 4-5 ms to avoid disconnecting the WiFi connection. See
// https://github.com/esp8266/Arduino/issues/1634 and
// https://github.com/esp8266/Arduino/issues/5083. To be safe, let's rate-limit
// this on all processors to about 200 samples/second.
void checkButtons() {
  static uint16_t prev = millis();

  // DO NOT USE delay(5) to do this.
  // The (uint16_t) cast is required on 32-bit processors, harmless on 8-bit.
  uint16_t now = millis();
  if ((uint16_t) (now - prev) >= 5) {
    prev = now;
    buttonConfig.checkButtons();
  }
}

//-----------------------------------------------------------------------------

void setup() {
  delay(1000); // some microcontrollers reboot twice
  Serial.begin(115200);
  while (! Serial); // Wait until Serial is ready - Leonardo/Micro
  Serial.println(F("setup(): begin"));

  // Don't use internal pull-up resistor because it will change the effective
  // resistance of the resistor ladder.
  pinMode(SWC_ANALOG_PIN, INPUT);

  // Configure the ButtonConfig with the event handler, and enable all higher
  // level events.
  buttonConfig.setEventHandler(handleEvent);
  buttonConfig.setFeature(ButtonConfig::kFeatureClick);
  buttonConfig.setFeature(ButtonConfig::kFeatureDoubleClick);
  buttonConfig.setFeature(ButtonConfig::kFeatureLongPress);
  buttonConfig.setFeature(ButtonConfig::kFeatureRepeatPress);

  Serial.println(F("setup(): ready"));
}

void loop() {
  checkButtons();
}
