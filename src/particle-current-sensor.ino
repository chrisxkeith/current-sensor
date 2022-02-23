const String githubHash = "to be replaced after git push";
const bool debug = false;

#include <limits.h>

class JSonizer {
  public:
    static void addFirstSetting(String& json, String key, String val);
    static void addSetting(String& json, String key, String val);
    static String toString(bool b);
    static void addJSON(String& json, String key, String& source);
};

class Utils {
  public:
    static bool publishDelay;
    static int setInt(String command, int& i, int lower, int upper);
    static void publish(String event, String data);
    static void publishJson();
    static String getName();
};

class TimeSupport {
  private:
    unsigned long ONE_DAY_IN_MILLISECONDS;
    unsigned long lastSyncMillis;
    String timeZoneString;
    String getSettings();
  public:
    int timeZoneOffset;
    TimeSupport(int timeZoneOffset, String timeZoneString);
    String timeStrZ(time_t t);
    String nowZ();
    void handleTime();
    int setTimeZoneOffset(String command);
    void publishJson();
};

class MillisecondTimer {
  private:
    long start;
    String name;
  public:
    MillisecondTimer(String name) {
      this->name = name;
      this->name.replace(" ", "_");
      doReset();
    }

    ~MillisecondTimer() {
      publish();
    }

    void doReset() {
      start = millis();
    }

    void publish() {
      if (debug) {
        long duration = millis() - start;
        String json("{");
        JSonizer::addFirstSetting(json, "milliseconds", String(duration));
        json.concat("}");
        Utils::publish("Diagnostic", json);
      }
    }
};

void JSonizer::addFirstSetting(String& json, String key, String val) {
    json.concat("\"");
    json.concat(key);
    json.concat("\":\"");
    json.concat(val);
    json.concat("\"");
}

void JSonizer::addSetting(String& json, String key, String val) {
    json.concat(",");
    addFirstSetting(json, key, val);
}

void JSonizer::addJSON(String& json, String key, String& source) {
  json.concat(",\"");
  json.concat(key);
  json.concat("\":");
  json.concat(source);
}

String JSonizer::toString(bool b) {
    if (b) {
        return "true";
    }
    return "false";
}

bool Utils::publishDelay = false;
int Utils::setInt(String command, int& i, int lower, int upper) {
    int tempMin = command.toInt();
    if (tempMin > lower && tempMin < upper) {
        i = tempMin;
        return 1;
    }
    return -1;
}
void Utils::publish(String event, String data) {
    Particle.publish(event, data, 1, PRIVATE);
    if (publishDelay) {
      delay(1000);
    }
}
int publishRateInSeconds = 3;
system_tick_t lastPublishInSeconds = 0;
unsigned int displayIntervalInSeconds = 3;
unsigned int lastDisplayInSeconds = 0;

void Utils::publishJson() {
    String json("{");
    JSonizer::addFirstSetting(json, "githubHash", githubHash);
    JSonizer::addSetting(json, "githubRepo", "https://github.com/chrisxkeith/current-sensor.git");
    JSonizer::addSetting(json, "lastPublishInSeconds ", String(lastPublishInSeconds));
    JSonizer::addSetting(json, "publishRateInSeconds", String(publishRateInSeconds));
    JSonizer::addSetting(json, "lastDisplayInSeconds", String(lastDisplayInSeconds));
    JSonizer::addSetting(json, "displayIntervalInSeconds", String(displayIntervalInSeconds));
    json.concat("}");
    publish("Utils json", json);
}

String TimeSupport::getSettings() {
    String json("{");
    JSonizer::addFirstSetting(json, "lastSyncMillis", String(lastSyncMillis));
    JSonizer::addSetting(json, "timeZoneOffset", String(timeZoneOffset));
    JSonizer::addSetting(json, "timeZoneString", String(timeZoneString));
    JSonizer::addSetting(json, "internalTime", nowZ());
    json.concat("}");
    return json;
}

TimeSupport::TimeSupport(int timeZoneOffset, String timeZoneString) {
    this->ONE_DAY_IN_MILLISECONDS = 24 * 60 * 60 * 1000;
    this->timeZoneOffset = timeZoneOffset;
    this->timeZoneString = timeZoneString;
    Time.zone(timeZoneOffset);
    Particle.syncTime();
    this->lastSyncMillis = millis();
}

String TimeSupport::timeStrZ(time_t t) {
    String fmt("%a %b %d %H:%M:%S ");
    fmt.concat(timeZoneString);
    fmt.concat(" %Y");
    return Time.format(t, fmt);
}

