// Example testing sketch for various DHT humidity/temperature sensors
// Written by ladyada, public domain

#include <Wire.h>
#include <SeeedOLED.h>
#include "DHT.h"
#include <TimerOne.h>
#include "Arduino.h"
#include "SI114X.h"


enum Status 
{
    Standby  =  0,
    Warning  =  1,
    Setting   = 2,
    Watering =  3,
};
typedef enum Status Systemstatus;
Systemstatus WorkingStatus;


enum EncoderDir
{
    Anticlockwise = 0,
    Clockwise     = 1,
};
typedef enum EncoderDir EncodedirStatus;
EncodedirStatus EncoderRoateDir;


enum WarningStatus
{
    NoWarning          = 0,
    AirHumitityWarning = 1,
    AirTemperWarning   = 2,
    UVIndexWarning     = 3,
};
typedef enum WarningStatus WarningStatusType;
WarningStatusType SystemWarning;


struct Limens 
{
    unsigned char UVIndex_Limen       = 5;
    unsigned char DHTHumidity_Limen   = 0;
    unsigned char DHTTemperature_Hi   = 32;
    unsigned char DHTTemperature_Low  = 0;
    unsigned char MoisHumidity_Limen  = 0;
    float         WaterVolume         = 1;
};
typedef struct Limens WorkingLimens;
WorkingLimens SystemLimens;


#define DHTPIN          A0     // what pin we're connected to
#define MoisturePin     A1
#define ButtonPin       2
// Uncomment whatever type you're using!
#define DHTTYPE DHT11   // DHT 11 
//#define DHTTYPE DHT22   // DHT 22  (AM2302)
//#define DHTTYPE DHT21   // DHT 21 (AM2301)
#define EncoderPin1     3
#define EncoderPin2     4
#define WaterflowPin    5
#define RelayPin        6

#define DataUpdateInterval 20000  // 10S
#define RelayOn         HIGH
#define RelayOff        LOW

#define OneSecond       1000
unsigned char WaterflowFlag = 0;
unsigned int  WaterflowRate = 0;  // L/Hour
unsigned int  NbTopsFan     = 0;  // count the edges

unsigned char EncoderFlag = 0;
unsigned long StartTime   = 0;
unsigned char ButtonFlag  = 0;
signed   char LCDPage     = 4;
unsigned char SwitchtoWateringFlag = 0;
unsigned char SwitchtoWarningFlag  = 0;
unsigned char ButtonIndex = 0;
float Volume     = 0;
unsigned long counter = 0;

SI114X SI1145 = SI114X();
DHT dht(DHTPIN, DHTTYPE);
float DHTHumidity    = 0;
float DHTTemperature = 0;
float MoisHumidity   = 0;
float UVIndex        = 0;
char buffer[30];
void setup() 
{
    /* Init OLED */
    Wire.begin();
    SeeedOled.init();  //initialze SEEED OLED display
    DDRB|=0x21;        
    PORTB |= 0x21;
    SeeedOled.clearDisplay();          //clear the screen and set start position to top left corner
    SeeedOled.setNormalDisplay();      //Set display to normal mode (i.e non-inverse mode)
    SeeedOled.setPageMode();           //Set addressing mode to Page Mode

//    encoder.Timer_init();
    /* Init DHT11 */
    Serial.begin(9600); 
    Serial.println("DHTxx test!");
    dht.begin();
    
    /* Init Button */
    pinMode(ButtonPin,INPUT);
    attachInterrupt(0,ButtonClick,FALLING);
    /* Init Encoder */
    pinMode(EncoderPin1,INPUT);
    pinMode(EncoderPin2,INPUT);
    attachInterrupt(1,EncoderRotate,RISING);   

    /* Init UV */
    Serial.println("Beginning Si1145!");
    while (!SI1145.Begin()) {
        Serial.println("Si1145 is not ready!");
        delay(1000);
    }
    Serial.println("Si1145 is ready!");
    
    /* Init Water flow */
    pinMode(WaterflowPin,INPUT);
    
    /* Init Relay      */
    pinMode(RelayPin,OUTPUT);
    
    StartTime = millis();
    WorkingStatus = Standby;
    SystemWarning = NoWarning;
}

