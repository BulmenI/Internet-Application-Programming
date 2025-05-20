#include <dht.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <Adafruit_BMP085.h>
#include <SoftwareSerial.h> // Для общения с Bluetooth HC-05

// --- Объекты датчиков и LCD ---
Adafruit_BMP085 bmp180;
dht DHT;
LiquidCrystal_I2C lcd(0x27, 20, 4);

// --- SoftwareSerial для Bluetooth HC-05 ---
#define BT_RX_PIN 10 // Arduino RX пин, подключен к TXD HC-05
#define BT_TX_PIN 11 // Arduino TX пин, подключен к RXD HC-05
SoftwareSerial bluetoothSerial(BT_RX_PIN, BT_TX_PIN); // RX, TX

// --- Пины ---
#define MQ2_ANA_PIN A1
#define MQ2_DIG_PIN 2
#define DHT22_PIN 5
#define BUZZER_PIN 7
#define LED_GAS_PIN 8
#define BUTTON_PIN 3
#define DHT_POWER_PIN 12

// --- Переменные для данных с датчиков ---
float humidity = 0.0;
float temperature = 0.0;
float pressure = 0.0;
float altitude = 0.0;
int mq2_analog_val = 0;
int mq2_digital_state = LOW;

// --- Переменные для Мин/Макс ---
float min_temp = 1000.0;
float max_temp = -1000.0;
float min_hum = 1000.0;
float max_hum = -1000.0;
bool first_reading = true;

// --- Переменные для неблокирующих задержек и состояний ---
unsigned long previous_millis_sensors = 0;
const long sensor_interval = 2000;

unsigned long previous_millis_gas = 0;
const long gas_interval = 200;

// Новый интервал для отправки данных по Bluetooth
unsigned long previous_millis_bt_send = 0;
const long bt_send_interval = 3000; // Отправлять данные каждые 3 секунды

int lcd_screen_mode = 0;
const int MAX_LCD_MODES = 2;

int button_state;
int last_button_state = LOW;
unsigned long last_debounce_time = 0;
unsigned long debounce_delay = 50;

const float SEA_LEVEL_PRESSURE_HPA = 1013.25;


void setup() {
  Serial.begin(9600);       // Для отладки Uno
  bluetoothSerial.begin(9600); // HC-05 по умолчанию часто работает на 9600 бод

  pinMode(MQ2_ANA_PIN, INPUT);
  pinMode(MQ2_DIG_PIN, INPUT);
  pinMode(LED_GAS_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(DHT_POWER_PIN, OUTPUT);
  digitalWrite(DHT_POWER_PIN, HIGH);

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("System Starting..."));
  lcd.setCursor(0, 1);
  lcd.print(F("Bluetooth Mode"));


  if (!bmp180.begin()) {
    Serial.println(F("BMP180/BMP085 not found!"));
    lcd.setCursor(0, 2);
    lcd.print(F("BMP180 Error!"));
    while (1) {}
  }
  Serial.println(F("BMP180 OK"));

  read_sensors_data();
  if (temperature > -100 && humidity > -1) {
    min_temp = temperature; max_temp = temperature;
    min_hum = humidity; max_hum = humidity;
    first_reading = false;
  }

  delay(1500); // Немного дольше, чтобы увидеть сообщение о Bluetooth
  update_lcd();
}

void loop() {
  unsigned long current_millis = millis();

  handle_button();

  if (current_millis - previous_millis_sensors >= sensor_interval) {
    previous_millis_sensors = current_millis;
    read_sensors_data();
    update_min_max();
    update_lcd();
    log_to_serial(); // Оставляем для отладки через Serial Monitor Uno
  }

  if (current_millis - previous_millis_gas >= gas_interval) {
    previous_millis_gas = current_millis;
    check_gas_sensor();
  }

  // Отправка данных по Bluetooth
  if (current_millis - previous_millis_bt_send >= bt_send_interval) {
    previous_millis_bt_send = current_millis;
    send_data_to_bluetooth();
  }
}

void log_data() {
  // Создаем строку лога в нужном формате
  String log_message = String("T: ") + String(temperature, 2) + "°C, H: " + String(humidity, 2) + "%, P: " + String(pressure / 100.0, 2) + "hPa, Alt: " + String(altitude, 2) + "m, Gas val: " + String(mq2_analog_val) + ", Gas state: ";
  if (mq2_digital_state == HIGH) {
    log_message += "ALERT";
  } else {
    log_message += "OK";
  }

  Serial.println(log_message); // Запись в Serial Monitor
}

