//file name:mf4015.h
//usaeg: header file of Utilitys to USE MF4015 with CAN

void SetupMCP2515_MF();

//comands with date[1] ~ date[7] = 0x00
void MF_MotorOff();   //Motor off command
void MF_MotorOn();    //Motor on command
void MF_MotorStop();  //Motor stop command
void MF_ReadEncode(); //Read encoder command
void MF_ReadStat1();  //Read motor state 1 and error state command
void MF_ReadStat2();  //Read motor state 2 command
void MF_ClearErr();   //Clear motor error state

// get Encoder Value for return of "Read encoder command"".
//uint16_t getEncVal(struct can_frame MF_CAN_MSG);
uint16_t getEncVal();

// Torque closed loop control command.
void MF_SetTorque(int16_t iTorquVal);
// get Encoder Value for return of "Open loop control command(0xA0)" or 
//uint16_t getEncValPow(struct can_frame MF_CAN_MSG);

bool chk_MF_rxBuffer();


