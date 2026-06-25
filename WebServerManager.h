// WebServerManager.h  –  ESP32-RC-Sound v1.22
#pragma once
#include <WiFi.h>
#include <WebServer.h>

class WebServerManager {
public:
  static void begin(const char* apSsid, const char* apPassword);
  static void Webpage();

private:
  static WebServer server;
  static int       Menu;
  static String    valueString;
  static void      handleRequest();
  static void      handleSport();
  static void      handleApiConfig();
  static void      handleApiConfigPost();
  static void      handleApiDebug();
  static void      handleApiSound();
  static String    urlDecode(const String& s);
};
