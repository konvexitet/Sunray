// Ardumower Sunray 
// Copyright (c) 2013-2020 by Alexander Grau, Grau GmbH
// Licensed GPLv3 for open source use
// or Grau GmbH Commercial License for commercial use (http://grauonline.de/cms2/?page_id=153)


#include "SerialRobotDriver.h"
#include "../../config.h"

#define COMM  ROBOT



void SerialRobotDriver::begin(){
  COMM.begin(ROBOT_BAUDRATE);
  encoderTicksLeft = 0;
  encoderTicksRight = 0;
  chargeVoltage = 0;
  chargeCurrent = 0;  
  batteryVoltage = 0;
  mowCurr = 0;
  motorLeftCurr = 0;
  motorRightCurr = 0;
  batteryTemp = 0;
  triggeredLeftBumper = false;
  triggeredRightBumper = false;
  triggeredRain = false;
  triggeredStopButton = false;
  triggeredLift = false;
  motorFault = false;
  receivedEncoders = false;
  nextSummaryTime = 0;
  nextConsoleTime = 0;
  cmdMotorResponseCounter = 0;
  cmdSummaryResponseCounter = 0;
  cmdMotorCounter = 0;
  cmdSummaryCounter = 0;
}

void SerialRobotDriver::sendRequest(String s){
  byte crc = 0;
  for (int i=0; i < s.length(); i++) crc += s[i];
  s += F(",0x");
  if (crc <= 0xF) s += F("0");
  s += String(crc, HEX);  
  s += F("\r\n");             
  //CONSOLE.print(s);  
  //cmdResponse = s;
  COMM.print(s);  
}


void SerialRobotDriver::requestSummary(){
  String req;
  req += "AT+S";  
  sendRequest(req);
  cmdSummaryCounter++;
}

void SerialRobotDriver::requestMotorPwm(int leftPwm, int rightPwm, int mowPwm){
  String req;
  req += "AT+M,";
  req +=  rightPwm;      
  req += ",";
  req +=  leftPwm;    
  req += ",";  
  if (abs(mowPwm) > 0)
    req += "1";
  else
    req += "0";  
  sendRequest(req);
  cmdMotorCounter++;
}

void SerialRobotDriver::motorResponse(){
  if (cmd.length()<6) return;  
  int counter = 0;
  int lastCommaIdx = 0;
  for (int idx=0; idx < cmd.length(); idx++){
    char ch = cmd[idx];
    //Serial.print("ch=");
    //Serial.println(ch);
    if ((ch == ',') || (idx == cmd.length()-1)){
      int intValue = cmd.substring(lastCommaIdx+1, idx+1).toInt();
      int floatValue = cmd.substring(lastCommaIdx+1, idx+1).toFloat();      
      if (counter == 1){                            
        encoderTicksRight = intValue;  // ag
      } else if (counter == 2){
        encoderTicksLeft = intValue;   // ag
      } else if (counter == 3){
        encoderTicksMow = intValue;
      } else if (counter == 4){
        chargeVoltage = floatValue;
      } else if (counter == 5){
        triggeredLeftBumper = (intValue != 0);
      } else if (counter == 6){
        triggeredLift = (intValue != 0);
      } else if (counter == 7){
        triggeredStopButton = (intValue != 0);
      } 
      counter++;
      lastCommaIdx = idx;
    }    
  }
  if (triggeredStopButton){
    CONSOLE.println("STOPBUTTON");
  }
  cmdMotorResponseCounter++;
  receivedEncoders=true;
}


