#include <WebServer.h>
#include <Arduino.h>
#include "fs_compat.h"
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
  // Start the web server

  // wenn kein handle für die URL vergeben wurde, überprüfe ob eine entsprechende
  // Datei im Dateisystem liegt und sende dann die. Wenn sie fehlt -> Fehlermeldung
  httpd.onNotFound([]()
                   {
    Serial.printf("http request: %s\n", httpd.uri().c_str());
    if (!handleFileRead(httpd.uri())) {
      httpd.send(404, "text/plain", "FileNotFound");
      Serial.printf("ERROR: Not found: %s\n", httpd.uri().c_str());
    } });
  httpd.begin();
}

void loop_httpd()
{
  httpd.handleClient();
}
