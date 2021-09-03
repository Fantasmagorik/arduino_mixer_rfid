/*
   Setting for ESP32 Dev Module
   CPU 240 MHz
   Flash Size: FS: 1MB  OTA 1019KB
*/

//MISO D6
//MOSI D7
//CLK D5

#define PROJECT_NAME    "Mixer RFID Zombie Metz 2021"
#define PROP_NAME    "Mixer RFID"


// ----Работа с файловой системой-------------------------------------------------------------
// Для файловой системы
#include "FS.h"
#include "SPIFFS.h"
File fsUploadFile;
//String tmpName = "";
// ----------------------------------------------------- Работа с файловой системой ----------

//--- WEB Server ------------------------------------------------------------------------------
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
WebServer server(80);
//--------------------------------------------------------------------------- WEB Server -----


#include <PubSubClient.h>
#include <SPI.h>
#include <PN532_SPI.h>
#include "PN532.h"
#include <EEPROM.h>
#include <Adafruit_NeoPixel.h>
#define MQTT_KEEPALIVE  3
#define readerN         3 //count readers
#define idLenght        7
#define comboCnt        8
#define COUNT_LED       9

#define LED_PIN         26
#define DEBUG_LED_PIN   2


String _DATE_ = __DATE__;
String _TIME_ = __TIME__;
String _FILE_ = __FILE__;
String _BOARD_ = "arduino";

int cntWiFiErr = 0;
int cntMQTTErr = 0;

#define CLIENT_ID   62
String CLIENT_NAME = "client_" + String(CLIENT_ID);

uint8_t ssPin[readerN] = {33, 25, 32};

uint8_t cardID[comboCnt][readerN][idLenght];  //massive with right id
uint8_t currentCardID[readerN][idLenght];             //buffer for current card id
bool isComboWin[comboCnt];
uint8_t counterWin;
uint8_t cardVisible;
uint8_t uidLength;   // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
uint8_t orderWinComboValue[comboCnt];

PN532_SPI pn532spi0(SPI, ssPin[0]);
PN532_SPI pn532spi1(SPI, ssPin[1]);
PN532_SPI pn532spi2(SPI, ssPin[2]);


PN532 nfc0(pn532spi0);
PN532 nfc1(pn532spi1);
PN532 nfc2(pn532spi2);


PN532 nfc[readerN] = {nfc0, nfc1, nfc2};

Adafruit_NeoPixel pixels_0 = Adafruit_NeoPixel(COUNT_LED, LED_PIN, NEO_GRB + NEO_KHZ800);

const char* ssid = "EntS";
const char* password = "09122019";
const char* mqtt_server = "192.168.0.60";

long step0 = 0;
long step1 = 0;
long step2 = 0;
long step3 = 0;
long step5 = 0;
bool lastStatusConnectionMQTT = false;

// Update these with values suitable for your network.
IPAddress ip(192, 168, 0, CLIENT_ID); //Node static IP
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);

const String topic_sub = "/" + CLIENT_NAME + "_sub";
const char *topic_rfid= "/client_62_set_combo";
const char *topic_rfid_answer = "/client_62_err_combo";
const String topic_pub = "/" + CLIENT_NAME + "_pub";
const String topic_err_pub = "/" + CLIENT_NAME + "_err_pub";
const String topic_rfid1 = "/" + CLIENT_NAME + "_rfid1";
const String topic_rfid2 = "/" + CLIENT_NAME + "_rfid2";
const String topic_rfid3 = "/" + CLIENT_NAME + "_rfid3";
const String broadcast_sub = "/broadcast";

bool isWin = false;

WiFiClient espClient;
PubSubClient client(espClient);
String sensErrNumModule = "";

#define animLedOnTime  300
#define animLedOffTime 100
#define COUNT_OF_BLINK_LED  14
uint8_t numberComboAnim = 99;
uint8_t comboCountAnim = 0;
long animTimer = 0;
bool isAnimAllowed = false;
uint32_t WS2812COLORACTIVE = pixels_0.Color(155, 55, 0);
uint32_t WS2812COLORBLACK  = pixels_0.Color(0, 0, 0);
bool isCardVisible[readerN];

