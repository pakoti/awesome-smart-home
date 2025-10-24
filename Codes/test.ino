#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// WiFi credentials for Access Point
const char* ap_ssid = "ESP8266_Relay_Controller";
const char* ap_password = "12345678";

ESP8266WebServer server(80);

// Relay pins
const int relayPins[] = {5, 4, 14, 12}; // D1, D2, D5, D6 on NodeMCU
const int numRelays = 4;

// Door relay timing
const unsigned long DOOR_OPEN_TIME_MS = 2000; // 2 seconds
unsigned long doorCloseTime = 0;
bool doorOpening = false;

// User credentials (change these in production!)
const String VALID_USERNAME = "admin"; //username
const String VALID_PASSWORD = "main123"; //password for entreing 
const String DOOR_PASSWORD = "door456"; // door key

// Authentication states
bool mainAuthenticated = false;
unsigned long authTimeout = 0;
const unsigned long AUTH_TIMEOUT_MS = 6000; // 5 minutes * 6000 ms for each minute (session time)

void setup() {
  Serial.begin(115200);
  
  // Initialize relays (LOW trigger relays)
  for (int i = 0; i < numRelays; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], HIGH); // Start with relays OFF
  }
  

 // Create Access Point
  WiFi.mode(WIFI_AP);
  
  // Configure IP address
  IPAddress local_IP(192, 168, 4, 1);    // Set your desired IP
  IPAddress gateway(192, 168, 4, 1);     // Set gateway IP
  IPAddress subnet(255, 255, 255, 0);    // Set subnet mask
  
  if (!WiFi.softAPConfig(local_IP, gateway, subnet)) {
    Serial.println("STA Failed to configure");
  }
  WiFi.softAP(ap_ssid, ap_password);
  
  Serial.println("Access Point Started");
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());


  // Setup server routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/login", HTTP_POST, handleLogin);
  server.on("/control", HTTP_POST, handleControl);
  server.on("/door-control", HTTP_POST, handleDoorControl);
  server.on("/logout", HTTP_POST, handleLogout);
  server.on("/status", HTTP_GET, handleStatus);
  
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
  
  // Check authentication timeout
  if (mainAuthenticated && millis() > authTimeout) {
    mainAuthenticated = false;
    Serial.println("Authentication timeout - logged out");
  }
  
  // Auto-close door after 2 seconds
  if (doorOpening && millis() > doorCloseTime) {
    digitalWrite(relayPins[3], HIGH); // Close door relay
    doorOpening = false;
    Serial.println("Door automatically closed");
  }
}

void handleRoot() {
  // Serve different content based on authentication status
  if (mainAuthenticated) {
    serveMainPage();
  } else {
    serveLoginPage();
  }
}

