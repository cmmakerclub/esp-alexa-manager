#include <CMMC_Manager.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <functional>

/*    init button for confix wifi   */
#define BUTTON_INPUT_PIN 13
CMMC_Manager manager(BUTTON_INPUT_PIN, LED_BUILTIN);

void prepareIds();
boolean connectUDP();
void startHttpServer();
void turnOnRelay();
void turnOffRelay();
void doHttpGet();

unsigned int localPort = 1900;      // local port to listen on

WiFiUDP UDP;
boolean udpConnected = false;
IPAddress ipMulti(239, 255, 255, 250);
unsigned int portMulti = 1900;      // local port to listen on

ESP8266WebServer HTTP(80);

char packetBuffer[UDP_TX_PACKET_MAX_SIZE]; //buffer to hold incoming packet,

String serial;
String persistent_uuid;
String device_name;

const int relayPin = 2;
String light;

void setup() {
  Serial.begin(115200);

  // Setup Relay
  pinMode(relayPin, OUTPUT);

  prepareIds();

  Serial.println(String(system_get_sdk_version()));
  pinMode(BUTTON_INPUT_PIN, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);

  // Initialise wifi connection
  manager.start();

  // Initialise udp connection
  udpConnected = connectUDP();
  if (udpConnected) {
    // initialise pins if needed
    startHttpServer();
  }
}

void loop() {
  HTTP.handleClient();
  delay(1);

  // if there's data available, read a packet
  // check if the WiFi and UDP connections were successful
  //  if (udpConnected) {
  // if there’s data available, read a packet
  int packetSize = UDP.parsePacket();

  if (packetSize) {
    Serial.println("");
    Serial.print("Received packet of size ");
    Serial.println(packetSize);
    Serial.print("From ");
    IPAddress remote = UDP.remoteIP();

    for (int i = 0; i < 4; i++) {
      Serial.print(remote[i], DEC);
      if (i < 3) {
        Serial.print(".");
      }
    }

    Serial.print(", port ");
    Serial.println(UDP.remotePort());

    int len = UDP.read(packetBuffer, 255);

    if (len > 0) {
      packetBuffer[len] = 0;
    }

    String request = packetBuffer;
    //Serial.println("Request:");
    //Serial.println(request);

    if (request.indexOf('M-SEARCH') > 0) {
      if (request.indexOf("urn:Belkin:device:**") > 0) {
        Serial.println("Responding to search request ...");
        respondToSearch();
      }
    }
  }
  delay(10);
}

void prepareIds() {
  uint32_t chipId = ESP.getChipId();
  char uuid[64];
  sprintf_P(uuid, PSTR("38323636-4558-4dda-9188-cda0e6%02x%02x%02x"),
            (uint16_t) ((chipId >> 16) & 0xff),
            (uint16_t) ((chipId >>  8) & 0xff),
            (uint16_t)   chipId        & 0xff);

  serial = String(uuid);
  persistent_uuid = "Socket-1_0-" + serial;
  device_name = "light";
}

