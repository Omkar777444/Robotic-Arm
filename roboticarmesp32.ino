#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP32Servo.h>
#include <iostream>
#include <sstream>

// ============= SERVO CONFIGURATION =============
struct ServoPins {
  Servo servo;
  int servoPin;
  String servoName;
  int initialPosition;
};

std::vector<ServoPins> servoPins = {
  { Servo(), 27, "Base", 90},
  { Servo(), 26, "Shoulder", 90},
  { Servo(), 25, "Elbow", 90},
  { Servo(), 33, "Gripper", 90},
  { Servo(), 32, "Wrist", 90},
  { Servo(), 14, "Rotate", 90},
};

// ============= SMOOTH MOTION =============
struct SmoothMove {
  int target;
  int current;
};
std::vector<SmoothMove> smoothMoves;

// ============= RECORDING SYSTEM =============
struct RecordedStep {
  int servoIndex;
  int value;
  int delayInStep;
};
std::vector<RecordedStep> recordedSteps;

// ============= STATE VARIABLES =============
bool recordSteps = false;
bool playRecordedSteps = false;
unsigned long previousTimeInMilli = 0;
unsigned long lastUpdateTime = 0;
unsigned long playbackStepTime = 0;
int playbackIndex = 0;
int speedDelay = 20;

const char* ssid = "RobotArm";
const char* password = "12345678";

AsyncWebServer server(80);
AsyncWebSocket wsRobotArmInput("/RobotArmInput");

// ============= FORWARD DECLARATIONS =============
void sendCurrentRobotArmState();
void handlePreset(const std::string &preset);
void writeServoValues(int servoIndex, int value);
void updateSmoothServos();
void playRecorded();