void loop() 
{
    // Reading temperature or humidity takes about 250 milliseconds!
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
    switch (WorkingStatus) {
    case Standby:
        if (millis() -StartTime > DataUpdateInterval) {
            StartTime      = millis();
            DHTHumidity    = dht.readHumidity();
            DHTTemperature = dht.readTemperature();
            MoisHumidity   = analogRead(MoisturePin)/7;
            UVIndex        = (float)SI1145.ReadUV()/100;
            if (MoisHumidity >100) {
                MoisHumidity = 100;
            }
            if (MoisHumidity < SystemLimens.MoisHumidity_Limen) {
                SwitchtoWateringFlag = 1;
            }
            
            if (DHTHumidity < SystemLimens.DHTHumidity_Limen) {
                SystemWarning = AirHumitityWarning;
            }
            else if ((DHTTemperature>SystemLimens.DHTTemperature_Hi) || (DHTTemperature < SystemLimens.DHTTemperature_Low)) {
                SystemWarning = AirTemperWarning;
            }
            else if (UVIndex > SystemLimens.UVIndex_Limen) {
                SystemWarning = UVIndexWarning;
            }
            else {
                SystemWarning = NoWarning;
            }
        }
        if (SystemWarning == NoWarning) {
            if (EncoderFlag == 1) {
                delay(100);
                EncoderFlag = 0;
                switch(EncoderRoateDir) {
                case Clockwise:
                    LCDPage++;
                    break;
                case Anticlockwise:
                    LCDPage--;
                    break;
                default:
                    break;
                }
                if (LCDPage > 5) {
                    LCDPage = 0;
                }
                if (LCDPage < 0) {
                    LCDPage = 5;
                }
                SeeedOled.clearDisplay(); 
            }
            switch (LCDPage) {
            case 0:
                DisplayMoisture();
                break;
            case 1:
                DisplayAirHumitity();
                break;
            case 2:
                DisplayAirTemp();
                break;
            case 3:
                DisplayUVIndex();
                break;
            case 4:
                DisplayHello();
                break;
            case 5:
                DisplayVolume();
                break;
            default:
                break;
            }
            if (ButtonFlag == 1) {
                if ((int)(LCDPage) == 4) {
                    StandbytoWatering();         
                } 
                if(ButtonIndex == 1) {
                    ButtonIndex = 0;
                } else {
                    ButtonIndex = 1;
                }
                ButtonFlag = 0;
            }
        } else {
            WorkingStatus = Warning;
            SeeedOled.clearDisplay();
            SeeedOled.setInverseDisplay();
        }
        if (SwitchtoWateringFlag == 1) {    
            StandbytoWatering();
        }
        break;
    case Warning:
        switch (SystemWarning) {
        case AirHumitityWarning:
                SeeedOled.setTextXY(3,3);
                SeeedOled.putString("AirHumitity");
                SeeedOled.setTextXY(5,4);
                SeeedOled.putString("Warning");
            break;
        case AirTemperWarning:
                SeeedOled.setTextXY(3,3);
                SeeedOled.putString("AirTemper");
                SeeedOled.setTextXY(5,4);
                SeeedOled.putString("Warning");             
            break;
        case UVIndexWarning:
                SeeedOled.setTextXY(3,4);
                SeeedOled.putString("UVIndex");
                SeeedOled.setTextXY(5,4);
                SeeedOled.putString("Warning");
            break;
        }
        delay(1000);
        WorkingStatus = Standby;
        if (ButtonFlag ==  1) {
            SystemWarning = NoWarning;
            ButtonFlag = 0;
            SeeedOled.clearDisplay();
            SeeedOled.setNormalDisplay();
            StartTime = millis();
        }
        break;
    case Setting:
        Serial.println("Setting");
        if (ButtonFlag == 1) {
            ButtonFlag = 0;
            WorkingStatus = Watering;
            WaterPumpOn();
            StartTime = millis();
        }
        break;
    case Watering:
        if(digitalRead(WaterflowPin) == 1) {
            if(digitalRead(WaterflowPin) == 1) {
                if (WaterflowFlag == 0) {
                    WaterflowFlag = 1;
                    NbTopsFan++; 
                } else {
                }
            }
        } else {
            if (WaterflowFlag == 1) {
                WaterflowFlag = 0;
            } else {
            }
        }
        if ((millis() - StartTime) > OneSecond ) {
            WaterflowRate = (NbTopsFan*60 / 73);
            Volume       += (float)((float)(NbTopsFan) / 73/60 + 0.005);           
            NbTopsFan = 0;
            sprintf(buffer,"%2d L/H",WaterflowRate);
            SeeedOled.setTextXY(2,10);
            SeeedOled.putString(buffer);  
            
            sprintf(buffer,"%2d.%2d L",(int)(Volume),(int)((int)(Volume*100) %100));
            SeeedOled.setTextXY(4,8);
            SeeedOled.putString(buffer);  
            
            if (Volume >= SystemLimens.WaterVolume) {
                Volume = 0;
                WorkingStatus = Standby;
                WaterPumpOff();
                SeeedOled.clearDisplay();
                LCDPage = 4;
            }
//            
            // sprintf(buffer,"Press Btn toSTOP");
            // SeeedOled.setTextXY(7,0);
            // SeeedOled.putString(buffer);
            StartTime = millis();
        }
        if (ButtonFlag == 1) {
            Volume = 0;
            ButtonFlag = 0;
            SeeedOled.clearDisplay();
            StartTime = millis();
            WorkingStatus = Standby;
            WaterPumpOff();
        }     
        break;
    default:
        break;
    }
}