String TimeSupport::nowZ() {
    return timeStrZ(Time.now());
}

void TimeSupport::handleTime() {
    if (millis() - lastSyncMillis > ONE_DAY_IN_MILLISECONDS) {    // If it's been a day since last sync...
                                                            // Request time synchronization from the Particle Cloud
        Particle.syncTime();
        lastSyncMillis = millis();
    }
}

int TimeSupport::setTimeZoneOffset(String command) {
    timeZoneString = "???";
    return Utils::setInt(command, timeZoneOffset, -24, 24);
}

void TimeSupport::publishJson() {
    Utils::publish("TimeSupport", getSettings());
}
TimeSupport    timeSupport(-8, "PST");

class Bucketizer {
  private:
     int    nBuckets;
     int    maxExpected;
     int*   buckets;
     int    nOutOfRange;
  public:
    Bucketizer(int nBuckets, int maxExpected) {
      this->nBuckets = nBuckets;
      this->maxExpected = maxExpected;
      this->buckets = new int[nBuckets];
      this->clear();
    }
    void clear() {
      for (int i = 0; i < this->nBuckets; i++) {
        this->buckets[i] = 0;
      }
      this->nOutOfRange = 0;
    }
    void addValue(int val) {
      if (val > this->maxExpected) {
        this->nOutOfRange++;
      } else {
        int bucketIndex = val / this->nBuckets;
        this->buckets[bucketIndex]++;
      }
    }
    String getValues() {
      String json("{");
      JSonizer::addFirstSetting(json, "nBuckets", String(this->nBuckets));
      JSonizer::addSetting(json, "maxExpected", String(this->maxExpected));
      JSonizer::addSetting(json, "nOutOfRange", String(this->nOutOfRange));
      JSonizer::addSetting(json, "bucketsize", String(this->maxExpected / this->nBuckets));
      String s = String();
      for (int i = 0; i < this->nBuckets; i++) {
        s.concat(",");
        s.concat(this->buckets[i]);
      }
      JSonizer::addSetting(json, "buckets", s);
      json.concat("}");
      return json;
    }
};

#include <math.h>

class Sensor {
  private:
    int     pin;
    String  name;
    int     nSamples;
    double  total;
    int     max;
    int     minValue;
    Bucketizer*  bucketizer;

  public:
    Sensor(int pin, String name) {
      this->pin = pin;
      this->name = name;
      pinMode(pin, INPUT);
      this->bucketizer = new Bucketizer(20,300);
      clear();
    }
    
    int getMaxValue() { return max; }
    int getMinValue() { return minValue; }
    String getName() { return name; }

    void sample() {
      int v;
      if (pin >= A0 && pin <= A5) {
        v = analogRead(pin);
      } else {
        v = digitalRead(pin);
      }
      total += v;
      nSamples++;
      if (v > max) {
        max = v;
      }
      if (v < minValue) {
        minValue = v;
      }
      this->bucketizer->addValue(v);
    }
    
    void clear() {
      nSamples = 0;
      total = 0.0;
      max = 0;
      minValue = 4095;
      bucketizer->clear();
    }

    String getState() {
      String json("{");
      JSonizer::addFirstSetting(json, "name", name);
      JSonizer::addSetting(json, "nSamples", String(nSamples));
      JSonizer::addSetting(json, "total", String(total));
      JSonizer::addSetting(json, "getAverageValue()", String(this->getAverageValue()));
      JSonizer::addSetting(json, "max", String(max));
      JSonizer::addSetting(json, "minValue", String(minValue));
      JSonizer::addSetting(json, "rms()", String(this->rms()));
      String vals(this->bucketizer->getValues());
      JSonizer::addJSON(json, "buckets", vals);
      json.concat("}");
      return json;
    }

    int publishState() {
      Utils::publish("Diagnostic", this->getState());
      return 1;
    }

    int getAverageValue() {
      return round(total / nSamples);
    }

    int rms() {
      int v = this->getMaxValue();  
      v = v * 3300 / 1023;    // get actual voltage (ADC voltage reference = 3.3V)
      v /= sqrt(2.0);           // calculate the RMS value ( = peak/√2 )
      return v;
    }
};

#include <SparkFunMicroOLED.h>

class OLEDWrapper {
  public:
    MicroOLED* oled = new MicroOLED();

    OLEDWrapper() {
        oled->begin();    // Initialize the OLED
        oled->clear(ALL); // Clear the display's internal memory
        oled->display();  // Display what's in the buffer (splashscreen)
        delay(1000);     // Delay 1000 ms
        oled->clear(PAGE); // Clear the buffer.
    }

