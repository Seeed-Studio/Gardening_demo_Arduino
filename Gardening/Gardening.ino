/*    
 * Gardening.c
 * Gardening Demo for Arduino 
 *   
 * Copyright (c) 2015 seeed technology inc.  
 * Author      : Jiankai.li  
 * Create Time:  Aug 2015
 * Change Log : 
 *
 * The MIT License (MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <Wire.h>
#include <SeeedOLED.h>
#include <EEPROM.h>
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
    AirHumidityWarning = 1,
    AirTemperWarning   = 2,
    UVIndexWarning     = 3,
    NoWaterWarning     = 4,
};
typedef enum WarningStatus WarningStatusType;
WarningStatusType SystemWarning;


struct Limens 
{
    unsigned char UVIndex_Limen       = 9;
    unsigned char DHTHumidity_Hi      = 60;
    unsigned char DHTHumidity_Low     = 0;
    unsigned char DHTTemperature_Hi   = 30;
    unsigned char DHTTemperature_Low  = 0;
    unsigned char MoisHumidity_Limen  = 0;
    float         WaterVolume         = 0.2;
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

#define OneSecond       1000
#define DataUpdateInterval 10000  // 20S
#define RelayOn         HIGH
#define RelayOff        LOW

#define NoWaterTimeOut  3        // 10s

unsigned int  uiWaterVolume = 0;
unsigned char WaterflowFlag = 0;
unsigned int  WaterflowRate = 0;  // L/Hour
unsigned int  NbTopsFan     = 0;  // count the edges

unsigned char EncoderFlag = 0;
unsigned long StartTime   = 0;
unsigned char ButtonFlag  = 0;
signed   char LCDPage     = 4;
unsigned char SwitchtoWateringFlag = 0;
unsigned char SwitchtoWarningFlag  = 0;
unsigned char SwitchtoStandbyFlag  = 0;
unsigned char UpdateDataFlag  = 0;
unsigned char ButtonIndex = 0;
unsigned char EEPROMAddress = 0;
float Volume     = 0;
unsigned long counter = 0;

SI114X SI1145 = SI114X();
DHT dht(DHTPIN, DHTTYPE);
float DHTHumidity    = 0;
float DHTTemperature = 0;
float MoisHumidity   = 100;
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
    Serial.println("Starting up...");
    dht.begin();
    
    /* Init Button */
    pinMode(ButtonPin,INPUT);
    attachInterrupt(0,ButtonClick,FALLING);
    /* Init Encoder */
    pinMode(EncoderPin1,INPUT);
    pinMode(EncoderPin2,INPUT);
    attachInterrupt(1,EncoderRotate,RISING);   

    /* Init UV */
    while (!SI1145.Begin()) {
        delay(1000);
    }

    
    /* Init Water flow */
    pinMode(WaterflowPin,INPUT);
    
    /* Init Relay      */
    pinMode(RelayPin,OUTPUT);
    /* The First time power on to write the default data to EEPROM */
    if (EEPROM.read(EEPROMAddress) == 0xff) {
        EEPROM.write(EEPROMAddress,0x00);
        EEPROM.write(++EEPROMAddress,SystemLimens.UVIndex_Limen);
        EEPROM.write(++EEPROMAddress,SystemLimens.DHTHumidity_Hi);
        EEPROM.write(++EEPROMAddress,SystemLimens.DHTHumidity_Low);
        EEPROM.write(++EEPROMAddress,SystemLimens.DHTTemperature_Hi);
        EEPROM.write(++EEPROMAddress,SystemLimens.DHTTemperature_Low);
        EEPROM.write(++EEPROMAddress,SystemLimens.MoisHumidity_Limen);
        EEPROM.write(++EEPROMAddress,((int)(SystemLimens.WaterVolume*100))/255);    /*  */
        EEPROM.write(++EEPROMAddress,((int)(SystemLimens.WaterVolume*100))%255);
    } else { /* If It's the first time power on , read the last time data */
        EEPROMAddress++;
        SystemLimens.UVIndex_Limen      = EEPROM.read(EEPROMAddress++);
        SystemLimens.DHTHumidity_Hi     = EEPROM.read(EEPROMAddress++);
        SystemLimens.DHTHumidity_Low    = EEPROM.read(EEPROMAddress++);
        SystemLimens.DHTTemperature_Hi  = EEPROM.read(EEPROMAddress++);
        SystemLimens.DHTTemperature_Low = EEPROM.read(EEPROMAddress++);
        SystemLimens.MoisHumidity_Limen = EEPROM.read(EEPROMAddress++);
        SystemLimens.WaterVolume =   (EEPROM.read(EEPROMAddress++)*255 + EEPROM.read(EEPROMAddress))/100.0;
    }
    
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
        if (millis() - StartTime > DataUpdateInterval) {
            StartTime      = millis();
            DHTHumidity    = dht.readHumidity();
            DHTTemperature = dht.readTemperature();
            MoisHumidity   = analogRead(MoisturePin)/7;
            UVIndex        = (float)SI1145.ReadUV()/100 + 0.5;
            if (MoisHumidity >100) {
                MoisHumidity = 100;
            }
            if (MoisHumidity < SystemLimens.MoisHumidity_Limen) {
                SwitchtoWateringFlag = 1;
            }  
            if (DHTHumidity < SystemLimens.DHTHumidity_Low || DHTHumidity > SystemLimens.DHTHumidity_Hi) {  /* DHTHumidity anomaly */
                SystemWarning = AirHumidityWarning;
            }
            else if ((DHTTemperature>SystemLimens.DHTTemperature_Hi) || (DHTTemperature < SystemLimens.DHTTemperature_Low)) {
                SystemWarning = AirTemperWarning;
            }
            else if (UVIndex > SystemLimens.UVIndex_Limen) {
                SystemWarning = UVIndexWarning;
            }
            else if (SystemWarning == NoWaterWarning) {
                SystemWarning = NoWaterWarning;
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
                DisplayAirHumidity();
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
            
            if (UpdateDataFlag == 1) {
                UpdateDataFlag = 0;
                EEPROMAddress  = 0;
                EEPROM.write(++EEPROMAddress,SystemLimens.UVIndex_Limen);
                EEPROM.write(++EEPROMAddress,SystemLimens.DHTHumidity_Hi);
                EEPROM.write(++EEPROMAddress,SystemLimens.DHTHumidity_Low);
                EEPROM.write(++EEPROMAddress,SystemLimens.DHTTemperature_Hi);
                EEPROM.write(++EEPROMAddress,SystemLimens.DHTTemperature_Low);
                EEPROM.write(++EEPROMAddress,SystemLimens.MoisHumidity_Limen);
                EEPROM.write(++EEPROMAddress,((int)(SystemLimens.WaterVolume*100))/255);    /*  */
                EEPROM.write(++EEPROMAddress,((int)(SystemLimens.WaterVolume*100))%255);
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
        SeeedOled.setTextXY(2,4);
        SeeedOled.putString("Warning!");
        switch (SystemWarning) {
        case AirHumidityWarning:
            if (DHTHumidity < SystemLimens.DHTHumidity_Low ) {
                SeeedOled.setTextXY(5,2);
                SeeedOled.putString("Humidity  Low");
            } else {
                SeeedOled.setTextXY(5,2);
                SeeedOled.putString("Humidity High");
            }
            break;
        case AirTemperWarning:
            if (DHTTemperature < SystemLimens.DHTTemperature_Low) {
                SeeedOled.setTextXY(5,3);
                SeeedOled.putString("Temp  Low");
            } else {
                SeeedOled.setTextXY(5,3);
                SeeedOled.putString("Temp High");
            }                
            break;
        case UVIndexWarning:
            if (DHTTemperature > SystemLimens.UVIndex_Limen) {
                SeeedOled.setTextXY(5,4);
                SeeedOled.putString("UV   High");
            } else {
            }
            break;
        case NoWaterWarning:
                SeeedOled.setTextXY(5,3);
                SeeedOled.putString("No  Water");
                if (ButtonFlag == 1) {
                    ButtonFlag = 0;
                    // SwitchtoStandbyFlag = 1;
                }  
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
        if (ButtonFlag == 1) {
            ButtonFlag = 0;
            WorkingStatus = Watering;
            WaterPumpOn();
            StartTime = millis();
        }
        break;
    case Watering:
        SwitchtoWarningFlag = 0;
        SeeedOled.setNormalDisplay();
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
        static char NoWaterTime = 0;
        if ((millis() - StartTime) > OneSecond ) {
            WaterflowRate = (NbTopsFan*60 / 73);
            if(((float)(NbTopsFan) / 73/60) < 0.005 ) {
                if(NoWaterTime++ >= NoWaterTimeOut) {
                    NoWaterTime = 0;
                    SystemWarning = NoWaterWarning;
                    SwitchtoWarningFlag = 1;
                    LCDPage = 3;
                }
                Volume       += (float)((float)(NbTopsFan) / 73/60);
            } else {
                NoWaterTime = 0;
                SystemWarning = NoWarning;
                SwitchtoWarningFlag = 0;
                Volume       += (float)((float)(NbTopsFan) / 73/60 + 0.005);
            }
           
            NbTopsFan = 0;
            sprintf(buffer,"%2d L/H",WaterflowRate);
            SeeedOled.setTextXY(2,10);
            SeeedOled.putString(buffer);  
            if ((int)((int)(Volume*100) %100) < 10 ) {
                sprintf(buffer,"%2d.0%d L",(int)(Volume),(int)((int)(Volume*100) %100));
            } else {
                sprintf(buffer,"%2d.%2d L",(int)(Volume),(int)((int)(Volume*100) %100));
            }
            SeeedOled.setTextXY(4,8);
            SeeedOled.putString(buffer);  
            
            sprintf(buffer,"%d.%d%%",(int)(MoisHumidity),(int)((int)(MoisHumidity*100) % 100));
            SeeedOled.setTextXY(6,6);          //Set the cursor to Xth Page, Yth Column  
            SeeedOled.putString(buffer);
            
            if (Volume >= SystemLimens.WaterVolume) {
                SwitchtoStandbyFlag = 1;
            }
//            
            // sprintf(buffer,"Press Btn toSTOP");
            // SeeedOled.setTextXY(7,0);
            // SeeedOled.putString(buffer);
            StartTime = millis();
        }
        if (ButtonFlag == 1) {
            ButtonFlag = 0;
            SwitchtoStandbyFlag = 1;
        }
        if (SwitchtoStandbyFlag == 1) {
            Volume = 0;
            WorkingStatus = Standby;
            WaterPumpOff();
            SeeedOled.clearDisplay();
            SeeedOled.setTextXY(4,2);
            SeeedOled.putString("Done Watering");
            SwitchtoStandbyFlag = 0;
            ButtonFlag = 0;
            LCDPage = 3;
            delay(DataUpdateInterval);
            StartTime = millis();
        } 
        if (SwitchtoWarningFlag == 1) {
            Volume = 0;
            SwitchtoWarningFlag = 0;
            WorkingStatus = Warning;
            StartTime = millis();
            WaterPumpOff();
            SeeedOled.clearDisplay();
            SeeedOled.setInverseDisplay();
            ButtonFlag = 0;
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
    sprintf(buffer,"UV Index");
    SeeedOled.setTextXY(1,4);          //Set the cursor to Xth Page, Yth Column  
    SeeedOled.putString(buffer);

    sprintf(buffer,"%d",(int)(UVIndex));
    SeeedOled.setTextXY(2,7);          //Set the cursor to Xth Page, Yth Column  
    SeeedOled.putString(buffer);
    
    sprintf(buffer,"Safe Value");
    SeeedOled.setTextXY(5,3);
    SeeedOled.putString(buffer);
    
    sprintf(buffer,"%2d",(int)(SystemLimens.UVIndex_Limen)); 
    SeeedOled.setTextXY(6,7);
    
    SeeedOled.putString(buffer);
    
    if (digitalRead(ButtonPin) == 1) {
        Serial.println("BUTTON");
        if (EncoderFlag == 1) {
        delay(100);
        EncoderFlag = 0;
        UpdateDataFlag  = 1;
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
        if (SystemLimens.UVIndex_Limen > 15) {
            SystemLimens.UVIndex_Limen = 15;
        }
        if (SystemLimens.UVIndex_Limen <= 0) {
            SystemLimens.UVIndex_Limen = 0;
        }
        } 
    }
    
    
}

void DisplayAirHumidity()
{
    sprintf(buffer,"Humidity");
    SeeedOled.setTextXY(1,4);          //Set the cursor to Xth Page, Yth Column  
    SeeedOled.putString(buffer);

    sprintf(buffer,"%d.%d%%",(int)(DHTHumidity),(int)((int)(DHTHumidity*100) % 100));
    SeeedOled.setTextXY(2,6);          //Set the cursor to Xth Page, Yth Column  
    SeeedOled.putString(buffer);
    
    sprintf(buffer,"< Safe Value <"); 
    SeeedOled.setTextXY(5,2);
    SeeedOled.putString(buffer);
    
    sprintf(buffer,"%2d.0%%",(int)(SystemLimens.DHTHumidity_Low)); 
    SeeedOled.setTextXY(7,0);
    SeeedOled.putString(buffer); 
    
    sprintf(buffer,"%2d.0%%",(int)(SystemLimens.DHTHumidity_Hi)); 
    SeeedOled.setTextXY(7,10);
    SeeedOled.putString(buffer);
    
    if(ButtonIndex == 0) {
        sprintf(buffer,"Low<--     High"); 
        SeeedOled.setTextXY(6,1);
        SeeedOled.putString(buffer);
        if (digitalRead(ButtonPin) == 1) {
            Serial.println("BUTTON");
            if (EncoderFlag == 1) {
                delay(100);
                EncoderFlag = 0;
                UpdateDataFlag  = 1;
                switch(EncoderRoateDir) {
                case Clockwise:
                    SystemLimens.DHTHumidity_Low++;
                    break;
                case Anticlockwise:
                    SystemLimens.DHTHumidity_Low--;
                    break;
                default:
                    break;
                }
                if (SystemLimens.DHTHumidity_Low > SystemLimens.DHTHumidity_Hi) {
                    SystemLimens.DHTHumidity_Low = SystemLimens.DHTHumidity_Hi;
                }
                if (SystemLimens.DHTHumidity_Low <= 0) {
                    SystemLimens.DHTHumidity_Low = 0;
                }
            } 
        }
    } else {
        sprintf(buffer,"Low     -->High"); 
        SeeedOled.setTextXY(6,1);
        SeeedOled.putString(buffer);
        if (digitalRead(ButtonPin) == 1) {

            Serial.println("BUTTON");
            if (EncoderFlag == 1) {
                delay(100);
                EncoderFlag = 0;
                UpdateDataFlag  = 1;
                switch(EncoderRoateDir) {
                case Clockwise:
                    SystemLimens.DHTHumidity_Hi++;
                    break;
                case Anticlockwise:
                    SystemLimens.DHTHumidity_Hi--;
                    break;
                default:
                    break;
                }
                if (SystemLimens.DHTHumidity_Hi > 100) {
                    SystemLimens.DHTHumidity_Hi = 100;
                }
                if (SystemLimens.DHTHumidity_Hi <= SystemLimens.DHTHumidity_Low) {
                    SystemLimens.DHTHumidity_Hi = SystemLimens.DHTHumidity_Low;
                }
            } 
        }
    }
}

void DisplayAirTemp()
{
    sprintf(buffer,"Temperature");
    SeeedOled.setTextXY(1,2);          //Set the cursor to Xth Page, Yth Column  
    SeeedOled.putString(buffer);

    sprintf(buffer,"%d.%d*C",(int)(DHTTemperature),(int)((int)(DHTTemperature*100)%100));
    SeeedOled.setTextXY(2,5);          //Set the cursor to Xth Page, Yth Column  
    SeeedOled.putString(buffer);
    
    sprintf(buffer,"< Safe Value <"); 
    SeeedOled.setTextXY(5,2);
    SeeedOled.putString(buffer);
    sprintf(buffer,"%2d.0*C",(int)(SystemLimens.DHTTemperature_Low)); 
    SeeedOled.setTextXY(7,0);
    SeeedOled.putString(buffer); 
    
    sprintf(buffer,"%2d.0*C",(int)(SystemLimens.DHTTemperature_Hi)); 
    SeeedOled.setTextXY(7,10);
    SeeedOled.putString(buffer);

    if(ButtonIndex == 0) {
        sprintf(buffer,"Low<--     High"); 
        SeeedOled.setTextXY(6,1);
        SeeedOled.putString(buffer);
        if (digitalRead(ButtonPin) == 1) {
                      Serial.println("BUTTON");
            if (EncoderFlag == 1) {
                delay(100);
                EncoderFlag = 0;
                UpdateDataFlag  = 1;
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
    } else {
        sprintf(buffer,"Low     -->High"); 
        SeeedOled.setTextXY(6,1);
        SeeedOled.putString(buffer);
        if (digitalRead(ButtonPin) == 1) {
            Serial.println("BUTTON");
            if (EncoderFlag == 1) {
                delay(100);
                EncoderFlag = 0;
                UpdateDataFlag  = 1;
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
    }
}

void DisplayMoisture() 
{
    sprintf(buffer,"Moisture");
    SeeedOled.setTextXY(1,4);          //Set the cursor to Xth Page, Yth Column  
    SeeedOled.putString(buffer);

    sprintf(buffer,"%d.%d%%",(int)(MoisHumidity),(int)((int)(MoisHumidity*100) % 100));
    SeeedOled.setTextXY(2,6);          //Set the cursor to Xth Page, Yth Column  
    SeeedOled.putString(buffer);
   
    
    SeeedOled.setTextXY(5,3);
    SeeedOled.putString("Safe Value");
    
    sprintf(buffer,"%d.0%%",(int)(SystemLimens.MoisHumidity_Limen)); 
    SeeedOled.setTextXY(6,6);
    SeeedOled.putString(buffer);  
    if (digitalRead(ButtonPin) == 1) {
        Serial.println("BUTTON");
        if (EncoderFlag == 1) {
        delay(100);
        EncoderFlag = 0;
        UpdateDataFlag  = 1;
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
    SeeedOled.setTextXY(0,0);          //Set the cursor to Xth Page, Yth Column  
    SeeedOled.putString("Smart Plant Care");    
    SeeedOled.setTextXY(2,0);          //Set the cursor to Xth Page, Yth Column  
    SeeedOled.putString("Press Button to");
    SeeedOled.setTextXY(3,0);          //Set the cursor to Xth Page, Yth Column  
    SeeedOled.putString("start watering");
    sprintf(buffer,"Moisture:");
    SeeedOled.setTextXY(6,4);          //Set the cursor to Xth Page, Yth Column  
    SeeedOled.putString(buffer);
    sprintf(buffer,"%d.%d%% ",(int)(MoisHumidity),(int)((int)(MoisHumidity*100) % 100));
    SeeedOled.setTextXY(7,6);          //Set the cursor to Xth Page, Yth Column  
    SeeedOled.putString(buffer);

}

void DisplayVolume()
{
    unsigned int Volumetemp;
    Volumetemp = SystemLimens.WaterVolume*10;
    sprintf(buffer,"WaterVolume");
    SeeedOled.setTextXY(2,3);          //Set the cursor to Xth Page, Yth Column  
    SeeedOled.putString(buffer);

    SeeedOled.setTextXY(5,2);
    SeeedOled.putString("Volume Limen");
    
    sprintf(buffer,"%2d.%dL",(Volumetemp/10),(Volumetemp%10)); 
    SeeedOled.setTextXY(6,5);

    SeeedOled.putString(buffer);
    
    if (digitalRead(ButtonPin) == 1) {
        Serial.println("BUTTON");
        if (EncoderFlag == 1) {
        delay(100);
        EncoderFlag = 0;
        UpdateDataFlag  = 1;
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
    
    if(digitalRead(ButtonPin) == 0){
        delay(10);
        if(digitalRead(ButtonPin) == 0){
            ButtonFlag = 1;
        }
    }  
}


void EncoderRotate()
{
    if(digitalRead(EncoderPin1) == 1) {
        delay(10);
        if(digitalRead(EncoderPin1) == 1) {
            if(EncoderFlag == 0) {
                EncoderFlag = 1;
                if(digitalRead(EncoderPin2) == 1) {
                    EncoderRoateDir = Clockwise;
                } else {
                    EncoderRoateDir = Anticlockwise;
                }
           }
        } else {
        }
    } else {
    }
}

