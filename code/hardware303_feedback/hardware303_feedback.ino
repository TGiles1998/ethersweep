#include <Ethernet.h>
#include <EthernetUdp.h>
#include <IPAddress.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include "SSD1306Ascii.h"
#include "SSD1306AsciiAvrI2c.h"
#include <Wire.h>
#include <AS5600.h>

#define SYS_VOL   3.3

AMS_5600 ams5600;


const int stepPin = 7;
const int dirPin = 6;
const int enablePin = 5;
//const  int sleepPin = 8;
//const  int resetPin = 9;

const int m0Pin = A1;
const int m1Pin = A0;
const int diagPin = A2;
const int faultPin = A3;
const int voltDetectPin = A6;
const int endStopPin = 3;
const int eStopPin = 4;

int driveMode = 0;
int motorSpeed = 0;
int motorSteps = 0;
int motorDirection = 0;
int motorStepMode = 0;
int motorHold = 0;
int feedback = 0;

boolean eStopActive = false;

boolean endStopPressed = false;
boolean endStopped = false;

boolean motorEnabled = false;

boolean jobDone = true;

String ip_mode = "DHCP";

#define I2C_ADDRESS 0x3C
SSD1306AsciiAvrI2c oled;

/*
  #define STATIC 0

  #if STATIC
  String ip_mode = "STATIC";
  static byte myip[] = { 192, 168, 1, 110 };
  static byte gwip[] = { 192, 168, 1, 1 };
  #endif
*/

byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xEE
};
IPAddress ip(192, 168, 1, 178);

unsigned int localPort = 8888;      // local port to listen on
unsigned int hostPort = 8889;

// buffers for receiving and sending data

const int packetBufferSize = 128;
char packetBuffer[packetBufferSize];  // buffer to hold incoming packet,
char ReplyBuffer[] = "";

// An EthernetUDP instance to let us send and receive packets over UDP
EthernetUDP Udp;

String received_data = "0";

static uint32_t timer;
unsigned long previousMillis = 0;

void setup() {
  Serial.begin(9600);
  Serial.println("Welcome to ethersweep...");
  Serial.print("initializing...");
  pinMode(stepPin, OUTPUT);
  pinMode(dirPin, OUTPUT);
  pinMode(enablePin, OUTPUT);
  // pinMode(sleepPin, OUTPUT);
  // pinMode(resetPin, OUTPUT);
  pinMode(m0Pin, OUTPUT);
  pinMode(m1Pin, OUTPUT);
  pinMode(diagPin, INPUT);
  pinMode(endStopPin, INPUT);
  pinMode(eStopPin, INPUT);

  digitalWrite(dirPin, LOW);

  oled.begin(&Adafruit128x32, I2C_ADDRESS);
  oled.setFont(Adafruit5x7);
  oled.clear();

  if (ams5600.detectMagnet() == 0 ) {
    while (1) {
      if (ams5600.detectMagnet() == 1 ) {
        break;
      }
      else {
        Serial.println("ERROR not magent detected!");
      }
      delay(1000);
    }
  }

  drawDisplay();

  // digitalWrite(sleepPin, HIGH);
  // digitalWrite(resetPin, HIGH);
  disableMotor();
  // setStepMode(2);

  Ethernet.begin(mac, ip);

  // Check for Ethernet hardware present
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    // Serial.println("Ethernet hardware not found. Critical ERROR");
    while (true) {
      delay(1); // do nothing, no point running without Ethernet hardware
    }
  }
  if (Ethernet.linkStatus() == LinkOFF) {
    // Serial.println("Ethernet cable is not connected.");
  }

  // start UDP
  // Serial.println(ip);
  drawDisplay();
  Udp.begin(localPort);

  Serial.println("done");
}