    void display(String title, int font, uint8_t x, uint8_t y) {
        oled->clear(PAGE);
        oled->setFontType(font);
        oled->setCursor(x, y);
        oled->print(title);
        oled->display();
    }

    void display(String title, int font) {
        display(title, font, 0, 0);
    }

    void invert(bool invert) {
      oled->invert(invert);
    }

    void displayNumber(String s) {
        // To reduce OLED burn-in, shift the digits (if possible) on the odd minutes.
        int x = 0;
        if (Time.minute() % 2) {
            const int MAX_DIGITS = 5;
            if (s.length() < MAX_DIGITS) {
                const int FONT_WIDTH = 12;
                x += FONT_WIDTH * (MAX_DIGITS - s.length());
            }
        }
        display(s, 3, x, 0);
    }

    bool errShown = false;
    void verify(int xStart, int yStart, int xi, int yi) {
      if (!errShown && (xi >= oled->getLCDWidth() || yi >= oled->getLCDHeight())) {
        String json("{");
        JSonizer::addSetting(json, "xStart", String(xStart));
        JSonizer::addSetting(json, "yStart", String(yStart));
        JSonizer::addSetting(json, "xi", String(xi));
        JSonizer::addSetting(json, "yi", String(yi));
        json.concat("}");
        Utils::publish("super-pixel coordinates out of range", json);
        errShown = true;
      }
    }

   void superPixel(int xStart, int yStart, int xSuperPixelSize, int ySuperPixelSize, int pixelVal,
         int left, int right, int top, int bottom) {
     int pixelSize = xSuperPixelSize * ySuperPixelSize;
     if (pixelVal < 0) {
       pixelVal = 0;
     } else if (pixelVal >= pixelSize) {
       pixelVal = pixelSize - 1;
     }
     for (int xi = xStart; xi < xStart + xSuperPixelSize; xi++) {
       for (int yi = yStart; yi < yStart + ySuperPixelSize; yi++) {
         verify(xStart, yStart, xi, yi);

         // Value between 1 and pixelSize - 2,
         // so pixelVal of 0 will have all pixels off
         // and pixelVal of pixelSize - 1 will have all pixels on. 
         int r = (rand() % (pixelSize - 2)) + 1;
         if (r < pixelVal) { // lower value maps to white pixel.
           oled->pixel(xi, yi);
         }
       }
     }
   }

    void publishJson() {
        String json("{");
        JSonizer::addFirstSetting(json, "getLCDWidth()", String(oled->getLCDWidth()));
        JSonizer::addSetting(json, "getLCDHeight()", String(oled->getLCDHeight()));
        json.concat("}");
        Utils::publish("OLED", json);
    }

    void testPattern() {
      int xSuperPixelSize = 6;	
      int ySuperPixelSize = 6;
      int pixelSize = xSuperPixelSize * ySuperPixelSize; 
      float diagonalDistance = sqrt((float)(xSuperPixelSize * xSuperPixelSize + ySuperPixelSize * ySuperPixelSize));
      float factor = (float)pixelSize / diagonalDistance;
      int pixelVals[64];
      for (int i = 0; i < 64; i++) {
        int x = (i % 8);
        int y = (i / 8);
        pixelVals[i] = (int)(round(sqrt((float)(x * x + y * y)) * factor));
      }
      displayArray(xSuperPixelSize, ySuperPixelSize, pixelVals);
      delay(5000);
      displayArray(xSuperPixelSize, ySuperPixelSize, pixelVals);
      delay(5000);	
    }

    void displayArray(int xSuperPixelSize, int ySuperPixelSize, int pixelVals[]) {
      oled->clear(PAGE);
      for (int i = 0; i < 64; i++) {
        int x = (i % 8) * xSuperPixelSize;
        int y = (i / 8) * ySuperPixelSize;
        int left = x;
        int right = x;
        int top = y;
        int bottom = y;
        if (x > 0) {
          left = pixelVals[i - 1];
        }
        if (x < 7) {
          right = pixelVals[i + 1];
        }
        if (y > 0) {
          top = pixelVals[i - 8];
        }
        if (y < 7) {
          top = pixelVals[i + 8];
        }
        // This (admittedly confusing) switcheroo of x and y axes is to make the orientation
        // of the sensor (with logo reading correctly) match the orientation of the OLED.
        superPixel(y, x, ySuperPixelSize, xSuperPixelSize, pixelVals[i],
            left, right, top, bottom);
      }
      oled->display();
    }

