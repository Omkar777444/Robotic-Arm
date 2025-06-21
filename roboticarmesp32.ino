#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP32Servo.h>
#include <iostream>
#include <sstream>

struct ServoPins {
  Servo servo;
  int servoPin;
  String servoName;
  int initialPosition;
};

std::vector<ServoPins> servoPins = {
  { Servo(), 27 , "Base", 90},
  { Servo(), 26 , "Shoulder", 90},
  { Servo(), 25 , "Elbow", 90},
  { Servo(), 33 , "Gripper", 90},
  { Servo(), 32 , "Wrist", 90},
  { Servo(), 14 , "Rotate", 90},
};

struct RecordedStep {
  int servoIndex;
  int value;
  int delayInStep;
};
std::vector<RecordedStep> recordedSteps;

bool recordSteps = false;
bool playRecordedSteps = false;
unsigned long previousTimeInMilli = millis();

const char* ssid     = "RobotArm";
const char* password = "12345678";

AsyncWebServer server(80);
AsyncWebSocket wsRobotArmInput("/RobotArmInput");

const char* htmlHomePage PROGMEM = R"HTMLHOMEPAGE(
<!DOCTYPE html>
<html>
  <head>
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
    <style>
    input[type=button] {
      background-color:red;color:white;border-radius:30px;width:100%;height:40px;font-size:20px;text-align:center;
    }
    .noselect {
      user-select: none;
    }
    .slidecontainer {
      width: 100%;
    }
    .slider {
      width: 100%; height: 20px; border-radius: 5px;
      background: #d3d3d3; outline: none; opacity: 0.7;
    }
    .slider:hover { opacity: 1; }
    .slider::-webkit-slider-thumb {
      width: 40px; height: 40px; border-radius: 50%;
      background: red; cursor: pointer;
    }
    </style>
  </head>
  <body class="noselect" align="center" style="background-color:white">
    <h1 style="color: teal;text-align:center;">Hash Include Electronics</h1>
    <h2 style="color: teal;text-align:center;">Robot Arm Control</h2>
    
    <table id="mainTable" style="width:400px;margin:auto;table-layout:fixed" CELLSPACING=10>
      <tr><td style="text-align:left;font-size:25px"><b>Gripper:</b></td><td colspan=2>
        <div class="slidecontainer"><input type="range" min="0" max="180" value="90" class="slider" id="Gripper" oninput='sendButtonInput("Gripper",value)'></div>
      </td></tr>
      <tr><td style="text-align:left;font-size:25px"><b>Elbow:</b></td><td colspan=2>
        <div class="slidecontainer"><input type="range" min="0" max="180" value="90" class="slider" id="Elbow" oninput='sendButtonInput("Elbow",value)'></div>
      </td></tr>
      <tr><td style="text-align:left;font-size:25px"><b>Shoulder:</b></td><td colspan=2>
        <div class="slidecontainer"><input type="range" min="0" max="180" value="90" class="slider" id="Shoulder" oninput='sendButtonInput("Shoulder",value)'></div>
      </td></tr>
      <tr><td style="text-align:left;font-size:25px"><b>Base:</b></td><td colspan=2>
        <div class="slidecontainer"><input type="range" min="0" max="180" value="90" class="slider" id="Base" oninput='sendButtonInput("Base",value)'></div>
      </td></tr>
      <tr><td style="text-align:left;font-size:25px"><b>Wrist:</b></td><td colspan=2>
        <div class="slidecontainer"><input type="range" min="0" max="180" value="90" class="slider" id="Wrist" oninput='sendButtonInput("Wrist",value)'></div>
      </td></tr>
      <tr><td style="text-align:left;font-size:25px"><b>Rotate:</b></td><td colspan=2>
        <div class="slidecontainer"><input type="range" min="0" max="180" value="90" class="slider" id="Rotate" oninput='sendButtonInput("Rotate",value)'></div>
      </td></tr>
      <tr><td style="text-align:left;font-size:25px"><b>Record:</b></td><td><input type="button" id="Record" value="OFF" ontouchend='onclickButton(this)'></td></tr>
      <tr><td style="text-align:left;font-size:25px"><b>Play:</b></td><td><input type="button" id="Play" value="OFF" ontouchend='onclickButton(this)'></td></tr>
    </table>

    <script>
      var webSocketRobotArmInputUrl = "ws:\/\/" + window.location.hostname + "/RobotArmInput";
      var websocketRobotArmInput;
      
      function initRobotArmInputWebSocket() {
        websocketRobotArmInput = new WebSocket(webSocketRobotArmInputUrl);
        websocketRobotArmInput.onopen = function(event){};
        websocketRobotArmInput.onclose = function(event){setTimeout(initRobotArmInputWebSocket, 2000);};
        websocketRobotArmInput.onmessage = function(event) {
          var keyValue = event.data.split(",");
          var button = document.getElementById(keyValue[0]);
          button.value = keyValue[1];
          if (button.id == "Record" || button.id == "Play") {
            button.style.backgroundColor = (button.value == "ON" ? "green" : "red");
            enableDisableButtonsSliders(button);
          }
        };
      }
      
      function sendButtonInput(key, value) {
        websocketRobotArmInput.send(key + "," + value);
      }

      function onclickButton(button) {
        button.value = (button.value == "ON") ? "OFF" : "ON";
        button.style.backgroundColor = (button.value == "ON" ? "green" : "red");
        sendButtonInput(button.id, (button.value == "ON") ? 1 : 0);
        enableDisableButtonsSliders(button);
      }

      function enableDisableButtonsSliders(button) {
        var ids = ["Gripper", "Elbow", "Shoulder", "Base", "Wrist", "Rotate"];
        if(button.id == "Play") {
          let disable = (button.value == "ON") ? "none" : "auto";
          ids.forEach(id => document.getElementById(id).style.pointerEvents = disable);
          document.getElementById("Record").style.pointerEvents = disable;
        }
        if(button.id == "Record") {
          let disable = (button.value == "ON") ? "none" : "auto";
          document.getElementById("Play").style.pointerEvents = disable;
        }
      }

      window.onload = initRobotArmInputWebSocket;
      document.getElementById("mainTable").addEventListener("touchend", function(event){ event.preventDefault(); });
    </script>
  </body>
