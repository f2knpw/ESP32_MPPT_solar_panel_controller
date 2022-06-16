void performCharge()
{
  if ((Vout - VoutMax) > 2)       step = 3;    //start fast then decrease
  else  step = 2;

  if ((Iout - IoutMax) > 1)       step = 3;    //do not allow over current


  if (Vin < 3.) dutyCycle = 2 * nbSteps / 3;       //security --> 0V if no Voltage input

  switch (outputMode)
  {
    case PSU_MANUAL:                                        //CC-CV PSU manual MODE
      //do nothing
      break;
    case PSU_CHARGER:                                       //CC-CV PSU charger MODE
      if (Iout > IoutMax) dutyCycle += step;                         //Current Is Above → decrease current
      else if (Vout > VoutMax) dutyCycle += step;                    //Voltage Is Above → decrease voltage
      else if (Vout < VoutMax) dutyCycle -= step;                    //decrease duty cycle when output is below minimal charging voltage (for CC-CV only mode)
      if ((Vout > VoutMax) && (Iout < IoutMax * 4. / 100.))
      {
        VoutMax = VbatteryFloat[myBattery];                          //switch to float charging mode
      }
      break;
    case SOLAR_CHARGER:                                     //charger mode MPPT & CC-CV CHARGING ALGORITHM
      if (Iout > IoutMax) dutyCycle += step;                         //Current Is Above → decrease current
      else if (Vout > VoutMax) dutyCycle += step;                    //Voltage Is Above → decrease voltage
      else performMPPT();                                            //tab #02
      if ((Vout > VoutMax) && (Iout < IoutMax * 4. / 100.))
      {
        VoutMax = VbatteryFloat[myBattery];                          //switch to float charging mode
      }
      break;
    case MPPT:                                              //MPPT only
      performMPPT();                                                //tab #02
      break;
    case CV_5V:
    case CV_9V:
    case CV_12V:                                            //constant voltage only
      if (abs(Vout - VoutMax) < 2)  step = 1;              //slow change when close
      if (Vout > VoutMax) dutyCycle += step;                    //Voltage Is Above → decrease voltage
      else if (Vout < VoutMax) dutyCycle -= step;               //decrease duty cycle when output is below minimal charging voltage (for CC-CV only mode);                                                //tab #02
      break;
    default:
      break;
  }
  if (dutyCycle < 0) dutyCycle = 0;
  if (dutyCycle > nbSteps) dutyCycle = 2 * nbSteps / 3;


  IoutPrev = Iout;
  IinPrev = Iin;
  VinPrev = Vin;
  WinPrev = Win;
  dutyCyclePrev = dutyCycle;
}
