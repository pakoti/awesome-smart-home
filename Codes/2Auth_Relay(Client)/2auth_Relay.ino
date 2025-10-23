#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

// WiFi credentials
const char* ssid = "Your_WiFi_SSID";
const char* password = "Your_WiFi_Password";

// Web server authentication
const char* www_username = "admin";
const char* www_password = "admin123";

// Door relay password
const char* door_password = "open123";

// Relay pins - Using GPIO numbers
#define TOTAL_RELAYS 4
#define REGULAR_RELAYS 3
#define DOOR_RELAY 3  // Fourth relay is for door

// GPIO pins
int relayPins[TOTAL_RELAYS] = {5, 4, 14, 12};
bool relayStates[TOTAL_RELAYS];

// Door open duration (2 seconds)
#define DOOR_OPEN_TIME 2000

ESP8266WebServer server(80);

bool isAuthenticated = false;

// Login page HTML
const char* loginPage = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>Relay Control - Login</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { 
      font-family: Arial, sans-serif; 
      text-align: center; 
      background-color: #f0f0f0;
      margin: 0;
      padding: 20px;
    }
    .login-container {
      max-width: 400px;
      margin: 100px auto;
      background: white;
      padding: 30px;
      border-radius: 10px;
      box-shadow: 0 0 10px rgba(0,0,0,0.1);
    }
    .input-group {
      margin: 15px 0;
      text-align: left;
    }
    label {
      display: block;
      margin-bottom: 5px;
      font-weight: bold;
    }
    input[type="text"], input[type="password"] {
      width: 100%;
      padding: 10px;
      border: 1px solid #ddd;
      border-radius: 5px;
      box-sizing: border-box;
    }
    .login-btn {
      width: 100%;
      padding: 12px;
      background-color: #007bff;
      color: white;
      border: none;
      border-radius: 5px;
      cursor: pointer;
      font-size: 16px;
      margin-top: 10px;
    }
    .login-btn:hover {
      background-color: #0056b3;
    }
    .error {
      color: red;
      margin-top: 10px;
    }
  </style>
</head>
<body>
  <div class="login-container">
    <h1>Relay Control System</h1>
    <h2>Please Login</h2>
    <form action="/login" method="POST">
      <div class="input-group">
        <label for="username">Username:</label>
        <input type="text" id="username" name="username" required>
      </div>
      <div class="input-group">
        <label for="password">Password:</label>
        <input type="password" id="password" name="password" required>
      </div>
      <button type="submit" class="login-btn">Login</button>
    </form>
    <div id="error" class="error"></div>
  </div>

  <script>
    const urlParams = new URLSearchParams(window.location.search);
    if(urlParams.has('error')) {
      document.getElementById('error').textContent = 'Invalid username or password!';
    }
  </script>
</body>
</html>
)rawliteral";