void animWrongCombo(uint8_t comboN)  {
  String mess = "combo_";
  if (isAnimAllowed)
    pixels_0.setPixelColor(numberComboAnim, WS2812COLORACTIVE);
  for (int i = 0; i < comboCnt; i++) {  //get current ID`s timeIndex
    if (orderWinComboValue[i] == comboN)  {
      numberComboAnim = i;
      break;
    }
  }
  comboCountAnim  = 1;
  mess += comboN + 1;
  mess += "_already_solved";

  client.publish(topic_pub.c_str(), mess.c_str());
  pixels_0.setPixelColor(numberComboAnim, WS2812COLORBLACK);
  pixels_0.show();
  animTimer = millis();
  isAnimAllowed = true;
}

void setup_rfid() {
  uint32_t versiondata;
  sensErrNumModule = "";
  for (uint8_t i = 0; i < readerN; i++) {
    nfc[i].begin();
    versiondata = nfc[i].getFirmwareVersion();
    if (! versiondata) {
      sensErrNumModule += String (i + 1);
      Serial.print("\n\n\n  --->>>    err modul num:");
      Serial.println(i);

      delay(1000);
    }
    nfc[i].setPassiveActivationRetries(0x01);
    nfc[i].SAMConfig();
  }
  if (sensErrNumModule == "") {
    sensErrNumModule = "ok";
  }
}

void reconnect() {
  Serial.println("Connecting to MQTT..");
  if (client.connect(CLIENT_NAME.c_str())) {
    Serial.println("Connected");
    client.publish(topic_pub.c_str(), "Connected");
    client.subscribe(topic_sub.c_str());
    client.subscribe(topic_rfid);
    
    digitalWrite(DEBUG_LED_PIN, LOW);
  }
  else {
    Serial.print("failed, rc=");
    Serial.print(client.state());
    cntMQTTErr++;
    digitalWrite(DEBUG_LED_PIN, HIGH);
    delay(100);
    digitalWrite(DEBUG_LED_PIN, LOW);
    delay(50);
    digitalWrite(DEBUG_LED_PIN, HIGH);
    delay(100);
    digitalWrite(DEBUG_LED_PIN, LOW);
    delay(50);
    digitalWrite(DEBUG_LED_PIN, HIGH);
    delay(100);
    digitalWrite(DEBUG_LED_PIN, LOW);
    delay(50);
  }
}

void wifi_status () {
  Serial.print("  WiFi status=");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WL_CONNECTED");
  }
  else if (WiFi.status() == WL_NO_SHIELD) {
    Serial.print("WL_NO_SHIELD");
  }
  else if (WiFi.status() == WL_IDLE_STATUS) {
    Serial.print("WL_IDLE_STATUS");
  }
  else if (WiFi.status() == WL_NO_SSID_AVAIL) {
    Serial.print("WL_NO_SSID_AVAIL");
  }
  else if (WiFi.status() == WL_SCAN_COMPLETED) {
    Serial.print("WL_SCAN_COMPLETED");
  }
  else if (WiFi.status() == WL_CONNECT_FAILED) {
    Serial.print("WL_CONNECT_FAILED");
  }
  else if (WiFi.status() == WL_CONNECTION_LOST) {
    Serial.print("WL_CONNECTION_LOST");
  }
  else if (WiFi.status() == WL_DISCONNECTED) {
    Serial.print("WL_DISCONNECTED");
  }
}