// ============= WEB INTERFACE =============
const char* htmlHomePage PROGMEM = R"HTMLHOMEPAGE(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }

    body {
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      padding: 20px;
      min-height: 100vh;
      color: #333;
    }

    .container {
      max-width: 600px;
      margin: 0 auto;
      background: white;
      border-radius: 20px;
      box-shadow: 0 20px 60px rgba(0,0,0,0.3);
      overflow: hidden;
    }

    .header {
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: white;
      padding: 30px 20px;
      text-align: center;
    }

    .header h1 { font-size: 24px; margin-bottom: 5px; font-weight: 700; }
    .header p  { font-size: 14px; opacity: 0.9; }

    .content { padding: 25px; }

    .control-group { margin-bottom: 25px; }

    .control-label {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 8px;
      font-weight: 600;
      font-size: 15px;
      color: #333;
    }

    .control-value {
      background: #f0f0f0;
      padding: 4px 12px;
      border-radius: 12px;
      font-size: 14px;
      min-width: 55px;
      text-align: center;
      color: #667eea;
      font-weight: 700;
    }

    .slider {
      width: 100%;
      height: 8px;
      border-radius: 5px;
      background: linear-gradient(to right, #ddd, #999);
      outline: none;
      -webkit-appearance: none;
      appearance: none;
      cursor: pointer;
    }

    .slider::-webkit-slider-thumb {
      -webkit-appearance: none;
      width: 24px; height: 24px;
      border-radius: 50%;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      cursor: pointer;
      box-shadow: 0 4px 12px rgba(102,126,234,0.4);
      transition: all 0.2s;
    }

    .slider::-webkit-slider-thumb:active {
      transform: scale(1.2);
      box-shadow: 0 6px 16px rgba(102,126,234,0.6);
    }

    .slider::-moz-range-thumb {
      width: 24px; height: 24px;
      border-radius: 50%;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      cursor: pointer;
      border: none;
      box-shadow: 0 4px 12px rgba(102,126,234,0.4);
    }

    .speed-section {
      background: #f8f9ff;
      padding: 18px;
      border-radius: 15px;
      margin-bottom: 25px;
      border-left: 4px solid #667eea;
    }

    .speed-label { font-weight: 700; font-size: 16px; color: #667eea; margin-bottom: 12px; }

    .button-group {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 12px;
      margin-top: 25px;
    }

    button {
      padding: 15px 20px;
      border: none;
      border-radius: 12px;
      font-size: 15px;
      font-weight: 700;
      cursor: pointer;
      transition: all 0.3s ease;
      text-transform: uppercase;
      letter-spacing: 0.5px;
      box-shadow: 0 4px 12px rgba(0,0,0,0.1);
    }

    button.toggle-btn {
      background: #f0f0f0;
      color: #333;
      border: 2px solid #ddd;
    }

    button.toggle-btn.recording {
      background: #ff6b6b;
      color: white;
      border-color: #ff6b6b;
    }

    button.toggle-btn.playing {
      background: #51cf66;
      color: white;
      border-color: #51cf66;
    }

    button:disabled {
      opacity: 0.4;
      cursor: not-allowed;
    }

    .status-indicator {
      display: inline-block;
      width: 8px; height: 8px;
      border-radius: 50%;
      margin-right: 6px;
      animation: pulse 1s infinite;
    }

    .status-indicator.rec { background: white; }
    .status-indicator.play { background: white; }

    @keyframes pulse {
      0%, 100% { opacity: 1; }
      50% { opacity: 0.3; }
    }

    .preset-group {
      display: grid;
      grid-template-columns: repeat(3, 1fr);
      gap: 10px;
      margin-bottom: 20px;
    }

    .preset-btn {
      padding: 12px 15px;
      font-size: 13px;
      background: #f0f0f0;
      border: 2px solid #e0e0e0;
      color: #333;
      border-radius: 10px;
    }

    .preset-btn:active { background: #667eea; color: white; border-color: #667eea; }

    .info-text { font-size: 12px; color: #999; margin-top: 10px; text-align: center; }

    .noselect { user-select: none; -webkit-user-select: none; }

    /* Recording count badge */
    #recCount {
      font-size: 11px;
      background: rgba(255,255,255,0.3);
      padding: 2px 7px;
      border-radius: 10px;
      margin-left: 6px;
    }
  </style>
</head>
<body class="noselect">

<div class="container">
  <div class="header">
    <h1>FutureTech Robot Arm</h1>
    <p>6-DOF Arm &mdash; Smooth Motion &amp; Recording</p>
  </div>

  <div class="content">

    <!-- Speed Control -->
    <div class="speed-section">
      <div class="control-label">
        <span class="speed-label">Motion Speed</span>
        <span class="control-value" id="speedValue">20 ms</span>
      </div>
      <input type="range" min="5" max="50" value="20" class="slider" id="speedSlider"
        oninput='updateSpeed(this.value)'>
      <div class="info-text">Low = Fast &amp; Smooth &nbsp;|&nbsp; High = Slow &amp; Precise</div>
    </div>

    <!-- Servo Sliders -->
    <div class="control-group">
      <div class="control-label"><span>Base</span><span class="control-value" id="BaseValue">90&deg;</span></div>
      <input type="range" min="0" max="180" value="90" class="slider" id="slBase"
        oninput='sendServo("Base", this.value)'>
    </div>

    <div class="control-group">
      <div class="control-label"><span>Shoulder</span><span class="control-value" id="ShoulderValue">90&deg;</span></div>
      <input type="range" min="0" max="180" value="90" class="slider" id="slShoulder"
        oninput='sendServo("Shoulder", this.value)'>
    </div>

    <div class="control-group">
      <div class="control-label"><span>Elbow</span><span class="control-value" id="ElbowValue">90&deg;</span></div>
      <input type="range" min="0" max="180" value="90" class="slider" id="slElbow"
        oninput='sendServo("Elbow", this.value)'>
    </div>

    <div class="control-group">
      <div class="control-label"><span>Wrist</span><span class="control-value" id="WristValue">90&deg;</span></div>
      <input type="range" min="0" max="180" value="90" class="slider" id="slWrist"
        oninput='sendServo("Wrist", this.value)'>
    </div>

    <div class="control-group">
      <div class="control-label"><span>Rotate</span><span class="control-value" id="RotateValue">90&deg;</span></div>
      <input type="range" min="0" max="180" value="90" class="slider" id="slRotate"
        oninput='sendServo("Rotate", this.value)'>
    </div>

    <div class="control-group">
      <div class="control-label"><span>Gripper</span><span class="control-value" id="GripperValue">90&deg;</span></div>
      <input type="range" min="0" max="180" value="90" class="slider" id="slGripper"
        oninput='sendServo("Gripper", this.value)'>
    </div>

    <!-- Presets -->
    <div class="preset-group">
      <button class="preset-btn" onclick='sendPreset("Home")'>Home</button>
      <button class="preset-btn" onclick='sendPreset("Pick")'>Pick</button>
      <button class="preset-btn" onclick='sendPreset("Place")'>Place</button>
    </div>

    <!-- Record / Play -->
    <div class="button-group">
      <button class="toggle-btn" id="recordBtn" onclick='toggleRecord()'>
        <span class="status-indicator rec" id="recDot" style="display:none;"></span>
        Record<span id="recCount" style="display:none;">0</span>
      </button>
      <button class="toggle-btn" id="playBtn" onclick='togglePlay()'>
        <span class="status-indicator play" id="playDot" style="display:none;"></span>
       Play Loop
      </button>
    </div>

  </div>
</div>

<script>
  var ws;
  var isRecording = false;
  var isPlaying   = false;

  // Map servo name -> slider id
  var sliderMap = {
    Base:     "slBase",
    Shoulder: "slShoulder",
    Elbow:    "slElbow",
    Wrist:    "slWrist",
    Rotate:   "slRotate",
    Gripper:  "slGripper"
  };

  // ---- WebSocket ----
  function initWebSocket() {
    ws = new WebSocket("ws://" + window.location.hostname + "/RobotArmInput");
    ws.onopen    = function() { console.log("WS connected"); };
    ws.onclose   = function() { setTimeout(initWebSocket, 2000); };
    ws.onmessage = function(e) {
      var parts = e.data.split(",");
      var key   = parts[0];
      var val   = parts[1];
      handleIncoming(key, val);
    };
  }

  // ---- Handle every incoming message ----
  function handleIncoming(key, val) {
    if (key === "Record") {
      setRecordUI(val === "ON");
    } else if (key === "Play") {
      setPlayUI(val === "ON");
    } else {
      // It's a servo position update (live feedback during playback)
      var dispEl = document.getElementById(key + "Value");
      if (dispEl) dispEl.textContent = val + "\u00B0";

      var slId = sliderMap[key];
      if (slId) {
        var sl = document.getElementById(slId);
        if (sl) sl.value = val;   // Move slider to match real position
      }
    }
  }

  // ---- Send servo command ----
  function sendServo(name, val) {
    if (isPlaying) return;           // Block manual input during playback
    document.getElementById(name + "Value").textContent = val + "\u00B0";
    ws.send(name + "," + val);
  }

  // ---- Speed ----
  function updateSpeed(val) {
    document.getElementById("speedValue").textContent = val + " ms";
    ws.send("Speed," + val);
  }

  // ---- Preset ----
  function sendPreset(preset) {
    if (isPlaying || isRecording) return;
    ws.send("Preset," + preset);
  }

  // ---- Record toggle ----
  function toggleRecord() {
    if (isPlaying) return;
    ws.send("Record," + (isRecording ? 0 : 1));
    // UI will update when ESP32 echoes back "Record,ON" or "Record,OFF"
  }

  // ---- Play toggle ----
  function togglePlay() {
    if (isRecording) return;
    ws.send("Play," + (isPlaying ? 0 : 1));
    // UI will update when ESP32 echoes back "Play,ON" or "Play,OFF"
  }

  // ---- UI helpers ----
  function setRecordUI(on) {
    isRecording = on;
    var btn  = document.getElementById("recordBtn");
    var dot  = document.getElementById("recDot");
    var cnt  = document.getElementById("recCount");

    if (on) {
      btn.classList.add("recording");
      dot.style.display = "inline-block";
      cnt.style.display = "inline";
      cnt.textContent   = "0";
      document.getElementById("playBtn").disabled = true;
    } else {
      btn.classList.remove("recording");
      dot.style.display = "none";
      cnt.style.display = "none";
      document.getElementById("playBtn").disabled = false;
    }
  }

  function setPlayUI(on) {
    isPlaying = on;
    var btn = document.getElementById("playBtn");
    var dot = document.getElementById("playDot");

    if (on) {
      btn.classList.add("playing");
      dot.style.display = "inline-block";
      document.getElementById("recordBtn").disabled = true;
      setAllSliders(true);
    } else {
      btn.classList.remove("playing");
      dot.style.display = "none";
      document.getElementById("recordBtn").disabled = false;
      setAllSliders(false);
    }
  }

  function setAllSliders(disable) {
    Object.values(sliderMap).forEach(function(id) {
      var sl = document.getElementById(id);
      sl.disabled     = disable;
      sl.style.opacity = disable ? "0.5" : "1";
    });
  }

  window.onload = initWebSocket;
</script>

</body>
</html>
)HTMLHOMEPAGE";

// ============= REQUEST HANDLERS =============
void handleRoot(AsyncWebServerRequest *request) {
  request->send_P(200, "text/html", htmlHomePage);
}
void handleNotFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "File Not Found");
}