void loop() {

  getButtonStates();


  // ether.packetLoop(ether.packetReceive());

  // char udpChars[received_data.length() + 1];
  // received_data.toCharArray(udpChars, received_data.length() + 1);
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= 100) {
    drawDisplay();
    previousMillis = currentMillis;
  }

  int packetSize = Udp.parsePacket();
  if (packetSize) {
    // Serial.print("Received packet of size ");
    // Serial.println(packetSize);
    // Serial.print("From ");
    IPAddress remote = Udp.remoteIP();
    // Serial.print(", port ");
    // Serial.println(Udp.remotePort());

    // read the packet into packetBufffer
    Udp.read(packetBuffer, packetBufferSize);
    // Serial.println("Contents:");
    // Serial.println(packetBuffer);

    // send a reply to the IP address and port that sent us the packet we received
    //Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
    //Udp.write(ReplyBuffer);
    //Udp.endPacket();

    jobDone = false;
  }



  if (jobDone == false) {
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(packetBuffer);

    String drive_mode = root["drivemode"];
    drive_mode = root["drivemode"].as<String>();

    String raw_steps = root["steps"];
    raw_steps = root["steps"].as<String>();

    String raw_speed = root["speed"];
    raw_speed = root["speed"].as<String>();

    String raw_direction = root["direction"];
    raw_direction = root["direction"].as<String>();

    String raw_step_mode = root["stepmode"];
    raw_step_mode = root["stepmode"].as<String>();

    String raw_hold = root["hold"];
    raw_hold = root["hold"].as<String>();

    String raw_answer = root["answer"];
    raw_answer = root["answer"].as<String>();

    driveMode = drive_mode.toInt();
    motorSteps = raw_steps.toInt();
    motorSpeed = raw_speed.toInt();
    motorDirection = raw_direction.toInt();
    motorStepMode = raw_step_mode.toInt();
    motorHold = raw_hold.toInt();
    feedback = raw_answer.toInt();

    switch (driveMode) {
      case 0:
        driveMotor(motorSteps, motorSpeed, motorDirection, motorStepMode, motorHold);
        Serial.println("motor driven");
        break;
      case 1:
        homeMotor(motorSteps, motorSpeed, motorDirection, motorStepMode, motorHold);
        Serial.println("motor homed");
        break;
      case 2:
        homeMotor(motorSteps, motorSpeed, motorDirection, motorStepMode, motorHold);
        getButtonStates();
        sendInfo(convertRawAngleToDegrees(ams5600.getRawAngle()), motorHold, endStopPressed, eStopActive, getVoltage());
        Serial.println("sent feedback");
        break;


    }

    /*
      Serial.print("raw_data: ");
      Serial.println(received_data);
      Serial.print("steps: ");
      Serial.println(raw_steps);
      Serial.print("speed: ");
      Serial.println(raw_speed);
      Serial.print("dir: ");
      Serial.println(raw_direction);
      Serial.print("stepMode: ");
      Serial.println(raw_step_mode);
    */


    jobDone = true;
  }


}

void homeMotor(int motorSteps, int motorSpeed, bool motorDirection, int motorStepMode, int hold) {
  int homingState = 0;
  const int homing_runs = 3;
  while (1) {
    if (homingState == homing_runs) {
      break;
    }
    getButtonStates();

    if (endStopPressed) {
      if (homingState < homing_runs - 1) {
        driveMotor(motorSteps, motorSpeed  * (homingState + 1), !motorDirection, motorStepMode, 1);
      }
      homingState += 1;
    }
    driveMotor(1, motorSpeed * (2 * homingState + 1), motorDirection, motorStepMode, 1);
  }
  // driveMotor(0, 0, 0, 0, 1); // set holding state
}