void serveLoginPage() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP8266 Relay Controller - Login</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; font-family: Arial, sans-serif; }
        body { background-color: #f5f5f5; color: #333; line-height: 1.6; padding: 20px; display: flex; justify-content: center; align-items: center; min-height: 100vh; }
        .login-container { max-width: 400px; width: 100%; background: white; border-radius: 10px; box-shadow: 0 0 20px rgba(0,0,0,0.1); padding: 30px; }
        h1 { color: #2c3e50; text-align: center; margin-bottom: 20px; }
        .form-group { margin-bottom: 20px; }
        label { display: block; margin-bottom: 5px; font-weight: 600; }
        input[type="text"], input[type="password"] { width: 100%; padding: 12px; border: 1px solid #ddd; border-radius: 4px; font-size: 1rem; }
        button { background: #3498db; color: white; border: none; padding: 12px 20px; border-radius: 4px; cursor: pointer; font-size: 1rem; width: 100%; transition: background 0.3s; }
        button:hover { background: #2980b9; }
        .message { padding: 10px; border-radius: 4px; margin-top: 15px; text-align: center; display: none; }
        .error { background: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; }
        .info { text-align: center; margin-top: 20px; color: #7f8c8d; font-size: 0.9rem; }
    </style>
</head>
<body>
    <div class="login-container">
        <h1>Relay Controller Login</h1>
        <p style="text-align: center; margin-bottom: 20px; color: #666;">Enter username and password to access relay controls</p>
        
        <div class="form-group">
            <label for="username">Username</label>
            <input type="text" id="username" placeholder="Enter username" autocomplete="username">
        </div>
        
        <div class="form-group">
            <label for="password">Password</label>
            <input type="password" id="password" placeholder="Enter password" autocomplete="current-password">
        </div>
        
        <button id="login-button">Login</button>
        
        <div id="login-message" class="message"></div>
        <div class="info">
            <p>IP: )rawliteral" + WiFi.softAPIP().toString() + R"rawliteral(</p>
        </div>
    </div>

    <script>
        document.addEventListener('DOMContentLoaded', function() {
            const loginButton = document.getElementById('login-button');
            const usernameInput = document.getElementById('username');
            const passwordInput = document.getElementById('password');
            const messageDiv = document.getElementById('login-message');

            loginButton.addEventListener('click', function() {
                const username = usernameInput.value;
                const password = passwordInput.value;
                
                if (!username || !password) {
                    showMessage('Please enter both username and password', 'error');
                    return;
                }

                fetch('/login', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                    body: 'username=' + encodeURIComponent(username) + '&password=' + encodeURIComponent(password)
                })
                .then(response => response.text())
                .then(data => {
                    if (data === 'success') {
                        window.location.href = '/';
                    } else {
                        showMessage('Invalid username or password. Please try again.', 'error');
                        passwordInput.value = '';
                    }
                })
                .catch(error => {
                    showMessage('Login failed. Please try again.', 'error');
                });
            });

            // Enter key support
            usernameInput.addEventListener('keypress', function(e) {
                if (e.key === 'Enter') {
                    passwordInput.focus();
                }
            });

            passwordInput.addEventListener('keypress', function(e) {
                if (e.key === 'Enter') {
                    loginButton.click();
                }
            });

            function showMessage(text, type) {
                messageDiv.textContent = text;
                messageDiv.className = 'message ' + type;
                messageDiv.style.display = 'block';
            }
        });
    </script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}

void serveMainPage() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP8266 Relay Controller</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; font-family: Arial, sans-serif; }
        body { background-color: #f5f5f5; color: #333; line-height: 1.6; padding: 20px; }
        .container { max-width: 800px; margin: 0 auto; background: white; border-radius: 10px; box-shadow: 0 0 20px rgba(0,0,0,0.1); padding: 30px; }
        header { text-align: center; margin-bottom: 30px; padding-bottom: 20px; border-bottom: 1px solid #eee; position: relative; }
        h1 { color: #2c3e50; margin-bottom: 10px; }
        .user-info { background: #ecf0f1; padding: 10px; border-radius: 5px; margin-bottom: 15px; text-align: center; }
        .status-bar { display: flex; justify-content: space-between; background: #3498db; color: white; padding: 10px 15px; border-radius: 5px; margin-bottom: 20px; }
        .relay-control { margin-bottom: 30px; }
        .relay-title { font-size: 1.2rem; margin-bottom: 15px; color: #2c3e50; }
        .radio-group { display: flex; gap: 20px; margin-bottom: 20px; }
        .radio-option { display: flex; align-items: center; }
        .form-group { margin-bottom: 15px; }
        label { display: block; margin-bottom: 5px; font-weight: 600; }
        input[type="password"] { width: 100%; padding: 10px; border: 1px solid #ddd; border-radius: 4px; font-size: 1rem; }
        button { background: #3498db; color: white; border: none; padding: 10px 15px; border-radius: 4px; cursor: pointer; font-size: 1rem; transition: background 0.3s; }
        button:hover { background: #2980b9; }
        button:disabled { background: #95a5a6; cursor: not-allowed; }
        .door-section { background: #e8f4fd; border-left: 4px solid #3498db; padding: 20px; border-radius: 8px; margin-top: 30px; }
        .door-button { background: #e74c3c; width: 100%; padding: 12px; font-size: 1.1rem; margin-top: 10px; }
        .door-button:hover { background: #c0392b; }
        .door-button:disabled { background: #95a5a6; }
        .countdown { font-weight: bold; color: #e74c3c; margin-left: 10px; }
        .message { padding: 10px; border-radius: 4px; margin-top: 15px; display: none; }
        .success { background: #d4edda; color: #155724; border: 1px solid #c3e6cb; }
        .error { background: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; }
        .logout-btn { position: absolute; top: 0; right: 0; background: #95a5a6; }
        .logout-btn:hover { background: #7f8c8d; }
        footer { text-align: center; margin-top: 30px; padding-top: 20px; border-top: 1px solid #eee; color: #7f8c8d; font-size: 0.9rem; }
        @media (max-width: 600px) { .container { padding: 15px; } .radio-group { flex-direction: column; gap: 10px; } .status-bar { flex-direction: column; gap: 10px; } }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <h1>ESP8266 Relay Controller</h1>
            <p>Control your 4-channel relay module securely</p>
            <button class="logout-btn" id="logout-button">Logout</button>
        </header>
        
        <div class="user-info">
            Welcome, <strong id="username-display">User</strong> | Session expires in: <span id="session-timeout">5:00</span>
        </div>
        
        <div class="status-bar">
            <div>Status: <span id="connection-status">Connected</span></div>
            <div>IP: )rawliteral" + WiFi.softAPIP().toString() + R"rawliteral(</div>
            <div>Door: <span id="door-status">Closed</span></div>
        </div>
        
        <div class="relay-control">
            <div class="relay-item">
                <h3 class="relay-title">Relay 1 - Output1</h3>
                <div class="radio-group">
                    <div class="radio-option">
                        <input type="radio" id="relay1-on" name="relay1" value="1">
                        <label for="relay1-on">ON</label>
                    </div>
                    <div class="radio-option">
                        <input type="radio" id="relay1-off" name="relay1" value="0" checked>
                        <label for="relay1-off">OFF</label>
                    </div>
                </div>
            </div>
            
            <div class="relay-item">
                <h3 class="relay-title">Relay 2 - Output2</h3>
                <div class="radio-group">
                    <div class="radio-option">
                        <input type="radio" id="relay2-on" name="relay2" value="1">
                        <label for="relay2-on">ON</label>
                    </div>
                    <div class="radio-option">
                        <input type="radio" id="relay2-off" name="relay2" value="0" checked>
                        <label for="relay2-off">OFF</label>
                    </div>
                </div>
            </div>
            
            <div class="relay-item">
                <h3 class="relay-title">Relay 3 - Output3</h3>
                <div class="radio-group">
                    <div class="radio-option">
                        <input type="radio" id="relay3-on" name="relay3" value="1">
                        <label for="relay3-on">ON</label>
                    </div>
                    <div class="radio-option">
                        <input type="radio" id="relay3-off" name="relay3" value="0" checked>
                        <label for="relay3-off">OFF</label>
                    </div>
                </div>
            </div>
            
            <div class="door-section">
                <h3 class="relay-title"> Door Control</h3>
                <p><strong>Additional authentication required for door control</strong></p>
                <p style="color: #e74c3c; font-size: 0.9rem;">Door will automatically close after 2 seconds</p>
                
                <div class="form-group">
                    <label for="door-password">Door Password</label>
                    <input type="password" id="door-password" placeholder="Enter door password">
                </div>
                
                <button id="door-open-button" class="door-button">
                     OPEN DOOR
                </button>
                <div id="door-countdown" style="text-align: center; margin-top: 10px; display: none;">
                    Door closing in: <span class="countdown" id="countdown-timer">2</span> seconds
                </div>
                
                <div id="door-message" class="message"></div>
            </div>
            
            <div id="control-message" class="message"></div>
        </div>
        
        <footer>
            <p>ESP8266 Relay Controller | Dual Authentication System</p>
        </footer>
    </div>

    <script>
        document.addEventListener('DOMContentLoaded', function() {
            const logoutButton = document.getElementById('logout-button');
            const doorOpenButton = document.getElementById('door-open-button');
            const doorPasswordInput = document.getElementById('door-password');
            const doorMessage = document.getElementById('door-message');
            const controlMessage = document.getElementById('control-message');
            const sessionTimeout = document.getElementById('session-timeout');
            const doorCountdown = document.getElementById('door-countdown');
            const countdownTimer = document.getElementById('countdown-timer');
            const doorStatus = document.getElementById('door-status');
            const usernameDisplay = document.getElementById('username-display');
            
            let timeoutMinutes = 5;
            let timeoutSeconds = 0;
            let doorCountdownInterval;
            
            // Display username (you can modify this to show actual username)
            usernameDisplay.textContent = 'Admin';
            
            // Update session timer
            function updateTimer() {
                if (timeoutSeconds === 0) {
                    if (timeoutMinutes === 0) {
                        window.location.href = '/';
                        return;
                    }
                    timeoutMinutes--;
                    timeoutSeconds = 59;
                } else {
                    timeoutSeconds--;
                }
                sessionTimeout.textContent = timeoutMinutes + ':' + (timeoutSeconds < 10 ? '0' : '') + timeoutSeconds;
            }
            
            setInterval(updateTimer, 1000);
            
            // Logout functionality
            logoutButton.addEventListener('click', function() {
                fetch('/logout', {
                    method: 'POST'
                })
                .then(() => {
                    window.location.href = '/';
                });
            });
            
            // Regular relay controls (no additional auth needed)
            const relayInputs = document.querySelectorAll('input[name="relay1"], input[name="relay2"], input[name="relay3"]');
            relayInputs.forEach(input => {
                input.addEventListener('change', function() {
                    const relayNum = this.name.replace('relay', '');
                    const relayState = this.value;
                    
                    fetch('/control', {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                        body: 'relay=' + relayNum + '&state=' + relayState
                    })
                    .then(response => response.text())
                    .then(data => {
                        if (data === 'success') {
                            showMessage(controlMessage, "Relay " + relayNum + " turned " + (relayState === '1' ? 'ON' : 'OFF'), "success");
                        } else {
                            showMessage(controlMessage, "Failed to control relay " + relayNum, "error");
                        }
                    })
                    .catch(error => {
                        showMessage(controlMessage, "Error controlling relay", "error");
                    });
                });
            });
            
            // Door control with additional authentication
            doorOpenButton.addEventListener('click', function() {
                const doorPassword = doorPasswordInput.value;
                
                if (!doorPassword) {
                    showMessage(doorMessage, "Please enter door password", "error");
                    return;
                }
                
                // Disable button during operation
                doorOpenButton.disabled = true;
                doorOpenButton.textContent = " OPENING...";
                doorStatus.textContent = "Opening...";
                
                fetch('/door-control', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                    body: 'password=' + encodeURIComponent(doorPassword)
                })
                .then(response => response.text())
                .then(data => {
                    if (data === 'success') {
                        showMessage(doorMessage, "Door opening command sent successfully!", "success");
                        startDoorCountdown();
                    } else {
                        showMessage(doorMessage, "Invalid door password. Access denied.", "error");
                        resetDoorButton();
                    }
                    doorPasswordInput.value = '';
                })
                .catch(error => {
                    showMessage(doorMessage, "Error controlling door", "error");
                    resetDoorButton();
                });
            });
            
            function startDoorCountdown() {
                let seconds = 2;
                doorCountdown.style.display = 'block';
                countdownTimer.textContent = seconds;
                doorStatus.textContent = "Open - Closing in " + seconds + "s";
                
                doorCountdownInterval = setInterval(function() {
                    seconds--;
                    countdownTimer.textContent = seconds;
                    doorStatus.textContent = "Open - Closing in " + seconds + "s";
                    
                    if (seconds <= 0) {
                        clearInterval(doorCountdownInterval);
                        doorCountdown.style.display = 'none';
                        resetDoorButton();
                        doorStatus.textContent = "Closed";
                        showMessage(controlMessage, "Door has been automatically closed", "success");
                    }
                }, 1000);
            }
            
            function resetDoorButton() {
                doorOpenButton.disabled = false;
                doorOpenButton.textContent = "OPEN DOOR";
            }
            
            function showMessage(element, text, type) {
                element.textContent = text;
                element.className = 'message ' + type;
                element.style.display = 'block';
                
                if (type === 'success') {
                    setTimeout(() => {
                        element.style.display = 'none';
                    }, 3000);
                }
            }
            
            // Check door status periodically
            function checkDoorStatus() {
                fetch('/status')
                .then(response => response.text())
                .then(status => {
                    if (status === 'opening') {
                        doorStatus.textContent = "Opening...";
                    } else {
                        doorStatus.textContent = "Ready";
                    resetDoorButton();
                        doorCountdown.style.display = 'none';
                        clearInterval(doorCountdownInterval);
                    }
                })
                .catch(error => {
                    console.log('Status check failed');
                });
            }
            
            // Check status every 5 seconds
            setInterval(checkDoorStatus, 5000);
        });
    </script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}

void handleLogin() {
  if (server.hasArg("username") && server.hasArg("password")) {
    String username = server.arg("username");
    String password = server.arg("password");
    
    if (username == VALID_USERNAME && password == VALID_PASSWORD) {
      mainAuthenticated = true;
      authTimeout = millis() + AUTH_TIMEOUT_MS;
      server.send(200, "text/plain", "success");
      Serial.println("User " + username + " logged in successfully");
      return;
    }
  }
  server.send(200, "text/plain", "fail");
  Serial.println("Login failed - invalid credentials");
}

void handleControl() {
  if (!mainAuthenticated) {
    server.send(401, "text/plain", "unauthorized");
    return;
  }
  
  if (server.hasArg("relay") && server.hasArg("state")) {
    int relay = server.arg("relay").toInt() - 1;
    int state = server.arg("state").toInt();
    
    if (relay >= 0 && relay < numRelays - 1) { // Only allow relays 1-3
      digitalWrite(relayPins[relay], state ? LOW : HIGH);
      server.send(200, "text/plain", "success");
      Serial.println("Relay " + String(relay + 1) + " set to " + (state ? "ON" : "OFF"));
      return;
    }
  }
  server.send(400, "text/plain", "bad_request");
}

void handleDoorControl() {
  if (!mainAuthenticated) {
    server.send(401, "text/plain", "unauthorized");
    return;
  }
  
  if (server.hasArg("password")) {
    String password = server.arg("password");
    
    if (password == DOOR_PASSWORD) {
      // Open door for 2 seconds
      digitalWrite(relayPins[3], LOW); // Activate door relay
      doorOpening = true;
      doorCloseTime = millis() + DOOR_OPEN_TIME_MS;
      
      server.send(200, "text/plain", "success");
      Serial.println("Door opened for 2 seconds with proper authentication");
      return;
    }
  }
  server.send(200, "text/plain", "fail");
}

void handleLogout() {
  mainAuthenticated = false;
  server.send(200, "text/plain", "logged_out");
  Serial.println("User logged out");
}

void handleStatus() {
  if (doorOpening) {
    server.send(200, "text/plain", "opening");
  } else {
    server.send(200, "text/plain", "ready");
  }
}