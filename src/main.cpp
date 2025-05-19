#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <vector> // Required for std::vector

// Wi-Fi AP credentials
const char* apSSID = "MeArm_Control";
const char* apPassword = "password"; // Consider a stronger password

// MeArm Pin Definitions
const int basePin = 13;
const int shoulderPin = 12;
const int elbowPin = 14;
const int gripperPin = 27;

// Servo objects
Servo baseServo;
Servo shoulderServo;
Servo elbowServo;
Servo gripperServo;

// Current servo positions
volatile int basePos = 90;
volatile int shoulderPos = 90;
volatile int elbowPos = 90;
volatile int gripperPos = 90;

// Home positions
int baseHome = 90;
int shoulderHome = 90;
int elbowHome = 90;
int gripperHome = 90;

// Servo enable flags
bool baseEnabled = false;
bool shoulderEnabled = false;
bool elbowEnabled = false;
bool gripperEnabled = false;

// --- Record & Playback Globals ---
// Structure to hold the position of all servos for one pose
struct Pose {
  int base;
  int shoulder;
  int elbow;
  int gripper;
};

// Global vector to store the recorded sequence of poses
std::vector<Pose> recordedSequence;

// Flag to indicate if recording is active
bool isRecording = false;
// --- End Record & Playback Globals ---

WebServer server(80);

// --- FORWARD DECLARATION ---
void moveServoSmoothly(Servo& servo, int currentPos, int targetPos, int step, int delayTime);

