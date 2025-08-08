#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SD.h>
#include <SPI.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>

// Use a different GPIO pin since 23 is for the SD card
#define LED_PIN    2 // Data pin for the NeoPixel strip
#define NUM_LEDS   60 // Number of LEDs in your 1-meter strip

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

File audioFile;
bool isPlaying = false;
#define SD_CS 5

// ====== WiFi Config ======
const char* ssid = "Dev Bhai OP";
const char* password = "devbhai123omsairam";
const char* ap_ssid = "ESP32-Rover-AP";
const char* ap_password = "robotpassword";
// ====== Motor Pins ======
const int IN1 = 26, IN2 = 27, IN3 = 14, IN4 = 12, ENA = 13, ENB = 33;

// ====== Speaker & Mic Config ======
const int speakerPin = 25;  // DAC1 -> PAM8403
const int micPin = 34;
int threshold = 1940;
unsigned long lastTriggerTime = 0;
unsigned long debounceDelay = 100;
int micValue = 0;
bool soundDetected = false;

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// ====== Globals ======
int currentSpeed = 150;
String currentDir = "stop";
AsyncWebServer server(80);

// Eye state variables (simplified)
int left_eye_x_pos = 0;
int left_eye_y_pos = 0;
int right_eye_x_pos = 0;
int right_eye_y_pos = 0;
const int eye_size = 40;
const int eye_spacing = 10;
const int corner_radius = 10;
const int eye_offset = 10; // Offset for movements



void draw_eyes() {
  u8g2.clearBuffer();
  u8g2.drawRBox(left_eye_x_pos, left_eye_y_pos, eye_size, eye_size, corner_radius);
  u8g2.drawRBox(right_eye_x_pos, right_eye_y_pos, eye_size, eye_size, corner_radius);
  u8g2.sendBuffer();
}

void center_eyes() {
  left_eye_x_pos = (SCREEN_WIDTH - (eye_size * 2 + eye_spacing)) / 2;
  left_eye_y_pos = (SCREEN_HEIGHT - eye_size) / 2;
  right_eye_x_pos = left_eye_x_pos + eye_size + eye_spacing;
  right_eye_y_pos = left_eye_y_pos;
  draw_eyes();
}

// ====== Tone Functions (unchanged) ======
void smoothBeep(int freq, int durationMs) {
  const int samplesPerCycle = 30;
  float step = 2 * 3.14159 / samplesPerCycle;
  unsigned long endTime = millis() + durationMs;
  while (millis() < endTime) {
    for (int i = 0; i < samplesPerCycle; i++) {
      float wave = sin(i * step);
      int val = 128 + int(wave * 100);
      dacWrite(speakerPin, val);
      delayMicroseconds(1000000 / (freq * samplesPerCycle));
    }
  }
}

void gap(int ms = 100) {
  dacWrite(speakerPin, 0);
  delay(ms);
}

void sayHello() {
  smoothBeep(500, 120); gap(80);
  smoothBeep(460, 100); gap(80);
  smoothBeep(420, 100); gap(80);
  smoothBeep(600, 150); gap(200);
}

void sayWorld() {
  smoothBeep(300, 120); gap(60);
  smoothBeep(360, 100); gap(60);
  smoothBeep(400, 100); gap(60);
  smoothBeep(440, 100); gap(60);
  smoothBeep(480, 120); gap(200);
}

void playWav(const char* filename) {
  if (isPlaying) return;
  audioFile = SD.open(filename);
  if (!audioFile) {
    Serial.println("❌ Cannot open WAV file!");
    return;
  }
  isPlaying = true;
  uint8_t buffer[1];
  for (int i = 0; i < 44; i++) audioFile.read();
  while (audioFile.available() && isPlaying) {
    audioFile.read(buffer, 1);
    dacWrite(speakerPin, buffer[0]);
    delayMicroseconds(125);
  }
  audioFile.close();
  isPlaying = false;
  dacWrite(speakerPin, 0);
}

void stopPlayback() {
  if (isPlaying) {
    isPlaying = false;
    if (audioFile) audioFile.close();
    dacWrite(speakerPin, 0);
  }
}