void driveMotor(int motorSteps, int motorSpeed, bool motorDirection, int motorStepMode, int hold) {
  if (!eStopActive) {
    enableMotor();
    setStepMode(motorStepMode);
    if (motorDirection == 1) {
      digitalWrite(dirPin, HIGH);
    }
    else if (motorDirection == 0) {
      digitalWrite(dirPin, LOW);
    }
    motorSpeed = constrain(motorSpeed, 0, 10000);
    for (int i = 0; i < motorSteps; i++) {
      digitalWrite(stepPin, LOW);
      delayMicroseconds(motorSpeed);
      digitalWrite(stepPin, HIGH);
      delayMicroseconds(motorSpeed);
    }
    if (hold == 0) {
      disableMotor();
    }

  }
}


void setStepMode(int mode) {
  // sets DRV8825 step modes: full step to 1/32 step mode
  switch (mode) {
    // MS2, MS1: 00: 1/8, 11: 1/16
    case 2:
      digitalWrite(m0Pin, HIGH);
      digitalWrite(m1Pin, LOW);
      //digitalWrite(m2Pin, LOW);
      break;
    case 4:
      digitalWrite(m0Pin, LOW);
      digitalWrite(m1Pin, HIGH);
      //digitalWrite(m2Pin, LOW);
      break;
    case 8:
      digitalWrite(m0Pin, LOW);
      digitalWrite(m1Pin, LOW);
      //digitalWrite(m2Pin, LOW);
      break;
    case 16:
      digitalWrite(m0Pin, HIGH);
      digitalWrite(m1Pin, HIGH);
      //digitalWrite(m2Pin, HIGH);
      break;
    default:
      digitalWrite(m0Pin, HIGH);
      digitalWrite(m1Pin, HIGH);
      //digitalWrite(m2Pin, HIGH);
      break;
  }
}



void disableMotor() {
  digitalWrite(enablePin, HIGH); // motor disabled
  motorEnabled = false;

}

void enableMotor() {
  if (!eStopActive) {
    digitalWrite(enablePin, LOW); // motor enabled
    motorEnabled = true;
  }
  else {
    disableMotor();
  }
}

boolean checkMotorDriverFailure() {
  return digitalRead(diagPin);
}

void drawDisplay() {
  oled.clear();
  oled.set1X();
  oled.println("ethersweep");
  oled.print("Mode: ");
  oled.println(ip_mode);
  oled.print(getVoltage());
  oled.print("V | ");
  oled.println(String(round(convertRawAngleToDegrees(ams5600.getRawAngle())), DEC) + "°");
  oled.print("IP: ");
  oled.print(displayAddress(ip));
}

float getVoltage() {
  float vin = (analogRead(voltDetectPin) * 3.3) / 1024.0 / (1000.0 / (10000.0 + 1000.0));
  return vin;
}

String displayAddress(IPAddress address)
{
  return String(address[0]) + "." +
         String(address[1]) + "." +
         String(address[2]) + "." +
         String(address[3]);
}

float convertRawAngleToDegrees(word newAngle)
{
  /* Raw data reports 0 - 4095 segments, which is 0.087 of a degree */
  float retVal = newAngle * 0.087;
  float ang = retVal;
  return ang;
}

void getButtonStates() {
  endStopPressed = !digitalRead(endStopPin); // END Stop pressed

  eStopActive = !digitalRead(eStopPin);

}

void sendInfo(float position, byte hold, byte home, byte stop, float voltage) {
  // {'fail': 0, 'position': 512.12, 'hold': 1, 'home': 0, 'stop': 0, 'voltage': 14.2, 'DHCP': 1, 'mac': 'DEADBEEFFEED'}
  StaticJsonBuffer<256> jsonBuffer;
  JsonObject& infoJson = jsonBuffer.createObject();
  // infoJson["fail"] = fail;
  infoJson["position"] = position;
  infoJson["hold"] = hold;
  infoJson["home"] = home;
  infoJson["stop"] = stop;
  infoJson["voltage"] = voltage;
  // infoJson["dhcp"] = dhcp;
  // infoJson["mac"] = mac;

  Udp.beginPacket(Udp.remoteIP(), hostPort);
  infoJson.printTo(Udp);
  // Udp.write(ReplyBuffer);
  Udp.endPacket();

}