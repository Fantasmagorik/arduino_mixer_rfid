//--- WEB Server --------------------------------------------------------------------------
//#include <ESP8266WebServer.h>
//ESP8266WebServer server(80);
// переменная хранит в себе ответ сервера в виду XML документа
String XML;
const char* serverIndex = "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";

String buildWebsite(bool flag) {
  String webSite = "";
  webSite = "<!DOCTYPE HTML>\n";
  webSite += "<title >Update</title>";
  webSite += "<meta http-equiv='Content-type' content='text/html; charset=utf-8'>";
  webSite += "<BODY>\n";
  if (flag) {
    webSite += "<h3>UPDATE ERROR</h3>";
  }
  else {
    webSite += "<h3>Update OK.</h3>";
  }
  webSite += "<BR><a href='/'>Main page. </a><BR>\n";
  webSite += "<BR><a href='/device.htm'>Settings page. </a><BR>\n";
  webSite += "</BODY>";
  webSite += "</HTML>";
  return webSite;
}

// web сервер с авторизацией
void webServer_init()
{
  server.on("/generate_204", handleWebsite);  //Android captive portal. Maybe not needed. Might be handled by notFound handler.
  server.on("/", handleWebsite);
  server.on("/reboot", handleReboot );
  server.on("/xml", handleXML);
  server.on("/RFIDxml", handleRFIDXML);
  server.on("/win", webWin);
  server.on("/restart", webRestart);
  server.on("/webSetNewComb", webSetNewComb);

  server.on("/uploadfile", HTTP_POST, []() {
    server.send(200, "text/plain", "");
  }, handleFileUpload);

  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });
  server.on("/update", HTTP_POST, []() {
    //client.disconnect();
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", buildWebsite(Update.hasError()));


    delay (200);
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.setDebugOutput(true);

      Serial.printf("Update: %s\n", upload.filename.c_str());

      uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
      if (!Update.begin(maxSketchSpace)) { //start with max available size
        Update.printError(Serial);
      }

      //DEBUG_OUT_LN("1");
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
      Serial.print ("# ");
    } else if (upload.status == UPLOAD_FILE_END) {
      Serial.println("");
      //DEBUG_OUT_LN("3");
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
      //DEBUG_OUT_LN("4");
      Serial.setDebugOutput(false);
    }
    yield();
  });


  server.begin();
}

//
void handleWebsite() {

  if (!handleFileRead("/")) server.send(200, "text/html", notFoundResponse);
}


// метод возвращает время вида чч:мм:сс получая на вход секунды
String millis2WorkTime(unsigned long sec, byte GMT)
{
  if ( sec == 0 )
    return "--:--:--";
  byte tmp = ( sec  % 86400L) / 3600 + GMT; // GMT +3
  String tmpTime = "";
  if (tmp > 23)
    tmp = tmp - 24;
  if ( tmp < 10)
    tmpTime = "0" + String(tmp);
  else
    tmpTime = String(tmp);
  //local_hour = tmpTime.toInt();
  //tcpLogString = String(local_hour);
  tmpTime += ":";
  tmp = (sec  % 3600) / 60 ;
  if ( tmp < 10)
    tmpTime += "0" + String(tmp);
  else
    tmpTime += String(tmp);
  tmpTime += ":";
  tmp = (sec  % 60);
  if ( tmp < 10)
    tmpTime += "0" + String(tmp);
  else
    tmpTime += String(tmp);
  return tmpTime;
}
void buildRFIDXML() {
  String mess = "";

  XML = "<?xml version='1.0'?>";
  XML += "<Main>";
  XML += "<pjN>";
  XML += PROJECT_NAME;
  XML += "</pjN>";

  for (int i = 0; i < comboCnt; i++) {
    XML += "<rfid";
    XML += i + 1;
    XML += ">";
    for (int x = 0; x < readerN; x++)  {
      for (int j = 0; j < idLenght; j++) {
        XML +=  String(cardID[i][x][j], HEX);
        //XML += "  ";
        XML += (j == idLenght - 1) ? "    ;    " : "  ";
      }
    }
    XML += "</rfid";
    XML += i + 1;
    XML += ">";
  }
  for (int x = 0; x < readerN; x++)  {
    XML += "<card";
    XML += x + 1;
    XML += ">";
    for (int j = 0; j < idLenght; j++) {
      XML +=  String(currentCardID[x][j], HEX);
      //XML += "  ";
      XML += (j == idLenght - 1) ? "    ;    " : "  ";
    }
    XML += "</card";
    XML += x + 1;
    XML += ">";
  }
  XML += "</Main>";
}


