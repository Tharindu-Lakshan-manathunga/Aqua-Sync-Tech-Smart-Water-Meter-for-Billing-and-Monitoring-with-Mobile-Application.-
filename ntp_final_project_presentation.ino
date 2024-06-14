#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>
#include <SPI.h>
#include <Wire.h> 
#include <SoftwareSerial.h>
 
//Provide the token generation process info.
#include <addons/TokenHelper.h>

//Provide the RTDB payload printing info and other helper functions.
#include <addons/RTDBHelper.h>

/* 1. Define the WiFi credentials */
#define WIFI_SSID "Dialog 4G 737"
#define WIFI_PASSWORD "LA20191021#"

//For the following credentials, see examples/Authentications/SignInAsUser/EmailPassword/EmailPassword.ino

/* 2. Define the API Key */
#define API_KEY "AIzaSyCGEN4xVALFVZFbzJQnxh1YTua5holbD3I"

/* 3. Define the RTDB URL */
#define DATABASE_URL "smart-water-meter-f9bb2-default-rtdb.firebaseio.com" //<databaseName>.firebaseio.com or <databaseName>.<region>.firebasedatabase.app

/* 4. Define the user Email and password that alreadey registerd or added in your project */
#define USER_EMAIL m"
#define USER_PASSWORD "#"

//Define Firebase Data object
FirebaseData fbdo;
FirebaseData valve; 

FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
unsigned long count = 0;

#define SENSOR  D6
 
const int relay_PIN = D0;

int TDS_Sensor = A0;
float Aref = 3.3;

float ec_Val = 0;
unsigned int tds_value = 0;
float ecCal = 1;

const int badWatertds = 300;
const int badwatered = 400;

//Initialize the LCD display
LiquidCrystal_I2C lcd(0x27, 20, 4);

int Buzzer = D3;

SoftwareSerial gsmSerial(D7, D8);


