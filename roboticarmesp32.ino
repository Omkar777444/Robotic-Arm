#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "ServoESP32.h"

// Wi-Fi credentials
const char* ssid = "Robotic arm";
const char* password = "12345678";

// Server on port 80
AsyncWebServer server(80);

// Create 6 Servo objects
ServoESP32 servo[6];
int servoPins[6] = {13, 12, 14, 27, 26, 25}; // Modify based on your wiring
int servoAngles[6] = {90, 90, 90, 90, 90, 90}; // Initial positions

void setup() {
  Serial.begin(115200);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("Connected to WiFi");
  Serial.println("IP Address: ");
  Serial.println(WiFi.localIP());

  // Attach servo motors
  for (int i = 0; i < 6; i++) {
    servo[i].attach(servoPins[i]);
    servo[i].write(servoAngles[i]);
  }

  // Serve the web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", htmlPage.c_str());
  });

  // Handle angle changes
  server.on("/set", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("id") && request->hasParam("angle")) {
      int id = request->getParam("id")->value().toInt();
      int angle = request->getParam("angle")->value().toInt();
      if (id >= 0 && id < 6 && angle >= 0 && angle <= 180) {
        servoAngles[id] = angle;
        servo[id].write(angle);
        request->send(200, "text/plain", "OK");
      } else {
        request->send(400, "text/plain", "Invalid range");
      }
    } else {
      request->send(400, "text/plain", "Missing params");
    }
  });

  server.begin();
}

// HTML Page with sliders
const String htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>ESP32 Robotic Arm Control</title>
  <style>
    body { font-family: Arial; text-align: center; margin-top: 40px; }
    input[type=range] { width: 300px; }
    .servo { margin: 20px; }
  </style>
</head>
<body>
  <h2>ESP32 Robotic Arm Control</h2>
  <div class='servo'>Servo 1: <input type='range' min='0' max='180' value='90' onchange='updateServo(0,this.value)' /></div>
  <div class='servo'>Servo 2: <input type='range' min='0' max='180' value='90' onchange='updateServo(1,this.value)' /></div>
  <div class='servo'>Servo 3: <input type='range' min='0' max='180' value='90' onchange='updateServo(2,this.value)' /></div>
  <div class='servo'>Servo 4: <input type='range' min='0' max='180' value='90' onchange='updateServo(3,this.value)' /></div>
  <div class='servo'>Servo 5: <input type='range' min='0' max='180' value='90' onchange='updateServo(4,this.value)' /></div>
  <div class='servo'>Servo 6: <input type='range' min='0' max='180' value='90' onchange='updateServo(5,this.value)' /></div>

<script>
function updateServo(id, angle) {
  fetch(`/set?id=${id}&angle=${angle}`);
}
</script>
</body>
</html>
)rawliteral";