    void clear() {
      oled->clear(ALL);
    }
};

OLEDWrapper oledWrapper;

Sensor currentSensor(A0,  "Current sensor");

class DryerMonitor {
  private:
    unsigned long minSecToMillis(unsigned long minutes, unsigned long seconds) {
      return (minutes * 60 * 1000) + (seconds * 1000);
    }
                  // When the dryer goes into wrinkle guard mode, it is off for 04:45,
                  // then powers on for 00:15.
    const unsigned long WRINKLE_GUARD_OFF = minSecToMillis(4, 45);
                  // Give onesself 30 seconds to get to the dryer.
    const unsigned int WARNING_IN_SECONDS = 30;
    const unsigned int WARNING_INTERVAL = minSecToMillis(0, WARNING_IN_SECONDS);
//    const unsigned int THRESHOLD = 198; // Current sensor reading where clothes dryer switches between on/off.
    const unsigned int THRESHOLD = 125 + ((255 - 125) / 2);   // Current sensor reading where hair dryer switches between Hi/off (for testing).

    enum class DryerState { Off, Drying, WrinkleGuardOff, WrinkleGuardOn };

    unsigned int previousChangeTimeInMS = millis();
                  // Assume that sensor unit has been initialized while dryer is off.
    DryerState previousDryerState = DryerState::Off;
    unsigned int previousCurrent = currentSensor.rms();
    unsigned int currentCurrent = 0;
    bool sentAlert = false;

    bool powerChanged() {
      currentCurrent = currentSensor.rms();
      if (this->previousCurrent <= THRESHOLD) {
        return (currentCurrent > THRESHOLD);
      }
      return currentCurrent <= THRESHOLD;
    }
    void sendAlert() {
      if (!this->sentAlert) {
        String s("Dryer will start tumble cycle in about ");
        s.concat(WARNING_IN_SECONDS);
        s.concat(" seconds.");
        this->sentAlert = true;
      }
    }
    String dryerStateStr(DryerMonitor::DryerState state) {
      switch (state) {
        case DryerState::Off: return "Off";
        case DryerState::Drying: return "Drying";
        case DryerState::WrinkleGuardOff: return "Wrinkle Cycle (Off)";
        case DryerState::WrinkleGuardOn: return "Wrinkle Cycle (On)";
        default: return "unknown DryerState";
      }
    }
    void displayStatus(DryerMonitor::DryerState s) {
      oledWrapper.display(this->dryerStateStr(s), this->STATE_FONT);
      delay(2000);
      oledWrapper.clear();
    }
  public:
    const static unsigned int STATE_FONT = 1;

    void doMonitor() {
      unsigned int nowMS = millis();
      unsigned int intervalSinceLastChange = nowMS - this->previousChangeTimeInMS;
      // Handle going back to DryerState::Off state.
      if (this->previousDryerState == DryerState::WrinkleGuardOff &&
          intervalSinceLastChange > 2 * this->WRINKLE_GUARD_OFF) {
        this->previousDryerState = DryerState::Off;
        this->previousChangeTimeInMS = nowMS;
      } else {
        if (this->powerChanged()) {
          switch (this->previousDryerState) {
            case DryerState::Off:
              this->previousDryerState = DryerState::Drying;
              break;
            case DryerState::Drying:
              this->previousDryerState = DryerState::WrinkleGuardOff;
              this->sentAlert = false;
              break;
            case DryerState::WrinkleGuardOff:
              this->previousDryerState = DryerState::WrinkleGuardOn;
              break;
            case DryerState::WrinkleGuardOn:
              this->previousDryerState = DryerState::WrinkleGuardOff;
              break;
          }
          this->sendStateChange();
          this->previousChangeTimeInMS = nowMS;
          this->previousCurrent = this->currentCurrent;
          oledWrapper.display(this->currentDryerState(), STATE_FONT);
        }
        if (this->previousDryerState == DryerState::WrinkleGuardOff &&
            intervalSinceLastChange > this->WRINKLE_GUARD_OFF - this->WARNING_INTERVAL) {
              this->sendAlert();
        }
      }
    }
    String currentDryerState() {
      return this->dryerStateStr(this->previousDryerState);
    }
    void displayStatuses() {
      this->displayStatus(DryerState::Off);
      this->displayStatus(DryerState::Drying);
      this->displayStatus(DryerState::WrinkleGuardOff);
      this->displayStatus(DryerState::WrinkleGuardOn);
    }
    void sendStateChange() {
      String json("{");
      JSonizer::addFirstSetting(json, "newState", this->dryerStateStr(this->previousDryerState));
      JSonizer::addSetting(json, "previousChangeTimeInMS", String(this->previousChangeTimeInMS));
      JSonizer::addSetting(json, "previousCurrent", String(this->previousCurrent));
      JSonizer::addSetting(json, "currentCurrent", String(this->currentCurrent));
      json.concat("}");
      Utils::publish("State change", json);
    }
    void start() {
      this->previousCurrent = currentSensor.rms();
    }
};
DryerMonitor dryerMonitor;

