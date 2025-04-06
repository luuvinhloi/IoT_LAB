#include <Arduino_MQTT_Client.h>
#include <ArduinoHttpClient.h>
#include <ArduinoJson.h>
#include <ThingsBoard.h>
#include <HTTPClient.h>
#include <ArduinoOTA.h>
#include <Adafruit_Sensor.h>
#include <Arduino.h>
#include <DHT_U.h>
#include <DHT.h>
#include <WiFi.h>
#include <Wire.h>
#include "time.h"

// Định nghĩa chân kết nối
#define LED_PIN 18        // Chân kết nối LED
#define LED_BUTTON_PIN 4  // Button LED
#define DHTPIN 14         // Chân kết nối DHT11 (G14 trên ESP32)
#define DHTTYPE DHT11     // Chọn loại cảm biến (DHT11 hoặc DHT22)

// Các biến trạng thái
volatile bool attributesChangedLED = false;
volatile bool ledState = false;     // Trạng thái của LED
static int lastLEDState = 0;   // Biến lưu trạng thái cũ
bool lastLedButtonState = false;    // Trạng thái trước đó của nút nhấn

// Semaphore bảo vệ biến ledState
SemaphoreHandle_t ledSemaphore;

// Lưu thời điểm gửi dữ liệu và kiểm tra kết nối
uint32_t previousDataSend;
constexpr int16_t telemetrySendInterval = 5000U; // 10 giây

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
constexpr char WIFI_SSID[] = "*******";
constexpr char WIFI_PASSWORD[] = "*********";

// Cấu hình ThingsBoard
constexpr char TOKEN[] = "**********";
constexpr char THINGSBOARD_SERVER[] = "app.coreiot.io";
constexpr uint16_t THINGSBOARD_PORT = 1883U;

DHT dht(DHTPIN, DHTTYPE);

// Xử lý RPC từ Dashboard để thay đổi LED
RPC_Response setLedSwitchState(const RPC_Data &data) {
  bool newLEDState = false;

  // Kiểm tra nếu dữ liệu có key "params" (Scheduler gửi)
  if (data.containsKey("params")) {
    newLEDState = data["params"].as<bool>();
  } else if (data.is<bool>()) {
    // Trường hợp người dùng gửi trực tiếp true/false từ dashboard
    newLEDState = data.as<bool>();
  } else {
    Serial.println("Dữ liệu RPC không hợp lệ.");
    return RPC_Response("setValueButtonLED", "Invalid params");
  }

  if (ledState != newLEDState) {
    xSemaphoreTake(ledSemaphore, portMAX_DELAY);
    ledState = newLEDState;
    digitalWrite(LED_PIN, ledState);
    xSemaphoreGive(ledSemaphore);

    Serial.printf("Dashboard yêu cầu: %s LED!\n", ledState ? "BẬT" : "TẮT");
    attributesChangedLED = true;
  }

  return RPC_Response("setValueButtonLED", ledState);
}

// RPC để Dashboard lấy trạng thái LED
RPC_Response getLedState(const RPC_Data &data) {
  return RPC_Response("getValueButtonLED", ledState);
}

// Mảng chứa callback RPC
const std::array<RPC_Callback, 2U> callbacks = {
  RPC_Callback{ "setValueButtonLED", setLedSwitchState },
  RPC_Callback{ "getValueButtonLED", getLedState }
};

// Prototype các task của FreeRTOS
void WiFiTask(void *pvParameters);
void ThingsBoardTask(void *pvParameters);
void ReconnectTask(void *pvParameters);
void DHTSensorTask(void *pvParameters);
void TaskButtonLEDControl(void *pvParameters);
void TaskSendLEDState(void *pvParameters);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(SERIAL_DEBUG_BAUD);
  // Khởi tạo Semaphore
  ledSemaphore = xSemaphoreCreateMutex();

  pinMode(LED_PIN, OUTPUT);
  pinMode(LED_BUTTON_PIN, INPUT_PULLUP);

  dht.begin();
  Wire.begin();

  // Khởi tạo các task của FreeRTOS
  xTaskCreate(WiFiTask, "WiFiTask", 4096, NULL, 1, NULL);
  xTaskCreate(ThingsBoardTask, "ThingsBoardTask", 4096, NULL, 1, NULL);
  xTaskCreate(ReconnectTask, "ReconnectTask", 4096, NULL, 1, NULL);
  xTaskCreate(DHTSensorTask, "DHTSensorTask", 4096, NULL, 1, NULL);
  xTaskCreate(TaskButtonLEDControl, "TaskButtonLEDControl", 4096, nullptr, 1, nullptr);
  xTaskCreate(TaskSendLEDState, "TaskSendLEDState", 2048, nullptr, 1, nullptr);
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
        tb.RPC_Subscribe(callbacks.cbegin(), callbacks.cend());
      }
      else {
          Serial.println("Failed to connect");
      }
    }

    tb.loop();
    vTaskDelay(800 / portTICK_PERIOD_MS);
  }
}

// Task kiểm tra và kết nối lại WiFi và ThingsBoard
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

// Cập nhật trạng thái LED lên Dashboard
void updateDashboardLEDState() {
  if (tb.connected()) {
      tb.sendAttributeData("getValueButtonLED", ledState);
      attributesChangedLED = false;  // Reset trạng thái thay đổi
  }
}

// Task điều khiển LED bằng nút nhấn
void TaskButtonLEDControl(void *pvParameters) {
  bool lastButtonState = digitalRead(LED_BUTTON_PIN);

  for (;;) {
    bool buttonState = digitalRead(LED_BUTTON_PIN);

    if (buttonState == LOW && lastButtonState == HIGH) { // Nhấn nút
      vTaskDelay(50 / portTICK_PERIOD_MS); // Debounce
      if (digitalRead(LED_BUTTON_PIN) == LOW) {  // Kiểm tra lại sau debounce

        ledState = !ledState; // Đảo trạng thái LED
        digitalWrite(LED_PIN, ledState);
        Serial.printf("Đèn LED: %s\n", ledState ? "BẬT" : "TẮT");
        updateDashboardLEDState();
      }
    }

    lastButtonState = buttonState;
    vTaskDelay(100 / portTICK_PERIOD_MS);  // Tránh CPU quá tải
  }
}

// Gửi trạng thái LED lên Dashboard mỗi 3 giây
void TaskSendLEDState(void *pvParameters) {
  bool lastSentState = ledState;  // Lưu trạng thái đã gửi

  for (;;) {
    if (attributesChangedLED && tb.connected()) {  
      xSemaphoreTake(ledSemaphore, portMAX_DELAY);
      if (lastSentState != ledState) {
        updateDashboardLEDState();
        lastSentState = ledState;
      }
      xSemaphoreGive(ledSemaphore);
      attributesChangedLED = false;  // Reset trạng thái thay đổi
    }
    vTaskDelay(3000 / portTICK_PERIOD_MS);
  }
}