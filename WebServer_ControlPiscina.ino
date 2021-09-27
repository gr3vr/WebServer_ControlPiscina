#include <WiFi.h>
#include <EEPROM.h>
#include <SPIFFS.h>
#include <ESPAsyncWebServer.h>
#include <WebSocketsServer.h>

// Constants

//const char  *ssid = "PoolControlSystem";
//const char  *password =  "12345678";

const char  *ssid = "";
const char  *password = "";

const char  *PARAM_INPUT_1 = "input1";
const char  *PARAM_INPUT_2 = "input2";


const char  *soft_ap_ssid = "SistemaControlPiscina";
const char  *soft_ap_password = "12345678";

const char  *msg_toggle_led = "toggleLED";
const char  *msg_get_led = "getLEDState";
const int   http_port = 80;
const int   ws_port = 1337;
const int   led_pin = 2;

int ip;

// Globals
AsyncWebServer server(http_port);
WebSocketsServer webSocket = WebSocketsServer(ws_port);
char msg_buf[10];
int led_state = 0;

/***********************************************************
   Functions
*/

// Callback: receiving any WebSocket message
void onWebSocketEvent(uint8_t client_num,
                      WStype_t type,
                      uint8_t * payload,
                      size_t length) {

  // Figure out the type of WebSocket event
  switch (type) {

    // Client has disconnected
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", client_num);
      break;

    // New client has connected
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(client_num);
        Serial.printf("[%u] Connection from ", client_num);
        Serial.println(ip.toString());
      }
      break;

    // Handle text messages from client
    case WStype_TEXT:

      // Print out raw message
      Serial.printf("[%u] Received text: %s\n", client_num, payload);

      // Toggle LED
      if ( strcmp((char *)payload, "toggleLED") == 0 ) {
        led_state = led_state ? 0 : 1;
        Serial.printf("Toggling LED to %u\n", led_state);
        digitalWrite(led_pin, led_state);

        // Report the state of the LED
      } else if ( strcmp((char *)payload, "getLEDState") == 0 ) {
        sprintf(msg_buf, "%d", led_state);
        Serial.printf("Sending to [%u]: %s\n", client_num, msg_buf);
        webSocket.broadcastTXT(msg_buf);

        // Message not recognized
      } else {
        Serial.println("[%u] Message not recognized");
      }
      break;

    // For everything else: do nothing
    case WStype_BIN:
    case WStype_ERROR:
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_FRAGMENT_FIN:
    default:
      break;
  }
}

// Callback: send homepage
void onIndexRequest(AsyncWebServerRequest *request) {

}

// Callback: send style sheet
void onCSSRequest(AsyncWebServerRequest *request) {
  IPAddress remote_ip = request->client()->remoteIP();
  Serial.println("[" + remote_ip.toString() +
                 "] HTTP GET request of " + request->url());
  request->send(SPIFFS, "/style.css", "text/css");
}

// Callback: send 404 if requested file does not exist
void onPageNotFound(AsyncWebServerRequest *request) {
  IPAddress remote_ip = request->client()->remoteIP();
  Serial.println("[" + remote_ip.toString() +
                 "] HTTP GET request of " + request->url());
  request->send(404, "text/plain", "Not found");
}

String readIP() {
  if (WiFi.status() == WL_CONNECTED) {
    
    return (WiFi.localIP().toString());
  }
  return String("DESCONECTADO");
}

void initSTA() {
  if ((ssid != "") && (password != "")) {
    int tryCount;

    WiFi.begin(ssid, password);

    Serial.println("");
    Serial.print("Connecting to ");
    Serial.print(ssid);

    while (WiFi.status() != WL_CONNECTED) {
      Serial.print(".");
      tryCount++;
      delay(500);
      if (tryCount > 5) {
        ssid = "";
        password = "";
        Serial.println("ERROR TRYING TO CONNECTING");
        break;
      }
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("");
      Serial.println("WiFi connected.");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());

      //WiFi.mode(WIFI_STA);
    }
  }
}

String processor(const String & var) {
  //Serial.println(var);
  if (var == "IP") {
    return readIP();
  }
}

void setup() {
  // Init LED and turn off
  pinMode(led_pin, OUTPUT);
  digitalWrite(led_pin, LOW);

  // Start Serial port
  Serial.begin(115200);

  // Start ESP32 Wifi Mode as Access Point (AP) and Station Mode (STA)
  WiFi.mode(WIFI_MODE_APSTA);

  // Make sure we can read the file system
  if ( !SPIFFS.begin()) {
    Serial.println("Error mounting SPIFFS");
    while (1);
  }

  // Start the WiFi STA Mode
  //  If we have the ssid and password we try to connect
  initSTA();
  // End the WiFi STA Mode

  // Start the WiFi AP Mode
  WiFi.softAP(soft_ap_ssid, soft_ap_password);

  Serial.println("");
  Serial.print("ESP32 IP as soft AP: ");
  Serial.println(WiFi.softAPIP());
  // End the WiFi AP Mode


  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    if (ON_AP_FILTER(request)) {
      // On HTTP request for root, provide index.html file
      Serial.println("sending");
      request->send(SPIFFS, "/login_index.html", "text/html");
    } else {
      IPAddress remote_ip = request->client()->remoteIP();
      Serial.println("[" + remote_ip.toString() +
                     "] HTTP GET request of " + request->url());
      request->send(SPIFFS, "/index.html", "text/html");
      WiFi.mode(WIFI_STA);
    }
  });

  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest * request) {
    if (ON_AP_FILTER(request)) {
      String wifi;
      String pass;

      // GET input1 value on <ESP_IP>/get?input1=<inputMessage>
      if (request->hasParam(PARAM_INPUT_1)) {
        wifi = request->getParam(PARAM_INPUT_1)->value();
        ssid = wifi.c_str();    // c_str() -> Convert String to const char*
        Serial.println(ssid);
        pass = request->getParam(PARAM_INPUT_2)->value();
        password = pass.c_str();
        Serial.println(password);
        request->send(SPIFFS, "/login_index.html", "text/html");
        initSTA();
      }
    }
  });

  server.on("/ip", HTTP_GET, [](AsyncWebServerRequest * request) {
    if (ON_AP_FILTER(request)) {
      Serial.println("THERE U GO BOY");
      request->send(200, "text/plain", readIP().c_str());
    }
  });

  // On HTTP request for root, provide index.html file
  //server.on("/", HTTP_GET, );

  // Handle requests for pages that do not exist
  //server.onNotFound(onPageNotFound);

  // Start web server
  server.begin();

  // Start WebSocket server and assign callback
  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);

}

void loop() {
  // Look for and handle WebSocket data
  webSocket.loop();
}