// Main control page HTML
const char* controlPage = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>Relay Control Panel</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { 
      font-family: Arial, sans-serif; 
      text-align: center; 
      background-color: #f0f0f0;
      margin: 0;
      padding: 20px;
    }
    .container {
      max-width: 600px;
      margin: 0 auto;
      background: white;
      padding: 20px;
      border-radius: 10px;
      box-shadow: 0 0 10px rgba(0,0,0,0.1);
    }
    .header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 20px;
    }
    .logout-btn {
      padding: 8px 15px;
      background-color: #dc3545;
      color: white;
      border: none;
      border-radius: 5px;
      cursor: pointer;
      text-decoration: none;
    }
    .relay-section, .door-section {
      margin: 20px 0;
      padding: 15px;
      background-color: #f8f9fa;
      border-radius: 5px;
    }
    .door-section {
      background-color: #e7f3ff;
    }
    .relay-btn {
      padding: 15px 30px;
      margin: 10px;
      font-size: 16px;
      border: none;
      border-radius: 5px;
      cursor: pointer;
      width: 200px;
    }
    .btn-on { background-color: #4CAF50; color: white; }
    .btn-off { background-color: #f44336; color: white; }
    .door-btn { 
      background-color: #007bff; 
      color: white; 
      width: 150px;
    }
    .door-input {
      padding: 10px;
      margin: 10px;
      width: 200px;
      border: 1px solid #ddd;
      border-radius: 5px;
    }
    .status-panel {
      display: flex;
      justify-content: space-around;
      flex-wrap: wrap;
      margin: 20px 0;
    }
    .status-item {
      padding: 15px;
      margin: 5px;
      border-radius: 5px;
      font-weight: bold;
      min-width: 120px;
    }
    .status-on { background-color: #d4edda; color: #155724; }
    .status-off { background-color: #f8d7da; color: #721c24; }
    .last-update {
      margin-top: 20px;
      color: #666;
      font-size: 14px;
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <h1>Relay Control Panel</h1>
      <a href="/" class="logout-btn">Logout</a>
    </div>

    <!-- Status Display -->
    <div class="status-panel">
      <div class="status-item" id="status0">Relay 1: OFF</div>
      <div class="status-item" id="status1">Relay 2: OFF</div>
      <div class="status-item" id="status2">Relay 3: OFF</div>
      <div class="status-item" id="status3">Door: LOCKED</div>
    </div>

    <!-- Regular Relay Controls -->
    <div class="relay-section">
      <h2>Regular Relays Control</h2>
      <button class="relay-btn btn-off" onclick="toggleRelay(0)" id="btn0">Relay 1: OFF</button><br>
      <button class="relay-btn btn-off" onclick="toggleRelay(1)" id="btn1">Relay 2: OFF</button><br>
      <button class="relay-btn btn-off" onclick="toggleRelay(2)" id="btn2">Relay 3: OFF</button>
    </div>

    <!-- Door Relay Control -->
    <div class="door-section">
      <h2>Door Control</h2>
      <p>Enter password to open door for 2 seconds</p>
      <input type="password" id="doorPassword" class="door-input" placeholder="Enter door password">
      <br>
      <button onclick="openDoor()" class="relay-btn door-btn">Open Door</button>
    </div>

    <div class="last-update" id="lastUpdate">
      Last updated: Just now
    </div>
  </div>

  <script>
    var relayStates = [false, false, false, false];

    function loadRelayStates() {
      fetch('/getStates')
        .then(response => response.json())
        .then(data => {
          relayStates = data;
          updateInterface();
          updateLastUpdateTime();
        })
        .catch(error => console.error('Error loading states:', error));
    }

    function toggleRelay(relayNum) {
      if(relayNum >= 3) return;
      
      var newState = !relayStates[relayNum];
      
      var xhr = new XMLHttpRequest();
      xhr.open('GET', '/toggle?relay=' + relayNum + '&state=' + (newState ? 1 : 0), true);
      xhr.onload = function() {
        if (xhr.status === 200) {
          relayStates[relayNum] = newState;
          updateInterface();
        } else {
          alert('Error controlling relay');
        }
      };
      xhr.send();
    }

    function openDoor() {
      var password = document.getElementById('doorPassword').value;
      
      if (!password) {
        alert('Please enter door password');
        return;
      }

      var xhr = new XMLHttpRequest();
      xhr.open('GET', '/openDoor?password=' + encodeURIComponent(password), true);
      xhr.onload = function() {
        if (xhr.status === 200) {
          alert('Door opened for 2 seconds!');
          document.getElementById('doorPassword').value = '';
          setTimeout(loadRelayStates, 2500);
        } else {
          alert('Error: ' + xhr.responseText);
        }
      };
      xhr.send();
    }

    function updateInterface() {
      // Update status display
      for(var i = 0; i < 4; i++) {
        var statusElement = document.getElementById('status' + i);
        if (i < 3) {
          statusElement.textContent = 'Relay ' + (i+1) + ': ' + (relayStates[i] ? 'ON' : 'OFF');
        } else {
          statusElement.textContent = 'Door: ' + (relayStates[i] ? 'OPEN' : 'LOCKED');
        }
        statusElement.className = 'status-item ' + (relayStates[i] ? 'status-on' : 'status-off');
      }
      
      // Update button colors for regular relays
      for(var i = 0; i < 3; i++) {
        var button = document.getElementById('btn' + i);
        button.textContent = 'Relay ' + (i+1) + ': ' + (relayStates[i] ? 'ON' : 'OFF');
        button.className = 'relay-btn ' + (relayStates[i] ? 'btn-on' : 'btn-off');
      }
    }

    function updateLastUpdateTime() {
      var now = new Date();
      var timeString = now.toLocaleTimeString();
      document.getElementById('lastUpdate').textContent = 'Last updated: ' + timeString;
    }

    // Load states on page load and update every 2 seconds
    window.onload = function() {
      loadRelayStates();
      setInterval(loadRelayStates, 2000);
    };
  </script>
</body>
</html>
)rawliteral";

// Function to save relay state to EEPROM
void saveRelayState(int relayIndex, bool state) {
  relayStates[relayIndex] = state;
  EEPROM.write(relayIndex, state);
  EEPROM.commit();
  Serial.println("Saved relay " + String(relayIndex) + " state: " + String(state));
}

// Function to load all relay states from EEPROM
void loadRelayStates() {
  for(int i = 0; i < TOTAL_RELAYS; i++) {
    relayStates[i] = EEPROM.read(i);
    digitalWrite(relayPins[i], relayStates[i] ? HIGH : LOW);
    Serial.println("Loaded relay " + String(i) + " state: " + String(relayStates[i]));
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\nStarting Relay Control System...");
  
  // Initialize EEPROM
  EEPROM.begin(512);
  
  // Initialize relays
  for(int i = 0; i < TOTAL_RELAYS; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], LOW);
    Serial.println("Initialized relay pin: " + String(relayPins[i]));
  }
  
  // Load saved states from EEPROM
  loadRelayStates();
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  // Setup server routes
  server.on("/", HTTP_GET, []() {
    Serial.println("GET / - Serving login page");
    isAuthenticated = false;
    server.send(200, "text/html", loginPage);
  });
  
  server.on("/login", HTTP_POST, []() {
    String username = server.arg("username");
    String password = server.arg("password");
    
    Serial.println("POST /login - Username: " + username + " Password: " + password);
    
    if(username == www_username && password == www_password) {
      Serial.println("Login successful");
      isAuthenticated = true;
      server.sendHeader("Location", "/control");
      server.send(302, "text/plain", "Login successful");
    } else {
      Serial.println("Login failed");
      server.sendHeader("Location", "/?error=1");
      server.send(302, "text/plain", "Login failed");
    }
  });
  
  server.on("/control", HTTP_GET, []() {
    Serial.println("GET /control");
    if(!isAuthenticated) {
      Serial.println("Not authenticated, redirecting to login");
      server.sendHeader("Location", "/");
      server.send(302, "text/plain", "Not authenticated");
      return;
    }
    server.send(200, "text/html", controlPage);
  });
  
  server.on("/toggle", HTTP_GET, []() {
    Serial.println("GET /toggle");
    if(!isAuthenticated) {
      server.send(401, "text/plain", "Not authenticated");
      return;
    }
    
    if(server.hasArg("relay") && server.hasArg("state")) {
      int relay = server.arg("relay").toInt();
      bool state = server.arg("state").toInt() == 1;
      
      Serial.println("Toggle relay " + String(relay) + " to " + String(state));
      
      if(relay >= 0 && relay < REGULAR_RELAYS) {
        digitalWrite(relayPins[relay], state ? HIGH : LOW);
        saveRelayState(relay, state);
        server.send(200, "text/plain", "OK");
        Serial.println("Relay toggled successfully");
      } else {
        server.send(400, "text/plain", "Invalid relay");
      }
    } else {
      server.send(400, "text/plain", "Missing parameters");
    }
  });
  
  server.on("/openDoor", HTTP_GET, []() {
    Serial.println("GET /openDoor");
    if(!isAuthenticated) {
      server.send(401, "text/plain", "Not authenticated");
      return;
    }
    
    if(server.hasArg("password")) {
      String enteredPassword = server.arg("password");
      Serial.println("Door password attempt: " + enteredPassword);
      
      if(enteredPassword != door_password) {
        server.send(401, "text/plain", "Invalid door password");
        return;
      }
      
      // Turn door relay ON (open door)
      digitalWrite(relayPins[DOOR_RELAY], HIGH);
      saveRelayState(DOOR_RELAY, true);
      server.send(200, "text/plain", "OK");
      Serial.println("Door opened successfully");
      
      // Turn door relay OFF after 2 seconds (close door)
      delay(DOOR_OPEN_TIME);
      digitalWrite(relayPins[DOOR_RELAY], LOW);
      saveRelayState(DOOR_RELAY, false);
      Serial.println("Door closed after 2 seconds");
      
    } else {
      server.send(400, "text/plain", "Missing password");
    }
  });
  
  server.on("/getStates", HTTP_GET, []() {
    if(!isAuthenticated) {
      server.send(401, "text/plain", "Not authenticated");
      return;
    }
    
    String json = "[";
    for(int i = 0; i < TOTAL_RELAYS; i++) {
      json += String(relayStates[i]);
      if(i < TOTAL_RELAYS - 1) json += ",";
    }
    json += "]";
    server.send(200, "application/json", json);
    Serial.println("Sent states: " + json);
  });
  
  server.onNotFound([]() {
    Serial.println("404 Not Found: " + server.uri());
    server.send(404, "text/plain", "Page not found");
  });
  
  server.begin();
  Serial.println("HTTP server started successfully!");
}

void loop() {
  server.handleClient();
}