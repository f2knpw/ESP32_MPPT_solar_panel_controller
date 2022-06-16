#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <TelnetStream.h>

//Wifi credentials
String ssid = "YOUR_SSID";
String password = "YOUR_WIFI_PASSWORD";

//touch interrupt
int threshold = 40;
bool touch1detected = false;
bool touch2detected = false;

//PWM
long freq = 20000;
int ledChannel = 0;
int resolution = 12;
int nbSteps = pow(2, resolution) - 1;
int dutyCycle = 2 * nbSteps / 3; //start at max power
int step = 1;

unsigned long previousMillis = 0;
unsigned long currentMillis;
int clockPeriod = 100;  //enter loop every clockPeriod ms
int nbTicks = 0;

//ADC
int Value = 0;
int Nb = 0;
int Smooth = 4000;        //acquire smooth*values for each ADC
float Vin = 0.;           //input Voltage (solar panel voltage)
float VinAverage = 15.;   //input Voltage average (solar panel voltage)
float Vout = 0.;          //Output Voltage (Buck converter)
float Iout = 0.;          //Output current (estimated from Iin, Win))
float Iin = 0; 	          //input current (read on ACS712 sensor))
float IinAverage = 0;     //input current average
float Wout = 0.;          //output power
float Win = 0.;           //input power
float efficiency = .85;   //DC-DC conversion efficiency of the Buck converter 85% is a good starting point !
float Wmin = 0.;
int nbSamples = 0;
float Wh = 0.;
float deltaWin = 0;
long minutesTimer;

//MPPT
float VinMPPT = 16.5;
float WinMPPT;
float VinPrev = 17., WinPrev = 0, IoutPrev, IinPrev = 0;    //voltage and Watt previous cycle
int dutyCyclePrev;
long MPPTchanged;

//operation Mode
enum typeMode {PSU_MANUAL, PSU_CHARGER, SOLAR_CHARGER, MPPT, CV_5V, CV_9V, CV_12V };        //   USER PARAMETER - 0 = PSU MODE, 1 = Charger Mode, 2 = MPPT only
int  outputMode    = PSU_MANUAL;

//battery
enum typeBat {ACID_12V, LIPO_1s, LIPO_2s, LIPO_3s, LIPO_4s};
int  myBattery = LIPO_3s ;
float VoutMax = 0.;                                    // Maximum voltage output of charger (including .3V ideal diode drop out
float IoutMax = 0.;                                    // Maximum current output of charger
float VbatteryMax[]       = {15., 4.5, 8.7, 12.9, 17.1};    //   USER PARAMETER - Maximum Battery Charging Voltage (includes .3V diode drop out)
float VbatteryFloat[]     = {14.4, 3.3, 6.3,  9.3, 12.3};   //   USER PARAMETER - Minimum Battery Charging Voltage (includes .3V diode drop out)
float Icharge[]           = {5.0,  1.0, 2.0,  3.0, 4.0};    //   USER PARAMETER - Maximum Charging Current
//lead acid charge : https://www.power-sonic.com/blog/how-to-charge-a-lead-acid-battery/
//Vmax = 6*2.45 + 0.3 = 15.58 V
//Vfloat = 6*2.35 + 0.3 = 14.4 V

//BLE
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

long LastBLEnotification;

//Json
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

#define PWM_PIN 22
#define VIN_PIN 36
#define VOUT_PIN 39
#define I_OUT_PIN 35
#define I_IN_PIN 34

void gotTouch1() {
  touch1detected = true;
}

void gotTouch2() {
  touch2detected = true;
}



//Preferences
#include <Preferences.h>
Preferences preferences;

//BLE declarations
BLECharacteristic *pCharacteristic;
bool deviceConnected = false;
// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331915e"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26b9"

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("client connected");
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("client disconnected");
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic)
    {
      std::string rxValue = pCharacteristic->getValue();
      String test = "";
      if (rxValue.length() > 0)
      {
        Serial.print("Received : ");
        for (int i = 0; i < rxValue.length(); i++)
        {
          Serial.print(rxValue[i]);
          test = test + rxValue[i];
        }
        Serial.println();
      }
      String Res;

      int i;
      if (test.startsWith("{")) //then it may contain JSON
      {
        StaticJsonDocument<2000> doc;
        DeserializationError error = deserializeJson(doc, test);
        // Test if parsing succeeds.
        if (error) {
          Serial.print(F("deserializeJson() failed: "));
          Serial.println(error.c_str());
          Serial.println("deserializeJson() failed");                             //answer with error : {"answer" : "error","detail":"decoding failed"}
          BLEnotify("{\"answer\" : \"error\",\"detail\":\"decoding failed\"}");
        }
        else
        {
          // Fetch values --> {"Cmd":"Wifi"}
          String Cmd = doc["Cmd"];
          if (Cmd == "Wifi")
          {
            const char* cpassword =  doc["Password"] ;
            const char* cssid = doc["SSID"];
            String strpassword(cpassword);
            String strssid(cssid);
            preferences.putString("password", strpassword);
            preferences.putString("ssid", strssid);
            delay(1000);
            ESP.restart();
            delay(1000);
          }
          else if (Cmd == "OTA")
          {
            //connect to WiFi
            WiFi.begin(ssid.c_str(), password.c_str());
            long start = millis();
            while ((WiFi.status() != WL_CONNECTED) && (millis() - start < 15000)) {
              delay(500);
              Serial.print(".");
            }
            BLEnotify("OTA on");
          }
          else if (Cmd == "Mode") //setup outputMode
          {
            outputMode =  doc["value"] ;
            preferences.putInt("outputMode", outputMode);
            Serial.print("outputMode ");
            Serial.println(outputMode);
            WinMPPT = 0.;
            switch (outputMode)
            {
              case CV_5V:                                        //CC-CV PSU manual MODE
                VoutMax = 5. + .3;
                break;
              case CV_9V:                                        //CC-CV PSU manual MODE
                VoutMax = 9. + .3;
                break;
              case CV_12V:                                        //CC-CV PSU manual MODE
                VoutMax = 12. + .3;
                break;
              default:
                break;
            }
          }
          else if (Cmd == "Battery") //setup outputMode
          {
            myBattery =  doc["value"] ;
            preferences.putInt("myBattery", myBattery);
            Serial.print("myBattery ");
            Serial.println(myBattery);
            VoutMax = VbatteryMax[myBattery];           //set max charging voltage and current
            IoutMax = Icharge[myBattery];
          }
        }
      }
    }
};

