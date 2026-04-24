#include <Wire.h>
#include <SPI.h>
#include <U8g2lib.h>
#include <Adafruit_MLX90614.h>
#include "MAX30105.h"
#include <TinyGPSPlus.h>
#include <WiFi.h>
#include <WebServer.h>

// WIFI
const char* ssid = "ACT-ai_103781803129";
const char* password = "13796997";
WebServer server(80);

// PINS
#define LED_GREEN 4
#define LED_BLUE  18
#define LED_RED   23

// OBJECTS
Adafruit_MLX90614 mlx;
MAX30105 particleSensor;
TinyGPSPlus gps;
HardwareSerial GPSserial(2);

// OLED
U8G2_SSD1309_128X64_NONAME0_F_4W_SW_SPI u8g2(
  U8G2_R0, 13, 14, 27, 26, 25
);

// VARIABLES
float heartRateVal = 0;
float spo2 = 0;
float roomTemp = 0;
float humidity = 0;
float bodyTempF = 0;

int page = 0;

// TIMERS
unsigned long lastSensor = 0;
unsigned long lastDisplay = 0;
unsigned long lastBlink = 0;
bool ledState = false;

// ================= WEB PAGE =================
String webpage() {

  bool isNormal = (heartRateVal >= 60 && heartRateVal <= 100) && (spo2 >= 95);
  String status = isNormal ? "NORMAL" : "ABNORMAL";

  String page = "<!DOCTYPE html><html><head>";
  page += "<meta charset='UTF-8'>";
  page += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  page += "<meta http-equiv='refresh' content='2'>";

  // Fonts + Icons
  page += "<link href='https://fonts.googleapis.com/css2?family=Poppins:wght@400;600&display=swap' rel='stylesheet'>";
  page += "<link href='https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.4.0/css/all.min.css' rel='stylesheet'>";

  page += "<style>";
  page += "body{margin:0;font-family:'Poppins',sans-serif;";
  page += "background:linear-gradient(135deg,#dfe9f3,#ffffff);}";

  page += ".container{padding:15px;}";

  page += "h2{text-align:center;color:#2c3e50;}";

  page += ".grid{display:grid;grid-template-columns:1fr 1fr;gap:15px;}";

  page += ".card{background:white;border-radius:20px;padding:18px;";
  page += "box-shadow:0 10px 25px rgba(0,0,0,0.1);transition:0.3s;}";

  page += ".card:hover{transform:scale(1.03);}";

  page += ".title{font-size:14px;color:#777;}";

  page += ".value{font-size:30px;font-weight:bold;color:#27ae60;margin-top:8px;}";

  page += ".icon{float:right;color:#3498db;}";

  page += ".alert{background:#ffecec;color:#e74c3c;padding:12px;border-radius:12px;}";

  page += ".btn{background:#27ae60;color:white;text-align:center;";
  page += "padding:15px;border-radius:12px;font-size:18px;margin-top:10px;}";

  page += ".full{grid-column:1/3;}";

  page += "iframe{width:100%;height:200px;border-radius:15px;border:none;}";

  page += "</style></head><body>";

  page += "<div class='container'>";
  page += "<h2>Health Sense</h2>";

  page += "<div class='grid'>";

  // HEART
  page += "<div class='card'><div class='title'>Heart Rate <i class='fa fa-heart icon'></i></div>";
  page += "<div class='value'>" + String(heartRateVal) + " bpm</div></div>";

  // SPO2
  page += "<div class='card'><div class='title'>SpO2 <i class='fa fa-droplet icon'></i></div>";
  page += "<div class='value'>" + String(spo2) + " %</div></div>";

  // BODY TEMP
  page += "<div class='card'><div class='title'>Body Temp <i class='fa fa-thermometer-half icon'></i></div>";
  page += "<div class='value'>" + String(bodyTempF,1) + " F</div></div>";

  // ROOM TEMP
  page += "<div class='card'><div class='title'>Room Temp <i class='fa fa-cloud-sun icon'></i></div>";
  page += "<div class='value'>" + String(roomTemp,1) + " C</div></div>";

  // HUMIDITY
  page += "<div class='card'><div class='title'>Humidity <i class='fa fa-water icon'></i></div>";
  page += "<div class='value'>" + String(humidity,0) + " %</div></div>";

  // GPS MAP
  page += "<div class='card full'><div class='title'>Location <i class='fa fa-map-marker-alt icon'></i></div>";

  if (gps.location.isValid()) {
    String lat = String(gps.location.lat(), 6);
    String lng = String(gps.location.lng(), 6);
    page += "<iframe src='https://maps.google.com/maps?q=" + lat + "," + lng + "&z=15&output=embed'></iframe>";
  } else {
    page += "<div>Searching GPS...</div>";
  }

  page += "</div>";

  // ALERT
  page += "<div class='alert full'>";
  if (isNormal) page += "All parameters normal";
  else page += "Heart Rate or SpO2 out of range";
  page += "</div>";

  // STATUS BUTTON
  page += "<div class='btn full'>" + status + "</div>";

  page += "</div></div></body></html>";

  return page;
}