void buildXML() {
  XML = "<?xml version='1.0'?>";
  XML += "<Main>";
  XML += "<r>";
  if (WiFi.SSID().length() == 0)
    XML += ("Режим точки доступа");
  else
    XML += (WiFi.SSID());
  XML += "</r>";
  XML += "<r1>";
  XML += WiFi.RSSI();
  XML += "dB";
  XML += "</r1>";
  XML += "<fH>";
  XML += ESP.getFreeHeap();
  XML += "</fH>";
  XML += "<wT>";
  XML += (millis2WorkTime(millis() / 1000, 0 ));
  XML += "</wT>";
  XML += "<mqS>";
  XML += mqtt_server;
  XML += "</mqS>";
  XML += "<pjN>";
  XML += PROJECT_NAME;
  XML += "</pjN>";
  XML += "<prN>";
  XML += PROP_NAME;
  XML += "</prN>";
  XML += "<date>";
  XML += _DATE_;
  XML += "</date>";
  XML += "<file>";
  XML += _FILE_;
  XML += "</file>";
  XML += "<time>";
  XML += _TIME_;
  XML += "</time>";
  XML += "<board>";
  XML += _BOARD_;
  XML += "</board>";
  XML += "<cntWiFiErr>";
  XML += String(cntWiFiErr);
  XML += "</cntWiFiErr>";
  XML += "<cntMQTTErr>";
  XML += String(cntMQTTErr);
  XML += "</cntMQTTErr>";
  XML += "<cntWin>";
  XML += counterWin;
  XML += "</cntWin>";
  XML += "<status>";
  XML += isWin ? "WIN" : "PLAY" ;
  XML += "</status>";
  for (int x = 0; x < readerN; x++)  {
    XML += "<card";
    XML += x + 1;
    XML += ">";
    for (int j = 0; j < idLenght; j++) {
      XML +=  String(currentCardID[x][j], HEX);
      XML += "  ";
      // XML += (j == idLenght - 1) ? "    ;    " : "  ";
    }
    XML += "</card";
    XML += x + 1;
    XML += ">";
  }
  XML += "</Main>";
}


String millis2time1() {
  String Time = "";
  Time += "Сеть: \"";
  Time += WiFi.SSID();
  Time += "\"   Уровень сигнала: ";
  Time += WiFi.RSSI();
  Time += "dB";
  return Time;
}

void handleXML() {
  buildXML();
  server.send(200, "text/xml", XML);
}

void handleRFIDXML()  {
  buildRFIDXML();
  server.send(200, "text/xml", XML);
}

// Перезагрузка модуля по запросу вида http://192.168.0.101/restart?device=ok
void handleReboot() {
  String restart = server.arg("device");          // Получаем значение device из запроса
  if (restart == "ok") {                         // Если значение равно Ок
    server.send(200, "text / plain", "Reset OK"); // Oтправляем ответ Reset OK
    delay (500);
    ESP.restart();                                // перезагружаем модуль
    delay (1000);
  }
  else {                                        // иначе
    server.send(200, "text / plain", "No Reset"); // Oтправляем ответ No Reset
  }
}

void webSetNewComb() {
  if (server.hasArg("setCombi")) {
    byte indx = server.arg("setCombi").toInt();
    Serial.print("indx=");
    Serial.println(indx);
    if (indx >= 0 and indx < comboCnt and setNewComb(indx)) {
      server.send(200, "text / plain", "set OK");
    }
    else {
      server.send(406, "text / plain", "set FAIL");
    }
  }
  else if (server.hasArg("getCombi")) {
    byte indx = server.arg("getCombi").toInt();
    Serial.print("indx=");
    Serial.println(indx);
    if (indx >= 0 or indx < comboCnt ) {
      //take_eeprom();
      server.send(200, "text / plain", "set OK");
    }
    else {
      server.send(406, "text / plain", "set FAIL");
    }
  }
  else {
    server.send(406, "text / plain", "set FAIL");
  }
}


void webWin() {
  Serial.print("webWin");
  Win();
  server.send(200, "text / plain", "webWin OK");
}

void webRestart() {
  Serial.print("webRestart");
  restart();
  server.send(200, "text / plain", "webRestart OK");
}
