#include <ArduinoOTA.h>
#include <Arduino_MQTT_Client.h>
#include <ArduinoHttpClient.h>
#include <ThingsBoard.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <Arduino.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <Wire.h>

// Định nghĩa chân kết nối
#define DHTPIN 14         // Chân kết nối DHT11 (G14 trên ESP32)
#define DHTTYPE DHT11     // Chọn loại cảm biến (DHT11 hoặc DHT22)

// Lưu thời điểm gửi dữ liệu và kiểm tra kết nối
uint32_t previousDataSend;
constexpr int16_t telemetrySendInterval = 10000U; // 10 giây

// Các hằng số cấu hình
constexpr uint32_t MAX_MESSAGE_SIZE = 1024U;
constexpr uint16_t reconnectInterval = 180000U; // 3 phút connect module
constexpr uint32_t SERIAL_DEBUG_BAUD = 115200U;

// Lưu thời điểm gửi dữ liệu và kiểm tra kết nối
uint32_t previousReconnectCheck;

WiFiClient wifiClient;
Arduino_MQTT_Client mqttClient(wifiClient);
ThingsBoard tb(mqttClient, MAX_MESSAGE_SIZE);

// Cấu hình WiFi
constexpr char WIFI_SSID[] = "******";
constexpr char WIFI_PASSWORD[] = "******";

// Cấu hình ThingsBoard
constexpr char TOKEN[] = "*********";
constexpr char THINGSBOARD_SERVER[] = "app.coreiot.io";
constexpr uint16_t THINGSBOARD_PORT = 1883U;

DHT dht(DHTPIN, DHTTYPE);

// Prototype các task của FreeRTOS
void WiFiTask(void *pvParameters);
void ThingsBoardTask(void *pvParameters);
void ReconnectTask(void *pvParameters);
void DHTSensorTask(void *pvParameters);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(SERIAL_DEBUG_BAUD);

  dht.begin();

  // Khởi tạo các task của FreeRTOS
  xTaskCreate(WiFiTask, "WiFiTask", 4096, NULL, 1, NULL);
  xTaskCreate(ThingsBoardTask, "ThingsBoardTask", 4096, NULL, 1, NULL);
  xTaskCreate(ReconnectTask, "ReconnectTask", 4096, NULL, 1, NULL);
  xTaskCreate(DHTSensorTask, "DHTSensorTask", 4096, NULL, 1, NULL);
}

void loop() {
  vTaskDelete(nullptr);
}

// Task kết nối WiFi
void WiFiTask(void *pvParameters) {
    for (;;) {
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("Connecting to WiFi...");
            WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
            while (WiFi.status() != WL_CONNECTED) {
                vTaskDelay(500 / portTICK_PERIOD_MS);
                Serial.print(".");
            }
            Serial.println("Connected to WiFi");
        }
        vTaskDelay(3000 / portTICK_PERIOD_MS);
    }
}

// Task kết nối ThingsBoard
void ThingsBoardTask(void *pvParameters) {
    for (;;) {
        // Kiểm tra kết nối ThingsBoard, nếu chưa kết nối thì thực hiện kết nối
        if (!tb.connected()) {
            Serial.println("Connecting to ThingsBoard...");
            if (tb.connect(THINGSBOARD_SERVER, TOKEN, THINGSBOARD_PORT)) {
                Serial.println("Connected to ThingsBoard");
                // Gửi MAC address và đăng ký RPC
                tb.sendAttributeData("macAddress", WiFi.macAddress().c_str());
            }
            else {
                Serial.println("Failed to connect");
            }
        }
        
        tb.loop();
        vTaskDelay(800 / portTICK_PERIOD_MS);
    }
}

// // Task kiểm tra và kết nối lại WiFi và ThingsBoard
void ReconnectTask(void *pvParameters) {
    for (;;) {
        if (millis() - previousReconnectCheck > reconnectInterval) {
            previousReconnectCheck = millis();
            if (WiFi.status() != WL_CONNECTED) {
                Serial.println("Reconnecting WiFi...");
                WiFi.disconnect();
                WiFi.reconnect();
            }
            if (!tb.connected()) {
                Serial.println("Reconnecting to ThingsBoard...");
                tb.connect(THINGSBOARD_SERVER, TOKEN, THINGSBOARD_PORT);
            }
        }
        vTaskDelay(reconnectInterval / portTICK_PERIOD_MS);
    }
}

// Task đọc dữ liệu từ cảm biến DHT11 và gửi lên ThingsBoard
void DHTSensorTask(void *pvParameters) {
  for (;;) {
    if (millis() - previousDataSend > telemetrySendInterval) {
      previousDataSend = millis();
      
      // Đọc dữ liệu từ cảm biến DHT11
      float temperature = dht.readTemperature();
      float humidity = dht.readHumidity();

      if (isnan(temperature) || isnan(humidity)) {
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        temperature = dht.readTemperature();
        humidity = dht.readHumidity();
      }
      
      // In ra dữ liệu và hiển thị lên Serial Monitor
      Serial.printf("Temperature: %.2f°C, Humidity: %.2f%%\n", temperature, humidity);
      
      // Gửi dữ liệu lên ThingsBoard
      if (!isnan(temperature) && !isnan(humidity)) {
        tb.sendTelemetryData("temperature", temperature);
        tb.sendTelemetryData("humidity", humidity);
      }
    }
      
    vTaskDelay(telemetrySendInterval / portTICK_PERIOD_MS);
  }
}