#ifndef CheckInputs_H
#define CheckInputs_H

#include "../Timer.hpp"

class CheckInputs : public Timer
{
public:
    CheckInputs();
    static CheckInputs *checkInputs;
    void setup();
    void loop();
    void exec();
    bool valueIsUP(uint8_t index);
    uint64_t getLastDown(uint8_t index);
private:
    static const int inputStop;

    //power, [0] = output pin, [1] = initial value
    static const int power[];
    static const int powercount;

    //the first input need match power[], [0] = input pin, [1] = initial value
    static const int inputs[][2];
    static const int inputcount;
    static const int valueCount;

    static const int lowValueHysteresis;
    static const int highValueHysteresis;
    //stop default to true, value: power, stop

    int indexLastValues=0;
    uint64_t *lastDown;
    uint64_t *downCountForReturn;//only updated when crom from HIGH to LOW
    bool *inputValues;
    bool *lastValues;
    int *sumValuesTrue;

    int lastPowerNumber;
    uint64_t previousMillis;        // will store last time was updated
    uint64_t interval;
    uint64_t upIfLastDownIsGreaterMsThan;
};

#endif // CheckInputs_H