</html>
)HTMLHOMEPAGE";

void handleRoot(AsyncWebServerRequest *request) {
  request->send_P(200, "text/html", htmlHomePage);
}

void handleNotFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "File Not Found");
}

void onRobotArmInputWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      sendCurrentRobotArmState();
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA: {
      AwsFrameInfo *info = (AwsFrameInfo*)arg;
      if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        std::string myData((char *)data, len);
        std::istringstream ss(myData);
        std::string key, value;
        std::getline(ss, key, ',');
        std::getline(ss, value, ',');
        int valueInt = atoi(value.c_str());

        if (key == "Record") {
          recordSteps = valueInt;
          if (recordSteps) {
            recordedSteps.clear();
            previousTimeInMilli = millis();
          }
        } else if (key == "Play") {
          playRecordedSteps = valueInt;
        } else {
          for (int i = 0; i < servoPins.size(); i++) {
            if (servoPins[i].servoName == key.c_str()) {
              writeServoValues(i, valueInt);
              break;
            }
          }
        }
      }
      break;
    }
    default: break;
  }
}

void sendCurrentRobotArmState() {
  for (auto &pin : servoPins) {
    wsRobotArmInput.textAll(pin.servoName + "," + pin.servo.read());
  }
  wsRobotArmInput.textAll(String("Record,") + (recordSteps ? "ON" : "OFF"));
  wsRobotArmInput.textAll(String("Play,") + (playRecordedSteps ? "ON" : "OFF"));
}

void writeServoValues(int servoIndex, int value) {
  if (recordSteps) {
    RecordedStep recordedStep;
    if (recordedSteps.empty()) {
      for (int i = 0; i < servoPins.size(); i++) {
        recordedSteps.push_back({i, servoPins[i].servo.read(), 0});
      }
    }
    unsigned long currentTime = millis();
    recordedStep = {servoIndex, value, (int)(currentTime - previousTimeInMilli)};
    recordedSteps.push_back(recordedStep);
    previousTimeInMilli = currentTime;
  }
  servoPins[servoIndex].servo.write(value);
}

void playRecordedRobotArmSteps() {
  if (recordedSteps.empty()) return;

  for (int i = 0; i < servoPins.size() && playRecordedSteps; i++) {
    auto &step = recordedSteps[i];
    int current = servoPins[step.servoIndex].servo.read();
    while (current != step.value && playRecordedSteps) {
      current += (current > step.value) ? -1 : 1;
      servoPins[step.servoIndex].servo.write(current);
      wsRobotArmInput.textAll(servoPins[step.servoIndex].servoName + "," + current);
      delay(50);
    }
  }
  delay(2000);

  for (int i = servoPins.size(); i < recordedSteps.size() && playRecordedSteps; i++) {
    auto &step = recordedSteps[i];
    delay(step.delayInStep);
    servoPins[step.servoIndex].servo.write(step.value);
    wsRobotArmInput.textAll(servoPins[step.servoIndex].servoName + "," + step.value);
  }
}

void setUpPinModes() {
  for (auto &pin : servoPins) {
    pin.servo.attach(pin.servoPin);
    pin.servo.write(pin.initialPosition);
  }
}

void setup() {
  setUpPinModes();
  Serial.begin(115200);
  WiFi.softAP(ssid, password);
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", HTTP_GET, handleRoot);
  server.onNotFound(handleNotFound);
  wsRobotArmInput.onEvent(onRobotArmInputWebSocketEvent);
  server.addHandler(&wsRobotArmInput);
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  wsRobotArmInput.cleanupClients();
  if (playRecordedSteps) playRecordedRobotArmSteps();
}

