#include <LiquidCrystal.h>
#include <Servo.h>

LiquidCrystal lcd(12, 11, 7, 6, 5, 4);
Servo lockerServo;

// Entradas de senha (3 botoes numericos)
const int BTN_1 = A0;
const int BTN_2 = A1;
const int BTN_3 = A2;

// Monitoramento e atuacao
const int LDR_PIN = A4;
const int BUZZER_PIN = 8;
const int SERVO_PIN = 9;

// Interrupcao de emergencia (toggle trava/destrava)
const int BTN_EMERGENCY = 2; // INT0

// Senha padrao
const byte PASSWORD_LENGTH = 3;
const byte PASSWORD[PASSWORD_LENGTH] = {1, 2, 3};

enum LockerState {
  STATE_LOCKED,
  STATE_OPEN,
  STATE_BLOCKED,
  STATE_ALARM,
  STATE_EMERGENCY
};

LockerState currentState = STATE_LOCKED;

volatile bool emergencyToggleRequested = false;

byte enteredPin[PASSWORD_LENGTH];
byte enteredLength = 0;
byte failedAttempts = 0;

unsigned long stateTimestamp = 0;
unsigned long lastUiRefresh = 0;
unsigned long lastLdrCheck = 0;
unsigned long lightExposureStart = 0;
unsigned long lastAlarmBeep = 0;
unsigned long lastEmergencyToggleAt = 0;

int ldrBaseline = 0;

const int SERVO_LOCKED_ANGLE = 0;
const int SERVO_OPEN_ANGLE = 90;
const unsigned long OPEN_TIME_MS = 8000;
const unsigned long BLOCK_TIME_MS = 5000;
const unsigned long ALARM_TIME_MS = 10000;
const unsigned long LIGHT_CONFIRM_MS = 2000;
const int LDR_DELTA_TRIGGER = 150;
const unsigned long EMERGENCY_TOGGLE_DEBOUNCE_MS = 250;

const int inputPins[] = {BTN_1, BTN_2, BTN_3};
const byte INPUT_COUNT = sizeof(inputPins) / sizeof(inputPins[0]);
bool lastStableState[INPUT_COUNT];
bool lastReadState[INPUT_COUNT];
unsigned long lastDebounceAt[INPUT_COUNT];
const unsigned long DEBOUNCE_MS = 25;

void isrEmergencyToggle() {
  emergencyToggleRequested = true;
}

void setLockedPosition() {
  lockerServo.write(SERVO_LOCKED_ANGLE);
}

void setOpenPosition() {
  lockerServo.write(SERVO_OPEN_ANGLE);
}

void shortBeep(unsigned int durationMs) {
  tone(BUZZER_PIN, 2000, durationMs);
}

void tripleBeep() {
  for (byte i = 0; i < 3; i++) {
    tone(BUZZER_PIN, 1800, 120);
    delay(220);
  }
}

bool readButtonEvent(byte idx) {
  bool rawPressed = (digitalRead(inputPins[idx]) == LOW);

  if (rawPressed != lastReadState[idx]) {
    lastReadState[idx] = rawPressed;
    lastDebounceAt[idx] = millis();
  }

  if ((millis() - lastDebounceAt[idx]) >= DEBOUNCE_MS) {
    if (rawPressed != lastStableState[idx]) {
      lastStableState[idx] = rawPressed;
      if (rawPressed) {
        return true;
      }
    }
  }

  return false;
}

bool pinMatches() {
  if (enteredLength != PASSWORD_LENGTH) {
    return false;
  }

  for (byte i = 0; i < PASSWORD_LENGTH; i++) {
    if (enteredPin[i] != PASSWORD[i]) {
      return false;
    }
  }

  return true;
}

void clearEnteredPin() {
  enteredLength = 0;
}

void drawLockedScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Trava: FECHADA");
  lcd.setCursor(0, 1);
  lcd.print("Senha: ");

  for (byte i = 0; i < enteredLength; i++) {
    lcd.print('*');
  }
}

void drawOpenScreen() {
  unsigned long elapsed = millis() - stateTimestamp;
  unsigned long remaining = (elapsed >= OPEN_TIME_MS) ? 0 : (OPEN_TIME_MS - elapsed) / 1000;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Trava: ABERTA");
  lcd.setCursor(0, 1);
  lcd.print("Fecha em ");
  lcd.print(remaining);
  lcd.print("s   ");
}

void drawBlockedScreen() {
  unsigned long elapsed = millis() - stateTimestamp;
  unsigned long remaining = (elapsed >= BLOCK_TIME_MS) ? 0 : (BLOCK_TIME_MS - elapsed + 999) / 1000;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("BLOQUEADO!");
  lcd.setCursor(0, 1);
  lcd.print("Aguarde ");
  lcd.print(remaining);
  lcd.print("s  ");
}

void drawAlarmScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ALERTA DE LUZ");
  lcd.setCursor(0, 1);
  lcd.print("Verifique cofre");
}

void drawEmergencyScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("MODO EMERGENCIA");
  lcd.setCursor(0, 1);
  lcd.print("Aguardando reset");
}

