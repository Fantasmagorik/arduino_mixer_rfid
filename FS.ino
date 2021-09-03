String notFoundResponse = ""
                      "<!DOCTYPE html><html lang='en'><head>"
                      //"<meta http-equiv=\"refresh\" content =\"1;URL=http://172.217.28.1/ \"/>"
                      "<meta name='viewport' content='width=device-width'>"
                      "<title>Upload File</title></head><body>"
                      "<BR><a href='/'>Main page. </a><BR>\n"
                      "<BR><a href='/device.htm'>Settings page</a><BR>\n"
                      "<h1>Please select file</h1>"
                      "<form method=\"post\" action=\"/uploadfile\" enctype=\"multipart/form-data\">"
                      "<input type=\"file\" name=\"name\">"
                      "<input class=\"button\" type=\"submit\" value=\"Upload\">"
                      "</form>"
                      "</body></html>";


// Инициализация FFS
void FS_init(void) {
  Serial.println("FS_init_1");
  SPIFFS.begin();
  {
    //Dir dir = SPIFFS.openDir("/");
    File root = SPIFFS.open("/");
    while (root.openNextFile()) {
      String fileName = root.name();
    }
  }
   Serial.println("FS_init_2");
  //server страницы для работы с FFS
  //list directory
  server.on("/list", HTTP_GET, handleFileList);
  //загрузка редактора editor
  server.on("/edit", HTTP_GET, []() {
    if (!handleFileRead("/edit.htm")) server.send(200, "text/html", notFoundResponse);
  });
  //Создание файла
  server.on("/edit", HTTP_PUT, handleFileCreate);
  //Удаление файла
  server.on("/edit", HTTP_DELETE, handleFileDelete);
  //first callback is called after the request has ended with all parsed arguments
  //second callback handles file uploads at that location
  server.on("/edit", HTTP_POST, []() {
    server.send(200, "text/plain", "");
  }, handleFileUpload);
  //called when the url is not defined here
  //use it to load content from SPIFFS
  server.onNotFound([]() {
    if (!handleFileRead(server.uri()))
      server.send(200, "text/html", notFoundResponse);
  });
}
// Здесь функции для работы с файловой системой
String getContentType(String filename) {
  Serial.println("getContentType");
  if (server.hasArg("download")) return "application/octet-stream";
  else if (filename.endsWith(".htm")) return "text/html";
  else if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".json")) return "application/json";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".png")) return "image/png";
  else if (filename.endsWith(".gif")) return "image/gif";
  else if (filename.endsWith(".jpg")) return "image/jpeg";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".xml")) return "text/xml";
  else if (filename.endsWith(".pdf")) return "application/x-pdf";
  else if (filename.endsWith(".zip")) return "application/x-zip";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

bool handleFileRead(String path) {
  Serial.print("handleFileRead path=");
  Serial.println(path);
  if (path.endsWith("/")) path += "index.htm";
  Serial.print("path=");
  Serial.println(path);
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) {
    if (SPIFFS.exists(pathWithGz))
      path += ".gz";
    File file = SPIFFS.open(path, "r");
    server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void handleFileUpload() {
  Serial.println("handleFileUpload");
  //if (server.uri() != "/edit") return;
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) filename = "/" + filename;
    fsUploadFile = SPIFFS.open(filename, "w");
    filename = String();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    Serial.print("handleFileUpload Data: "); 
    Serial.println(upload.currentSize);
    if (fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile)
      fsUploadFile.close();
    if (server.uri() == "/uploadfile"){
      if (server.hasArg("hide")){
        Serial.println("if (server.hasArg(hide)");
        server.send(200, "text/html", "<meta http-equiv=\"refresh\" content=\"0; URL=/device.htm\" />");
      }
      else{
        server.send(200, "text/html", notFoundResponse);
      }
    }
  }
}

void handleFileDelete() {
  Serial.println("handleFileDelete");
  if (server.args() == 0) return server.send(500, "text/plain", "BAD ARGS");
  String path = server.arg(0);
  if (path == "/")
    return server.send(500, "text/plain", "BAD PATH");
  if (!SPIFFS.exists(path))
    return server.send(200, "text/html", notFoundResponse);
  SPIFFS.remove(path);
  server.send(200, "text/plain", "");
  path = String();
}

void handleFileCreate() {
  Serial.println("handleFileCreate");
  if (server.args() == 0)
    return server.send(500, "text/plain", "BAD ARGS");
  String path = server.arg(0);
  if (path == "/")
    return server.send(500, "text/plain", "BAD PATH");
  if (SPIFFS.exists(path))
    return server.send(500, "text/plain", "FILE EXISTS");
  File file = SPIFFS.open(path, "w");
  if (file)
    file.close();
  else
    return server.send(500, "text/plain", "CREATE FAILED");
  server.send(200, "text/plain", "");
  path = String();

}

void handleFileList() {
  Serial.println("handleFileList");
  if (!server.hasArg("dir")) {
    server.send(500, "text/plain", "BAD ARGS");
    return;
  }
  /*
  String path = server.arg("dir");
  Dir dir = SPIFFS.openDir(path);
  path = String();
  String output = "[";
  while (root.openNextFile()) {
    File entry = root.open("r");
    if (output != "[") output += ',';
    bool isDir = false;
    output += "{\"type\":\"";
    output += (isDir) ? "dir" : "file";
    output += "\",\"name\":\"";
    output += String(entry.name()).substring(1);
    output += "\"}";
    entry.close();
  }
  output += "]";
  */
  String path = server.arg("dir");
  File root = SPIFFS.open(path);
  if(!root){
      Serial.println("- failed to open directory");
      return;
  }
  if(!root.isDirectory()){
      Serial.println(" - not a directory");
      return;
  }
  path = String();
  String output = "[";
    File file = root.openNextFile();
    while(file){
      /*
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
            //if(levels){
              //  listDir(SPIFFS, file.name(), levels -1);
            //}
        } else {
        */
            if (output != "[") output += ',';
            bool isDir = false;
            output += "{\"type\":\"";
            output += (isDir) ? "dir" : "file";
            output += "\",\"name\":\"";
            output += String(file.name()).substring(1);
            //output += String(file.name());
            output += "\"}";
            
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("\tSIZE: ");
            Serial.println(file.size());
        //}
        file = root.openNextFile();
    }
    file.close();
    output += "]";
  Serial.println("server.send(200, text/json, output);");  
  server.send(200, "text/json", output);
}
