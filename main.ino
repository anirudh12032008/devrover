#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH1106.h>
#include <Wifi.h>
#include <WebServer.h>

const char* ssid  ="xxx";
const char* password = "xxx";

WebServer server(80);

#define M1 5
#define M2 18
#define M3 19
#define M4 21
#define MENB 22
#define MENA 23


#define speaker 25
#define mic 34
#define oledreset -1
#define battery 35
#define rgb 27
#define led 8
#define speakerPin 25  // Must be a DAC or PWM pin (GPIO 25 is DAC1)

#define LED_BAR_SEGMENTS 10
int ledBarPins[LED_BAR_SEGMENTS] = {2, 4, 13, 12, 14, 15, 16, 17, 32, 33};


File soundFile;
unsigned long lastMicSample = 0;
const int micSampleInterval = 100; 
const int micThreshold = 800; 


#define W 128
#define H 64
Adafruit_SH1106 display(W, H, &Wire, oledreset);


const int freq = 30000;
const int resolution = 8;
int dutyCycle = 0;
String val = String(0);



void handleRoot(){
    const char html[] PROGMEM = R"rawliteral(
    <!DOCTYPE HTML><html>
  <head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link rel="icon" href="data:,">
    <style>
    html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center; }
      .button { -webkit-user-select: none; -moz-user-select: none; -ms-user-select: none; user-select: none; background-color: #4CAF50; border: none; color: white; padding: 12px 28px; text-decoration: none; font-size: 26px; margin: 1px; cursor: pointer; }
      .button2 {background-color: #555555;}
    </style>

    <script>
      function forw() { fetch('/forward'); }
      function left() { fetch('/left'); }
      function stop() { fetch('/stop'); }
      function right() { fetch('/right'); }
      function rev() { fetch('/reverse'); }

      function setspeed(s){
      document.getElementById('motorSpeed').innerHTML = s;
        fetch(`/speed?value=${s}`);
      }

      </script>
      </head>

      <body>
    <h1>ESP32 Motor Control</h1>
    <p><button class="button" onclick="forw()">FORWARD</button></p>
    <div style="clear: both;">
      <p>
        <button class="button" onclick="left()">LEFT</button>
        <button class="button button2" onclick="stop()">STOP</button>
        <button class="button" onclick="right()">RIGHT</button>
      </p>
    </div>
    <p><button class="button" onclick="rev()">REVERSE</button></p>
    <p>Motor Speed: <span id="motorSpeed">0</span></p>
    <input type="range" min="0" max="100" step="25" id="motorSlider" oninput="setspeed(this.value)" value="0"/>
  </body>
    </html>)rawliteral";
    server.send(200, "text/html", html);
}

void hforward(){
    Serial.println("Forward");
    digitalWrite(M1, LOW);
    digitalWrite(M2, HIGH);
    digitalWrite(M3, LOW);
    digitalWrite(M4, HIGH);
    setColor(0, 255, 0);
    playMelody(); 
    logEvent("FORWARD");
    beep(1, 100); 

    server.send(200);
}

void hleft(){
    Serial.println("Left");
    digitalWrite(M1, LOW);
    digitalWrite(M2, HIGH);
    digitalWrite(M3, HIGH);
    digitalWrite(M4, LOW);
    setColor(0, 0, 255);
    server.send(200);
}

void hstop(){
    Serial.println("Stop");
    digitalWrite(M1, LOW);
    digitalWrite(M2, LOW);
    digitalWrite(M3, LOW);
    digitalWrite(M4, LOW);
    setColor(255, 0, 0);
    beep(1, 100); 
    server.send(200);
}

void hright(){
    Serial.print("Right");
    digitalWrite(M1, HIGH);
    digitalWrite(M2, LOW);
    digitalWrite(M3, LOW);
    digitalWrite(M4, HIGH);
    setColor(255, 255, 0);
    server.send(200);
}

void hreverse(){
    Serial.println("Reverse");
    digitalWrite(M1, HIGH);
    digitalWrite(M2, LOW);
    digitalWrite(M3, HIGH);
    digitalWrite(M4, LOW);
    setColor(255, 0, 255);
    beep(1, 100); 
    server.send(200);
}