// ====== Motor Movement and Eye Control ======
void moveCar(String dir, int speed) {
  currentDir = dir;
  currentSpeed = map(speed, 0, 100, 0, 255);

  digitalWrite(IN1, dir == "forward" || dir == "right");
  digitalWrite(IN2, dir == "backward" || dir == "left");
  digitalWrite(IN3, dir == "forward" || dir == "left");
  digitalWrite(IN4, dir == "backward" || dir == "right");

  if (dir == "stop") {
    digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
    digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
    center_eyes();
  } else if (dir == "forward") {
    center_eyes();
  } else if (dir == "backward") {
    int centerX = (SCREEN_WIDTH - (eye_size * 2 + eye_spacing)) / 2;
    int centerY = (SCREEN_HEIGHT - eye_size) / 2;
    left_eye_x_pos = centerX;
    left_eye_y_pos = centerY + eye_offset;
    right_eye_x_pos = centerX + eye_size + eye_spacing;
    right_eye_y_pos = centerY + eye_offset;
    draw_eyes();
  } else if (dir == "left") {
    int centerX = (SCREEN_WIDTH - (eye_size * 2 + eye_spacing)) / 2;
    int centerY = (SCREEN_HEIGHT - eye_size) / 2;
    left_eye_x_pos = centerX - eye_offset;
    left_eye_y_pos = centerY;
    right_eye_x_pos = centerX + eye_size + eye_spacing - eye_offset;
    right_eye_y_pos = centerY;
    draw_eyes();
  } else if (dir == "right") {
    int centerX = (SCREEN_WIDTH - (eye_size * 2 + eye_spacing)) / 2;
    int centerY = (SCREEN_HEIGHT - eye_size) / 2;
    left_eye_x_pos = centerX + eye_offset;
    left_eye_y_pos = centerY;
    right_eye_x_pos = centerX + eye_size + eye_spacing + eye_offset;
    right_eye_y_pos = centerY;
    draw_eyes();
  }

  analogWrite(ENA, currentSpeed);
  analogWrite(ENB, currentSpeed);
}