void setup() {
  Serial.begin(115200);
  delay(1000); // give me time to bring up serial monitor

  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  //connect to WiFi
  WiFi.begin(ssid.c_str(), password.c_str());
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
  TelnetStream.begin();

  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // Hostname defaults to esp3232-[MAC]
  ArduinoOTA.setHostname("ESP32SolarPanel");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA
  .onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  })
  .onEnd([]() {
    Serial.println("\nEnd");
  })
  .onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  })
  .onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  //touchpad
  touchAttachInterrupt(T2, gotTouch1, threshold);
  touchAttachInterrupt(T3, gotTouch2, threshold);

  //PWM
  ledcSetup(ledChannel, freq, resolution);
  ledcAttachPin(22, ledChannel);

  //ADC
  //analogSetClockDiv(255);
  //analogReadResolution(12);             // Sets the sample bits and read resolution, default is 12-bit (0 - 4095), range is 9 - 12 bits
  analogSetWidth(12);                   // Sets the sample bits and read resolution, default is 12-bit (0 - 4095), range is 9 - 12 bits
  analogSetAttenuation(ADC_11db);        //Sets the input attenuation for ALL ADC inputs, default is ADC_11db, range is ADC_0db=0, ADC_2_5db=1, ADC_6db=2, ADC_11db=3
  analogSetCycles(8);                   // Set number of cycles per sample, default is 8 and provides an optimal result, range is 1 - 255

  //timers
  minutesTimer = millis();

  //Preferences
  preferences.begin("SolarPanel", false);

  //preferences.clear();              // Remove all preferences under the opened namespace
  //preferences.remove("counter");   // remove the counter key only
  //  ssid = preferences.getString("ssid", ""); // Get the ssid  value, if the key does not exist, return a default value of ""
  //  password = preferences.getString("password", "");
  myBattery = preferences.getInt("myBattery", 3);       //default is LIPO_3s
  VoutMax = VbatteryMax[myBattery];                     //set max charging voltage and current
  IoutMax = Icharge[myBattery];
  outputMode = preferences.getInt("outputMode", 3);     //default is MPPT
  //fade = preferences.getBool("fade", false);
  //brightness = preferences.getInt("brightness",20);

  //preferences.end();  // Close the Preferences




  //BLE
  // Create the BLE Device
  BLEDevice::init("JP eSolarPanel");

  // Create the BLE Server
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_INDICATE
                    );

  // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
  // Create a BLE Descriptor
  pCharacteristic->addDescriptor(new BLE2902());

  pCharacteristic->setCallbacks(new MyCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");
  delay(100);
}
void BLEnotify(String theString )
{
  if (deviceConnected == true)
  {
    char message[21];
    String small = "";          //BLE notification MTU is limited to 20 bytes
    while (theString.length() > 0)
    {
      small = theString.substring(0, 19); //cut into 20 chars slices
      theString = theString.substring(19);
      small.toCharArray(message, 20);
      pCharacteristic->setValue(message);
      pCharacteristic->notify();
      delay(3);             // bluetooth stack will go into congestion, if too many packets are sent
      LastBLEnotification = millis(); //will prevent to send new notification before this one is not totally sent
    }
  }
}


void loop() {
  //OTA
  ArduinoOTA.handle();

  //touchpad
  if (touch1detected) {
    touch1detected = false;
    step = 1;
    dutyCycle += step;
    if (dutyCycle > nbSteps) dutyCycle = nbSteps;
  }
  if (touch2detected) {
    touch2detected = false;
    step = 1;
    dutyCycle -= step;
    if (dutyCycle < 0) dutyCycle = 0;
  }

  //MPPT PWM
  ledcWrite(ledChannel, dutyCycle);                   //apply PWM computed previous loop

  currentMillis = millis();
  if (currentMillis - previousMillis >= clockPeriod) // clocking in 100ms
  {
    previousMillis = currentMillis;
    readSensors();  //tab #01
    nbTicks++;
    if (nbTicks > 1) //200ms
    {
      nbTicks = 0;
      performCharge();  //tab #03
    }
  }
  if ((millis() - minutesTimer) > 60000) //every minute
  {
    minutesTimer = millis();
    Wmin = Wmin / nbSamples;
    nbSamples = 0;
    Wh = Wh + Wmin / 60.; //compute total power produced
    Wmin = 0.;
    Serial.print("P total (Wh) ");
    Serial.println(Wh);
  }

  if ((millis() - LastBLEnotification) > 2000) // send BLE message to Android App (no need to be connected, a simple notification, send and forget)
  {
    LastBLEnotification = millis();
    String res;
    res = "{\"Vi\":\"" + String(Vin) + "\",\"Ii\": " + String( Iin) + ",\"Vo\": " + String( Vout) + ",\"Io\": " + String( Iout) ;
    res += + ",\"Vm\": " + String( VinMPPT) + ",\"Wm\": " + String( WinMPPT) + ",\"Wh\": " + String(Wh) + ",\"Dc\": " + String(dutyCycle) + "}";
    BLEnotify(res);
  }
}