void setup_wifi(bool reboot) {
  WiFi.mode(WIFI_OFF);
  delay(2000);
  WiFi.mode(WIFI_STA);

  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.config(ip, gateway, subnet);
  WiFi.begin(ssid, password);
  unsigned long tmp = millis();
  while (WiFi.status() != WL_CONNECTED)
  {
    digitalWrite(DEBUG_LED_PIN, HIGH);
    delay(200);
    digitalWrite(DEBUG_LED_PIN, LOW);
    delay(200);
    Serial.println("");
    wifi_status ();
    //Serial.print(".");
    if (millis() > tmp + 20000) {
      if (reboot) {
        WiFi.disconnect();
        WiFi.mode(WIFI_OFF);
        Serial.println("\n\nESP wait 20sec and will be reset()");
        delay(20000);
        Serial.println("\n\nESP will be reset because 60sec not connecting..");
        delay(200);
        ESP.restart();
      }
      else {
        WiFi.disconnect();
        WiFi.mode(WIFI_OFF);
        Serial.println("\n\nESP wait 10sec ");
        delay(10000);
        break;
      }
    }
  }
  randomSeed(micros());
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.print("WiFi ");
    Serial.print(ssid);
    Serial.println(" connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    reconnect();
  }
  else {
    Serial.println("");
    Serial.print("WiFi ");
    Serial.print(ssid);
    Serial.println(" connection FAIL");
  }
  FS_init();
  webServer_init();
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");
  if(String(topic) == String(topic_rfid)) {
    Serial.println("set combo topic");
    if((*payload < '0') || (*payload > '7'))  {
      client.publish(topic_rfid_answer, "SET_FAIL");
      return;
    }
    
    client.publish(topic_rfid_answer, (setNewComb(*payload - '0'))? "SET_OK": "SET_FAIL");
    return;
  }
    
  String command = "";
  for (int i = 0; i < length; i++)
  {
    command += (char)payload[i];
  }
  Serial.print("command=");
  Serial.println(command);
  if ( command == "solve") {
    Win();
  }
  if ( command == "restart") {
    restart();
  }

  if ( command == "30") {
    delay(30);
    client.publish(topic_pub.c_str(), "31");
  }
  //return;
}

void setup() {
#ifdef ESP32
  Serial.begin(115200);
  Serial.println("\n\nESP32");
  _BOARD_ = "ESP32";
  pinMode(DEBUG_LED_PIN, OUTPUT);
  digitalWrite(DEBUG_LED_PIN, HIGH);
#endif
#ifdef ESP8266
  Serial.begin(74880);
  Serial.println("\n\nESP8266");
  _BOARD_ = "ESP8266";
  pinMode(DEBUG_LED_PIN, OUTPUT);
  digitalWrite(DEBUG_LED_PIN, LOW);
#endif
  pixels_0.clear();
  pixels_0.show();

  delay(1500);
  Serial.print("\n\n  starting ");
  Serial.println(PROJECT_NAME);
  Serial.print(_DATE_);
  Serial.print("  ");
  Serial.println(_TIME_);
  Serial.println("File -> " + _FILE_ );
  delay(500);
  EEPROM.begin(200);

  setup_rfid();

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  setup_wifi(true);
  client.publish(topic_err_pub.c_str(), sensErrNumModule.c_str());
  for (int i = 0 ; i < COUNT_LED; i++) {
    pixels_0.setPixelColor(i, pixels_0.Color(255, 0, 0));
    pixels_0.show();
    delay(300);
  }
  readCombo();
  Serial.println("____________________________");
}