void WaterPumpOn()
{
    digitalWrite(RelayPin,RelayOn);
}

void WaterPumpOff()
{
    digitalWrite(RelayPin,RelayOff);
}


void DisplayUVIndex()
{
    sprintf(buffer,"UVIndex");
    SeeedOled.setTextXY(0,0);          //Set the cursor to Xth Page, Yth Column  
    SeeedOled.putString(buffer);

    sprintf(buffer,"%d.%d",(int)(UVIndex),(int)((int)(UVIndex*100) % 100));
    SeeedOled.setTextXY(1,5);          //Set the cursor to Xth Page, Yth Column  
    SeeedOled.putString(buffer);
    sprintf(buffer,"Limen:"); 

    SeeedOled.setTextXY(6,0);
    SeeedOled.putString(buffer);
    SeeedOled.setTextXY(7,5);
    sprintf(buffer,"%d.00",(int)(SystemLimens.UVIndex_Limen)); 
            
    SeeedOled.putString(buffer);
    
    if (digitalRead(ButtonPin) == 1) {
        Serial.println("BUTTON");
        if (EncoderFlag == 1) {
        delay(100);
        EncoderFlag = 0;
        switch(EncoderRoateDir) {
        case Clockwise:
            SystemLimens.UVIndex_Limen++;
            break;
        case Anticlockwise:
            SystemLimens.UVIndex_Limen--;
            break;
        default:
            break;
            }
        if (SystemLimens.UVIndex_Limen > 12) {
            SystemLimens.UVIndex_Limen = 12;
        }
        if (SystemLimens.UVIndex_Limen <= 0) {
            SystemLimens.UVIndex_Limen = 0;
        }
        } 
    }
    
    
}

