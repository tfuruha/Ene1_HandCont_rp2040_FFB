//file name: Ene1HandCont_IO.h
//Ene1HandController IO Header

//Setup Ene1HandController IO
void Setup_eHanConIO();

//Button IO Check
int chkBtnUP();
int chkBtnDown();

//Accel & Break 
void getADCAccBreak();
void AveADCAccBreak();
int getAccVal();
int getBreakVal();