void SerialRobotDriver::summaryResponse(){
  if (cmd.length()<6) return;  
  int counter = 0;
  int lastCommaIdx = 0;
  for (int idx=0; idx < cmd.length(); idx++){
    char ch = cmd[idx];
    //Serial.print("ch=");
    //Serial.println(ch);
    if ((ch == ',') || (idx == cmd.length()-1)){
      int intValue = cmd.substring(lastCommaIdx+1, idx+1).toInt();
      int floatValue = cmd.substring(lastCommaIdx+1, idx+1).toFloat();      
      if (counter == 1){                            
        batteryVoltage = floatValue;
      } else if (counter == 2){
        chargeVoltage = floatValue;
      } else if (counter == 3){
        chargeCurrent = floatValue;
      } else if (counter == 4){
        triggeredLift = (intValue != 0);
      } else if (counter == 5){
        triggeredLeftBumper = (intValue != 0);
      } else if (counter == 6){
        triggeredRain = (intValue != 0);
      } else if (counter == 7){
        motorFault = (intValue != 0);
      } else if (counter == 8){
        mowCurr = floatValue;
      } else if (counter == 9){
        motorLeftCurr = floatValue;
      } else if (counter == 10){
        motorRightCurr = floatValue;
      } else if (counter == 11){
        batteryTemp = floatValue;
      } 
      counter++;
      lastCommaIdx = idx;
    }    
  }
  cmdSummaryResponseCounter++;
  //CONSOLE.print("batteryTemp=");
  //CONSOLE.println(batteryTemp);
}

// process response
void SerialRobotDriver::processResponse(bool checkCrc){
  cmdResponse = "";      
  if (cmd.length() < 4) return;
  byte expectedCrc = 0;
  int idx = cmd.lastIndexOf(',');
  if (idx < 1){
    if (checkCrc){
      CONSOLE.println("SerialRobot: CRC ERROR");
      return;
    }
  } else {
    for (int i=0; i < idx; i++) expectedCrc += cmd[i];  
    String s = cmd.substring(idx+1, idx+5);
    int crc = strtol(s.c_str(), NULL, 16);  
    if (expectedCrc != crc){
      if (checkCrc){
        CONSOLE.print("SerialRobot: CRC ERROR");
        CONSOLE.print(crc,HEX);
        CONSOLE.print(",");
        CONSOLE.print(expectedCrc,HEX);
        CONSOLE.println();
        return;  
      }      
    } else {
      // remove CRC
      cmd = cmd.substring(0, idx);
      //CONSOLE.print("SerialRobot resp:");
      //CONSOLE.println(cmd);
    }    
  }     
  if (cmd[0] == 'M') motorResponse();
  if (cmd[0] == 'S') summaryResponse();
}


// process console input
void SerialRobotDriver::processComm(){
  char ch;      
  if (COMM.available()){
    //battery.resetIdle();  
    while ( COMM.available() ){               
      ch = COMM.read();          
      if ((ch == '\r') || (ch == '\n')) {        
        //CONSOLE.println(cmd);
        processResponse(true);              
        //CONSOLE.print(cmdResponse);    
        cmd = "";
      } else if (cmd.length() < 500){
        cmd += ch;
      }
    }
  }     
}


void SerialRobotDriver::run(){
  processComm();
  if (millis() > nextSummaryTime){
    nextSummaryTime = millis() + 500;
    requestSummary();
  }
  if (millis() > nextConsoleTime){
    nextConsoleTime = millis() + 1000;
    if ( (abs(cmdMotorResponseCounter) < 17) || (cmdSummaryResponseCounter == 0) ){
      CONSOLE.print("WARN: SerialRobot unmet communication frequency: motorFreq=");
      CONSOLE.print(cmdMotorCounter);
      CONSOLE.print("/");
      CONSOLE.print(cmdMotorResponseCounter);
      CONSOLE.print("  summaryFreq=");
      CONSOLE.print(cmdSummaryCounter);
      CONSOLE.print("/");
      CONSOLE.println(cmdSummaryResponseCounter);
    }   
    cmdMotorCounter=cmdMotorResponseCounter=cmdSummaryCounter=cmdSummaryResponseCounter=0;
  }
}


// ------------------------------------------------------------------------------------

SerialMotorDriver::SerialMotorDriver(SerialRobotDriver &sr): serialRobot(sr){
} 

void SerialMotorDriver::begin(){
  lastEncoderTicksLeft=0;
  lastEncoderTicksRight=0;
  started = false;       
}

void SerialMotorDriver::run(){
}

