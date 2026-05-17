#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <IRremote.h>

// Пины для первого ультразвукового датчика
#define TRIGGER_PIN_1  3
#define ECHO_PIN_1     2

// Пины для второго ультразвукового датчика
#define TRIGGER_PIN_2  7
#define ECHO_PIN_2     8

// Пины для обычных светодиодов
#define PIN_RED_LED   10
#define PIN_GREEN_LED 9
#define PIN_BLUE_LED  6

// Пин для пассивного зуммера
#define PIN_BUZZER 5

// Пин для ИК-приемника
#define IR_PIN A0

// Создаём объект дисплея
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Переменные для ИК-приемника (синтаксис для версии 2.x)
IRrecv irrecv(IR_PIN);
decode_results results;

// Переменные для измерения расстояния
long duration1, duration2;
int distance1, distance2;
int minDistance;

// Переменные для режимов работы
int mode = 0; // 0 - оба датчика, 1 - только первый, 2 - только второй
bool soundEnabled = true; // Состояние зуммера (вкл/выкл)

// Коды кнопок пульта (замените на свои после тестирования)
#define BTN_UP      0xFF18E7
#define BTN_DOWN    0xFF4AB5
#define BTN_OK      0xFF38C7
#define BTN_ZERO    0xFF9867

// Флаги для отображения сообщений
unsigned long messageStartTime = 0;
bool showMessage = false;
String currentMessage = "";

// САМОДЕЛЬНАЯ ФУНКЦИЯ ДЛЯ ЗВУКА (без конфликта с IRremote)
void myTone(int pin, int frequency, int duration_ms) {
  if (frequency <= 0) return;
  
  int period = 1000000L / frequency;
  int pulse = period / 2;
  unsigned long startTime = millis();
  unsigned long endTime = startTime + duration_ms;
  
  while (millis() < endTime) {
    digitalWrite(pin, HIGH);
    delayMicroseconds(pulse);
    digitalWrite(pin, LOW);
    delayMicroseconds(pulse);
  }
}

