//Utilitys to USE MF4015 with CAN
// abstract:
// This library is commands for MF4015 ,using with arduino-mcp2515
// refers:
// SHANGHAI LINGKONG TECHNOLOGY CO.,LTD CAN PROTOCOL V2.35 
// http://en.lkmotor.cn/upload/20230706100134f.pdf
// arduino-mcp2515
// https://github.com/autowp/arduino-mcp2515
// Step 1: get encode date to get Steering angle.
// Step 2: Set torque to use Steering FFB.

#define MCP2515_CS 10

#include <mcp2515.h>
MCP2515 mcp2515_mf(MCP2515_CS);
struct can_frame MFcanMsg ;

#define CmdMotorOff 	0x80	//Motor off command
#define CmdMotorOn  	0x88	//Motor on command
#define CmdMotorStop	0x81	//Motor stop command
#define CmdOpnLoop  	0xA0	//Open loop control command
#define CmdClsTrqu  	0xA1	//Torque closed loop control command
#define CmdReadEnc  	0x90	//Read encoder command
#define CmdReadStat1	0x9A	//Read motor state 1 and error state command
#define CmdClearErr 	0x9B	//Clear motor error state
#define CmdReadStat2	0x9C	//Read motor state 2 command


//Setup MF on CAN-bus
void SetupMCP2515_MF(){
  //to setup mcp2515
  mcp2515_mf.reset();
  mcp2515_mf.setBitrate(CAN_500KBPS, MCP_8MHZ); //Clock 8MHz, 500kBPS
  //mcp2515_mf.setBitrate(CAN_500KBPS);
  mcp2515_mf.setNormalMode();
}

#define MF_ID 1		//
// The command frame and reply frame message format are as follows:
// Identifier:0x140 + ID(1~32)
// Frame format: data frame
// Frame type: standard frame
// DLC:8bytes
uint32_t MFcanid = 0x140 + MF_ID;
uint8_t MFcandlc = 8;	//DLC:8bytes
uint8_t MFdate[8];

//--------------------------------------
// for date[1] ~ date[7] = 0x00 
void MF_Cmd00(uint8_t MF_Cmd){
  MFcanMsg.can_id  = MFcanid;
  MFcanMsg.can_dlc = MFcandlc;
  MFcanMsg.data[0] = MF_Cmd;
  MFcanMsg.data[1] = 0x00;  MFcanMsg.data[2] = 0x00;  MFcanMsg.data[3] = 0x00;  
  MFcanMsg.data[4] = 0x00;  MFcanMsg.data[5] = 0x00;  MFcanMsg.data[6] = 0x00;  
  MFcanMsg.data[7] = 0x00;
  mcp2515_mf.sendMessage(&MFcanMsg);

}
//Motor off command
void MF_MotorOff(){ 
  MF_Cmd00(CmdMotorOff);
}
//Motor on command
void MF_MotorOn(){
  MF_Cmd00(CmdMotorOn);
}
//Motor stop command
void MF_MotorStop(){
  MF_Cmd00(CmdMotorStop);
}
//Read encoder command
void MF_ReadEncode(){
  MF_Cmd00(CmdReadEnc);
}
//Read motor state 1 and error state command
void MF_ReadStat1(){
  MF_Cmd00(CmdReadStat1);
}
//Read motor state 2 command
void MF_ReadStat2(){
  MF_Cmd00(CmdReadStat2);
}
//Clear motor error state
void MF_ClearErr(){
  MF_Cmd00(CmdClearErr);
}
//---------------------------------
// Torque closed loop control command.
// (the command can only be applied to MF,MH,MG series)
// Host send commands to control the torque current output,
// iqControl value is int16_t, range is -2048 ~ 2048, 
// corresponding MF motor actual torque current range is -16.5A ~ 16.5A, 
// corresponding MG motor actual torque current range is -33A ~ 33A. 
// The bus current and the actual torque of the motor vary from motor to motor.
//---------------------------------
void MF_SetTorque(int16_t iTorquVal){
  if(iTorquVal < -2048) iTorquVal = -2048;
  else if (iTorquVal > 2048) iTorquVal = 2048;
  uint8_t CurrentLowByte = (uint8_t)(iTorquVal & 0x00FF);
  uint8_t CurrentHiByte  = (uint8_t)((iTorquVal >> 8)& 0x00FF) ;
  MFcanMsg.can_id  = MFcanid;
  MFcanMsg.can_dlc = MFcandlc;
  MFcanMsg.data[0] = CmdClsTrqu;
  MFcanMsg.data[1] = 0x00;
  MFcanMsg.data[2] = 0x00;
  MFcanMsg.data[3] = 0x00;
  MFcanMsg.data[4] = CurrentLowByte;
  MFcanMsg.data[5] = CurrentHiByte ;
  MFcanMsg.data[6] = 0x00;
  MFcanMsg.data[7] = 0x00;

  mcp2515_mf.sendMessage(&MFcanMsg);

}
uint16_t EncValue;
// for public access
uint16_t getEncVal(){
  return EncValue;
}

// get Encoder Value for return of "Read encoder command"".
// call after a CAN Freame recieved.
uint16_t getEncValue(struct can_frame MF_CAN_MSG){
  uint8_t EncLow = MF_CAN_MSG.data[2];
  uint8_t EncHi  = MF_CAN_MSG.data[3];
  uint16_t retVal = EncHi * 256;
  retVal = retVal +  EncLow;
  return retVal;
}

// get Encoder Value for return of "Open loop control command(0xA0)" or 
// "Torque control command(0xA1)"
// call after a CAN Freame recieved.

uint16_t getEncValPow(struct can_frame MF_CAN_MSG){
  //--- reserve ---
  //uint8_t Temperature = MF_CAN_MSG.data[1];
  //uint8_t TorqCurrLow = MF_CAN_MSG.data[2];
  //uint8_t TorqCurrHi  = MF_CAN_MSG.data[3];
  //uint8_t MotorSpdLow = MF_CAN_MSG.data[4];
  //uint8_t MotorSpdHi  = MF_CAN_MSG.data[5];
  //--- reserve ---
  uint8_t EncLow = MF_CAN_MSG.data[6];
  uint8_t EncHi  = MF_CAN_MSG.data[7];
  uint16_t retVal = EncHi * 256;
  retVal = retVal +  EncLow;
  return retVal;
}

bool chk_MF_rxBuffer(){
  if (mcp2515_mf.readMessage(&MFcanMsg) == MCP2515::ERROR_OK) {
    /*//-VVVVV------ for message test -----------
    Serial.print(MFcanMsg.can_id, HEX); // print ID
    Serial.print(" "); 
    Serial.print(MFcanMsg.can_dlc, HEX); // print DLC
    Serial.print(" ");
    
    for (int i = 0; i<MFcanMsg.can_dlc; i++)  {  // print the data
      Serial.print(MFcanMsg.data[i],HEX);
      Serial.print(" ");
    }
    *///-AAAAA------------- for message test -----------
    if(MFcanMsg.data[0] == CmdReadEnc){
      EncValue = getEncValue(MFcanMsg);
    }
    else if(MFcanMsg.data[0] == CmdOpnLoop || MFcanMsg.data[0] == CmdClsTrqu ){
      EncValue = getEncValPow(MFcanMsg);
    }
    

    //Serial.println();
    return true;
  }
  else return false;
}

