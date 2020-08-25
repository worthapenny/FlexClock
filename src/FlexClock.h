#include <Arduino.h>
#include "OLEDDisplay.h"
#include <SH1106Wire.h>
#include <ESP8266HTTPClient.h>

enum FRAME_MIN_POINT
{
    FC_FMP_ALL = 0,
    FC_FMP_HOUR = 1,
    FC_FMP_QUAD = 2,
    FC_FMP_EXCEPT_HOUR = 3,
    FC_FMP_EXCEPT_QUAD = 4
};

struct line
{
    float stX;
    float spX;
    float stY;
    float spY;

    String toStr()
    {
        char str[40];
        sprintf(str, "(%2.2f, %2.2f) --> (%2.2f, %2.2f)", stX, stY, spX, spY);
        return String(str);
    }
};

struct hourMinSec
{
    uint16_t hr;
    double hrAngle;
    char hrStr[3];

    uint16_t minute;
    double minuteAngle;
    char minuteStr[3];

    uint16_t sec;
    double secAngle;
    char secStr[3];

    void set(uint seconds)
    {
        hr = seconds / 3600;
        minute = (seconds % 3600) / 60;
        sec = (seconds % 60);

        setAngles();

        itoa(hr, hrStr, 10);
        itoa(minute, minuteStr, 10);
        itoa(sec, secStr, 10);
    }

    void parseStr(String timeStr)
    {
        if (timeStr.length() >= 8)
        {
            char timeStrChars[9];
            timeStr.toCharArray(timeStrChars, 9);
            hrStr[0] = timeStrChars[0];
            hrStr[1] = timeStrChars[1];
            hrStr[2] = '\0';
            minuteStr[0] = timeStrChars[3];
            minuteStr[1] = timeStrChars[4];
            minuteStr[2] = '\0';
            secStr[0] = timeStrChars[6];
            secStr[1] = timeStrChars[7];
            secStr[2] = '\0';

            hr = atoi(hrStr);
            minute = atoi(minuteStr);
            sec = atoi(secStr);

            setAngles();
        }
    }

    void setAngles()
    {
        //uint16_t hr12 = hr > 12 ? hr - 12 : hr;
        // secAngle = 6 * sec - 90;
        // minuteAngle = 6 * minute  - 90;
        // hrAngle = 30 * hr + 0.5 * minute - 90;

        secAngle = 6.0 * sec;
        minuteAngle = 6.0 * minute + secAngle / 60.0;
        hrAngle = 30.0 * hr + minuteAngle / 12.0;

        secAngle -= 90.0;
        minuteAngle -= 90.0;
        hrAngle -= 90.0;
    }

    uint16_t getSecondsSum()
    {
        return hr * 3600 + minute * 60 + sec;
    }

    String toStr()
    {
        return String(hrStr) + ":" + String(minuteStr) + ":" + String(secStr);
    }
    String toAngleStr()
    {
        return String(hrAngle) + ":" + String(minuteAngle) + ":" + String(secAngle);
    }
};

class FlexClock
{

public:
    static String TIME_URL;
    static String CLOCK_HOURS[12];
    static String CLOCK_HOURS_ROMAN[12];
    static bool FC_DEBUG_TO_SERIAL;

    FlexClock(
        uint16_t clockScale = 100, uint16_t hrArmScale = 50, uint16_t minArmScale = 75, uint16_t secArmScale = 90, int16_t offsetX = 0, int16_t offsetY = 0);

    void init(OLEDDisplay *display, bool isDebug = false);
    void drawFrameBorder(uint16_t thickness);
    void drawFrameNotches(FRAME_MIN_POINT hourPoints = FC_FMP_HOUR, uint16_t len = 3, int16_t offset = 0);
    void drawFrameText(FRAME_MIN_POINT hourPoints = FC_FMP_QUAD, bool inRoman = false, int16_t offset = 0);
    void drawFrameDots(FRAME_MIN_POINT hourPoints = FC_FMP_EXCEPT_HOUR, uint16_t size = 2, int16_t offset = 0);
    void drawCenterDot(uint16_t size);
    void drawHourArm();
    void drawMinuteArm();
    void drawSecondArm();
    void setOffsets(int16_t x, int16_t y);
    void setClockScale(uint16_t scale);
    void incrementSeconds();
    hourMinSec getTime();
    hourMinSec fetchCurrentTime(bool isDebug = false);


private:
    line drawArm(double scale, double angle);
    void drawLine(line l);
    void eraseLine(line l);
    double rotateX(double angle);
    double rotateY(double angle);
    String splitAndGet(String data, char separator, uint16_t index);
    void applyScale();

    OLEDDisplay *_screen;
    uint16_t _lengthX;
    uint16_t _lengthY;
    uint16_t _originX;
    uint16_t _originY;
    uint16_t _ptX;
    uint16_t _ptY;
    hourMinSec _hrMinSec;

    uint16_t _seconds;
    double radius;
    double _radius;
    double hrArmScale;
    double _hrArmScale;
    double minArmScale;
    double _minArmScale;
    double secArmScale;
    double _secArmScale;
    double clockScale;
    double _clockScale;
    double offsetScaleX;
    double _offsetScaleX;
    double offsetScaleY;
    double _offsetScaleY;
    double originX;
    double originY;
    double _maxOffsetX;
    double _maxOffsetY;

    line _l;
    line _hrArmLine;
    line _minArmLine;
    line _secArmLine;
};
