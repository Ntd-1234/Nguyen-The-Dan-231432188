
#include <WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ================= WIFI =================
const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASS = "";

// ================= MQTT =================
const char* MQTT_HOST = "broker.emqx.io";
const int MQTT_PORT = 1883;

const char* ROOT_TOPIC = "aquatank/demo_83921";

String TOPIC_DATA = String(ROOT_TOPIC) + "/data";
String TOPIC_MODE = String(ROOT_TOPIC) + "/cmd/mode";
String TOPIC_PUMP = String(ROOT_TOPIC) + "/cmd/pump";
String TOPIC_VALVE = String(ROOT_TOPIC) + "/cmd/valve";

String TOPIC_PUMP_STATE = String(ROOT_TOPIC) + "/status/pump";
String TOPIC_VALVE_STATE = String(ROOT_TOPIC) + "/status/valve";
String TOPIC_MODE_STATE = String(ROOT_TOPIC) + "/status/mode";

// ================= GPIO =================
#define TRIG_PIN 5
#define ECHO_PIN 18

#define TEMP_PIN 4

#define PUMP_RELAY 26
#define VALVE_RELAY 27

// Relay kích mức HIGH
#define RELAY_ON HIGH
#define RELAY_OFF LOW

// Chiều cao bể (cm)
#define TANK_HEIGHT_CM 200.0

// ================= MQTT OBJECT =================
WiFiClient espClient;
PubSubClient mqtt(espClient);

// ================= DS18B20 =================
OneWire oneWire(TEMP_PIN);
DallasTemperature sensors(&oneWire);

// ================= DATA STRUCT =================
struct SensorData {
  float distance;
  float level;
  float temperature;
};

enum CommandType {
  CMD_MODE,
  CMD_PUMP,
  CMD_VALVE
};

struct WebCommand {
  CommandType type;
  bool value;
};

// ================= QUEUE =================
QueueHandle_t dataQueue;
QueueHandle_t dataAlertQueue;
QueueHandle_t cmdQueue;

// ================= DEVICE STATE =================
volatile bool pumpState = false;
volatile bool valveState = false;
volatile bool autoMode = true;

// ================= SENSOR =================
float readDistance() {

  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(3);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);

  digitalWrite(TRIG_PIN, LOW);

  unsigned long duration =
      pulseIn(ECHO_PIN, HIGH, 30000);

  if (duration == 0)
    return -1;

  return duration * 0.0343 / 2;
}

float calculateLevel(float distance) {

  if (distance < 0)
    return -1;

  float level = TANK_HEIGHT_CM - distance;

  if (level < 0)
    level = 0;

  if (level > TANK_HEIGHT_CM)
    level = TANK_HEIGHT_CM;

  return level;
}

// ================= RELAY CONTROL =================
void controlPump(bool state) {

  pumpState = state;

  digitalWrite(
      PUMP_RELAY,
      state ? RELAY_ON : RELAY_OFF
  );
}

void controlValve(bool state) {

  valveState = state;

  digitalWrite(
      VALVE_RELAY,
      state ? RELAY_ON : RELAY_OFF
  );
}

// ================= MQTT STATUS =================
void publishStatus() {

  if (!mqtt.connected())
    return;

  mqtt.publish(
      TOPIC_PUMP_STATE.c_str(),
      pumpState ? "ON" : "OFF",
      true
  );

  mqtt.publish(
      TOPIC_VALVE_STATE.c_str(),
      valveState ? "ON" : "OFF",
      true
  );

  mqtt.publish(
      TOPIC_MODE_STATE.c_str(),
      autoMode ? "AUTO" : "MANUAL",
      true
  );
}

// ================= MQTT CALLBACK =================
void mqttCallback(
    char* topic,
    byte* payload,
    unsigned int length
) {

  String message = "";

  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  message.trim();
  message.toUpperCase();

  WebCommand cmd;
  bool valid = true;

  String receivedTopic = String(topic);

  if (receivedTopic == TOPIC_MODE) {

    cmd.type = CMD_MODE;

    if (message == "AUTO")
      cmd.value = true;

    else if (message == "MANUAL")
      cmd.value = false;

    else
      valid = false;
  }

  else if (receivedTopic == TOPIC_PUMP) {

    cmd.type = CMD_PUMP;

    if (message == "ON")
      cmd.value = true;

    else if (message == "OFF")
      cmd.value = false;

    else
      valid = false;
  }

  else if (receivedTopic == TOPIC_VALVE) {

    cmd.type = CMD_VALVE;

    if (message == "ON")
      cmd.value = true;

    else if (message == "OFF")
      cmd.value = false;

    else
      valid = false;
  }

  else {
    valid = false;
  }

  if (valid && cmdQueue != NULL) {
    xQueueSend(cmdQueue, &cmd, 0);
  }
}