void SerialMotorDriver::setMotorPwm(int leftPwm, int rightPwm, int mowPwm){  
  serialRobot.requestMotorPwm(leftPwm, rightPwm, mowPwm);
}

void SerialMotorDriver::getMotorFaults(bool &leftFault, bool &rightFault, bool &mowFault){
  leftFault = serialRobot.motorFault;
  rightFault = serialRobot.motorFault;
  if (serialRobot.motorFault){
    CONSOLE.println("serialRobot: motorFault");
  }
  mowFault = false;
}

void SerialMotorDriver::resetMotorFaults(){
  CONSOLE.println("serialRobot: resetting motor fault");
  //serialRobot.requestMotorPwm(1, 1, 0);
  //delay(1);
  //serialRobot.requestMotorPwm(0, 0, 0);
}

void SerialMotorDriver::getMotorCurrent(float &leftCurrent, float &rightCurrent, float &mowCurrent) {  
  //leftCurrent = 0.5;
  //rightCurrent = 0.5;
  //mowCurrent = 0.8;
  leftCurrent = serialRobot.motorLeftCurr;
  rightCurrent = serialRobot.motorRightCurr;
  mowCurrent = serialRobot.mowCurr;
}

void SerialMotorDriver::getMotorEncoderTicks(int &leftTicks, int &rightTicks, int &mowTicks){
  if (!started){
    if (serialRobot.receivedEncoders){
      started = true;
      lastEncoderTicksLeft = serialRobot.encoderTicksLeft;
      lastEncoderTicksRight = serialRobot.encoderTicksRight;
    }
  } 
  leftTicks = serialRobot.encoderTicksLeft - lastEncoderTicksLeft;
  rightTicks = serialRobot.encoderTicksRight - lastEncoderTicksRight;
  lastEncoderTicksLeft = serialRobot.encoderTicksLeft;
  lastEncoderTicksRight = serialRobot.encoderTicksRight;
  mowTicks = 0;
}


// ------------------------------------------------------------------------------------

SerialBatteryDriver::SerialBatteryDriver(SerialRobotDriver &sr) : serialRobot(sr){
}

void SerialBatteryDriver::begin(){
}

void SerialBatteryDriver::run(){
}    

float SerialBatteryDriver::getBatteryVoltage(){
  return serialRobot.batteryVoltage;
}

float SerialBatteryDriver::getChargeVoltage(){
  return serialRobot.chargeVoltage;
}
    
float SerialBatteryDriver::getChargeCurrent(){
  return serialRobot.chargeCurrent;
} 

void SerialBatteryDriver::enableCharging(bool flag){
}

void SerialBatteryDriver::keepPowerOn(bool flag){
}


// ------------------------------------------------------------------------------------

SerialBumperDriver::SerialBumperDriver(SerialRobotDriver &sr): serialRobot(sr){
}

void SerialBumperDriver::begin(){
}

void SerialBumperDriver::run(){

}

bool SerialBumperDriver::obstacle(){
  return (serialRobot.triggeredLeftBumper || serialRobot.triggeredRightBumper); 
}

void SerialBumperDriver::getTriggeredBumper(bool &leftBumper, bool &rightBumper){
  leftBumper = serialRobot.triggeredLeftBumper;
  rightBumper = serialRobot.triggeredRightBumper;
}  	  		    


// ------------------------------------------------------------------------------------


SerialStopButtonDriver::SerialStopButtonDriver(SerialRobotDriver &sr): serialRobot(sr){
}

void SerialStopButtonDriver::begin(){
}

void SerialStopButtonDriver::run(){

}

bool SerialStopButtonDriver::triggered(){
  return (serialRobot.triggeredStopButton); 
}

// ------------------------------------------------------------------------------------


SerialRainSensorDriver::SerialRainSensorDriver(SerialRobotDriver &sr): serialRobot(sr){
}

void SerialRainSensorDriver::begin(){
}

void SerialRainSensorDriver::run(){

}

bool SerialRainSensorDriver::triggered(){
  return (serialRobot.triggeredRain); 
}

