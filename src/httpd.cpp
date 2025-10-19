#include <WebServer.h>
#include <Arduino.h>
#include "filesystem.h"
#include "httpd.h"

WebServer httpd(80);

String getContentType(String filename)
{
  if (httpd.hasArg("download"))
  {
    return "application/octet-stream";
  }
  else if (filename.endsWith(".htm"))
  {
    return "text/html";
  }
  else if (filename.endsWith(".html"))
  {
    return "text/html";
  }
  else if (filename.endsWith(".css"))
  {
    return "text/css";
  }
  else if (filename.endsWith(".js"))
  {
    return "application/javascript";
  }
  else if (filename.endsWith(".png"))
  {
    return "image/png";
  }
  else if (filename.endsWith(".gif"))
  {
    return "image/gif";
  }
  else if (filename.endsWith(".jpg"))
  {
    return "image/jpeg";
  }
  else if (filename.endsWith(".ico"))
  {
    return "image/x-icon";
  }
  else if (filename.endsWith(".xml"))
  {
    return "text/xml";
  }
  else if (filename.endsWith(".pdf"))
  {
    return "application/x-pdf";
  }
  else if (filename.endsWith(".zip"))
  {
    return "application/x-zip";
  }
  else if (filename.endsWith(".gz"))
  {
    return "application/x-gzip";
  }
  return "text/plain";
}

bool exists(String path)
{
  bool yes = false;
  File file = FILESYSTEM.open(path, "r");
  if (!file.isDirectory())
  {
    yes = true;
  }
  file.close();
  return yes;
}

bool handleFileRead(String path)
{
  // Wenn keine explizite Datei angegeben ist: index.html verwenden
  // Hier könnten auch Unterverzeichnisse auf Dateien umgeleitet werden
  if (path.endsWith("/"))
  {
    path += "index.html";
  }
  String contentType = getContentType(path);
  if (exists(path))
  {
    File file = FILESYSTEM.open(path, "r");
    httpd.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}


void setup_httpd()
{
  // Route for dedicated download
  httpd.on("/download", HTTP_GET, []() {
    if (!httpd.hasArg("file")) {
      httpd.send(400, "text/plain", "Missing 'file' parameter");
      return;
    }

    String path = "/" + httpd.arg("file");  // z.B. ?file=config.json
    if (!exists(path)) {
      httpd.send(404, "text/plain", "File not found");
      return;
    }

    File file = FILESYSTEM.open(path, "r");
    if (!file) {
      httpd.send(500, "text/plain", "Failed to open file");
      return;
    }

    String contentType = "application/octet-stream";
    httpd.streamFile(file, contentType);
    file.close();
  });

  // Upload: via POST /upload mit multipart/form-data
  httpd.on("/upload", HTTP_POST, []() {
    httpd.send(200, "text/plain", "Upload successful");
  }, []() {
    HTTPUpload& upload = httpd.upload();
    static File uploadFile;

    if (upload.status == UPLOAD_FILE_START) {
      String filename = "/" + upload.filename;
      Serial.printf("Upload start: %s\n", filename.c_str());
      uploadFile = FILESYSTEM.open(filename, "w");
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (uploadFile) uploadFile.write(upload.buf, upload.currentSize);
    } else if (upload.status == UPLOAD_FILE_END) {
      if (uploadFile) {
        uploadFile.close();
        Serial.printf("Upload finished: %s (%u bytes)\n", upload.filename.c_str(), upload.totalSize);
      }
    }
  });

  // Root handler – serve index.html or fallback text for old Android 4 systems
  httpd.on("/", HTTP_GET, []() {
    if (!handleFileRead("/index.html")) {
      httpd.send(200, "text/html",
                 "<html><body><h1>APPlausometer</h1>"
                 "<p>No Web UI found on SPIFFS.</p>"
                 "</body></html>");
    }
  });

  // Fallback: static files from SPIFFS via e.g. http://ip/config.json
  httpd.onNotFound([]() {
    Serial.printf("http request: %s\n", httpd.uri().c_str());
    if (!handleFileRead(httpd.uri())) {
      httpd.send(404, "text/plain", "FileNotFound");
      Serial.printf("ERROR: Not found: %s\n", httpd.uri().c_str());
    }
  });

  httpd.begin();
}


void loop_httpd()
{
  httpd.handleClient();
}