void loop() {
  if (isAnimAllowed && (millis() - animTimer > ((comboCountAnim % 2) ? animLedOnTime : animLedOffTime))) {
    animTimer = millis();
    pixels_0.setPixelColor(numberComboAnim, (comboCountAnim % 2) ? WS2812COLORACTIVE : WS2812COLORBLACK);
    if (++comboCountAnim == COUNT_OF_BLINK_LED)  {
      isAnimAllowed = false;
      pixels_0.setPixelColor(numberComboAnim,  WS2812COLORACTIVE);
    }
    pixels_0.show();
  }

  if (millis() - step3 > 200)  {
    step3 = millis();
    checkBeacon();
  }
  if (lastStatusConnectionMQTT != client.connected()) {
    lastStatusConnectionMQTT = client.connected();
    reconnect();
  } else if (!client.connected() and (millis() - step1) > 45000) {
    step1 = millis();
    reconnect();
  }
  client.loop();
  delay(10);
  if (millis() - step0 > 1000) {
    step0 = millis();
#ifdef ESP32
    digitalWrite (DEBUG_LED_PIN, HIGH);
    delay(5);
    digitalWrite (DEBUG_LED_PIN, LOW);
#endif
#ifdef ESP8266
    digitalWrite (DEBUG_LED_PIN, LOW);
    delay(5);
    digitalWrite (DEBUG_LED_PIN, HIGH);
#endif
    Serial.println("");
    Serial.print(PROP_NAME);
    Serial.print(". IP = ");
    Serial.print(WiFi.localIP());
    Serial.print(". freeHeap = ");
    Serial.print(ESP.getFreeHeap());
    wifi_status ();
    Serial.print("  upTime=");
    Serial.print(millis() / 1000.);
    Serial.print(". Signal = ");
    Serial.println(WiFi.RSSI());
  }

  if (millis() - step2 > 1500) {
    step2 = millis();
    if (sensErrNumModule != "ok") 
      client.publish(topic_err_pub.c_str(), sensErrNumModule.c_str());
      String rfidSerial = "";
      for(int x = 0; x < idLenght; x++) 
        rfidSerial += String(currentCardID[0][x], HEX);
      client.publish(topic_rfid1.c_str(), rfidSerial.c_str()); 
      rfidSerial = "";
      for(int x = 0; x < idLenght; x++) 
        rfidSerial += String(currentCardID[1][x], HEX);
      client.publish(topic_rfid2.c_str(), rfidSerial.c_str()); 
      rfidSerial = "";
      for(int x = 0; x < idLenght; x++) 
        rfidSerial += String(currentCardID[2][x], HEX);
      client.publish(topic_rfid3.c_str(), rfidSerial.c_str()); 
    }

  if ( millis() - step5 >= 60000 ) {
    if ((WiFi.status() != WL_CONNECTED)) {
      Serial.print(millis());
      Serial.println("\n\n Reconnecting to WiFi...");

      setup_wifi(false);
      cntWiFiErr ++;
    }
    step5 = millis();
  }

  server.handleClient();
  yield();
  client.loop();
  yield();
}

void cardChanged(int cardN) {
  String stat = "";
  Serial.print("\ncard ");
  Serial.print(cardN);
  Serial.println(" changed");
  //publish isCardVisible[i] = true;
  stat += (isCardVisible[cardN]) ? "placed_" : "out_";
  stat += cardN;
  client.publish(topic_pub.c_str(), stat.c_str());
  
}

void comboWin(int comboN) {
  String cmbWin = "comboWin_";
  Serial.print("\ncombo ");
  Serial.print(comboN);
  Serial.println(" win");
  isComboWin[comboN] = true;
  pixels_0.setPixelColor(counterWin, WS2812COLORACTIVE);
  pixels_0.show();
  orderWinComboValue[counterWin] = comboN;
  counterWin++;
  cmbWin += counterWin;
  client.publish(topic_pub.c_str(), cmbWin.c_str());
  switch (comboN) {
    case 0:
      client.publish(topic_pub.c_str(), "water");
      break;

    case 1:
      client.publish(topic_pub.c_str(), "whiteboard");
      break;

    case 2:
      client.publish(topic_pub.c_str(), "endoscope");
      break;

    case 3:
      client.publish(topic_pub.c_str(), "blood_spots");
      break;

    case 4:
      client.publish(topic_pub.c_str(), "petri");
      break;

    case 5:
      client.publish(topic_pub.c_str(), "uv");
      break;

    case 6:
      client.publish(topic_pub.c_str(), "thick");
      break;

    case 7:
      client.publish(topic_pub.c_str(), "shakers");
      break;

    default:
      client.publish(topic_pub.c_str(), "wrong comb data");
      break;
  }
  if ((!isWin) && (counterWin == comboCnt))
    Win();
}