// Function to handle web requests
void handleRoot() {
  String html = "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>MeArm Control</title>";
  html += "<style>";
  html += "body { font-family: sans-serif; background-color: #222; color: #eee; display: flex; flex-direction: column; align-items: center; }";
  html += "h1 { color: #ffd700; margin-bottom: 20px; }";
  html += ".servo-control { margin-bottom: 20px; text-align: center; background-color: #303030; padding: 15px; border-radius: 8px; width: 250px;}";
  html += "h2 { color: #bbb; margin-top: 0; }";
  html += "label { display: block; margin-bottom: 5px; }";
  html += "input[type='range'], input[type='number'] { width: 180px; padding: 5px; margin-bottom: 10px; }";
  html += "button { background-color: #4CAF50; color: white; border: none; padding: 10px 18px; border-radius: 5px; cursor: pointer; margin-right: 5px; transition: background-color 0.3s;}";
  html += "button:hover { background-color: #45a049; }";
  html += "button.disable { background-color: #f44336;}";
  html += "button.disable:hover { background-color: #da190b;}";
  html += "button.recording { background-color: #f44336; }"; // Red when recording
  html += "button.recording:hover { background-color: #da190b; }";
  html += ".controls-container { display: flex; flex-wrap: wrap; justify-content: center; gap: 20px; margin-bottom: 20px;}";
  html += ".record-play-controls { margin-bottom: 20px; text-align: center; background-color: #303030; padding: 15px; border-radius: 8px; width: auto; min-width:300px;}"; // Adjusted width
  html += ".settings { margin-top: 30px; border-top: 1px solid #555; padding-top: 20px; text-align: center; background-color: #303030; padding: 20px; border-radius: 8px;}";
  html += "#statusMessage { color: #ccc; margin-top: 10px; min-height: 1.2em; }"; // For feedback
  html += "</style>";
  html += "</head><body>";
  html += "<h1>MeArm Control Interface</h1>";

  html += "<div class='controls-container'>"; // Start of servo controls container
  html += "<div class='servo-control'>";
  html += "<h2>Base</h2>";
  // Sliders now use fixed 0-180 min/max
  html += "<p><label for='baseSlider'>Position: <span id='baseValue'>" + String(basePos) + "</span></label> <input type='range' id='baseSlider' min='0' max='180' value='" + String(basePos) + "' oninput='updateServo(this.id, this.value); document.getElementById(\"baseValue\").textContent = this.value;'></p>";
  html += "<p><button onclick='toggleServo(\"base\")' id='baseButton'>" + String(baseEnabled ? "Disable" : "Enable") + "</button></p>";
  html += "</div>";

  html += "<div class='servo-control'>";
  html += "<h2>Shoulder</h2>";
  html += "<p><label for='shoulderSlider'>Position: <span id='shoulderValue'>" + String(shoulderPos) + "</span></label> <input type='range' id='shoulderSlider' min='0' max='180' value='" + String(shoulderPos) + "' oninput='updateServo(this.id, this.value); document.getElementById(\"shoulderValue\").textContent = this.value;'></p>";
  html += "<p><button onclick='toggleServo(\"shoulder\")' id='shoulderButton'>" + String(shoulderEnabled ? "Disable" : "Enable") + "</button></p>";
  html += "</div>";

  html += "<div class='servo-control'>";
  html += "<h2>Elbow</h2>";
  html += "<p><label for='elbowSlider'>Position: <span id='elbowValue'>" + String(elbowPos) + "</span></label> <input type='range' id='elbowSlider' min='0' max='180' value='" + String(elbowPos) + "' oninput='updateServo(this.id, this.value); document.getElementById(\"elbowValue\").textContent = this.value;'></p>";
  html += "<p><button onclick='toggleServo(\"elbow\")' id='elbowButton'>" + String(elbowEnabled ? "Disable" : "Enable") + "</button></p>";
  html += "</div>";

  html += "<div class='servo-control'>";
  html += "<h2>Gripper</h2>";
  html += "<p><label for='gripperSlider'>Position: <span id='gripperValue'>" + String(gripperPos) + "</span></label> <input type='range' id='gripperSlider' min='0' max='180' value='" + String(gripperPos) + "' oninput='updateServo(this.id, this.value); document.getElementById(\"gripperValue\").textContent = this.value;'></p>"; // Gripper also 0-180, adjust if your gripper has a smaller range physically
  html += "<p><button onclick='toggleServo(\"gripper\")' id='gripperButton'>" + String(gripperEnabled ? "Disable" : "Enable") + "</button></p>";
  html += "</div>";
  html += "</div>"; // End of servo controls container

  // --- Record & Play Controls ---
  html += "<div class='record-play-controls'>";
  html += "<h2>Record & Play</h2>";
  html += "<button id='recordButton' onclick='toggleRecording()'>Start Record</button>";
  html += "<button onclick='playSequence()'>Play Sequence</button>";
  html += "<button onclick='deleteSequence()'>Delete Sequence</button>";
  html += "<p id='statusMessage'></p>"; // For feedback
  html += "</div>";
  // --- End Record & Play Controls ---

  // Settings (Home Positions Only)
  html += "<div class='settings'>";
  html += "<h2>Settings</h2>";
  html += "<h3>Home Positions</h3>";
  html += "<p>Base: <input type='number' id='baseHome' value='" + String(baseHome) + "'> Shoulder: <input type='number' id='shoulderHome' value='" + String(shoulderHome) + "'></p>";
  html += "<p>Elbow: <input type='number' id='elbowHome' value='" + String(elbowHome) + "'> Gripper: <input type='number' id='gripperHome' value='" + String(gripperHome) + "'></p>";
  // Limits section removed
  html += "<button onclick='saveAllSettings()'>Save Home Settings</button>"; // Button text changed
  html += "<button onclick='goHome()'>Go Home</button>";
  html += "</div>";

  html += "<script>";
  html += "function updateServo(sliderId, value) {";
  html += "  var servoName = sliderId.replace('Slider', '');";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  xhr.open('GET', '/set_servo?servo=' + servoName + '&pos=' + value, true);";
  html += "  xhr.send();";
  html += "}";

  html += "function toggleServo(servoName) {";
  html += "  var button = document.getElementById(servoName + 'Button');";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  xhr.open('GET', '/toggle_servo?servo=' + servoName, true);";
  html += "  xhr.onload = function() {";
  html += "    if (xhr.status === 200) {";
  html += "      if (xhr.responseText === 'enabled') {";
  html += "        button.textContent = 'Disable';";
  html += "        button.classList.add('disable');";
  html += "      } else if (xhr.responseText === 'disabled') {";
  html += "        button.textContent = 'Enable';";
  html += "        button.classList.remove('disable');";
  html += "      }";
  html += "    }";
  html += "  };";
  html += "  xhr.send();";
  html += "}";

  html += "function saveAllSettings() {"; // Only saves home positions now
  html += "  var params = 'baseHome=' + document.getElementById('baseHome').value;";
  html += "  params += '&shoulderHome=' + document.getElementById('shoulderHome').value;";
  html += "  params += '&elbowHome=' + document.getElementById('elbowHome').value;";
  html += "  params += '&gripperHome=' + document.getElementById('gripperHome').value;";
  // Limit parameters removed
  html += "  var xhr = new XMLHttpRequest();";
  html += "  xhr.open('GET', '/save_settings?' + params, true);";
  html += "  var statusMsg = document.getElementById('statusMessage');";
  html += "  xhr.onload = function() { if(xhr.status === 200) statusMsg.textContent = 'Home settings saved!'; else statusMsg.textContent = 'Error saving settings.'; };";
  html += "  xhr.send();";
  html += "}";

  html += "function goHome() {";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  xhr.open('GET', '/go_home', true);";
  html += "  var statusMsg = document.getElementById('statusMessage');";
  html += "  xhr.onload = function() {";
  html += "    if (xhr.status === 200) {";
  html += "      var parts = xhr.responseText.split(',');";
  html += "      if (parts.length === 8) {";
  html += "        var homePositions = { base: parts[0], shoulder: parts[1], elbow: parts[2], gripper: parts[3] };";
  html += "        var enabledStates = { base: parts[4] === '1', shoulder: parts[5] === '1', elbow: parts[6] === '1', gripper: parts[7] === '1' };";
  html += "        if(enabledStates.base) { document.getElementById('baseSlider').value = homePositions.base; document.getElementById('baseValue').textContent = homePositions.base; }";
  html += "        if(enabledStates.shoulder) { document.getElementById('shoulderSlider').value = homePositions.shoulder; document.getElementById('shoulderValue').textContent = homePositions.shoulder; }";
  html += "        if(enabledStates.elbow) { document.getElementById('elbowSlider').value = homePositions.elbow; document.getElementById('elbowValue').textContent = homePositions.elbow; }";
  html += "        if(enabledStates.gripper) { document.getElementById('gripperSlider').value = homePositions.gripper; document.getElementById('gripperValue').textContent = homePositions.gripper; }";
  html += "        statusMsg.textContent = 'Moving to home positions!';";
  html += "      }";
  html += "    } else { statusMsg.textContent = 'Error going home.'; }";
  html += "  };";
  html += "  xhr.send();";
  html += "}";

  // --- JavaScript for Record & Play ---
  html += "function toggleRecording() {";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  xhr.open('GET', '/toggle_record', true);";
  html += "  var statusMsg = document.getElementById('statusMessage');";
  html += "  xhr.onload = function() {";
  html += "    if (xhr.status === 200) {";
  html += "      var recordButton = document.getElementById('recordButton');";
  html += "      if (xhr.responseText === 'RECORDING_STARTED') {";
  html += "        recordButton.textContent = 'Stop Recording';";
  html += "        recordButton.classList.add('recording');";
  html += "        statusMsg.textContent = 'Recording started...';";
  html += "      } else if (xhr.responseText === 'RECORDING_STOPPED') {";
  html += "        recordButton.textContent = 'Start Record';";
  html += "        recordButton.classList.remove('recording');";
  html += "        statusMsg.textContent = 'Recording stopped.';";
  html += "      } else if (xhr.responseText === 'MUST_DELETE_FIRST') {";
  html += "        statusMsg.textContent = 'A recording already exists. Delete it to start a new one.';";
  html += "      } else { statusMsg.textContent = xhr.responseText; }";
  html += "    } else { statusMsg.textContent = 'Error: ' + xhr.status; }";
  html += "  };";
  html += "  xhr.send();";
  html += "}";

  html += "function playSequence() {";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  xhr.open('GET', '/play_sequence', true);";
  html += "  var statusMsg = document.getElementById('statusMessage');";
  html += "  statusMsg.textContent = 'Playback starting...';";
  html += "  xhr.onload = function() {";
  html += "    if (xhr.status === 200) {";
  html += "      statusMsg.textContent = xhr.responseText;"; // e.g., "PLAYBACK_STARTED" or "No sequence"
  html += "    } else { statusMsg.textContent = 'Error: ' + xhr.status; }";
  html += "  };";
  html += "  xhr.send();";
  html += "}";

  html += "function deleteSequence() {";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  xhr.open('GET', '/delete_sequence', true);";
  html += "  var statusMsg = document.getElementById('statusMessage');";
  html += "  xhr.onload = function() {";
  html += "    if (xhr.status === 200) {";
  html += "      statusMsg.textContent = xhr.responseText;";
  html += "      var recordButton = document.getElementById('recordButton');";
  html += "      recordButton.textContent = 'Start Record';"; // Reset record button text
  html += "      recordButton.classList.remove('recording');"; // Reset record button style
  html += "    } else { statusMsg.textContent = 'Error: ' + xhr.status; }";
  html += "  };";
  html += "  xhr.send();";
  html += "}";
  // --- End JavaScript for Record & Play ---

  html += "document.addEventListener('DOMContentLoaded', function() {";
  html += "  if (" + String(baseEnabled ? "true" : "false") + ") { var btn = document.getElementById('baseButton'); btn.textContent = 'Disable'; btn.classList.add('disable'); }";
  html += "  if (" + String(shoulderEnabled ? "true" : "false") + ") { var btn = document.getElementById('shoulderButton'); btn.textContent = 'Disable'; btn.classList.add('disable'); }";
  html += "  if (" + String(elbowEnabled ? "true" : "false") + ") { var btn = document.getElementById('elbowButton'); btn.textContent = 'Disable'; btn.classList.add('disable'); }";
  html += "  if (" + String(gripperEnabled ? "true" : "false") + ") { var btn = document.getElementById('gripperButton'); btn.textContent = 'Disable'; btn.classList.add('disable'); }";
  // Check if a recording exists on page load (more advanced, would require another endpoint or initial data)
  // For now, record button defaults to "Start Record".
  html += "});";
  html += "</script>";

  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleSetServo() {
  String servoName = server.arg("servo");
  String posStr = server.arg("pos");
  int targetPos = posStr.toInt();
  int step = 1;
  int delayTime = 3;

  // Apply hardcoded safety limit for all servos
  targetPos = constrain(targetPos, 0, 180);

  if (servoName == "base") {
    if (baseEnabled) {
      moveServoSmoothly(baseServo, basePos, targetPos, step, delayTime);
      basePos = targetPos;
    }
  } else if (servoName == "shoulder") {
    if (shoulderEnabled) {
      moveServoSmoothly(shoulderServo, shoulderPos, targetPos, step, delayTime);
      shoulderPos = targetPos;
    }
  } else if (servoName == "elbow") {
    if (elbowEnabled) {
      moveServoSmoothly(elbowServo, elbowPos, targetPos, step, delayTime);
      elbowPos = targetPos;
    }
  } else if (servoName == "gripper") {
    if (gripperEnabled) {
      moveServoSmoothly(gripperServo, gripperPos, targetPos, step, delayTime);
      gripperPos = targetPos;
    }
  }

  // If recording, add current state of ALL servos as a new pose
  // Only record if the new pose is different from the last recorded one
  if (isRecording) {
    bool shouldRecord = true;
    if (!recordedSequence.empty()) {
      const Pose& lastPose = recordedSequence.back();
      if (lastPose.base == basePos && lastPose.shoulder == shoulderPos &&
          lastPose.elbow == elbowPos && lastPose.gripper == gripperPos) {
        shouldRecord = false; // Same as last pose, don't record
      }
    }
    
    if (shouldRecord) {
      Pose currentPose;
      currentPose.base = basePos;
      currentPose.shoulder = shoulderPos;
      currentPose.elbow = elbowPos;
      currentPose.gripper = gripperPos;
      recordedSequence.push_back(currentPose);
      Serial.println("Pose recorded. Sequence size: " + String(recordedSequence.size()));
    }
  }
  server.send(200, "text/plain", "OK");
}

void handleToggleServo() {
  String servoName = server.arg("servo");
  bool nowEnabled = false;

  if (servoName == "base") {
    baseEnabled = !baseEnabled;
    nowEnabled = baseEnabled;
    if (baseEnabled) baseServo.attach(basePin); else baseServo.detach();
  } else if (servoName == "shoulder") {
    shoulderEnabled = !shoulderEnabled;
    nowEnabled = shoulderEnabled;
    if (shoulderEnabled) shoulderServo.attach(shoulderPin); else shoulderServo.detach();
  } else if (servoName == "elbow") {
    elbowEnabled = !elbowEnabled;
    nowEnabled = elbowEnabled;
    if (elbowEnabled) elbowServo.attach(elbowPin); else elbowServo.detach();
  } else if (servoName == "gripper") {
    gripperEnabled = !gripperEnabled;
    nowEnabled = gripperEnabled;
    if (gripperEnabled) gripperServo.attach(gripperPin); else gripperServo.detach();
  } else {
    server.send(400, "text/plain", "Invalid servo");
    return;
  }
  server.send(200, "text/plain", nowEnabled ? "enabled" : "disabled");
}

void handleSaveSettings() { // Now only saves Home positions
  auto getArgInt = [&](const String& argName, int currentValue) {
    if (server.hasArg(argName)) {
      return (int)server.arg(argName).toInt();
    }
    return currentValue;
  };

  baseHome = getArgInt("baseHome", baseHome);
  shoulderHome = getArgInt("shoulderHome", shoulderHome);
  elbowHome = getArgInt("elbowHome", elbowHome);
  gripperHome = getArgInt("gripperHome", gripperHome);
  // Limit settings removed

  server.send(200, "text/plain", "Home settings saved");
  Serial.println("Home Settings Saved:");
  Serial.println("  Base H:" + String(baseHome) + ", Shoulder H:" + String(shoulderHome) +
                 ", Elbow H:" + String(elbowHome) + ", Gripper H:" + String(gripperHome));
}

void handleGoHome() {
  if (baseEnabled) {
    moveServoSmoothly(baseServo, basePos, baseHome, 1, 3);
    basePos = baseHome;
  }
  if (shoulderEnabled) {
    moveServoSmoothly(shoulderServo, shoulderPos, shoulderHome, 1, 3);
    shoulderPos = shoulderHome;
  }
  if (elbowEnabled) {
    moveServoSmoothly(elbowServo, elbowPos, elbowHome, 1, 3);
    elbowPos = elbowHome;
  }
  if (gripperEnabled) {
    moveServoSmoothly(gripperServo, gripperPos, gripperHome, 1, 3);
    gripperPos = gripperHome;
  }
  String response = String(baseHome) + "," + String(shoulderHome) + "," + String(elbowHome) + "," + String(gripperHome) + ",";
  response += String(baseEnabled ? "1" : "0") + "," + String(shoulderEnabled ? "1" : "0") + ",";
  response += String(elbowEnabled ? "1" : "0") + "," + String(gripperEnabled ? "1" : "0");
  server.send(200, "text/plain", response);
}

// --- New Handler Functions for Record & Play ---
void handleToggleRecord() {
  if (!isRecording) { // Trying to start recording
    if (!recordedSequence.empty()) {
      Serial.println("Attempt to record over existing seq. Must delete first.");
      server.send(200, "text/plain", "MUST_DELETE_FIRST");
      return;
    }
    isRecording = true;
    // recordedSequence.clear(); // Ensure it's empty if we were to auto-clear
    Serial.println("Recording started.");
    server.send(200, "text/plain", "RECORDING_STARTED");
  } else { // Trying to stop recording
    isRecording = false;
    Serial.println("Recording stopped. Total Poses: " + String(recordedSequence.size()));
    server.send(200, "text/plain", "RECORDING_STOPPED");
  }
}

void handlePlaySequence() {
  if (recordedSequence.empty()) {
    server.send(200, "text/plain", "No sequence recorded to play.");
    Serial.println("Playback attempt: No sequence.");
    return;
  }

  Serial.println("Playing sequence of " + String(recordedSequence.size()) + " poses...");
  server.send(200, "text/plain", "PLAYBACK_STARTED"); 

  bool tempBaseEnabled = !baseEnabled; // True if we need to temp enable
  bool tempShoulderEnabled = !shoulderEnabled;
  bool tempElbowEnabled = !elbowEnabled;
  bool tempGripperEnabled = !gripperEnabled;

  if (tempBaseEnabled) baseServo.attach(basePin);
  if (tempShoulderEnabled) shoulderServo.attach(shoulderPin);
  if (tempElbowEnabled) elbowServo.attach(elbowPin);
  if (tempGripperEnabled) gripperServo.attach(gripperPin);
  
  int currentPlaybackBasePos = basePos;
  int currentPlaybackShoulderPos = shoulderPos;
  int currentPlaybackElbowPos = elbowPos;
  int currentPlaybackGripperPos = gripperPos;

  for (const auto& pose : recordedSequence) {
    int playStep = 1;   
    int playDelay = 10; 

    moveServoSmoothly(baseServo, currentPlaybackBasePos, pose.base, playStep, playDelay);
    currentPlaybackBasePos = pose.base; 

    moveServoSmoothly(shoulderServo, currentPlaybackShoulderPos, pose.shoulder, playStep, playDelay);
    currentPlaybackShoulderPos = pose.shoulder;

    moveServoSmoothly(elbowServo, currentPlaybackElbowPos, pose.elbow, playStep, playDelay);
    currentPlaybackElbowPos = pose.elbow;

    moveServoSmoothly(gripperServo, currentPlaybackGripperPos, pose.gripper, playStep, playDelay);
    currentPlaybackGripperPos = pose.gripper;

    delay(100); 
  }

  basePos = currentPlaybackBasePos;
  shoulderPos = currentPlaybackShoulderPos;
  elbowPos = currentPlaybackElbowPos;
  gripperPos = currentPlaybackGripperPos;

  if (tempBaseEnabled) baseServo.detach();
  if (tempShoulderEnabled) shoulderServo.detach();
  if (tempElbowEnabled) elbowServo.detach();
  if (tempGripperEnabled) gripperServo.detach();

  Serial.println("Playback finished.");
}

void handleDeleteSequence() {
  recordedSequence.clear();
  bool wasRecording = isRecording;
  isRecording = false; 
  
  Serial.println("Recorded sequence deleted.");
  if (wasRecording) {
    server.send(200, "text/plain", "Sequence deleted. Recording stopped.");
  } else {
    server.send(200, "text/plain", "Sequence deleted.");
  }
}
// --- End New Handler Functions ---


void moveServoSmoothly(Servo& servo, int currentServoPos, int targetPos, int step, int delayTime) {
  if (!servo.attached()) {
    // Serial.println("Servo not attached, cannot move."); // Optional debug
    return; 
  }
  
  // Ensure targetPos is within valid servo range (0-180) if not already done
  targetPos = constrain(targetPos, 0, 180);

  if (targetPos > currentServoPos) {
    for (int p = currentServoPos; p <= targetPos; p += step) {
      servo.write(p);
      delay(delayTime);
    }
  } else if (targetPos < currentServoPos) {
    for (int p = currentServoPos; p >= targetPos; p -= step) {
      servo.write(p);
      delay(delayTime);
    }
  } else {
    servo.write(targetPos); // If already at target, ensure it's written
  }
  // Final write to ensure it reaches the exact target position,
  // especially if step doesn't divide evenly.
  if (currentServoPos != targetPos) { // Only if a move actually happened
      servo.write(targetPos); 
  }
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000); // Wait for Serial up to 3s
  Serial.println();
  Serial.println("Configuring Access Point...");

  WiFi.softAP(apSSID, apPassword);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/set_servo", HTTP_GET, handleSetServo);
  server.on("/toggle_servo", HTTP_GET, handleToggleServo);
  server.on("/save_settings", HTTP_GET, handleSaveSettings);
  server.on("/go_home", HTTP_GET, handleGoHome);

  // New routes for record and play
  server.on("/toggle_record", HTTP_GET, handleToggleRecord);
  server.on("/play_sequence", HTTP_GET, handlePlaySequence);
  server.on("/delete_sequence", HTTP_GET, handleDeleteSequence);

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");
  Serial.println("Connect to Wi-Fi AP: " + String(apSSID));
  Serial.println("Open http://" + myIP.toString() + " in your browser.");
}

void loop() {
  server.handleClient();
  // delay(1); // Can be useful for stability with ESP32 background tasks
}