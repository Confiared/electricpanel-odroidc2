#include "CheckInputs.hpp"
#include <wiringPi.h>
#include <chrono>
#include <iostream>
#include <stdlib.h>//for system()

/*
 +-----+-----+---------+------+---+--- C2 ---+---+------+---------+-----+-----+
 | I/O | wPi |   Name  | Mode | V | Physical | V | Mode |  Name   | wPi | I/O |
 +-----+-----+---------+------+---+----++----+---+------+---------+-----+-----+
 |     |     |    3.3V |      |   |  1 || 2  |   |      | 5V      |     |     |
 | 205 |   8 |   SDA.1 |   IN | 1 |  3 || 4  |   |      | 5V      |     |     |
 | 206 |   9 |   SCL.1 |   IN | 1 |  5 || 6  |   |      | 0V      |     |     |
 | 249 |   7 |  IO.249 |   IN | 1 |  7 || 8  |   |      | TxD1    | 15  |     |
 |     |     |      0V |      |   |  9 || 10 |   |      | RxD1    | 16  |     |
 | 247 |   0 |  IO.247 |   IN | 1 | 11 || 12 | 1 | IN   | IO.238  | 1   | 238 |
 | 239 |   2 |  IO.239 |   IN | 1 | 13 || 14 |   |      | 0V      |     |     |
 | 237 |   3 |  IO.237 |   IN | 1 | 15 || 16 | 1 | IN   | IO.236  | 4   | 236 |
 |     |     |    3.3V |      |   | 17 || 18 | 1 | IN   | IO.233  | 5   | 233 |
 | 235 |  12 |  IO.235 |  OUT | 0 | 19 || 20 |   |      | 0V      |     |     |
 | 232 |  13 |  IO.232 |  OUT | 1 | 21 || 22 | 1 | IN   | IO.231  | 6   | 231 |
 | 230 |  14 |  IO.230 |  OUT | 1 | 23 || 24 | 1 | IN   | IO.229  | 10  | 229 |
 |     |     |      0V |      |   | 25 || 26 | 1 | OUT  | IO.225  | 11  | 225 |
 |     |  30 |   SDA.2 |      |   | 27 || 28 |   |      | SCL.2   | 31  |     |
 | 228 |  21 |  IO.228 |   IN | 1 | 29 || 30 |   |      | 0V      |     |     |
 | 219 |  22 |  IO.219 |   IN | 0 | 31 || 32 | 1 | IN   | IO.224  | 26  | 224 |
 | 234 |  23 |  IO.234 |   IN | 1 | 33 || 34 |   |      | 0V      |     |     |
 | 214 |  24 |  IO.214 |   IN | 0 | 35 || 36 | 1 | IN   | IO.218  | 27  | 218 |
 |     |  25 |   AIN.1 |      |   | 37 || 38 |   |      | 1V8     | 28  |     |
 |     |     |      0V |      |   | 39 || 40 |   |      | AIN.0   | 29  |     |
 +-----+-----+---------+------+---+----++----+---+------+---------+-----+-----+
 | I/O | wPi |   Name  | Mode | V | Physical | V | Mode |  Name   | wPi | I/O |
 +-----+-----+---------+------+---+--- C2 ---+---+------+---------+-----+-----+
 */

CheckInputs *CheckInputs::checkInputs=nullptr;

int CheckInputs::inputStop=24;

//power, [0] = output pin
std::vector<int> CheckInputs::power;
//the first input need match power[], [0] = input pin, [1] = initial value
std::vector<CheckInputs::InputV> CheckInputs::inputs;

const int CheckInputs::valueCount=256;

const int CheckInputs::lowValueHysteresis=30;
const int CheckInputs::highValueHysteresis=70;

CheckInputs::CheckInputs()
{
    //stop default to true, value: power, stop
    indexLastValues=0;
    lastDown=new uint64_t[CheckInputs::inputs.size()];
    if(lastDown==nullptr)
    {
        std::cerr << "lastDown==nullptr (abort)" << std::endl;
        abort();
    }
    downCountForReturn=new uint64_t[CheckInputs::inputs.size()];
    if(downCountForReturn==nullptr)
    {
        std::cerr << "downCountForReturn==nullptr (abort)" << std::endl;
        abort();
    }
    inputValues=(bool*)malloc(sizeof(bool)*CheckInputs::inputs.size()*valueCount);
    if(inputValues==nullptr)
    {
        std::cerr << "inputValues==nullptr (abort)" << std::endl;
        abort();
    }
    lastValues=new bool[CheckInputs::inputs.size()];
    if(lastValues==nullptr)
    {
        std::cerr << "lastValues==nullptr (abort)" << std::endl;
        abort();
    }
    sumValuesTrue=new int[CheckInputs::inputs.size()];
    if(sumValuesTrue==nullptr)
    {
        std::cerr << "sumValuesTrue==nullptr (abort)" << std::endl;
        abort();
    }

    lastPowerNumber = -1;
    previousMillis = 0;        // will store last time was updated
    interval = 1000;
    upIfLastDownIsGreaterMsThan = 10000;

    setup();
}