int sendState(String command) {
  dryerMonitor.sendStateChange();
  return 1;
}

void display_digits(unsigned int num) {
  oledWrapper.displayNumber(String(num));
}

int sumOfCurrent = 0;
int averageSamples = 0;
void updateAverage(int v) {
  sumOfCurrent += v;
  averageSamples++;
}
int publishAverage(String command) {
  int avg = sumOfCurrent / averageSamples;
  String json("{");
  JSonizer::addFirstSetting(json, "sumOfCurrent", String(sumOfCurrent));
  JSonizer::addSetting(json, "averageSamples", String(averageSamples));
  JSonizer::addSetting(json, "Average", String(avg));
  json.concat("}");
  Utils::publish("Average", json);
  return 1;
}
int resetAverage(String command) {
  sumOfCurrent = 0;
  averageSamples = 0;
  return 1;
}

// getSettings() is already defined somewhere.
int pubSettings(String command) {
    if (command.compareTo("") == 0) {
        Utils::publishJson();
    } else if (command.compareTo("time") == 0) {
        timeSupport.publishJson();
    } else if (command.compareTo("current") == 0) {
        currentSensor.publishState();
    } else {
        Utils::publish("GetSettings bad input", command);
    }
    return 1;
}
 
int pubData(String command) {
  Utils::publish(currentSensor.getName(), String(currentSensor.rms()));
  return 1;
}

int pubState(String command) {
  currentSensor.publishState();
  return 1;
}

String previousState = "not set";
void sample() {
                                    // 60 cycles / second -> 16.666... ms / cycle
  const int SAMPLE_INTERVAL = 100;  // ~1000 samples, ~6 cycles(?)
  int start = millis();
  while (millis() - start < SAMPLE_INTERVAL) {
    currentSensor.sample();
  }
  previousState = currentSensor.getState();
}

void clear() {
  currentSensor.clear();
}

// Use Particle console to publish an event, usually for the server to pick up.
int pubConsole(String paramStr) {
    const unsigned int  BUFFER_SIZE = 64;       // Maximum of 63 chars in an argument. +1 to include null terminator
    char                paramBuf[BUFFER_SIZE];  // Pre-allocate buffer for incoming args

    paramStr.toCharArray(paramBuf, BUFFER_SIZE);
    char *pch = strtok(paramBuf, "=");
    String event(pch);
    pch = strtok (NULL, ",");
    Utils::publish(event, String(pch));
    return 1;
}

int setPubRate(String command) {
  Utils::setInt(command, publishRateInSeconds, 1, 60 * 60); // don't allow publish rate > 1 hour.
  return 1;
}

void displayOnOLED() {
  if ((lastDisplayInSeconds + displayIntervalInSeconds) <= (millis() / 1000)) {
    display_digits(currentSensor.rms());
    lastDisplayInSeconds = millis() / 1000;
  }
}

void publishToWeb() {
  if ((lastPublishInSeconds + publishRateInSeconds) <= (millis() / 1000)) {
    pubData("");
    lastPublishInSeconds = millis() / 1000;
  }
}

void setup() {
  Utils::publish("Message", "Started setup...");
  Particle.function("getSettings", pubSettings);
  Particle.function("getData", pubData);
  Particle.function("pubFromCon", pubConsole);
  Particle.function("setPubRate", setPubRate);
  Particle.function("pubState", pubState);
  Particle.function("pubAverage", publishAverage);
  Particle.function("rstAverage", resetAverage);
  Particle.function("sendState", sendState);
  sample();
  lastPublishInSeconds = millis() / 1000;
  pubData("");
  clear();
  pubSettings("");
  oledWrapper.clear();
  oledWrapper.display(githubHash, 1);
  delay(5000);
  oledWrapper.clear();
  dryerMonitor.start();
  oledWrapper.display(dryerMonitor.currentDryerState(), DryerMonitor::STATE_FONT);
  Utils::publish("Message", "Finished setup...");
}


void loop() {
  timeSupport.handleTime();
  sample();
  dryerMonitor.doMonitor();
//  publishToWeb();
  clear();
}