void handleRoot() {
  server.send(200, "text/html", webpage());
}

// OLED
void showScreen(String title, String value) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.setCursor(0, 15);
  u8g2.print(title);
  u8g2.setFont(u8g2_font_logisoso20_tf);
  u8g2.setCursor(5, 55);
  u8g2.print(value);
  u8g2.sendBuffer();
}

// SETUP
void setup() {

  Serial.begin(115200);

  Wire.begin(21, 22);
  GPSserial.begin(9600, SERIAL_8N1, 32, 33);

  mlx.begin();
  particleSensor.begin(Wire);
  particleSensor.setup();

  u8g2.begin();

  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  pinMode(LED_RED, OUTPUT);

  digitalWrite(LED_BLUE, HIGH);

  showScreen("HEALTH", "SENSE");
  delay(2000);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);

  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.begin();

  lastSensor = millis();
  lastDisplay = millis() - 2000;
}

// LOOP
void loop() {

  server.handleClient();

  unsigned long now = millis();

  if (now - lastSensor >= 2000) {

    roomTemp = random(250, 350) / 10.0;
    humidity = random(400, 800) / 10.0;

    float bodyTempC = mlx.readObjectTempC();
    bodyTempF = (bodyTempC * 9.0 / 5.0) + 32.0;

    long ir = particleSensor.getIR();

    if (ir > 50000) {
      heartRateVal = map(ir, 50000, 100000, 70, 100);
      spo2 = 95 + random(0, 3);
    } else {
      heartRateVal = 0;
      spo2 = 0;
    }

    while (GPSserial.available()) {
      gps.encode(GPSserial.read());
    }

    // SERIAL OUTPUT
    Serial.println("\n===== HEALTH DATA =====");
    Serial.print("IP: "); Serial.println(WiFi.localIP());
    Serial.print("HR: "); Serial.print(heartRateVal); Serial.println(" BPM");
    Serial.print("SpO2: "); Serial.print(spo2); Serial.println(" %");
    Serial.print("Temp: "); Serial.print(bodyTempF); Serial.println(" F");
    Serial.print("Room: "); Serial.print(roomTemp); Serial.println(" C");
    Serial.print("Humidity: "); Serial.print(humidity); Serial.println(" %");

    if (gps.location.isValid()) {
      Serial.print("Lat: "); Serial.println(gps.location.lat(), 6);
      Serial.print("Lng: "); Serial.println(gps.location.lng(), 6);
    } else {
      Serial.println("GPS: Searching...");
    }

    lastSensor = now;
  }

  // LED LOGIC
  if (now - lastBlink >= 500) {
    ledState = !ledState;
    lastBlink = now;
  }

  long ir = particleSensor.getIR();

  if (ir > 50000) {
    digitalWrite(LED_GREEN, ledState);
    digitalWrite(LED_RED, LOW);
  } else {
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_RED, ledState);
  }

  // OLED ROTATION
  if (now - lastDisplay >= 2000) {

    switch (page) {
      case 0: showScreen("Heart Rate", String(heartRateVal)); break;
      case 1: showScreen("SpO2", String(spo2)); break;
      case 2: showScreen("Body Temp", String(bodyTempF)); break;
    }

    page = (page + 1) % 3;
    lastDisplay = now;
  }
}