void switchToState(LockerState nextState) {
  currentState = nextState;
  stateTimestamp = millis();

  if (nextState == STATE_LOCKED) {
    setLockedPosition();
    clearEnteredPin();
  }

  if (nextState == STATE_OPEN) {
    setOpenPosition();
    clearEnteredPin();
  }

  if (nextState == STATE_BLOCKED) {
    setLockedPosition();
    clearEnteredPin();
    tripleBeep();
  }

  if (nextState == STATE_ALARM) {
    setLockedPosition();
    clearEnteredPin();
    drawAlarmScreen();
  }

  if (nextState == STATE_EMERGENCY) {
    setLockedPosition();
    clearEnteredPin();
    tripleBeep();
  }

  // Forca refresh visual imediato na troca de estado.
  lastUiRefresh = 0;
}

void handleEmergencyRequests() {
  noInterrupts();
  bool toggleReq = emergencyToggleRequested;
  emergencyToggleRequested = false;
  interrupts();

  if (!toggleReq) {
    return;
  }

  if ((millis() - lastEmergencyToggleAt) < EMERGENCY_TOGGLE_DEBOUNCE_MS) {
    return;
  }

  lastEmergencyToggleAt = millis();

  if (currentState == STATE_EMERGENCY) {
    failedAttempts = 0;
    switchToState(STATE_LOCKED);
    return;
  }

  switchToState(STATE_EMERGENCY);
}

void handlePinInput() {
  for (byte i = 0; i < INPUT_COUNT; i++) {
    if (readButtonEvent(i) && enteredLength < PASSWORD_LENGTH) {
      enteredPin[enteredLength] = i + 1;
      enteredLength++;
      shortBeep(40);

      if (enteredLength == PASSWORD_LENGTH) {
        if (pinMatches()) {
          failedAttempts = 0;
          switchToState(STATE_OPEN);
        } else {
          failedAttempts++;
          shortBeep(250);
          clearEnteredPin();

          if (failedAttempts >= 3) {
            failedAttempts = 0;
            switchToState(STATE_BLOCKED);
          }
        }
      }
    }
  }
}

void handleLightMonitoring() {
  if ((millis() - lastLdrCheck) < 150) {
    return;
  }

  lastLdrCheck = millis();
  int currentLight = analogRead(LDR_PIN);

  if (currentLight > (ldrBaseline + LDR_DELTA_TRIGGER)) {
    if (lightExposureStart == 0) {
      lightExposureStart = millis();
    }

    if ((millis() - lightExposureStart) > LIGHT_CONFIRM_MS) {
      switchToState(STATE_ALARM);
      lightExposureStart = 0;
    }
  } else {
    lightExposureStart = 0;
  }
}

void updateScreen() {
  if ((millis() - lastUiRefresh) < 200) {
    return;
  }

  lastUiRefresh = millis();

  if (currentState == STATE_LOCKED) {
    drawLockedScreen();
    return;
  }

  if (currentState == STATE_OPEN) {
    drawOpenScreen();
    return;
  }

  if (currentState == STATE_BLOCKED) {
    drawBlockedScreen();
    return;
  }

  if (currentState == STATE_ALARM) {
    drawAlarmScreen();
    return;
  }

  if (currentState == STATE_EMERGENCY) {
    drawEmergencyScreen();
  }
}

void setup() {
  lcd.begin(16, 2);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LDR_PIN, INPUT);

  for (byte i = 0; i < INPUT_COUNT; i++) {
    pinMode(inputPins[i], INPUT_PULLUP);
    lastStableState[i] = false;
    lastReadState[i] = false;
    lastDebounceAt[i] = 0;
  }

  pinMode(BTN_EMERGENCY, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(BTN_EMERGENCY), isrEmergencyToggle, FALLING);

  lockerServo.attach(SERVO_PIN);
  setLockedPosition();

  long sum = 0;
  for (byte i = 0; i < 30; i++) {
    sum += analogRead(LDR_PIN);
    delay(15);
  }
  ldrBaseline = sum / 30;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Smart Locker IOT");
  lcd.setCursor(0, 1);
  lcd.print("Inicializando...");
  delay(1200);

  switchToState(STATE_LOCKED);
}

void loop() {
  handleEmergencyRequests();

  if (currentState == STATE_EMERGENCY) {
    updateScreen();
    return;
  }

  if (currentState == STATE_LOCKED) {
    handlePinInput();
    handleLightMonitoring();
  }

  if (currentState == STATE_OPEN) {
    if ((millis() - stateTimestamp) >= OPEN_TIME_MS) {
      switchToState(STATE_LOCKED);
    }
  }

  if (currentState == STATE_BLOCKED) {
    if ((millis() - stateTimestamp) >= BLOCK_TIME_MS) {
      switchToState(STATE_LOCKED);
    }
  }

  if (currentState == STATE_ALARM) {
    if ((millis() - lastAlarmBeep) > 350) {
      lastAlarmBeep = millis();
      tone(BUZZER_PIN, 2200, 100);
    }

    if ((millis() - stateTimestamp) >= ALARM_TIME_MS) {
      switchToState(STATE_LOCKED);
    }
  }

  updateScreen();
}