void respondToSearch() {
  Serial.println("");
  Serial.print("Sending response to ");
  Serial.println(UDP.remoteIP());
  Serial.print("Port : ");
  Serial.println(UDP.remotePort());

  IPAddress localIP = WiFi.localIP();
  char s[16];
  sprintf(s, "%d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]);

  String response =
    "HTTP/1.1 200 OK\r\n"
    "CACHE-CONTROL: max-age=86400\r\n"
    "DATE: Fri, 15 Apr 2016 04:56:29 GMT\r\n"
    "EXT:\r\n"
    "LOCATION: http://" + String(s) + ":80/setup.xml\r\n"
    "OPT: \"http://schemas.upnp.org/upnp/1/0/\"; ns=01\r\n"
    "01-NLS: b9200ebb-736d-4b93-bf03-835149d13983\r\n"
    "SERVER: Unspecified, UPnP/1.0, Unspecified\r\n"
    "ST: urn:Belkin:device:**\r\n"
    "USN: uuid:" + persistent_uuid + "::urn:Belkin:device:**\r\n"
    "X-User-Agent: redsonic\r\n\r\n";

  UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
  UDP.write(response.c_str());
  UDP.endPacket();

  Serial.println("Response sent !");
}

void startHttpServer() {
  HTTP.on("/index.html", HTTP_GET, []() {
    Serial.println("Got Request index.html ...\n");
    HTTP.send(200, "text/plain", "Hello World!");
  });

  HTTP.on("/upnp/control/basicevent1", HTTP_POST, []() {
    Serial.println("########## Responding to  /upnp/control/basicevent1 ... ##########");

    //for (int x=0; x <= HTTP.args(); x++) {
    //  Serial.println(HTTP.arg(x));
    //}

    String request = HTTP.arg(0);
    Serial.print("request:");
    Serial.println(request);

    if (request.indexOf("<BinaryState>1</BinaryState>") > 0) {
      Serial.println("Got Turn on request");
      turnOnRelay();
    }

    if (request.indexOf("<BinaryState>0</BinaryState>") > 0) {
      Serial.println("Got Turn off request");
      turnOffRelay();
    }

    HTTP.send(200, "text/plain", "");
  });

  HTTP.on("/eventservice.xml", HTTP_GET, []() {
    Serial.println(" ########## Responding to eventservice.xml ... ########\n");
    String eventservice_xml = "<?scpd xmlns=\"urn:Belkin:service-1-0\"?>"
                              "<actionList>"
                              "<action>"
                              "<name>SetBinaryState</name>"
                              "<argumentList>"
                              "<argument>"
                              "<retval/>"
                              "<name>BinaryState</name>"
                              "<relatedStateVariable>BinaryState</relatedStateVariable>"
                              "<direction>in</direction>"
                              "</argument>"
                              "</argumentList>"
                              "<serviceStateTable>"
                              "<stateVariable sendEvents=\"yes\">"
                              "<name>BinaryState</name>"
                              "<dataType>Boolean</dataType>"
                              "<defaultValue>0</defaultValue>"
                              "</stateVariable>"
                              "<stateVariable sendEvents=\"yes\">"
                              "<name>level</name>"
                              "<dataType>string</dataType>"
                              "<defaultValue>0</defaultValue>"
                              "</stateVariable>"
                              "</serviceStateTable>"
                              "</action>"
                              "</scpd>\r\n"
                              "\r\n";

    HTTP.send(200, "text/plain", eventservice_xml.c_str());
  });

  HTTP.on("/setup.xml", HTTP_GET, []() {
    Serial.println(" ########## Responding to setup.xml ... ########\n");

    IPAddress localIP = WiFi.localIP();
    char s[16];
    sprintf(s, "%d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]);

    String setup_xml = "<?xml version=\"1.0\"?>"
                       "<root>"
                       "<device>"
                       "<deviceType>urn:Belkin:device:controllee:1</deviceType>"
                       "<friendlyName>" + device_name + "</friendlyName>"
                       "<manufacturer>Belkin International Inc.</manufacturer>"
                       "<modelName>Emulated Socket</modelName>"
                       "<modelNumber>3.1415</modelNumber>"
                       "<UDN>uuid:" + persistent_uuid + "</UDN>"
                       "<serialNumber>221517K0101769</serialNumber>"
                       "<binaryState>0</binaryState>"
                       "<serviceList>"
                       "<service>"
                       "<serviceType>urn:Belkin:service:basicevent:1</serviceType>"
                       "<serviceId>urn:Belkin:serviceId:basicevent1</serviceId>"
                       "<controlURL>/upnp/control/basicevent1</controlURL>"
                       "<eventSubURL>/upnp/event/basicevent1</eventSubURL>"
                       "<SCPDURL>/eventservice.xml</SCPDURL>"
                       "</service>"
                       "</serviceList>"
                       "</device>"
                       "</root>\r\n"
                       "\r\n";

    HTTP.send(200, "text/xml", setup_xml.c_str());

    Serial.print("Sending :");
    Serial.println(setup_xml);
  });

  HTTP.begin();
  Serial.println("HTTP Server started ..");
}

boolean connectUDP() {
  boolean state = false;

  Serial.println("");
  Serial.println("Connecting to UDP");

  if (UDP.beginMulticast(WiFi.localIP(), ipMulti, portMulti)) {
    Serial.println("Connection successful");
    state = true;
  }
  else {
    Serial.println("Connection failed");
  }

  return state;
}

void turnOnRelay() {
  digitalWrite(relayPin, LOW); // turn on relay with voltage HIGH
  light = "ON";
  doHttpGet(light);
}

void turnOffRelay() {
  digitalWrite(relayPin, HIGH);  // turn off relay with voltage LOW
  light = "OFF";
  doHttpGet(light);
}

void doHttpGet(String state) {
  HTTPClient http;
  Serial.print("[HTTP] begin...\n");


  http.begin("http://www.espert.io/bridge//CMMC/pisiri/command?status=" + String(state)); //HTTP

  // start connection and send HTTP header
  int httpCode = http.GET();

  // httpCode will be negative on error
  if (httpCode > 0) {
    // HTTP header has been send and Server response header has been handled
    Serial.printf("[HTTP] GET... code: %d\n", httpCode);
    Serial.print("[CONTENT]\n");

    // file found at server
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.println(payload);
    }
  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();
  delay(2000);
}