// ================= MQTT CONNECT =================
bool connectMQTT() {

  if (mqtt.connected())
    return true;

  String clientID =
      "ESP32-AquaTank-" +
      String((uint32_t)ESP.getEfuseMac(), HEX);

  if (mqtt.connect(clientID.c_str())) {

    mqtt.subscribe(TOPIC_MODE.c_str());
    mqtt.subscribe(TOPIC_PUMP.c_str());
    mqtt.subscribe(TOPIC_VALVE.c_str());

    publishStatus();

    Serial.println("[MQTT] Connected!");

    return true;
  }

  return false;
}

// =================================================
// TASK 1: READ SENSOR - CORE 1
// =================================================
void TaskReadData(void *parameter) {

  SensorData data;

  for (;;) {

    // Đọc HC-SR04
    data.distance = readDistance();

    // Đọc DS18B20
    sensors.requestTemperatures();

    data.temperature =
        sensors.getTempCByIndex(0);

    if (data.temperature == DEVICE_DISCONNECTED_C) {
      data.temperature = -127;
    }

    // Tính mực nước
    data.level = calculateLevel(data.distance);

    // Hiển thị Serial
    Serial.println("\n===== DU LIEU BE NUOI =====");

    Serial.printf(
        "Khoang cach: %.1f cm\n",
        data.distance
    );

    Serial.printf(
        "Muc nuoc: %.1f cm\n",
        data.level
    );

    Serial.printf(
        "Nhiet do: %.1f C\n",
        data.temperature
    );

    Serial.println("==========================");

    // Gửi dữ liệu vào data Queue
    if (xQueueSend(dataQueue, &data, 0) != pdPASS) {

      SensorData oldData;

      xQueueReceive(dataQueue, &oldData, 0);

      xQueueSend(dataQueue, &data, 0);
    }

    // Gửi mực nước vào Alert Queue
    if (data.level >= 0) {

      xQueueOverwrite(
          dataAlertQueue,
          &data.level
      );
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// =================================================
// TASK 2: MQTT TRANSMIT - CORE 0
// =================================================
void TaskTransmitData(void *parameter) {

  SensorData data;

  char json[256];

  unsigned long lastStatus = 0;

  for (;;) {

    // Kết nối WiFi
    if (WiFi.status() != WL_CONNECTED) {

      WiFi.begin(WIFI_SSID, WIFI_PASS);

      while (WiFi.status() != WL_CONNECTED) {

        vTaskDelay(pdMS_TO_TICKS(500));
      }

      Serial.println("[WiFi] Connected!");

      Serial.println(WiFi.localIP());
    }

    // Kết nối MQTT
    if (!mqtt.connected()) {

      connectMQTT();
    }

    if (mqtt.connected()) {

      mqtt.loop();

      // Đọc dữ liệu từ data Queue
      if (
          xQueueReceive(
              dataQueue,
              &data,
              pdMS_TO_TICKS(100)
          ) == pdPASS
      ) {

        snprintf(
            json,
            sizeof(json),

            "{\"distance_cm\":%.1f,"
            "\"level_cm\":%.1f,"
            "\"temperature_c\":%.1f,"
            "\"pump\":\"%s\","
            "\"valve\":\"%s\","
            "\"mode\":\"%s\"}",

            data.distance,
            data.level,
            data.temperature,

            pumpState ? "ON" : "OFF",

            valveState ? "ON" : "OFF",

            autoMode ? "AUTO" : "MANUAL"
        );

        mqtt.publish(
            TOPIC_DATA.c_str(),
            json
        );

        Serial.print("[MQTT] ");

        Serial.println(json);
      }

      // Cập nhật trạng thái định kỳ
      if (millis() - lastStatus >= 2000) {

        publishStatus();

        lastStatus = millis();
      }
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// =================================================
// TASK 3: ALERT AND CONTROL - CORE 1
// =================================================
void TaskAlert(void *parameter) {

  float latestLevel = -1;

  float receivedLevel;

  WebCommand cmd;

  enum Action {
    IDLE,
    PUMP_50,
    PUMP_100,
    VALVE_OPEN
  };

  Action action = IDLE;

  unsigned long actionStart = 0;

  unsigned long lastToggle = 0;

  bool pumpHalfCycle = false;

  int previousZone = 0;

  for (;;) {

    // Nhận lệnh điều khiển từ Web
    while (
        xQueueReceive(cmdQueue, &cmd, 0) == pdPASS
    ) {

      if (cmd.type == CMD_MODE) {

        autoMode = cmd.value;

        action = IDLE;

        controlPump(false);
        controlValve(false);

        previousZone = 0;

        Serial.println(
            autoMode ? "MODE AUTO" : "MODE MANUAL"
        );
      }

      else if (
          cmd.type == CMD_PUMP && !autoMode
      ) {

        action = IDLE;

        if (cmd.value) {

          controlValve(false);
          controlPump(true);
        }

        else {
          controlPump(false);
        }
      }

      else if (
          cmd.type == CMD_VALVE && !autoMode
      ) {

        action = IDLE;

        if (cmd.value) {

          controlPump(false);
          controlValve(true);
        }

        else {
          controlValve(false);
        }
      }
    }

    // Đọc mực nước từ Alert Queue
    if (
        xQueueReceive(
            dataAlertQueue,
            &receivedLevel,
            0
        ) == pdPASS
    ) {

      latestLevel = receivedLevel;
    }

    unsigned long now = millis();

    // ================= AUTO MODE =================
    if (autoMode && latestLevel >= 0) {

      int currentZone = 0;

      if (
          latestLevel >= 100 &&
          latestLevel < 150
      ) {
        currentZone = 1;
      }

      else if (
          latestLevel >= 150 &&
          latestLevel <= 200
      ) {
        currentZone = 2;
      }

      else if (latestLevel < 50) {

        currentZone = 3;
      }

      // Chỉ kích hoạt khi đi vào vùng mới
      if (
          currentZone != previousZone &&
          currentZone != 0 &&
          action == IDLE
      ) {

        actionStart = now;

        lastToggle = now;

        controlPump(false);
        controlValve(false);

        if (currentZone == 1) {

          action = PUMP_50;

          pumpHalfCycle = true;

          controlPump(true);

          Serial.println(
              "AUTO: BOM 50% - 10 GIAY"
          );
        }

        else if (currentZone == 2) {

          action = PUMP_100;

          controlPump(true);

          Serial.println(
              "AUTO: BOM 100% - 20 GIAY"
          );
        }

        else if (currentZone == 3) {

          action = VALVE_OPEN;

          controlValve(true);

          Serial.println(
              "AUTO: MO VAN XA - 10 GIAY"
          );
        }
      }

      previousZone = currentZone;
    }

    // ================= TIMED CONTROL =================
    if (action == PUMP_50) {

      if (now - actionStart >= 10000) {

        controlPump(false);

        action = IDLE;

        Serial.println("KET THUC BOM 50%");
      }

      else if (now - lastToggle >= 1000) {

        lastToggle = now;

        pumpHalfCycle = !pumpHalfCycle;

        controlPump(pumpHalfCycle);
      }
    }

    else if (action == PUMP_100) {

      if (now - actionStart >= 20000) {

        controlPump(false);

        action = IDLE;

        Serial.println("KET THUC BOM 100%");
      }
    }

    else if (action == VALVE_OPEN) {

      if (now - actionStart >= 10000) {

        controlValve(false);

        action = IDLE;

        Serial.println("DONG VAN XA");
      }
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

// =================================================
// SETUP
// =================================================
void setup() {

  Serial.begin(115200);

  // GPIO
  pinMode(TRIG_PIN, OUTPUT);

  pinMode(ECHO_PIN, INPUT);

  pinMode(PUMP_RELAY, OUTPUT);

  pinMode(VALVE_RELAY, OUTPUT);

  controlPump(false);

  controlValve(false);

  // Khởi tạo cảm biến nhiệt độ
  sensors.begin();

  // Tạo Queue
  dataQueue = xQueueCreate(
      10,
      sizeof(SensorData)
  );

  dataAlertQueue = xQueueCreate(
      1,
      sizeof(float)
  );

  cmdQueue = xQueueCreate(
      10,
      sizeof(WebCommand)
  );

  if (
      dataQueue == NULL ||
      dataAlertQueue == NULL ||
      cmdQueue == NULL
  ) {

    Serial.println("ERROR: CREATE QUEUE FAILED!");

    while (true) {
      delay(1000);
    }
  }

  // Cấu hình MQTT
  mqtt.setServer(
      MQTT_HOST,
      MQTT_PORT
  );

  mqtt.setCallback(mqttCallback);

  mqtt.setBufferSize(512);

  // Tạo Task truyền dữ liệu - Core 0
  xTaskCreatePinnedToCore(
      TaskTransmitData,
      "TaskTransmitData",
      8192,
      NULL,
      2,
      NULL,
      0
  );

  // Tạo Task đọc dữ liệu - Core 1
  xTaskCreatePinnedToCore(
      TaskReadData,
      "TaskReadData",
      4096,
      NULL,
      2,
      NULL,
      1
  );

  // Tạo Task cảnh báo - Core 1
  xTaskCreatePinnedToCore(
      TaskAlert,
      "TaskAlert",
      4096,
      NULL,
      2,
      NULL,
      1
  );

  Serial.println("HE THONG DA KHOI DONG!");
}

// =================================================
// LOOP
// =================================================
void loop() {

  vTaskDelay(pdMS_TO_TICKS(1000));
}