void DisplayAirHumitity()
{
    sprintf(buffer,"AirHumitity:");
    SeeedOled.setTextXY(0,0);          //Set the cursor to Xth Page, Yth Column  
    SeeedOled.putString(buffer);

    sprintf(buffer,"%d.%d%%",(int)(DHTHumidity),(int)((int)(DHTHumidity*100) % 100));
    SeeedOled.setTextXY(1,5);          //Set the cursor to Xth Page, Yth Column  
    SeeedOled.putString(buffer);
    
    SeeedOled.setTextXY(5,0);
    SeeedOled.putString("Low ");
    
    sprintf(buffer,"Limen:"); 
    SeeedOled.setTextXY(6,0);
    SeeedOled.putString(buffer);
    SeeedOled.setTextXY(7,2);
    sprintf(buffer,"%d.00",(int)(SystemLimens.DHTHumidity_Limen)); 

    SeeedOled.putString(buffer);
    
    if (digitalRead(ButtonPin) == 1) {
        Serial.println("BUTTON");
        if (EncoderFlag == 1) {
        delay(100);
        EncoderFlag = 0;
        switch(EncoderRoateDir) {
        case Clockwise:
            SystemLimens.DHTHumidity_Limen++;
            break;
        case Anticlockwise:
            SystemLimens.DHTHumidity_Limen--;
            break;
        default:
            break;
            }
        if (SystemLimens.DHTHumidity_Limen > 100) {
            SystemLimens.DHTHumidity_Limen = 0;
        }
        if (SystemLimens.DHTHumidity_Limen < 0) {
            SystemLimens.DHTHumidity_Limen = 100;
        }
        } 
    }
}

void DisplayAirTemp()
{
    sprintf(buffer,"AirTemper:");
    SeeedOled.setTextXY(0,0);          //Set the cursor to Xth Page, Yth Column  
    SeeedOled.putString(buffer);

    sprintf(buffer,"%d.%d*C",(int)(DHTTemperature),(int)((int)(DHTTemperature*100)%100));
    SeeedOled.setTextXY(1,5);          //Set the cursor to Xth Page, Yth Column  
    SeeedOled.putString(buffer);
    sprintf(buffer,"HiLimen LowLimen"); 
    SeeedOled.setTextXY(6,0);
    SeeedOled.putString(buffer);
    SeeedOled.setTextXY(7,2);
    sprintf(buffer,"%d.00",(int)(SystemLimens.DHTTemperature_Hi)); 
    SeeedOled.putString(buffer);
    SeeedOled.setTextXY(7,10);
    sprintf(buffer,"%d.00",(int)(SystemLimens.DHTTemperature_Low)); 
    SeeedOled.putString(buffer); 
    if(ButtonIndex == 0) {
        SeeedOled.setTextXY(5,0);
        SeeedOled.putString("High");
        if (digitalRead(ButtonPin) == 1) {
            Serial.println("BUTTON");
            if (EncoderFlag == 1) {
                delay(100);
                EncoderFlag = 0;
                switch(EncoderRoateDir) {
                case Clockwise:
                    SystemLimens.DHTTemperature_Hi++;
                    break;
                case Anticlockwise:
                    SystemLimens.DHTTemperature_Hi--;
                    break;
                default:
                    break;
                }
                if (SystemLimens.DHTTemperature_Hi > 100) {
                    SystemLimens.DHTTemperature_Hi = 100;
                }
                if (SystemLimens.DHTTemperature_Hi <= SystemLimens.DHTTemperature_Low) {
                    SystemLimens.DHTTemperature_Hi = SystemLimens.DHTTemperature_Low;
                }
            } 
        }
    } else {
        SeeedOled.setTextXY(5,0);
        SeeedOled.putString("Low ");
        if (digitalRead(ButtonPin) == 1) {
            Serial.println("BUTTON");
            if (EncoderFlag == 1) {
                delay(100);
                EncoderFlag = 0;
                switch(EncoderRoateDir) {
                case Clockwise:
                    SystemLimens.DHTTemperature_Low++;
                    break;
                case Anticlockwise:
                    SystemLimens.DHTTemperature_Low--;
                    break;
                default:
                    break;
                }
                if (SystemLimens.DHTTemperature_Low > SystemLimens.DHTTemperature_Hi) {
                    SystemLimens.DHTTemperature_Low = SystemLimens.DHTTemperature_Hi;
                }
                if (SystemLimens.DHTTemperature_Low <= 0) {
                    SystemLimens.DHTTemperature_Low = 0;
                }
            } 
        }
    }
}