// ============= WEBSOCKET HANDLER =============
void onRobotArmInputWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                                   AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {

    case WS_EVT_CONNECT:
      Serial.printf("Client #%u connected\n", client->id());
      sendCurrentRobotArmState();
      break;

    case WS_EVT_DISCONNECT:
      Serial.printf("Client #%u disconnected\n", client->id());
      break;

    case WS_EVT_DATA: {
      AwsFrameInfo *info = (AwsFrameInfo *)arg;
      if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {

        std::string myData((char *)data, len);
        std::istringstream ss(myData);
        std::string key, value;
        std::getline(ss, key, ',');
        std::getline(ss, value, ',');
        int valueInt = atoi(value.c_str());

        if (key == "Speed") {
          speedDelay = constrain(valueInt, 5, 50);

        } else if (key == "Record") {
          bool newState = (valueInt == 1);
          if (newState && !recordSteps) {
            // START recording — clear old steps
            recordedSteps.clear();
            previousTimeInMilli = millis();
            Serial.println("Recording STARTED");
          } else if (!newState && recordSteps) {
            Serial.printf("Recording STOPPED — %d steps saved\n", recordedSteps.size());
          }
          recordSteps = newState;
          wsRobotArmInput.textAll(String("Record,") + (recordSteps ? "ON" : "OFF"));

        } else if (key == "Play") {
          bool newState = (valueInt == 1);
          if (newState && !recordedSteps.empty()) {
            playRecordedSteps = true;
            playbackIndex     = 0;
            playbackStepTime  = millis();
            Serial.printf("Playback STARTED — %d steps\n", recordedSteps.size());
            wsRobotArmInput.textAll("Play,ON");
          } else if (!newState) {
            playRecordedSteps = false;
            wsRobotArmInput.textAll("Play,OFF");
            Serial.println("Playback STOPPED");
          } else {
            // Play requested but no steps recorded
            Serial.println("Play: nothing recorded yet");
            wsRobotArmInput.textAll("Play,OFF");
          }

        } else if (key == "Preset") {
          handlePreset(value);

        } else {
          // Servo command
          for (int i = 0; i < (int)servoPins.size(); i++) {
            if (servoPins[i].servoName == key.c_str()) {
              writeServoValues(i, valueInt);
              break;
            }
          }
        }
      }
      break;
    }
    default:
      break;
  }
}