void hspeed(){
    if (server.hasArg("value")) {
        int speed = server.arg("value").toInt();
        if (speed == 0){
          ledcWrite(MENA, 0);
          ledcWrite(MENB, 0);
          digitalWrite(M1, LOW);
          digitalWrite(M2, LOW);
          digitalWrite(M3, LOW);
          digitalWrite(M4, LOW);
          setBarGraphLevel(0); 

          server.send(200);
        } else {
          dutyCycle = map(speed, 25, 100, 200, 255);
        ledcWrite(MENA, dutyCycle);
        ledcWrite(MENB, dutyCycle);
        int level = map(speed, 0, 100, 0, LED_BAR_SEGMENTS);
        setBarGraphLevel(level);
        server.send(200, "text/plain", String(speed));
        }
        
    } 
    server.send(200);
}

void displayMessage(msg) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("DEV ROVER");
  display.println("Status:");
  display.println(msg);
  display.display();
}

void playTone(int frequency, int duration) {
  ledcAttachPin(speakerPin, 0); // Channel 0
  ledcWriteTone(0, frequency);
  delay(duration);
  ledcWriteTone(0, 0); // Stop tone
}

void beep(int count, int duration) {
  for (int i = 0; i < count; i++) {
    playTone(1000, duration);
    delay(duration);
  }
}

int melody[] = {262, 196, 196, 220, 196, 0, 247, 262}; // C4, G3...
int noteDurations[] = {250, 125, 125, 250, 250, 250, 250, 250};

void playMelody() {
  for (int i = 0; i < 8; i++) {
    int noteDuration = noteDurations[i];
    playTone(melody[i], noteDuration);
    delay(noteDuration * 1.30);
  }
}


#define NUM_LEDS 60  
#define BRIGHTNESS 100

Adafruit_NeoPixel strip(NUM_LEDS, rgb, NEO_GRB + NEO_KHZ800);

void setColor(uint8_t r, uint8_t g, uint8_t b) {
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  strip.show();
}


void setBarGraphLevel(int level) {
  level = constrain(level, 0, LED_BAR_SEGMENTS);
  for (int i = 0; i < LED_BAR_SEGMENTS; i++) {
    digitalWrite(ledBarPins[i], i < level ? HIGH : LOW);
  }
}



void setup() {
  Serial.begin(115200);
  pinMode(M1, OUTPUT);
  pinMode(M2, OUTPUT);
  pinMode(M3, OUTPUT);
  pinMode(M4, OUTPUT);

  ledcAttach(MENA, freq, resolution);
  ledcAttach(MENB, freq, resolution);

  ledcWrite(MENA, 0);
  ledcWrite(MENB, 0);

  if (!display.begin(SH1106_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED failed"));
    while (1);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("DEV ROVER");


  for (int i = 0; i < LED_BAR_SEGMENTS; i++) {
  pinMode(ledBarPins[i], OUTPUT);
  digitalWrite(ledBarPins[i], LOW);
}


  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to WiFi");
  Serial.println(WiFi.localIP());

  Serial.print("Initializing SD card...");
  if (!SD.begin()) {
    Serial.println("Card failed or not present");
  } else {
    Serial.println("Card initialized.");
    String fileName = "/mic_log.txt";
    soundFile = SD.open(fileName, FILE_WRITE);
    if (soundFile) {
      soundFile.println("Timestamp(ms),Analog_Value");
      soundFile.flush();
    } else {
      Serial.println("Failed to open log file");
    }
  }

  pinMode(mic, INPUT);



  server.on("/", handleRoot);
  server.on("/forward", hforward);
  server.on("/left", hleft);
  server.on("/stop", hstop);
  server.on("/right", hright);
  server.on("/reverse", hreverse);
  server.on("/speed", hspeed);

  server.begin();

}


void loop(){
  server.handleClient();
  server.handleClient();
    unsigned long currentMillis = millis();
  if (currentMillis - lastMicSample > micSampleInterval) {
    lastMicSample = currentMillis;
    int micVal = analogRead(mic);

    if (soundFile) {
      soundFile.printf("%lu,%d\n", currentMillis, micVal);
      soundFile.flush(); 
    }

    if (micVal > micThreshold) {
      Serial.println("Noise spike detected: " + String(micVal));
      displayMessage("LOUD NOISE!");
    }
  }


}


