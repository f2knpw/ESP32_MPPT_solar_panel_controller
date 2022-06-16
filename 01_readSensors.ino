void readSensors()
{
  //sensors measurement
  float VinTemp = 0, VoutTemp = 0, IoutTemp = 0, IinTemp = 0;
  float IinZero = 2850; //must be calibrated --> raw value with current = 0A
  int i = 0;
  for (i = 0; i < Smooth; i++) VinTemp += analogRead(VIN_PIN);
  for (i = 0; i < Smooth; i++) VoutTemp += analogRead(VOUT_PIN);
  //  analogSetAttenuation(ADC_0db);        //Sets the input attenuation for ALL ADC inputs, default is ADC_11db, range is ADC_0db=0, ADC_2_5db=1, ADC_6db=2, ADC_11db=3
  for (i = 0; i < Smooth; i++)  IinTemp += analogRead(I_IN_PIN);

  //  Serial.print("VinMPPT ");
  //  Serial.print(VinMPPT);
  //  TelnetStream.print("vMPPT ");
  //  TelnetStream.print(VinMPPT);

  VinTemp = VinTemp / Smooth ;
  Serial.print(", Vin ");
  TelnetStream.print(", Vin ");
  Serial.print(VinTemp);
  TelnetStream.print(VinTemp);
  Serial.print(" / ");
  TelnetStream.print(" / ");
  Vin = volts(VinTemp);
  Serial.print(Vin);
  TelnetStream.print(Vin);

  //if (Vin > 15.)
  {
    VinAverage = (15. * VinAverage + Vin) / 16.;
    IinAverage = (15. * IinAverage + Iin) / 16.;
  }
  //  Serial.print(", VinAv ");
  //  Serial.print(VinAverage);
  //  TelnetStream.print(", VinAv ");
  //  TelnetStream.print(VinAverage);


  Serial.print(", Iin ");
  TelnetStream.print(", Iin ");
  IinTemp = IinTemp / Smooth;
  Serial.print(IinTemp);
  TelnetStream.print(IinTemp);
  Serial.print(" / ");
  TelnetStream.print(" / ");
  Iin = (IinZero - IinTemp) * 3.65 / 280.;
  Serial.print(Iin);
  TelnetStream.print(Iin);

  Serial.print(", Vout ");
  TelnetStream.print(", Vout ");
  VoutTemp = VoutTemp / Smooth;
  Serial.print(VoutTemp);
  TelnetStream.print(VoutTemp);
  Serial.print(" / ");
  TelnetStream.print(" / ");
  Vout = volts(VoutTemp);
  Serial.print(Vout);
  TelnetStream.print(Vout);

  Win = Iin * Vin;
  Wmin += Win;
  nbSamples++;
  Wout = Win * efficiency;  //power is almost preserved but efficiency is not 100%
  if (Vout < 2) Wout = 0.;
  Iout = Wout / Vout;  //compute Iout with assumption of power preservation and Buck converter efficiency
  Serial.print(", Iout ");
  TelnetStream.print(", Iout ");
  Serial.print(Iout);
  TelnetStream.print(Iout);

  Serial.print(", Watt ");
  Serial.print(Win);
  TelnetStream.print(", Watt ");
  TelnetStream.println(Win);

  deltaWin = abs(Win - WinPrev) / Win * 100.;
  //  Serial.print(", delta ");
  //  Serial.print(deltaWin);

  Serial.print(", MPPTduty ");
  Serial.println(dutyCycle);
  //  TelnetStream.print(", duty ");
  //  TelnetStream.println(dutyCycle);
}

float volts(float raw)
{
  if (raw < 735.1) return 0.0096 * raw + 2.2767; //see excel spreadsheet (multi linear regression) y = 0,0096x + 2,2767
  else return 0.0117 * raw + 0.7156;                                                            //y = 0,0117x + 0,7156 




}