void send_data_to_bluetooth() {
  // Формируем JSON строку
  // Пример: {"temp":25.5,"hum":60.1,"pres":1012.3,"alt":123.0,"gas_a":345,"gas_d":0}
  String json_data = "{";
  json_data += "\"temp\":" + String(temperature, 1) + ",";
  json_data += "\"hum\":" + String(humidity, 1) + ",";
  json_data += "\"pres\":" + String(pressure / 100.0, 1) + ","; // в hPa
  json_data += "\"alt\":" + String(altitude, 0) + ",";
  json_data += "\"gas_a\":" + String(mq2_analog_val) + ",";
  json_data += "\"gas_d\":" + String(mq2_digital_state == HIGH ? 1 : 0);
  json_data += "}";

  bluetoothSerial.println(json_data); // Отправляем по Bluetooth с символом новой строки
  Serial.print(F("Sent to BT: ")); Serial.println(json_data); // Отладка в Serial Monitor Uno
    String log_message_bt = String("Sent to BT: {") + "\"temp\":" + String(temperature, 1) + "," + "\"hum\":" + String(humidity, 1) + "," + "\"pres\":" + String(pressure / 100.0, 1) + "," + "\"alt\":" + String(altitude, 0) + "," + "\"gas_a\":" + String(mq2_analog_val) + "," + "\"gas_d\":" + String(mq2_digital_state == HIGH ? 1 : 0) + "}";
  Serial.println(log_message_bt); //Запись в Serial Monitor
}



void read_sensors_data() {
  int chk = DHT.read22(DHT22_PIN);
  if (chk == DHTLIB_OK) {
      humidity = DHT.humidity;
      temperature = DHT.temperature;
  } else {
      Serial.print(F("DHT22 Error code: ")); Serial.println(chk);
  }
  pressure = bmp180.readPressure();
  altitude = bmp180.readAltitude(SEA_LEVEL_PRESSURE_HPA * 100);
}

void update_min_max() {
  if (first_reading && temperature > -100 && humidity > -1) {
      min_temp = temperature; max_temp = temperature;
      min_hum = humidity; max_hum = humidity;
      first_reading = false;
      return;
  }
  if (temperature > -100) {
    if (temperature < min_temp) min_temp = temperature;
    if (temperature > max_temp) max_temp = temperature;
  }
  if (humidity > -1) {
    if (humidity < min_hum) min_hum = humidity;
    if (humidity > max_hum) max_hum = humidity;
  }
}

void check_gas_sensor() {
  mq2_analog_val = analogRead(MQ2_ANA_PIN);
  mq2_digital_state = digitalRead(MQ2_DIG_PIN);

  if (mq2_digital_state == HIGH) {
    digitalWrite(LED_GAS_PIN, HIGH);
    tone(BUZZER_PIN, 1000, 150);
  } else {
    digitalWrite(LED_GAS_PIN, LOW);
    noTone(BUZZER_PIN);
  }
}

void handle_button() {
  int reading = digitalRead(BUTTON_PIN);
  if (reading != last_button_state) {
    last_debounce_time = millis();
  }
  if ((millis() - last_debounce_time) > debounce_delay) {
    if (reading != button_state) {
      button_state = reading;
      if (button_state == LOW) {
        lcd_screen_mode = (lcd_screen_mode + 1) % MAX_LCD_MODES;
        update_lcd();
      }
    }
  }
  last_button_state = reading;
}

void update_lcd() {
  lcd.clear();
  char buffer[21];

  if (lcd_screen_mode == 0) { // Основной экран
    lcd.setCursor(0, 0);
    snprintf(buffer, sizeof(buffer), "T:%.1f%cC H:%.1f%%", temperature, (char)223, humidity);
    lcd.print(buffer);

    lcd.setCursor(0, 1);
    snprintf(buffer, sizeof(buffer), "P:%.0fhPa", pressure / 100.0);
    lcd.print(buffer);

    lcd.setCursor(0, 2);
    snprintf(buffer, sizeof(buffer), "Alt:%.0fm", altitude);
    lcd.print(buffer);

    lcd.setCursor(0, 3);
    snprintf(buffer, sizeof(buffer), "Gas: %d D:%s", mq2_analog_val, mq2_digital_state == HIGH ? "ALERT" : "OK");
    lcd.print(buffer);

  } else if (lcd_screen_mode == 1) { // Экран Мин/Макс
    lcd.setCursor(0, 0);
    lcd.print(F("Min/Max Values:"));

    lcd.setCursor(0, 1);
    snprintf(buffer, sizeof(buffer), "T:%.1f-%.1f%c", min_temp, max_temp, (char)223);
    lcd.print(buffer);

    lcd.setCursor(0, 2);
    snprintf(buffer, sizeof(buffer), "H:%.1f-%.1f%%", min_hum, max_hum);
    lcd.print(buffer);

    lcd.setCursor(0, 3);
    lcd.print(F("Press to toggle"));
  }
}

void log_to_serial() { // Эта функция остается для отладки самого Arduino Uno
  Serial.print(F("T: ")); Serial.print(temperature); Serial.print((char)223); Serial.print(F("C, "));
  Serial.print(F("H: ")); Serial.print(humidity); Serial.print(F("%, "));
  Serial.print(F("P: ")); Serial.print(pressure / 100.0); Serial.print(F("hPa, "));
  Serial.print(F("Alt: ")); Serial.print(altitude); Serial.print(F("m, "));
  Serial.print(F("Gas val: ")); Serial.print(mq2_analog_val); Serial.print(F(", "));
  Serial.print(F("Gas state: ")); Serial.println(mq2_digital_state == HIGH ? "ALERT" : "OK");
}