void DisplayMoisture() 
{
    sprintf(buffer,"Moisture:");
    SeeedOled.setTextXY(0,0);          //Set the cursor to Xth Page, Yth Column  
    SeeedOled.putString(buffer);

    sprintf(buffer,"%d.%d%%",(int)(MoisHumidity),(int)((int)(MoisHumidity*100) % 100));
    SeeedOled.setTextXY(1,5);          //Set the cursor to Xth Page, Yth Column  
    SeeedOled.putString(buffer);
   
    
    SeeedOled.setTextXY(5,0);
    SeeedOled.putString("Low ");
    
    sprintf(buffer,"Limen"); 
    SeeedOled.setTextXY(6,0);
    SeeedOled.putString(buffer);
    SeeedOled.setTextXY(7,2);
    sprintf(buffer,"%d.00",(int)(SystemLimens.MoisHumidity_Limen)); 
    


    SeeedOled.putString(buffer);   
    if (digitalRead(ButtonPin) == 1) {
        Serial.println("BUTTON");
        if (EncoderFlag == 1) {
        delay(100);
        EncoderFlag = 0;
        switch(EncoderRoateDir) {
        case Clockwise:
            SystemLimens.MoisHumidity_Limen++;
            break;
        case Anticlockwise:
            SystemLimens.MoisHumidity_Limen--;
            break;
        default:
            break;
            }
        if (SystemLimens.MoisHumidity_Limen > 100) {
            SystemLimens.MoisHumidity_Limen = 0;
        }
        if (SystemLimens.MoisHumidity_Limen < 0) {
            SystemLimens.MoisHumidity_Limen = 100;
        }
        } 
    }
}


void DisplayHello()
{
    SeeedOled.setTextXY(2,0);          //Set the cursor to Xth Page, Yth Column  
    SeeedOled.putString("Gardening Demo");    
    SeeedOled.setTextXY(4,2);          //Set the cursor to Xth Page, Yth Column  
    SeeedOled.putString("SeeedStudio");
}

void DisplayVolume()
{
    unsigned int Volumetemp;
    Volumetemp = SystemLimens.WaterVolume*10;
    sprintf(buffer,"WaterVolume");
    SeeedOled.setTextXY(2,3);          //Set the cursor to Xth Page, Yth Column  
    SeeedOled.putString(buffer);

    SeeedOled.setTextXY(5,0);
    SeeedOled.putString("High");
    
    sprintf(buffer,"Limen:"); 
    SeeedOled.setTextXY(6,0);
    SeeedOled.putString(buffer);
    SeeedOled.setTextXY(7,2);
    sprintf(buffer,"%2d.%dL",(Volumetemp/10),(Volumetemp%10)); 
    SeeedOled.putString(buffer);
    
    if (digitalRead(ButtonPin) == 1) {
        Serial.println("BUTTON");
        if (EncoderFlag == 1) {
        delay(100);
        EncoderFlag = 0;
        switch(EncoderRoateDir) {
        case Clockwise:
            Volumetemp++;
            break;
        case Anticlockwise:
            Volumetemp--;
            break;
        default:
            break;
            }
        if (Volumetemp > 1000) {
            Volumetemp = 0;
        }
        if (Volumetemp < 0) {
            Volumetemp = 1000;
        }
        } 
    }
    SystemLimens.WaterVolume = (float)Volumetemp / 10;
}

void StandbytoWatering()
{
    SwitchtoWateringFlag = 0;
    SeeedOled.clearDisplay(); 
    WorkingStatus = Watering;
    WaterPumpOn();
    SeeedOled.setTextXY(0,3);
    SeeedOled.putString("Watering");
    SeeedOled.setTextXY(2,0);
    SeeedOled.putString("FlowRate:");
    SeeedOled.setTextXY(4,0);
    SeeedOled.putString("Volume:");
    StartTime = millis();
}



void ButtonClick()
{
    ButtonFlag = 1;
}


void EncoderRotate()
{
    if(digitalRead(EncoderPin1) == 1) {
        delay(10);
        if(digitalRead(EncoderPin1) == 1) {
            if(EncoderFlag == 0) {
                EncoderFlag = 1;
                if(digitalRead(EncoderPin2) == 1) {
                    Serial.println("1");
                    EncoderRoateDir = Clockwise;
                } else {
                    Serial.println("0");
                    EncoderRoateDir = Anticlockwise;
                }
           }
        } else {
        }
    } else {
    }
}