void checkCombo() {
  bool isBreak;
  int rightLabel = 0;
  for (int i = 0; i < comboCnt; i++, isBreak = false) {
    for (int x = 0; x < readerN; x++)  {
      for (int j = 0; j < idLenght; j++) {
        if (currentCardID[x][j] != cardID[i][x][j]) {
          isBreak = true;
          break;
        }
      }
      if (isBreak) {
        rightLabel = 0;
        break;
      }
      else if (++rightLabel == readerN) {
        if (!isComboWin[i])
          comboWin(i);
        else
          animWrongCombo(i);
        return;
      }
    }
  }
}

int checkBeacon() {
  int rightCombo = 0;
  int i, x;
  int goodCard;
  bool isRightCard, isCardChanged, globalFCh = false;
  uint8_t card[idLenght];             //buffer for card id
  int cardDetected = 0;

  for (int i = 0; i < readerN; isRightCard = true, isCardChanged = false, i++) {
    for (int x = 0; x < idLenght; x++)
      card[x] = 0;
    goodCard = nfc[i].readPassiveTargetID(PN532_MIFARE_ISO14443A, card, &uidLength);
    if (goodCard == 1)  {
      cardDetected++;
      isCardVisible[i] = true;
    }
    else
      isCardVisible[i] = false;
    for (int x = 0; x < idLenght; x++) {
      if (card[x] != currentCardID[i][x])  {
        isCardChanged = true;
        globalFCh = true;
        currentCardID[i][x] = card[x];
      }
    }
    if (isCardChanged)
      cardChanged(i);
  }
  cardVisible = cardDetected;
  if ((cardVisible == readerN) && (globalFCh) && (!isWin))
    checkCombo();
}

void readCombo()  {
  int byteCnt = 0;
  Serial.println("read from EEPROM");
  for (int i = 0; i < comboCnt; i++)
    for (int x = 0; x < readerN; x++)
      for (int j = 0; j < idLenght; j++, byteCnt++)
        cardID[i][x][j] = EEPROM.read(byteCnt);

}

bool setNewComb(byte indx) {
  if (indx < 0 or indx >= comboCnt) {
    Serial.println("Set new combination ERROR, index out of range");
    return false;
  }
  Serial.print("\nSet new ");
  checkBeacon();
  if (cardVisible != readerN)  {
    Serial.print("\nset all beacons. need: ");
    Serial.println(readerN - cardVisible);
    return false;
  }
  Serial.print(indx);
  Serial.println(" comlect");
  uint8_t ptrByte = indx * idLenght * readerN;
  for (int i = 0; i < readerN; i++)
    for (int j = 0; j < idLenght; j++)
      EEPROM.write(ptrByte++, currentCardID[i][j]);
  //EEPROM.write(indx * idLenght * i + j, currentCardID[i][j]);
  EEPROM.commit();
  ///take_eeprom();
  client.publish(topic_pub.c_str(), "set_ok");
  delay(50);
  readCombo();
  return true;
}

void Win() {
  Serial.println("WIN");
  isWin = true;
  client.publish(topic_pub.c_str(), "solved");
  pixels_0.fill(pixels_0.Color(0, 255, 0));
  pixels_0.show();
}

void restart() {
  Serial.println("RESTART");
  isWin = false;
  pixels_0.clear();
  pixels_0.show();
  delay(200);
  client.publish(topic_pub.c_str(), "false");
  pixels_0.setPixelColor(8, pixels_0.Color(155, 55, 0));
  for (int i = 0; i < comboCnt; i++)  {
    isComboWin[i] = false;
    orderWinComboValue[i] = 99;
  }
  counterWin = 0;
  pixels_0.show();
  client.publish(topic_pub.c_str(), "restarted");
}
