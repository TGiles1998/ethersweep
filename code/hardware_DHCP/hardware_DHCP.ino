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
const int ledPin = 2;

int driveMode = 0;
int motorSpeed = 0;
int motorSteps = 0;
int motorDirection = 0;
int motorStepMode = 0;
int motorHold = 0;

boolean endStopped = false;

boolean eStopActive = false;
boolean endStopPressed = false;
boolean motorEnabled = false;
boolean jobDone = true;

boolean eStopActiveLast = false;
boolean endStopPressedLast = false;
boolean jobDoneLast = true;




#define I2C_ADDRESS 0x3C
SSD1306AsciiAvrI2c oled;


#define STATIC 0
#define DEBUG 1

#if STATIC
String ip_mode = "STATIC";
static byte myip[] = { 192, 168, 1, 111 };
static byte gwip[] = { 192, 168, 1, 1 };
#else
String ip_mode = "DHCP";
#endif



byte mac[] = {
  0xDE, 0xAD, 0xBC, 0xEA, 0xFE, 0xEE
};
IPAddress ip(192, 168, 1, 111);


unsigned int localPort = 8888;      // local port to listen on
// unsigned int remotePort = 8889;

// buffers for receiving and sending data

const int packetBufferSize = 128;
char packetBuffer[packetBufferSize];  // buffer to hold incoming packet,

// An EthernetUDP instance to let us send and receive packets over UDP
EthernetUDP Udp;

String received_data = "0";

static uint32_t timer;
unsigned long previousMillis = 0;

void setup() {
  Serial.begin(9600);
  Serial.print("initializing...");
  pinMode(stepPin, OUTPUT);
  pinMode(dirPin, OUTPUT);
  pinMode(enablePin, OUTPUT);
  // pinMode(sleepPin, OUTPUT);
  // pinMode(resetPin, OUTPUT);
  pinMode(m0Pin, OUTPUT);
  pinMode(m1Pin, OUTPUT);
  pinMode(ledPin, OUTPUT);

  pinMode(diagPin, INPUT);
  pinMode(endStopPin, INPUT);
  pinMode(eStopPin, INPUT);

  digitalWrite(dirPin, LOW);


  oled.begin(&Adafruit128x32, I2C_ADDRESS);
  oled.setFont(Adafruit5x7);
  oled.clear();
  setupDisplay();

  if (ams5600.detectMagnet() == 0 ) {
    while (1) {
      if (ams5600.detectMagnet() == 1 ) {
        break;
      }
      else {
        Serial.println("magnet err");
      }
      delay(1000);
    }
  }

  // drawDisplay();

  // digitalWrite(sleepPin, HIGH);
  // digitalWrite(resetPin, HIGH);
  disableMotor();
  // setStepMode(2);



#if STATIC
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
#else
  if (Ethernet.begin(mac) == 0) {
    Serial.println("DHCP error");
    // Check for Ethernet hardware present
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
      Serial.println("eth hardware err");
      while (true) {
        delay(1); // do nothing, no point running without Ethernet hardware
      }
    }
    if (Ethernet.linkStatus() == LinkOFF) {
      Serial.println("eth cable err");
    }
    // try to congifure using IP address instead of DHCP:
    Ethernet.begin(mac, ip);
  } else {
    Serial.print("DHCP ok: ");
    Serial.println(Ethernet.localIP());
  }
#endif

  // start UDP
  // Serial.println(ip);
  // drawDisplay();
  Udp.begin(localPort);
  delay(500);
  Serial.println("ok");
  initializeDisplay();
}

void loop() {
  getButtonStates();
  
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
  else {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= 100) {
      drawDisplay(); // time killer!
      previousMillis = currentMillis;
    }
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

    driveMode = drive_mode.toInt();
    motorSteps = raw_steps.toInt();
    motorSpeed = raw_speed.toInt();
    motorDirection = raw_direction.toInt();
    motorStepMode = raw_step_mode.toInt();
    motorHold = raw_hold.toInt();
    
    
    switch (driveMode) {
      case 0:
        driveMotor(motorSteps, motorSpeed, motorDirection, motorStepMode, motorHold);
        // Serial.println("motor driven");
        break;
      case 1:
        homeMotor(motorSteps, motorSpeed, motorDirection, motorStepMode, motorHold);
        // Serial.println("motor homed");
        break;
    }


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
  drawDisplay();
  digitalWrite(ledPin, HIGH);
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
  digitalWrite(ledPin, LOW);
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

void setupDisplay() {
  oled.setFont(System5x7);
  oled.set2X();
  oled.clear();
  oled.println("ethersweep");
  oled.set1X();
  oled.println(" ");
  oled.println("       v3.0.4");
  // rows = oled.fontRows();
}


void initializeDisplay() {
  oled.setFont(System5x7);
  oled.clear();
  oled.println("ethersweep    v3.0.4");
  oled.println("00.0V | DHCP | 000.0°");
  oled.println("END   | STOP |  ACT");
  oled.println("IP: " + displayAddress(Ethernet.localIP()));
  // rows = oled.fontRows();
}


void drawDisplay() {
  getButtonStates();

  oled.setInvertMode(0);
  float voltage = getVoltage();
  oled.clearField(0, 1, 3);
  //if (voltage < 10.0) oled.print("");
  oled.print(getVoltage(), 1);

  oled.clearField(90, 1, 3);

  oled.print(getEncoderAngle(), 1);

  if (endStopPressed != endStopPressedLast) {
    oled.clearField(0, 2, 4);
    if (endStopPressed) oled.setInvertMode(1);
    oled.print("END");
  }

  if (eStopActive != eStopActiveLast) {
    oled.clearField(47, 2, 4);
    if (eStopActive) oled.setInvertMode(1);
    oled.print("STOP");
  }

  if (jobDone != jobDoneLast) {
    oled.clearField(96, 2, 4);
    if (!jobDone) oled.setInvertMode(1);
    oled.print("ACT");
  }

  endStopPressedLast = endStopPressed;
  eStopActiveLast = eStopActive;
  jobDoneLast = jobDone;
}



float getVoltage() {
  float vin = (analogRead(A6) * 3.3) / 1024.0 / (1000.0 / (10000.0 + 1000.0)) ;
  return vin;
}

String displayAddress(IPAddress address)
{
  return String(address[0]) + "." +
         String(address[1]) + "." +
         String(address[2]) + "." +
         String(address[3]);
}

float getEncoderAngle() {
  float normalAngle;
  float rawAngle = ams5600.getRawAngle() - 32768.0;
  /* Raw data reports 0 - 4095 segments, which is 0.087 of a degree */
  normalAngle = rawAngle * 0.087;
  return normalAngle;
}


void getButtonStates() {
  endStopPressed = !digitalRead(endStopPin); // END Stop pressed
  eStopActive = !digitalRead(eStopPin);
}