void CheckInputs::setup()
{
    wiringPiSetup();

    for( size_t i = 0; i < CheckInputs::inputs.size(); i++ )
    {
        lastDown[i]=0;
        downCountForReturn[i]=0;
    }

    for( size_t i = 0; i < CheckInputs::power.size(); i++ )
    {
        // set the digital pin as output:
        pinMode(power[i], OUTPUT);
        digitalWrite(power[i], HIGH);
        //VERY IMPORTANT due to delay of relay to change, if drop then short circuit damage
        //shorter delay to init faster, presume all relay was in position of no energy
        //delay(100);
    }

    pinMode(inputStop, INPUT);

    for( size_t i = 0; i < CheckInputs::power.size(); i++ )
        pinMode(inputs.at(i).pin, INPUT);
    for( size_t i = 0; i < CheckInputs::inputs.size(); i++ )
    {
        if(!inputs.at(i).value)
        {
            sumValuesTrue[i]=0;
            lastValues[i]=false;
            for( int j = 0; j < valueCount; j++ )
                inputValues[i*valueCount+j]=0;
        }
        else
        {
            sumValuesTrue[i]=valueCount;
            lastValues[i]=true;
            for( int j = 0; j < valueCount; j++ )
                inputValues[i*valueCount+j]=1;
        }
    }

    system("echo default-on > /sys/class/leds/blue:heartbeat/trigger");
}

bool CheckInputs::valueIsUP(uint8_t index)
{
    if(index>=CheckInputs::inputs.size())
        return false;
    else
        return lastValues[index];
}

uint64_t CheckInputs::getLastDown(uint8_t index)
{
    if(index>=CheckInputs::inputs.size())
        return 0;
    else
        return downCountForReturn[index];
}

void CheckInputs::loop()
{
    uint64_t currentMillis = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    int previousStopVar = lastValues[3];
    //parse the input with hysteresis + average to fix some corrupted input
    for( size_t i = 0; i < CheckInputs::inputs.size(); i++ )
    {
        int v = digitalRead(inputs.at(i).pin);
        bool t=inputValues[i*valueCount+indexLastValues];
        if(t==true)
            sumValuesTrue[i]--;
        if(v==HIGH)
            sumValuesTrue[i]++;
        bool l=lastValues[i];
        if(i<CheckInputs::inputs.size()) //in the future, then time drift, fix by 0 because no action before 10s (see upIfLastDownIsGreaterMsThan)
        {
            if(lastDown[i]>currentMillis)
                lastDown[i]=0;
        }
        if(l==false && sumValuesTrue[i]*100/valueCount>highValueHysteresis)
        lastValues[i]=true;
        else if(l==true && sumValuesTrue[i]*100/valueCount<lowValueHysteresis)
        {
            if(lastValues[i]==true)
            {
                if(i<CheckInputs::inputs.size())
                    downCountForReturn[i]++;
            }
            lastValues[i]=false;
            if(i<CheckInputs::inputs.size())
                lastDown[i]=currentMillis;
        }
        inputValues[i*valueCount+indexLastValues]=v;
    }
    indexLastValues++;
    if(indexLastValues>=valueCount)
        indexLastValues=0;

    int stopVar = lastValues[3];
    if(stopVar==HIGH)
    {
        for( size_t i = 0; i < CheckInputs::power.size(); i++ )
            digitalWrite(power[i], HIGH);
        lastPowerNumber = -1;
        previousMillis = currentMillis;
        if(previousStopVar==LOW)
        system("echo default-on > /sys/class/leds/blue:heartbeat/trigger");
    }
    else
    {
        if(previousStopVar==HIGH)
            system("echo none > /sys/class/leds/blue:heartbeat/trigger");
        //detect down
        for( size_t i = 0; i < CheckInputs::power.size(); i++ )
        {
            if(lastValues[i]==false)
                lastDown[i]=currentMillis;
        }

        if(previousMillis>currentMillis)
            previousMillis = currentMillis;
        if(currentMillis>upIfLastDownIsGreaterMsThan && currentMillis - previousMillis > interval)
        {
            int bestPowerNumber=-1;
            //power is considered as UP only if no down during the last 10s
            for( size_t i = 0; i < CheckInputs::power.size(); i++ )
            {
                if(lastDown[i]<(currentMillis-upIfLastDownIsGreaterMsThan))
                {
                    bestPowerNumber=i;
                    break;
                }
            }
            if(bestPowerNumber!=lastPowerNumber)
            {
                if(lastPowerNumber!=-1)
                {
                    for( size_t i = 0; i < CheckInputs::power.size(); i++ )
                        digitalWrite(power[i], HIGH);
                    //VERY IMPORTANT due to delay of relay to change, if drop then short circuit damage
                    lastPowerNumber = -1;
                    previousMillis = currentMillis;
                }
                else
                {
                    if(bestPowerNumber>=0 && bestPowerNumber<(int)CheckInputs::power.size())
                        digitalWrite(power[bestPowerNumber], LOW);
                    lastPowerNumber=bestPowerNumber;
                    //prevent change during 1s
                    previousMillis = currentMillis;
                }
            }
        }
    }
}

void CheckInputs::exec()
{
    loop();
}
