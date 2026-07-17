
#include "TableWiFi.h"

namespace TableWiFi
{
    AsyncWebSocket webSocket("/ws");
    WiFiState wifiState;
}

uint8_t CLOCK_ADDRESS[] = {0xd8, 0xbc, 0x38, 0xe5, 0xb8, 0x34};

esp_now_peer_info_t clockInfo;

AsyncWebServer server(80);

unsigned long SSETimer;

TaskHandle_t wifiStartTask;
uint8_t *buffer;

// Starts WiFi, OTA, and server
bool TableWiFi::StartWiFi()
{
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    unsigned long timeout = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - timeout < 20000)
    {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("connected to wifi");
        Serial.println(WiFi.localIP());

        ArduinoOTA.setHostname(wifiState.deviceName.c_str());
        ArduinoOTA.setPassword(OTA_PASS);

        ArduinoOTA.begin();

        wifiState.OTA = true;

        wifiState.wifi = true;
        return true;
    }
    else
    {
        Serial.println("Failed to start wifi");
        return false;
    }
}

// Starts Access Point server
bool TableWiFi::StartAccessPoint()
{
    wifiState.accessPoint = WiFi.softAP(wifiState.deviceName);
    if (wifiState.accessPoint)
    {
        Serial.println("Access Point started!");
        Serial.print("AP IP: ");
        Serial.println(WiFi.softAPIP());
        wifiState.AP_IP = WiFi.softAPIP();
        return true;
    }
    else
    {
        Serial.println("Failed to start AP");
        return false;
    }
}

// Starts ESP communication between clock
bool TableWiFi::StartESPNow()
{
    if (esp_now_init() != ESP_OK)
    {
        Serial.println("Error initializing ESP-NOW");
        return false;
    }

    memcpy(clockInfo.peer_addr, CLOCK_ADDRESS, 6);
    clockInfo.channel = 0;
    clockInfo.encrypt = false;

    // Add peer
    if (esp_now_add_peer(&clockInfo) != ESP_OK)
    {
        Serial.println("Failed to add peer");
        return false;
    }
    Serial.println("added peer clock");
    wifiState.ESPNow = true;
    return true;
}

void TableWiFi::HandleWebSocketMessage(void *arg, uint8_t *data, size_t len, AsyncWebSocketClient *client)
{
    AwsFrameInfo *info = (AwsFrameInfo *)arg;

    if (wifiState.OnRawMessage != NULL)
        wifiState.OnRawMessage(info, data, len, client);

    if (info->final && info->index == 0 && info->len == len)
    {
        if (info->opcode == WS_TEXT)
        {
            data[len] = 0;
            Serial.println(String((char *)data));
            if (wifiState.OnMessage != NULL)
                wifiState.OnMessage(String((char *)data), client);
        }
        if (info->opcode == WS_BINARY)
        {
            Serial.println("got bin data");

            if (wifiState.OnBinaryMessage != NULL)
                wifiState.OnBinaryMessage(data, len, client);
        }
    }
}

void TableWiFi::WebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
                               void *arg, uint8_t *data, size_t len)
{
    switch (type)
    {
    case WS_EVT_CONNECT:
        Serial.printf("WebSocket client #%lu connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        break;
    case WS_EVT_DISCONNECT:
        Serial.printf("WebSocket client #%lu disconnected\n", client->id());
        break;
    case WS_EVT_DATA:
        HandleWebSocketMessage(arg, data, len, client);
        break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
    case WS_EVT_PING:
        break;
    }
}

String TableWiFi::defaultProcessor(const String &var)
{
    Serial.println(var);
    return String();
}

/*
 * Calls wifi initialization, add events, and start MDNS
 * Runs as a task
 */
void TableWiFi::InitWifiLibTask(void *pvParameters)
{
    WiFi.mode(WIFI_AP_STA);

    StartWiFi();
    StartAccessPoint();
    // StartESPNow();

    if (MDNS.begin(wifiState.mdnsName))
    {
        Serial.println("mdns connected");
        MDNS.addService("http", "tcp", 80);
    }
    else
    {
        Serial.println("mdns could not connect!!!");
    }

    webSocket.onEvent(WebSocketEvent);
    server.addHandler(&webSocket);

    Serial.println("setup websocket");

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "text/html", wifiState.webpage, wifiState.Processor); });

    for (int i = 0; i < wifiState.jsFiles.count; i++)
    {
        JSFile *file = (JSFile *)ListGetIndex(wifiState.jsFiles);
        server.on(file->url, HTTP_GET, [](AsyncWebServerRequest *request)
                  { 
                    for(int i = 0; i < wifiState.jsFiles.count; i++){
                        JSFile *file = (JSFile*)ListGetIndex(wifiState.jsFiles);
                        if(file->url != request->url())
                            continue;
                        request->send(200, "text/javascript", *(file->contents)); 
                        break;
                    } });
    }

    server.begin();

    GetTime();

    vTaskDelete(NULL);
}