long currentMillis = 0;
long previousMillis = 0;
int interval = 1000;
bool ledState = LOW;
float calibrationFactor = 4.5;
volatile byte pulseCount = 0;
float pulse1Sec = 0;
float flowRate = 0;
unsigned long flowMilliLitres = 0;
unsigned int totalMilliLitres = 0;
float flowLitres;
float totalLitres;
int waterunit;
int billammount;



    void IRAM_ATTR pulseCounter()
    {
      pulseCount++;
    }

    void setup() 
    {
      Serial.begin(9600);

      gsmSerial.begin(9600);

      lcd.begin();
      lcd.backlight();

      pinMode(relay_PIN, OUTPUT);
      digitalWrite(relay_PIN, LOW);
      pinMode(Buzzer, OUTPUT);
      
      
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      Serial.print("Connecting to Wi-Fi");
      while (WiFi.status() != WL_CONNECTED)
      {
        Serial.print(".");
        delay(300);
      }
      Serial.println();
      Serial.print("Connected with IP: ");
      Serial.println(WiFi.localIP());
      Serial.println();

      Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);

      /* Assign the api key (required) */
      config.api_key = API_KEY;

      /* Assign the user sign in credentials */
      auth.user.email = USER_EMAIL;
      auth.user.password = USER_PASSWORD;

      /* Assign the RTDB URL (required) */
      config.database_url = DATABASE_URL;

      /* Assign the callback function for the long running token generation task */
      config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h

      Firebase.begin(&config, &auth);

      //Comment or pass false value when WiFi reconnection will control by your code or third party library
      Firebase.reconnectWiFi(true);

      Firebase.setDoubleDigits(5);

    //  pinMode(Buzzer, OUTPUT);
      //     timer.setInterval(2500L, sendSensor);

      lcd.setCursor(5, 1);
      lcd.print("Smart Water ");  
      lcd.setCursor(4, 3);
      lcd.print("Meter System");
      delay(5000);
      lcd.clear();

      pinMode(SENSOR, INPUT_PULLUP);
 
      pulseCount = 0;
      flowRate = 0.0;
      flowMilliLitres = 0;
      totalMilliLitres = 0;
      previousMillis = 0;
 
      attachInterrupt(digitalPinToInterrupt(SENSOR), pulseCounter, FALLING);
    }

    double TempRead()
    {
      double temperature = 25.0; // Replace with your actual temperature reading
      return temperature;
    }

    void tdsSensor()
    {
      double wTemp = TempRead()* 0.0625;  // conversion accuracy is 0.0625 / LSB
      float V_level= Aref / 1024.0;
      float rawEc = analogRead(TDS_Sensor) * V_level;  // Raw  data of EC
      float T_Cof = 1.0 + 0.02 * (wTemp - 25.0);  // Temperature Coefficient
  
      ec_Val = (rawEc / T_Cof) * ecCal;// temperature and calibration compensation
      tds_value = (133.42 * pow(ec_Val, 3) - 255.86 * ec_Val * ec_Val + 857.39 * ec_Val) * 0.5; 
      double Far= (((wTemp *9)/5) + 32); // Temp in Far*
  
        

    }

  void initializeGSM() 
  {
      // Initialize GSM module
    gsmSerial.println("AT");
    delay(1000);
    gsmSerial.println("AT+CSQ");
    delay(1000);
    gsmSerial.println("AT+CMGF=1"); // Set SMS mode to text
    delay(1000);
    gsmSerial.println("AT+CNMI=2,2,0,0,0"); // New SMS notification
    delay(1000);
  }

  void sendSMS(String message) 
  {
    gsmSerial.println("AT+CMGS=\"+94702763025\""); // Replace with your recipient's phone number
    delay(1000);
    gsmSerial.print(message);
    delay(1000);
    gsmSerial.write(26); // Ctrl+Z to send the message
    delay(1000);
  }
   
  
  void flowSensor()
    {
      currentMillis = millis();

      if (currentMillis - previousMillis > interval) 
      {
        pulse1Sec = pulseCount;
        pulseCount = 0;
 
        flowRate = ((1000.0 / (millis() - previousMillis)) * pulse1Sec) / calibrationFactor;
        previousMillis = millis();
 
        // Divide the flow rate in litres/minute by 60 to determine how many litres have
        // passed through the sensor in this 1 second interval, then multiply by 1000 to
        // convert to millilitres.
        flowMilliLitres = (flowRate / 60) * 1000;
        flowLitres = (flowRate / 60);
 
        // Add the millilitres passed in this second to the cumulative total
        totalMilliLitres += flowMilliLitres;
        totalLitres += flowLitres;

        waterunit =  totalLitres /1000;       
    
       

        if(totalLitres < 5)
        {
          billammount = (waterunit * 60 ) + 300;
        }
        else if(6 < totalLitres < 10)
        {
          billammount = (waterunit * 80 ) + 300;        
        }
        else if(11 < totalLitres < 15)
        {
          billammount = (waterunit * 100 ) + 300;        
        }
        else if(16 < totalLitres < 20)
        {
          billammount = (waterunit * 110 ) + 400;        
        }
        
        else if(21 < totalLitres < 25)
        {
          billammount = (waterunit * 130 ) + 500;        
        }
        else if(26 < totalLitres < 30)
        {
          billammount = (waterunit * 160 ) + 600;        
        }
        else if(31 < totalLitres < 40)
        {
          billammount = (waterunit * 180 ) + 1500;        
        }           
        else if(41 < totalLitres < 50)
        {
          billammount = (waterunit * 210 ) + 3000;        
        } 
        else if(51 < totalLitres < 75)
        {
          billammount = (waterunit * 240 ) + 3500;        
        }
        else if(76 < totalLitres < 100)
        {
          billammount = (waterunit * 270 ) + 4000;        
        }
        else if (totalLitres > 100)
        {
          billammount = (waterunit * 300 ) + 4500;        
        }

        

       

           
      }
    }   
        void firebase()
        {
          if (Firebase.getInt(valve, "/test/water_switch"))
          {
            Serial.println(valve.intData());
          
            if (valve.intData() == 1) 
            {
              //lcd.clear();
              digitalWrite(relay_PIN, HIGH);
              digitalWrite(Buzzer, HIGH);
              Serial.println("valve open");
              lcd.setCursor(3, 2);
              lcd.print(" Valve open! ");
             // delay(10000);
             // lcd.clear();
            }
  
            else if (valve.intData() == 0)
            {
            digitalWrite(relay_PIN, LOW);
            digitalWrite(Buzzer, LOW);
            Serial.println("valve close");
            lcd.setCursor(3, 2);
            lcd.print(" Valve close! ");
            //delay(10000);
            //lcd.clear();
            }
          }

        if (Firebase.ready() && (millis() - sendDataPrevMillis > 1 || sendDataPrevMillis == 0))
        { 
          sendDataPrevMillis = millis();
          
          Serial.printf("Set flow Rate... %s L/min\n", Firebase.setFloat(fbdo, F("/test/flow_rate"), flowRate) ? "ok" : fbdo.errorReason().c_str());
          Serial.printf("Get flow Rate... %s L/min\n", Firebase.getFloat(fbdo, F("/test/flow_rate")) ? String(fbdo.to<float>()).c_str() : fbdo.errorReason().c_str());

          Serial.printf("Set total Litres... %s L\n", Firebase.setDouble(fbdo, F("/test/total_litres"), totalLitres) ? "ok" : fbdo.errorReason().c_str());
          Serial.printf("Get total Litres... %s L\n", Firebase.getDouble(fbdo, F("/test/total_litres")) ? String(fbdo.to<double>()).c_str() : fbdo.errorReason().c_str());
          
          Serial.printf("Set total MilliLitres... %s ml\n", Firebase.setDouble(fbdo, F("/test/total_milliLitres"), totalMilliLitres) ? "ok" : fbdo.errorReason().c_str());
          Serial.printf("Get total MilliLitres... %s ml\n", Firebase.getDouble(fbdo, F("/test/total_milliLitres")) ? String(fbdo.to<double>()).c_str() : fbdo.errorReason().c_str());

          Serial.printf("Set water unit... %s\n", Firebase.setDouble(fbdo, F("/test/water_unit"), waterunit) ? "ok" : fbdo.errorReason().c_str());
          Serial.printf("Get water unit... %s\n", Firebase.getDouble(fbdo, F("/test/water_unit")) ? String(fbdo.to<double>()).c_str() : fbdo.errorReason().c_str());

          Serial.printf("Set bill ammount...Rs. %s\n", Firebase.setDouble(fbdo, F("/test/bill_ammount"), billammount) ? "ok" : fbdo.errorReason().c_str());
          Serial.printf("Get bill ammount...Rs. %s\n", Firebase.getDouble(fbdo, F("/test/bill_ammount")) ? String(fbdo.to<double>()).c_str() : fbdo.errorReason().c_str());

          Serial.printf("Set TDS value %s ppm\n", Firebase.setDouble(fbdo, F("/test/tds_value"), tds_value) ? "ok" : fbdo.errorReason().c_str());
          Serial.printf("Get TDS value %s ppm\n", Firebase.getDouble(fbdo, F("/test/tds_value")) ? String(fbdo.to<double>()).c_str() : fbdo.errorReason().c_str());

          Serial.printf("Set EC value %s\n", Firebase.setDouble(fbdo, F("/test/ec_value"), ec_Val) ? "ok" : fbdo.errorReason().c_str());
          Serial.printf("Get EC value %s\n", Firebase.getDouble(fbdo, F("/test/ec_value")) ? String(fbdo.to<double>()).c_str() : fbdo.errorReason().c_str());


          //For the usage of FirebaseJson, see examples/FirebaseJson/BasicUsage/Create_Parse_Edit.ino
          FirebaseJson json;

          if (count == 0)
          {
            json.set("value/round/" + String(count), F("cool!"));
            json.set(F("vaue/ts/.sv"), F("timestamp"));
            Serial.printf("Set json... %s\n", Firebase.set(fbdo, F("/test/json"), json) ? "ok" : fbdo.errorReason().c_str());
          }
          else
          {
            json.add(String(count), "smart!");
            Serial.printf("Update node... %s\n", Firebase.updateNode(fbdo, F("/test/json/value/round"), json) ? "ok" : fbdo.errorReason().c_str());
          }

          Serial.println();
          count++;
    
        }  
        }  

        void loop() 
    {

      firebase();
      flowSensor();
      tdsSensor();
      //rtcSensor();
      //tftDisplay();

      lcd.setCursor(0, 0);
        lcd.print("UN:");
        lcd.print(waterunit);
 
        lcd.setCursor(10, 0);
        lcd.print("FR:");
        lcd.print(float(flowRate));
        lcd.setCursor(18,0);  
        lcd.print("LM"); 

        lcd.setCursor(0, 1);
        lcd.print("TL:");
        lcd.print(totalLitres);
        lcd.setCursor(8, 1);
        lcd.print("L");

        lcd.setCursor(0, 2);
        lcd.print("--------------------");

        lcd.setCursor(10, 1);
        lcd.print("bi:");
        lcd.setCursor(16, 1);
        lcd.print(billammount);
        lcd.setCursor(13, 1);
        lcd.print("Rs.");

        lcd.setCursor(0, 3);
        lcd.print("TDS:");
        lcd.print(tds_value);
        lcd.setCursor(8, 3);
        lcd.print("PPM");

        lcd.setCursor(12, 3);
        lcd.print("ES:");
        lcd.print(ec_Val);
        lcd.setCursor(18, 3);
        lcd.print("mS");

        

       if (tds_value > badWatertds  )
      {
        sendSMS("Warning: Bad Water detected!");
         //Firebase.setFloat(fbdo, F("/test/water_switch"), valve)  fbdo.errorReason().c_str());
         //Firebase.getFloat(fbdo, F("/test/water_switch"))  String(fbdo.to<int>()).c_str() : fbdo.errorReason().c_str());
        digitalWrite(relay_PIN, LOW);
        digitalWrite(Buzzer, HIGH);
        lcd.print("Warning: Bad Water detected!");
        delay(300000);
        lcd.clear();
      }
      if (flowRate > 45)
      {
        sendSMS("Warning: Water leakage detected!");
        //Firebase.setFloat(fbdo, F("/test/water_switch"), valve) ? "ok" : fbdo.errorReason().c_str();
         //Firebase.getFloat(fbdo, F("/test/water_switch")) ? String(fbdo.to<int>()).c_str() : fbdo.errorReason().c_str();
        digitalWrite(relay_PIN, LOW);
        digitalWrite(Buzzer, HIGH);
        lcd.setCursor(0, 2);
        lcd.print("Warning: Water leakage detected!");
        delay(300000);
        lcd.clear();
      }
  
      
    }
      
    
