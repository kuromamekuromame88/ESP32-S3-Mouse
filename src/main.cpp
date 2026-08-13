#include <Arduino.h>
#include <Preferences.h>

#include "switch_ESP32.h"

NSGamepad Gamepad;
Preferences prefs;

// ===== ピン定義 =====
#define MStart1 42

#define LX 18
#define LY 17
#define RX 15
#define RY 16

#define LB 39
#define RB 40

#define UP 3
#define RIGHT 9
#define DOWN 10
#define LEFT 11

#define ZL 35
#define ZR 36
#define L 2
#define R 1

#define PLUS 38
#define MINUS 37

#define BTN_B 5
#define BTN_A 4
#define BTN_X 7
#define BTN_Y 6

// ===== スティック変換 =====
uint8_t axis(int pin){
  return map(analogRead(pin), 0, 4095, 0, 255);
}

// ===== マクロ用 =====
String macro = "";
bool recording = false;
bool playing   = false;

unsigned long lastChange = 0;
unsigned long pressTime  = 0;

uint16_t lastState     = 0;
uint16_t macroButtons = 0;

// ===== ボタン読み取り =====
uint16_t readButtons(){
  uint16_t s = 0;

  if(!digitalRead(BTN_Y)) s |= 1 << NSButton_Y;
  if(!digitalRead(BTN_B)) s |= 1 << NSButton_B;
  if(!digitalRead(BTN_A)) s |= 1 << NSButton_A;
  if(!digitalRead(BTN_X)) s |= 1 << NSButton_X;

  if(!digitalRead(ZL)) s |= 1 << NSButton_LeftTrigger;
  if(!digitalRead(ZR)) s |= 1 << NSButton_RightTrigger;

  if(!digitalRead(L))  s |= 1 << NSButton_LeftThrottle;
  if(!digitalRead(R))  s |= 1 << NSButton_RightThrottle;

  if(!digitalRead(MINUS)) s |= 1 << NSButton_Minus;
  if(!digitalRead(PLUS))  s |= 1 << NSButton_Plus;

  if(!digitalRead(LB)) s |= 1 << NSButton_LeftStick;
  if(!digitalRead(RB)) s |= 1 << NSButton_RightStick;

  return s;
}

// ===== マクロ記録 =====
void recordState(uint16_t s){
  macro += String(s) + "," + String(millis() - lastChange) + ",";
  lastChange = millis();
}

void saveMacro(){
  prefs.begin("macro", false);
  prefs.putString("data", macro);
  prefs.end();
}

void loadMacro(){
  prefs.begin("macro", true);
  macro = prefs.getString("data", "");
  prefs.end();
}

// ===== setup =====
void setup(){
  int pins[] = {
    MStart1, LB, RB,
    UP, RIGHT, DOWN, LEFT,
    ZL, ZR, L, R,
    PLUS, MINUS,
    BTN_A, BTN_B, BTN_X, BTN_Y
  };

  for(int p : pins) pinMode(p, INPUT_PULLUP);

  analogReadResolution(12);
  loadMacro();

  Gamepad.begin();
  USB.begin();
}

// ===== loop =====
void loop(){

  uint16_t manual = readButtons();

  // ==== MStart 判定 ====
  if(!digitalRead(MStart1)){
    if(!pressTime) pressTime = millis();
  }
  else if(pressTime){
    unsigned long held = millis() - pressTime;

    if(held > 1000){
      // 録画トグル
      recording = !recording;
      if(recording){
        macro = "";
        lastState  = manual;
        lastChange = millis();
      }else{
        recordState(manual);
        saveMacro();
      }
    }else{
      // マクロ再生
      if(macro.length() > 0){
        playing = true;
      }
    }
    pressTime = 0;
  }

  // ==== 録画処理 ====
  if(recording && manual != lastState){
    recordState(lastState);
    lastState = manual;
  }

  // ==== マクロ再生 ====
  static int idx = 0;
  static unsigned long waitUntil = 0;

  if(playing && millis() >= waitUntil){
    int c1 = macro.indexOf(",", idx);
    if(c1 == -1){
      playing = false;
      idx = 0;
      macroButtons = 0;
    }else{
      macroButtons = macro.substring(idx, c1).toInt();
      idx = c1 + 1;

      int c2 = macro.indexOf(",", idx);
      int delayMs = macro.substring(idx, c2).toInt();
      idx = c2 + 1;

      waitUntil = millis() + delayMs;
    }
  }

  // ==== 手動 + マクロ合成 ====
  uint16_t merged = manual | macroButtons;

  // ==== スティック ====
  Gamepad.leftXAxis(255 - axis(LX));
  Gamepad.leftYAxis(255 - axis(LY));
  Gamepad.rightXAxis(axis(RX));
  Gamepad.rightYAxis(axis(RY));

  // ==== DPad ====
  Gamepad.dPad(
    !digitalRead(UP),
    !digitalRead(DOWN),
    !digitalRead(LEFT),
    !digitalRead(RIGHT)
  );

  // ==== ボタン送信 ====
  Gamepad.buttons(merged);
  Gamepad.loop();

  delay(5);
}

