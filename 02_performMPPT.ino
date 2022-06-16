void performMPPT()
{
  float dI, dV, temp;
  dI = Iin - IinPrev;
  dV = Vin - VinPrev;

  if (Vin > 19.5) step = 150;                   //start fast then decrease
  else if (abs(Vin - VinMPPT) > 2) step = 100;
  else if (abs(Vin - VinMPPT) > 1) step = 20;
  else if (abs(Vin - VinMPPT) > .3) step = 8;
  else step = 1;
  if (deltaWin > 15) step = 1;              //if power is oscillating too fast (20%), slow down
 if ((outputMode != MPPT)&&  (abs(VoutMax - Vout) < 3.0))  step = 1;
  //incremental conductance MPPT algo : https://www.ijareeie.com/upload/june/82_Incremental.pdf
  if (dV == 0)
  {
    if (dI > 0) dutyCycle ++;             //go "up" slowly
    else if (dI < 0) dutyCycle -= step;   //go down fast at the beginning
  }
  else
  {
    temp = dI / dV + (Iin / Vin);
    if (temp < 0)	dutyCycle -= step;      //go down fast at the beginning
    else if (temp > 0) dutyCycle ++;      //go "up"
    //when (temp == 0) ==> dI / dV = -(Iin / Vin) ==> we are at VinMPPT
  }

  if (Vin < 14.5)                         //too low voltage
  {
    dutyCycle +=  step;                   //increase dutyCycle
  }
  else if (VinAverage * IinAverage > WinMPPT)     //power higher than before --> change MPPT
  {
    VinMPPT = VinAverage;
    WinMPPT = VinMPPT * IinAverage;
    VinMPPT = max (float(14.5), VinAverage);      //go back to value just before crash but not lower than 15.5V
    VinMPPT = min (float(16.2), VinMPPT);         //but not to much
    MPPTchanged = millis();
  }
  if ((millis() - MPPTchanged) > 60000)           //force a refresh every minute
  {
    dutyCycle -= 10;                             //decrease; //avoid staying into local minimum
    WinMPPT = 0;
    MPPTchanged = millis();
    Serial.println("local min ");
    TelnetStream.println("local min ");
  }
}