void setupAP() {
  Serial.println("❌ WiFi failed, starting Access Point.");
  WiFi.softAP(ap_ssid, ap_password);
  IPAddress apIP = WiFi.softAPIP();
  Serial.print("✅ AP IP address: ");
  Serial.println(apIP);

  // You may want to display this info on the OLED
  // u8g2.clearBuffer();
  // u8g2.setFont(u8g2_font_6x10_tf);
  // u8g2.drawStr(0, 10, "AP Mode Active");
  // u8g2.drawStr(0, 25, "SSID:");
  // u8g2.drawStr(40, 25, ap_ssid);
  // u8g2.drawStr(0, 40, "Password:");
  // u8g2.drawStr(60, 40, ap_password);
  // u8g2.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT); pinMode(ENB, OUTPUT);
  dacWrite(speakerPin, 0);

  u8g2.begin();
  u8g2.setDrawColor(1);
  center_eyes();
  strip.begin();
  strip.show();

  // ====== Wi-Fi Connection Logic with Fallback ======
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);

  unsigned long wifiConnectStartTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiConnectStartTime < 10000) { // 10 second timeout
    delay(500);
    Serial.print(".");
    center_eyes(); // Animate eyes while trying to connect
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    setupAP(); // Fallback to Access Point if connection fails
  }

  // ... (rest of your setup code remains the same)
  SPI.begin(18, 19, 23, SD_CS);
  if (!SD.begin(SD_CS, SPI)) {
    Serial.println("❌ SD card failed!");
  } else {
    Serial.println("✅ SD card initialized!");
  }

  server.on("/control", HTTP_GET, [](AsyncWebServerRequest *request){
    String dir = request->getParam("dir")->value();
    int speed = request->getParam("speed")->value().toInt();
    moveCar(dir, speed);
    request->send(200, "text/plain", "OK");
  });

  server.on("/say", HTTP_GET, [](AsyncWebServerRequest *request){
    String word = request->getParam("word")->value();
    if (word == "hello") sayHello();
    else if (word == "world") sayWorld();
    else if (word == "startup") { sayHello(); sayWorld(); }
    request->send(200, "text/plain", "Sound Played");
  });

  server.on("/play", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("track")) {
      String name = request->getParam("track")->value();
      if (name == "test1") playWav("/test1.wav");
      else if (name == "test2") playWav("/test2.wav");
      request->send(200, "text/plain", "Playing " + name);
    } else {
      request->send(400, "text/plain", "No track specified");
    }
  });

  server.on("/stop", HTTP_GET, [](AsyncWebServerRequest *request) {
    stopPlayback();
    request->send(200, "text/plain", "Stopped");
  });

  server.on("/mic", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{\"value\":" + String(micValue) + ",\"status\":\"" +
                  (soundDetected ? "Sound Detected!" : "Silent") + "\"}";
    request->send(200, "application/json", json);
  });

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Dev Rover Control</title>
    <style>
        /* A slightly modified version of Tailwind CSS for a dark theme */
        body {
            font-family: ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, "Noto Sans", sans-serif;
            background-color: #1a202c; /* dark gray */
            color: #e2e8f0; /* light gray */
            padding: 1rem;
        }
        h1 {
            color: #ecc94b; /* yellow */
            font-size: 1.875rem; /* 30px */
            line-height: 2.25rem; /* 36px */
            text-align: center;
            margin-bottom: 2rem;
        }
        h2 {
            font-size: 1.5rem; /* 24px */
            line-height: 2rem; /* 32px */
            margin-top: 1.5rem;
            margin-bottom: 0.5rem;
            color: #a0aec0;
            text-align: center;
        }
        .container {
            max-width: 42rem; /* 672px */
            margin-left: auto;
            margin-right: auto;
            display: flex;
            flex-direction: column;
            gap: 1.5rem;
        }
        .grid {
            display: grid;
            grid-template-columns: repeat(2, minmax(0, 1fr));
            gap: 0.5rem;
        }
        .grid-full {
            display: grid;
            grid-template-columns: 1fr;
            gap: 0.5rem;
        }
        @media (min-width: 640px) {
            .grid {
                grid-template-columns: repeat(3, minmax(0, 1fr));
            }
        }
        button {
            display: flex;
            justify-content: center;
            align-items: center;
            padding: 1rem 1.25rem;
            border-radius: 0.5rem;
            font-weight: 600;
            font-size: 1rem;
            line-height: 1.5rem;
            color: #1a202c;
            background-color: #ecc94b;
            border: none;
            cursor: pointer;
            transition-property: background-color;
            transition-duration: 150ms;
        }
        button:active {
            background-color: #d69e2e; /* dark yellow on press */
        }
        .slider-container {
            width: 100%;
            display: flex;
            flex-direction: column;
            align-items: center;
            margin: 1rem 0;
            background-color: #2d3748;
            padding: 1rem;
            border-radius: 0.5rem;
        }
        .slider-container label {
            font-size: 1rem;
            line-height: 1.5rem;
            margin-bottom: 0.5rem;
        }
        .slider-container input[type="range"] {
            width: 90%;
            -webkit-appearance: none;
            height: 0.5rem;
            border-radius: 9999px;
            background: #4a5568;
            outline: none;
            transition: opacity 0.2s;
        }
        .slider-container input[type="range"]::-webkit-slider-thumb {
            -webkit-appearance: none;
            appearance: none;
            width: 1.25rem;
            height: 1.25rem;
            border-radius: 9999px;
            background: #ecc94b;
            cursor: pointer;
        }
        .mic-box {
            background-color: #2d3748; /* gray */
            padding: 1rem;
            border-radius: 0.5rem;
            margin-top: 1rem;
        }
        .mic-box p {
            margin: 0;
            font-size: 1rem;
            line-height: 1.5rem;
            text-align: left;
        }
        .mic-box .status-text {
            color: #68d391; /* green */
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🚗 Dev Rover Control</h1>
        
        <h2>📷 Live Camera</h2>
        <img src="http://192.168.29.222:81/stream" style="width:100%; border-radius:10px; margin-bottom:10px;">
        
        <div class="slider-container">
            <label for="flashSlider">Flash Brightness: <span id="flashVal">0</span></label>
            <input type="range" min="0" max="255" value="0" id="flashSlider" oninput="updateFlash(this.value)">
        </div>
        <div class="slider-container">
            <label for="resSlider">📷 Resolution: <span id="resLabel">Medium</span></label>
            <input type="range" id="resSlider" min="2" max="10" step="1" value="6" oninput="changeRes(this.value)">
        </div>
        
        <div class="grid">
            <button onmousedown="send('forward',100)" onmouseup="send('stop',0)" ontouchstart="send('forward',100)" ontouchend="send('stop',0)">Forward</button>
            <button onmousedown="send('backward',100)" onmouseup="send('stop',0)" ontouchstart="send('backward',100)" ontouchend="send('stop',0)">Backward</button>
            <button onmousedown="send('left',100)" onmouseup="send('stop',0)" ontouchstart="send('left',100)" ontouchend="send('stop',0)">Left</button>
            <button onmousedown="send('right',100)" onmouseup="send('stop',0)" ontouchstart="send('right',100)" ontouchend="send('stop',0)">Right</button>
            <button class="grid-full" onclick="send('stop',0)">Stop</button>
        </div>
        <div class="slider-container">
            <label for="spd">Speed: <span id="speedVal">50</span>%</label>
            <input type="range" min="0" max="100" value="50" id="spd" oninput="sendSpeed(this.value)">
        </div>
        <div class="grid">
            <button onclick="fetch('/play?track=test1')">▶️ Play test1.wav</button>
            <button onclick="fetch('/play?track=test2')">▶️ Play test2.wav</button>
            <button onclick="fetch('/stop')">⏹️ Stop</button>
        </div>
        <div class="mic-box">
            <h2>🎤 Mic Monitor</h2>
            <p>Value: <span id="micVal">---</span></p>
            <p>Status: <span id="micStatus" class="status-text">Loading...</span></p>
        </div>
    </div>
    <script>
        function updateFlash(val) {
            document.getElementById("flashVal").textContent = val;
            fetch("http://192.168.29.222/flash?level=" + val);
        }
        function changeRes(val) {
            const labels = {
                2: "Ultra High",
                3: "Very High",
                4: "High",
                6: "Medium",
                8: "Low",
                9: "Very Low",
                10: "Ultra Low"
            };
            document.getElementById("resLabel").textContent = labels[val] || "Custom";
            fetch("http://192.168.29.222/control?var=framesize&val=" + val).then(() => console.log("Resolution changed to", val));
        }
        let currentDir = 'stop';
        function send(dir, speed) {
            currentDir = dir;
            fetch(`/control?dir=${dir}&speed=${speed}`);
        }
        function sendSpeed(val) {
            document.getElementById("speedVal").textContent = val;
            fetch(`/control?dir=${currentDir}&speed=${val}`);
        }
        setInterval(() => {
            fetch('/mic')
                .then(res => res.json())
                .then(data => {
                    document.getElementById("micVal").textContent = data.value;
                    document.getElementById("micStatus").textContent = data.status;
                });
        }, 500);
    </script>
</body>
</html>
)rawliteral";
    request->send(200, "text/html", html);
  });

  server.begin();
  sayHello(); sayWorld();
}
uint8_t chase_color_index = 0; // 0-255, for the color wheel

void chase() {
  // Use a static variable to keep track of the chasing segment's starting point
  static int i = 0;
  
  // Update the color index for the next animation frame
  chase_color_index++;
  
  // Clear the entire strip to black before drawing the next frame
  strip.clear();
  
  // Set the color for the 30-LED chasing segment
  for(int k = 0; k < 45; k++) {
    // The Wheel() function maps a 0-255 value to a color on the RGB spectrum
    strip.setPixelColor((i + k) % NUM_LEDS, strip.ColorHSV( (chase_color_index * 256 / 30) + (k * 256 / 30) ) );
  }

  // Update strip and move the starting point
  strip.show();
  i = (i + 1) % NUM_LEDS; // Move the starting LED of the chase
  delay(30); // Adjust this value to change the speed of the chase
}

void loop() {
  micValue = analogRead(micPin);
  soundDetected = (micValue > threshold && millis() - lastTriggerTime > debounceDelay);
  if (soundDetected) lastTriggerTime = millis();

  chase(); // Green

}