/*
 *This function must be called before Begin() to take effect
 *The name parameter is passed without any prefix or suffix (ie: .js)
 */
void TableWiFi::AddJSFile(String name, String *content)
{
    JSFile *newFile = (JSFile *)malloc(sizeof(JSFile));
    newFile->url = "/" + name + ".js";
    newFile->content = content;
    PushList(wifiState.jsFiles, newFile);
}

void TableWiFi::Begin()
{
    xTaskCreatePinnedToCore(
        InitWifiLibTask,
        "startWiFi",
        10000,
        NULL,
        1,
        &wifiStartTask,
        0);
}

// Calls ArduinoOTA and pings HTML pages
void TableWiFi::HandleWifi()
{
    webSocket.cleanupClients();
    ArduinoOTA.handle();

    if (millis() - wifiState.lastPoll > wifiState.pollTime)
    {
        wifiState.lastPoll = millis();
        SendMessage(wifiState.pollMessage);
    }
}

// Sets the current HTML page
void TableWiFi::SetWebpage(String webpage)
{
    wifiState.webpage = webpage;
}

void TableWiFi::SendMessage(String message)
{
    webSocket.textAll(message);
}

void TableWiFi::SendMessageToClient(String message, AsyncWebSocketClient *client)
{
    client->text(message);
}

////////////// Weather Functions
#pragma region
void TableWiFi::UpdateWeather()
{
    if (millis() - wifiState.lastWeatherUpdate > wifiState.weatherUpdateFrequency)
    {
        wifiState.lastWeatherUpdate = millis();

        if (!wifiState.wifi)
            return;

        HTTPClient http;
        http.begin(weatherAPI);

        if (http.GET() <= 0)
        {
            Serial.println("http response error, could not get weather");
            http.end();
            return;
        }

        String weatherResponse = http.getString();
        wifiState.weatherData = JSON.parse(weatherResponse);

        if (JSON.typeof(weatherJson) == "undefined")
        {
            Serial.println("json parse fail");
        }
        http.end();
    }
}
#pragma endregion

////////////// Time functions
#pragma region

void TableWiFi::GetTime()
{
    if (!wifiState.wifi)
    {
        return;
    }
    configTime(-18000, 3600, "pool.ntp.org");

    Serial.println("Getting time");

    if (getLocalTime(&wifiState.timeInfo))
    {
        Serial.println("got time");
        wifiState.updateTime = millis();
    }
    else
    {
        Serial.println("did not get time");
    }
}

void TableWiFi::TickTime()
{
    while (millis() - wifiState.updateTime >= 1000)
    {
        wifiState.updateTime += 1000;
        wifiState.timeInfo.tm_sec += 1;
        mktime(&wifiState.timeInfo);
    }
}

int TableWiFi::GetYear()
{
    TickTime();
    return wifiState.timeInfo.tm_year + 1900;
}
byte TableWiFi::GetMonth()
{
    TickTime();
    return wifiState.timeInfo.tm_mon;
}
byte TableWiFi::GetDay()
{
    TickTime();
    return wifiState.timeInfo.tm_mday;
}
byte TableWiFi::GetDayOfWeek()
{
    TickTime();
    return wifiState.timeInfo.tm_wday;
}
byte TableWiFi::GetHour()
{
    TickTime();
    return wifiState.timeInfo.tm_hour;
}
byte TableWiFi::GetMinute()
{
    TickTime();
    return wifiState.timeInfo.tm_min;
}
byte TableWiFi::GetSecond()
{
    TickTime();
    return wifiState.timeInfo.tm_sec;
}
int8_t TableWiFi::GetDaylightSavings()
{
    TickTime();
    return wifiState.timeInfo.tm_isdst;
}

#pragma endregion