// ============= STATE BROADCAST =============
void sendCurrentRobotArmState() {
  for (auto &pin : servoPins) {
    wsRobotArmInput.textAll(pin.servoName + "," + String(pin.servo.read()));
  }
  wsRobotArmInput.textAll(String("Record,") + (recordSteps       ? "ON" : "OFF"));
  wsRobotArmInput.textAll(String("Play,")   + (playRecordedSteps ? "ON" : "OFF"));
}

// ============= WRITE SERVO + RECORD =============
void writeServoValues(int servoIndex, int value) {
  value = constrain(value, 0, 180);
  smoothMoves[servoIndex].target = value;

  if (recordSteps) {
    RecordedStep s;
    s.servoIndex  = servoIndex;
    s.value       = value;
    s.delayInStep = (int)(millis() - previousTimeInMilli);
    recordedSteps.push_back(s);
    previousTimeInMilli = millis();
    // Send step count back so UI badge updates
    wsRobotArmInput.textAll("RecCount," + String(recordedSteps.size()));
  }
}

// ============= SMOOTH MOTION =============
void updateSmoothServos() {
  if (millis() - lastUpdateTime < (unsigned long)speedDelay) return;
  lastUpdateTime = millis();

  for (int i = 0; i < (int)servoPins.size(); i++) {
    int c = smoothMoves[i].current;
    int t = smoothMoves[i].target;
    if (c != t) {
      c += (c < t) ? 1 : -1;
      smoothMoves[i].current = c;
      servoPins[i].servo.write(c);
      // Broadcast live position so UI sliders follow during playback
      wsRobotArmInput.textAll(servoPins[i].servoName + "," + String(c));
    }
  }
}