void setup() {
  // Инициализация дисплея
  lcd.init();
  lcd.backlight();
  lcd.clear();
  
  // Инициализация пинов
  pinMode(TRIGGER_PIN_1, OUTPUT);
  pinMode(ECHO_PIN_1, INPUT);
  pinMode(TRIGGER_PIN_2, OUTPUT);
  pinMode(ECHO_PIN_2, INPUT);
  pinMode(PIN_RED_LED, OUTPUT);
  pinMode(PIN_GREEN_LED, OUTPUT);
  pinMode(PIN_BLUE_LED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  
  // Инициализация ИК-приемника (синтаксис для версии 2.x)
  Serial.begin(9600);
  irrecv.enableIRIn();
  
  // Выключаем все светодиоды
  digitalWrite(PIN_RED_LED, LOW);
  digitalWrite(PIN_GREEN_LED, LOW);
  digitalWrite(PIN_BLUE_LED, LOW);
  
  // Приветственное сообщение
  lcd.setCursor(0, 0);
  lcd.print("System Ready");
  lcd.setCursor(0, 1);
  lcd.print("IR INIT...");
  delay(2000);
  lcd.clear();
  
  // Начальный режим - оба датчика
  mode = 0;
  lcd.setCursor(0, 0);
  lcd.print("D1:    D2:");
  
  Serial.println("System ready. Press buttons on remote...");
}

void loop() {
  // Проверяем ИК-сигналы (синтаксис для версии 2.x)
  if (irrecv.decode(&results)) {
    uint32_t receivedValue = results.value;
    
    // Выводим информацию в Serial
    Serial.print("Raw: 0x");
    Serial.println(receivedValue, HEX);
    
    handleIRCommand(receivedValue);
    irrecv.resume(); // Возобновляем прием
  }
  
  // Измеряем расстояния в зависимости от режима
  measureDistancesByMode();
  
  // Обновляем дисплей в зависимости от режима
  updateDisplay();
  
  // Показываем временное сообщение если нужно
  if (showMessage && (millis() - messageStartTime < 1500)) {
    lcd.setCursor(0, 1);
    lcd.print(currentMessage);
    for(int i = currentMessage.length(); i < 16; i++) lcd.print(" ");
  } else {
    showMessage = false;
    updateStatusLine();
  }
  
  // Управление светодиодами и зуммером
  if (soundEnabled) {
    controlLEDsAndSound();
  } else {
    controlLEDsOnly();
  }
  
  delay(100);
}

void handleIRCommand(uint32_t cmd) {
  // Игнорируем повторяющиеся сигналы
  if (cmd == 0xFFFFFFFF) return;
  
  Serial.print("Command received: 0x");
  Serial.println(cmd, HEX);
  
  switch(cmd) {
    // Кнопка ВВЕРХ (основной код и дополнительный)
    case BTN_UP:
    case 0x3D9AE3F7:  // дополнительный код для ВВЕРХ
      mode = 1;
      showTemporaryMessage("Moving forward");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("D1:         ");
      Serial.println("Mode: Forward (Sensor 1 only)");
      break;
      
    // Кнопка ВНИЗ (основной код и дополнительный)
    case BTN_DOWN:
    case 0x1BC0157B:  // дополнительный код для ВНИЗ
      mode = 2;
      showTemporaryMessage("Moving backward");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("D2:         ");
      Serial.println("Mode: Backward (Sensor 2 only)");
      break;
      
    // Кнопка OK (основной код и дополнительный)
    case BTN_OK:
    case 0x488F3CBB:  // дополнительный код для OK
      soundEnabled = !soundEnabled;
      showTemporaryMessage(soundEnabled ? "Sound ON" : "Sound OFF");
      Serial.print("Sound: ");
      Serial.println(soundEnabled ? "ON" : "OFF");
      break;
      
    // Кнопка 0 (основной код и дополнительный)
    case BTN_ZERO:
    case 0x97483BFB:  // дополнительный код для 0
      mode = 0;
      showTemporaryMessage("Both sensors");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("D1:    D2:");
      Serial.println("Mode: Both sensors");
      break;
      
    default:
      Serial.println("Unknown button");
      break;
  }
}
void showTemporaryMessage(String message) {
  currentMessage = message;
  showMessage = true;
  messageStartTime = millis();
}

void measureDistancesByMode() {
  if (mode == 0 || mode == 1) {
    digitalWrite(TRIGGER_PIN_1, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIGGER_PIN_1, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIGGER_PIN_1, LOW);
    
    duration1 = pulseIn(ECHO_PIN_1, HIGH, 30000);
    distance1 = duration1 * 0.034 / 2;
    if (distance1 > 300 || distance1 < 0 || duration1 == 0) distance1 = 999;
  } else {
    distance1 = 999;
  }
  
  if (mode == 0 || mode == 2) {
    digitalWrite(TRIGGER_PIN_2, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIGGER_PIN_2, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIGGER_PIN_2, LOW);
    
    duration2 = pulseIn(ECHO_PIN_2, HIGH, 30000);
    distance2 = duration2 * 0.034 / 2;
    if (distance2 > 300 || distance2 < 0 || duration2 == 0) distance2 = 999;
  } else {
    distance2 = 999;
  }
  
  if (mode == 0) {
    int d1 = (distance1 <= 300) ? distance1 : 999;
    int d2 = (distance2 <= 300) ? distance2 : 999;
    if (d1 <= 300 && d2 <= 300) minDistance = min(d1, d2);
    else if (d1 <= 300) minDistance = d1;
    else if (d2 <= 300) minDistance = d2;
    else minDistance = 300;
  } else if (mode == 1) {
    minDistance = (distance1 <= 300) ? distance1 : 300;
  } else {
    minDistance = (distance2 <= 300) ? distance2 : 300;
  }
}

void updateDisplay() {
  if (mode == 0) {
    lcd.setCursor(3, 0); lcd.print("   ");
    lcd.setCursor(3, 0); 
    if (distance1 <= 300) lcd.print(distance1); else lcd.print("---");
    
    lcd.setCursor(12, 0); lcd.print("   ");
    lcd.setCursor(12, 0); 
    if (distance2 <= 300) lcd.print(distance2); else lcd.print("---");
  } 
  else if (mode == 1) {
    lcd.setCursor(3, 0); lcd.print("        ");
    lcd.setCursor(3, 0); 
    if (distance1 <= 300) lcd.print(distance1); else lcd.print("---");
    lcd.print("cm");
  } 
  else if (mode == 2) {
    lcd.setCursor(3, 0); lcd.print("        ");
    lcd.setCursor(3, 0); 
    if (distance2 <= 300) lcd.print(distance2); else lcd.print("---");
    lcd.print("cm");
  }
}

void updateStatusLine() {
  if (showMessage) return;
  
  if (minDistance > 0 && minDistance < 10) {
    lcd.setCursor(0, 1); lcd.print("STOP! "); lcd.print(minDistance); lcd.print("cm          ");
  } else if (minDistance >= 10 && minDistance < 20) {
    lcd.setCursor(0, 1); lcd.print("Attention "); lcd.print(minDistance); lcd.print("cm");
  } else if (minDistance >= 20 && minDistance <= 30) {
    lcd.setCursor(0, 1); lcd.print("OK "); lcd.print(minDistance); lcd.print("cm             ");
  } else {
    lcd.setCursor(0, 1); lcd.print("Clear             ");
  }
}

void controlLEDsAndSound() {
  if (minDistance > 0 && minDistance < 10) {
    digitalWrite(PIN_RED_LED, HIGH);
    digitalWrite(PIN_GREEN_LED, LOW);
    digitalWrite(PIN_BLUE_LED, LOW);
    
    if (soundEnabled) {
      // Непрерывное "пииииииииииииии" 1 секунду
      myTone(PIN_BUZZER, 1000, 1000);
      delay(100);
    }
}
   else if (minDistance >= 10 && minDistance < 20) {
    // СИНИЙ - ЧАСТЫЕ короткие звуки "пи-пи-пи-пи-пи"
    digitalWrite(PIN_RED_LED, LOW);
    digitalWrite(PIN_GREEN_LED, LOW);
    digitalWrite(PIN_BLUE_LED, HIGH);
    
    if (soundEnabled) {

      myTone(PIN_BUZZER, 1000, 50);      // Короткий "пи" 50 мс
      delay(60);
    }
  } 
  else if (minDistance >= 20 && minDistance <= 50) {
    // ЗЕЛЕНЫЙ - РЕДКИЕ звуки "пи....пи....пи" с большими пробелами
    digitalWrite(PIN_RED_LED, LOW);
    digitalWrite(PIN_GREEN_LED, HIGH);
    digitalWrite(PIN_BLUE_LED, LOW);
    
    if (soundEnabled) {
      myTone(PIN_BUZZER, 1000, 80);        // Короткий "пи"
      delay(800);  
    }
  }
  else {
    digitalWrite(PIN_RED_LED, LOW);
    digitalWrite(PIN_GREEN_LED, LOW);
    digitalWrite(PIN_BLUE_LED, LOW);
  }
}
void controlLEDsOnly() {
  if (minDistance > 0 && minDistance < 10) {
    digitalWrite(PIN_RED_LED, HIGH);
    digitalWrite(PIN_GREEN_LED, LOW);
    digitalWrite(PIN_BLUE_LED, LOW);
  } 
  else if (minDistance >= 10 && minDistance < 20) {
    digitalWrite(PIN_RED_LED, LOW);
    digitalWrite(PIN_GREEN_LED, LOW);
    digitalWrite(PIN_BLUE_LED, HIGH);
  } 
  else if (minDistance >= 20 && minDistance <= 50) {
    digitalWrite(PIN_RED_LED, LOW);
    digitalWrite(PIN_GREEN_LED, HIGH);
    digitalWrite(PIN_BLUE_LED, LOW);
  }
  else {
    digitalWrite(PIN_RED_LED, LOW);
    digitalWrite(PIN_GREEN_LED, LOW);
    digitalWrite(PIN_BLUE_LED, LOW);
  }
}
