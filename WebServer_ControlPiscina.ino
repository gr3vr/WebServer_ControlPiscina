#include <WiFi.h>
#include <EEPROM.h>
#include <SPIFFS.h>
#include <ESPAsyncWebServer.h>
#include <WebSocketsServer.h>
#include <ModbusIP_ESP8266.h>

#define ssid_address  1
#define pass_address  2
#define state_address  3

#define light_register    2
#define filter_register   3
#define cascade_register  4
#define jacuzzi_register  5

// Constants

//const char  *ssid = "PoolControlSystem";
//const char  *password =  "12345678";

const char  *ssid = "TopGun";
const char  *password = "Maveric-k";

const char  *PARAM_INPUT_1 = "input1";
const char  *PARAM_INPUT_2 = "input2";

const char  *soft_ap_ssid = "SistemaControlPiscina";
const char  *soft_ap_password = "12345678";

const char  *msg_toggle_led = "toggleLED";
const char  *msg_get_led = "getLEDState";

const int   http_port = 80;
const int   ws_port = 1337;

const int   lights = 2;
const int   filter = 16;
const int   cascade = 17;
const int   jacuzzi = 18;

int ip;

// Globals
AsyncWebServer server(http_port);
WebSocketsServer webSocket = WebSocketsServer(ws_port);
char msg_buf[10];

int light_state = 0;
int filter_state = 0;
int cascade_state = 0;
int jacuzzi_state = 0;

/***********************************************************
   Functions
*/

ModbusIP mb;

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

      // Toggle Lights
      if ( strcmp((char *)payload, "lightBtn") == 0 ) {
        light_state = light_state ? 0 : 1;
        Serial.printf("Toggling LIGHT to %u\n", light_state);
        digitalWrite(lights, light_state);

        // Toggle Filter
      } else if ( strcmp((char *)payload, "filterBtn") == 0 ) {
        filter_state = filter_state ? 0 : 1;
        Serial.printf("Toggling FILTER to %u\n", filter_state);
        digitalWrite(filter, filter_state);

        // Toggle Cascade
      } else if ( strcmp((char *)payload, "cascadeBtn") == 0 ) {
        cascade_state = cascade_state ? 0 : 1;
        Serial.printf("Toggling CASCADE to %u\n", cascade_state);
        digitalWrite(cascade, cascade_state);

        // Toggle Jacuzzi
      } else if ( strcmp((char *)payload, "jacuzziBtn") == 0 ) {
        jacuzzi_state = jacuzzi_state ? 0 : 1;
        Serial.printf("Toggling JACUZZI to %u\n", jacuzzi_state);
        digitalWrite(jacuzzi, jacuzzi_state);

        // Report the state of the LIGHT
      } else if ( strcmp((char *)payload, "getLIGHTState") == 0 ) {
        sprintf(msg_buf, "L%d", light_state);
        Serial.printf("Sending to [%u]: %s\n", client_num, msg_buf);
        webSocket.broadcastTXT(msg_buf);

        // Message not recognized
      } else if ( strcmp((char *)payload, "getFILTERState") == 0 ) {
        sprintf(msg_buf, "F%d", filter_state);
        Serial.printf("Sending to [%u]: %s\n", client_num, msg_buf);
        webSocket.broadcastTXT(msg_buf);

        // Message not recognized
      } else if ( strcmp((char *)payload, "getCASCADEState") == 0 ) {
        sprintf(msg_buf, "C%d", cascade_state);
        Serial.printf("Sending to [%u]: %s\n", client_num, msg_buf);
        webSocket.broadcastTXT(msg_buf);

        // Message not recognized
      } else if ( strcmp((char *)payload, "getJACUZZIState") == 0 ) {
        sprintf(msg_buf, "J%d", jacuzzi_state);
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

    Serial.println("");
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);
    int tryCount = 0;
    while (WiFi.status() != WL_CONNECTED) {
      tryCount++;
      Serial.println(tryCount);
      delay(750);
      if (tryCount > 6) {
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

      // Procedimiento para guardar los datos en la EEPROM

      EEPROM.writeString(10, ssid);
      EEPROM.commit();

      EEPROM.writeString(20, password);
      EEPROM.commit();

      EEPROM.writeByte(0, 1);   // E indicamos que la proxima vez no inicie el modo AP
      EEPROM.commit();
    }
  }
}

void initAP() {
  WiFi.softAP(soft_ap_ssid, soft_ap_password);

  Serial.println("");
  Serial.print("ESP32 IP as soft AP: ");
  Serial.println(WiFi.softAPIP());
}

void setup() {
  // Init LED and turn off
  pinMode(lights, OUTPUT);
  pinMode(filter, OUTPUT);
  pinMode(cascade, OUTPUT);
  pinMode(jacuzzi, OUTPUT);

  digitalWrite(lights, LOW);
  digitalWrite(filter, LOW);
  digitalWrite(cascade, LOW);
  digitalWrite(jacuzzi, LOW);

  // Start Serial port
  Serial.begin(115200);

  // EEPROM Begin
  EEPROM.begin(64);
  delay(10);

  // Read the state if we start as STA: 1 or FACTORY: 0
  /*
    EEPROM.writeByte(0, 0);
    EEPROM.commit();
  */
  int state = EEPROM.readByte(0);

  Serial.println(state);

  if (state == 0) {
    // Start ESP32 Wifi Mode as Access Point (AP) and Station Mode (STA)
    Serial.println("WIFI MODE: APSTA");
    WiFi.mode(WIFI_MODE_APSTA);

    initAP();
  } else if (state == 1) {
    Serial.println("WIFI MODE: STA");
    WiFi.mode(WIFI_STA);

    Serial.println(EEPROM.readString(10));
    Serial.println(EEPROM.readString(20));

    String wifi = EEPROM.readString(10);
    String pass = EEPROM.readString(20);

    ssid = wifi.c_str();
    password = pass.c_str();

    initSTA();

    mb.server();    //Start Modbus IP
    mb.addCoil(light_register);
    mb.addCoil(filter_register);
    mb.addCoil(cascade_register);
    mb.addCoil(filter_register);
  }

  // Make sure we can read the file system
  if ( !SPIFFS.begin()) {
    Serial.println("Error mounting SPIFFS");
    while (1);
  }


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

  server.on("/lights", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/img/lights.png", "image/png");
  });

  server.on("/lights_off", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/img/lights_off.png", "image/png");
  });

  server.on("/filter", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/img/filter.png", "image/png");
  });

  server.on("/filter_off", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/img/filter_off.png", "image/png");
  });

  server.on("/jacuzzi", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/img/jacuzzi.png", "image/png");
  });

  server.on("/jacuzzi_off", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/img/jacuzzi_off.png", "image/png");
  });

  server.on("/waterfall", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/img/waterfall.png", "image/png");
  });

  server.on("/waterfall_off", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/img/waterfall_off.png", "image/png");
  });

  server.on("/ip", HTTP_GET, [](AsyncWebServerRequest * request) {
    if (ON_AP_FILTER(request)) {
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
  mb.task();
  mb.Coil(light_register, digitalRead(lights));
  mb.Coil(filter_register, digitalRead(filter));
  mb.Coil(cascade_register, digitalRead(cascade));
  mb.Coil(jacuzzi_register, digitalRead(jacuzzi));
}