// ============= PLAYBACK =============
void playRecorded() {
  if (!playRecordedSteps || recordedSteps.empty()) return;

  if (playbackIndex < (int)recordedSteps.size()) {
    RecordedStep &s = recordedSteps[playbackIndex];
    if (millis() - playbackStepTime >= (unsigned long)s.delayInStep) {
      smoothMoves[s.servoIndex].target = s.value;
      playbackStepTime = millis();
      playbackIndex++;
    }
  } else {
    // Loop back to start — continuous playback
    playbackIndex    = 0;
    playbackStepTime = millis();
    Serial.println("Playback LOOPING");
  }
}

// ============= PRESETS =============
void handlePreset(const std::string &preset) {
  if (preset == "Home") {
    for (int i = 0; i < (int)servoPins.size(); i++)
      smoothMoves[i].target = 90;
  } else if (preset == "Pick") {
    smoothMoves[1].target = 120;
    smoothMoves[2].target = 140;
    smoothMoves[3].target = 40;
  } else if (preset == "Place") {
    smoothMoves[1].target = 60;
    smoothMoves[2].target = 80;
    smoothMoves[3].target = 100;
  }
}

// ============= INIT =============
void setUpPinModes() {
  for (auto &p : servoPins) {
    p.servo.attach(p.servoPin);
    p.servo.write(p.initialPosition);
    SmoothMove sm;
    sm.target  = p.initialPosition;
    sm.current = p.initialPosition;
    smoothMoves.push_back(sm);
  }
}

void setup() {
  Serial.begin(115200);
  setUpPinModes();

  WiFi.softAP(ssid, password);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", HTTP_GET, handleRoot);
  server.onNotFound(handleNotFound);
  wsRobotArmInput.onEvent(onRobotArmInputWebSocketEvent);
  server.addHandler(&wsRobotArmInput);
  server.begin();
  Serial.println("Server started");
}

void loop() {
  wsRobotArmInput.cleanupClients();
  updateSmoothServos();
  playRecorded();
}
