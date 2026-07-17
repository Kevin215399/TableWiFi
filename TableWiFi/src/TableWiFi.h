

#include "Arduino.h"

struct CompressedData
{
  uint8_t data[245 * 4];
  uint16_t length;
};

// Compression Declarations
CompressedData CompressLeds();

// WiFi declarations
#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <TimeLib.h>
#include <GeneralList.h>

#include <esp_wifi.h>
#include <esp_now.h>

#include <Arduino_JSON.h>

struct JSFile
{
  String url;
  String *contents;
};

struct WiFiState
{
  IPAddress wifi_IP = "";
  IPAddress AP_IP = "";
  bool wifi = false;
  bool accessPoint = false;
  bool ESPNow = false;
  bool OTA = false;
  String webpage = "";

  GeneralList jsFiles;

  GeneralList peerAddresses;

  void (*OnMessage)(String, AsyncWebSocketClient *);
  void (*OnBinaryMessage)(uint8_t *, size_t, AsyncWebSocketClient *);
  void (*OnRawMessage)(AwsFrameInfo *, uint8_t *, size_t, AsyncWebSocketClient *);
  String (*Processor)(const String &var);
  String mdnsName = "Desk";
  String deviceName = "Desk";

  unsigned long lastPoll = 0;
  unsigned long pollTime = 5 * 1000;
  String pollMessage = "POLL";

  unsigned long lastWeatherUpdate = 0;
  unsigned long weatherUpdateFrequency = 2 * 60 * 1000; // default: every 2 minutes
  JSONVar weatherData;

  struct tm timeInfo;
  unsigned long updateTime;
};

namespace TableWiFi
{
  bool StartWiFi();
  bool StartAccessPoint();
  bool StartESPNow();
  void Begin();
  void HandleWifi();
  void SetWebpage(String webpage);
  void SendMessage(String message);
  void SendMessageToClient(String message, AsyncWebSocketClient *client);

  void AddJSFile(String name, String *content);

  void UpdateWeather();

  void GetTime();
  void TickTime();

  int GetYear();
  byte GetMonth();
  byte GetDay();
  byte GetDayOfWeek();
  byte GetHour();
  byte GetMinute();
  byte GetSecond();
  int8_t GetDaylightSavings();

  extern AsyncWebSocket webSocket;
  extern WiFiState wifiState;

  void HandleWebSocketMessage(void *arg, uint8_t *data, size_t len, AsyncWebSocketClient *client);
  void WebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
                      void *arg, uint8_t *data, size_t len);
  String defaultProcessor(const String &var);
  void InitWifiLibTask(void *pvParameters);
}