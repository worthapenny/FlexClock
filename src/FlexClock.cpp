#include <Arduino.h>
#include <OLEDDisplay.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <FlexClock.h>
#include <SH1106Wire.h>
#include <ESP8266HTTPClient.h>

String FlexClock::TIME_URL = "http://worldtimeapi.org/api/ip";
String FlexClock::CLOCK_HOURS[12] = {"3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "1", "2"};
String FlexClock::CLOCK_HOURS_ROMAN[12] = {"iii", "iv", "v", "vi", "vii", "viii", "ix", "x", "xi", "xii", "i", "ii"};
bool FlexClock::FC_DEBUG_TO_SERIAL = false;

FlexClock::FlexClock(uint16_t clockScale, uint16_t hrArmScale, uint16_t minArmScale, uint16_t secArmScale, int16_t offsetScaleX, int16_t offsetScaleY)
{
    this->clockScale = clockScale;
    this->hrArmScale = hrArmScale;
    this->minArmScale = minArmScale;
    this->secArmScale = secArmScale;

    this->offsetScaleX = offsetScaleX;
    this->offsetScaleY = offsetScaleY;
};

void FlexClock::init(OLEDDisplay *display, bool isDebug)
{
    clockScale /= 100.0;
    hrArmScale /= 100.0;
    minArmScale /= 100.0;
    secArmScale /= 100.0;
    offsetScaleX /= 100;
    offsetScaleY /= 100;

    if (isDebug)
    {
        Serial.println();
        Serial.println("init:");
        Serial.println("   Values without scaling");
        Serial.printf("   Clock scale: %2.2f\n", clockScale);
        Serial.printf("   Hr. arm scale: %2.2f\n", hrArmScale);
        Serial.printf("   Min. arm scale: %2.2f\n", minArmScale);
        Serial.printf("   Sec. arm scale: %2.2f\n", secArmScale);
        Serial.printf("   Offset X: %2.2f\n", offsetScaleX);
        Serial.printf("   Offset Y: %2.2f\n", offsetScaleY);
    }

    _screen = display;
    _lengthX = _screen->getWidth();
    _lengthY = _screen->getHeight();
    originX = _lengthX / 2 + _offsetScaleX;
    originY = _lengthY / 2 + _offsetScaleY;
    radius = (double)min(_lengthX / 2, _lengthY / 2);

    if (isDebug)
    {
        Serial.println();
        Serial.println("   Without Scaling:");
        Serial.printf("   Display length X: %d\n", _lengthX);
        Serial.printf("   Display length Y: %d\n", _lengthY);
        Serial.printf("   Origin X: %2.2f\n", originX);
        Serial.printf("   Origin Y: %2.2f\n", originY);
        Serial.printf("   Radius: %f\n", radius);
    }
    applyScale();

    // Get DateTime from internet
    _hrMinSec = fetchCurrentTime(isDebug);
    _seconds = _hrMinSec.getSecondsSum();
}

void FlexClock::drawFrameBorder(uint16_t thickness)
{
    for (int i = 1; i <= thickness; i++)
    {
        if (_radius > i)
        {
            _screen->drawCircle(_originX, _originY, _radius - i);
        }
    }
}

void FlexClock::drawFrameNotches(FRAME_MIN_POINT hourPoints, uint16_t len, int16_t offset)
{
    uint16_t step = 6; // ALL
    if (hourPoints == FC_FMP_HOUR)
        step = 30;
    else if (hourPoints == FC_FMP_QUAD)
        step = 90;

    if (FlexClock::FC_DEBUG_TO_SERIAL)
    {
        Serial.println();
        Serial.println("Frame Notches:");
        Serial.println("   Step: " + String(step));
    }

    offset *= _clockScale;

    for (int i = 0; i < 360; i += step)
    {
        if (hourPoints == FC_FMP_EXCEPT_HOUR && i % 30 == 0)
            continue;

        if (hourPoints == FC_FMP_EXCEPT_QUAD && i % 30 == 0)
            continue;

        _l.stX = rotateX(i) * (_radius - len + offset) + _lengthX / 2 + _offsetScaleX;
        _l.stY = rotateY(i) * (_radius - len + offset) + _lengthY / 2 + _offsetScaleY;
        _l.spX = rotateX(i) * (_radius + offset) + _lengthX / 2 + _offsetScaleX;
        _l.spY = rotateY(i) * (_radius + offset) + _lengthY / 2 + _offsetScaleY;
        drawLine(_l);

        if (FlexClock::FC_DEBUG_TO_SERIAL)
        {
            Serial.println("   Coord: " + String(i) + ": " + _l.toStr());
        }
    }
}
void FlexClock::drawFrameText(FRAME_MIN_POINT hourPoints, bool inRoman, int16_t offset)
{
    // Text are only supported for HOURS places
    if (hourPoints == FC_FMP_ALL ||
        hourPoints == FC_FMP_EXCEPT_HOUR)
        return;

    _screen->setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
    _screen->setFont(ArialMT_Plain_10);

    uint16_t step = 30; // HOURS
    if (hourPoints == FC_FMP_QUAD)
        step = 90;

    if (FlexClock::FC_DEBUG_TO_SERIAL)
    {
        Serial.println();
        Serial.println("Frame Text:");
        Serial.println("   Step: " + String(step));
    }

    for (int i = 0; i < 360; i += step)
    {
        if (hourPoints == FC_FMP_EXCEPT_QUAD && i % 90 == 0)
            continue;

        _ptX = rotateX(i) * (_radius - offset) + _lengthX / 2 + _offsetScaleX;
        _ptY = rotateY(i) * (_radius - offset) + _lengthY / 2 + _offsetScaleY;

        _screen->drawString(_ptX, _ptY,
                            inRoman
                                ? FlexClock::CLOCK_HOURS_ROMAN[i / 30]
                                : FlexClock::CLOCK_HOURS[i / 30]);

        if (FlexClock::FC_DEBUG_TO_SERIAL)
        {
            Serial.printf("   Coord: %d: (%d, %d)\n", i, _ptX, _ptY);
        }
    }
}

void FlexClock::drawFrameDots(FRAME_MIN_POINT hourPoints, uint16_t size, int16_t offset)
{
    uint16_t step = 6; // ALL
    if (hourPoints == FC_FMP_HOUR)
        step = 30;
    else if (hourPoints == FC_FMP_QUAD)
        step = 90;

    if (FlexClock::FC_DEBUG_TO_SERIAL)
    {
        Serial.println();
        Serial.println("Frame Dots:");
        Serial.println("   Step: " + String(step));
    }

    for (int i = 0; i < 360; i += step)
    {
        if (hourPoints == FC_FMP_EXCEPT_HOUR && i % 30 == 0)
            continue;

        if (hourPoints == FC_FMP_EXCEPT_QUAD && i % 90 == 0)
            continue;

        _ptX = rotateX(i) * (_radius - offset) + _lengthX / 2 + _offsetScaleX;
        _ptY = rotateY(i) * (_radius - offset) + _lengthY / 2 + _offsetScaleY;

        _screen->fillCircle(_ptX, _ptY, size);
        if (FlexClock::FC_DEBUG_TO_SERIAL)
        {
            Serial.printf("   Coord: %d: (%d, %d)\n", i, _ptX, _ptY);
        }
    }
}
void FlexClock::drawCenterDot(uint16_t size)
{
    _screen->setPixel(_originX, _originY);
    _screen->fillCircle(_originX, _originY, size * _clockScale);
}

void FlexClock::drawHourArm()
{
    eraseLine(_hrArmLine);
    _hrArmLine = drawArm(_hrArmScale, _hrMinSec.hrAngle);
}
void FlexClock::drawMinuteArm()
{
    eraseLine(_minArmLine);
    _minArmLine = drawArm(_minArmScale, _hrMinSec.minuteAngle);
}
void FlexClock::drawSecondArm()
{
    eraseLine(_secArmLine);
    _secArmLine = drawArm(_secArmScale, _hrMinSec.secAngle);
}
void FlexClock::setOffsets(int16_t x, int16_t y)
{
    // make sure clock doesn't get chopped off
    x = x < -100 || x > 100 ? 100 : x;
    y = y < -100 || y > 100 ? 100 : y;

    offsetScaleX = x / 100.0;
    offsetScaleY = y / 100.0;
    applyScale();
}
void FlexClock::setClockScale(uint16_t scale)
{
    clockScale = scale / 100.0;
    applyScale();
}
void FlexClock::incrementSeconds()
{
    _seconds = _seconds == 86400 ? 1 : _seconds + 1;
    _hrMinSec.set(_seconds);
}
hourMinSec FlexClock::getTime()
{
    return _hrMinSec;
}

line FlexClock::drawArm(double scale, double angle)
{
    _l.stX = _originX;
    _l.stY = _originY;
    _l.spX = rotateX(angle) * _radius * scale + _lengthX / 2 + _offsetScaleX;
    _l.spY = rotateY(angle) * _radius * scale + _lengthY / 2 + _offsetScaleY;

    drawLine(_l);
    if (FlexClock::FC_DEBUG_TO_SERIAL)
    {
        Serial.print("Arm coord [" + _hrMinSec.toStr() + "]: ");
        Serial.printf("%2.2f, %2.2f, %2.2f, %2.2f\n", _l.stX, _l.stY, _l.spX, _l.spY);
    }

    return _l;
}

double FlexClock::rotateX(double angle)
{
    return cos(angle * 71 / 4068.0);
}
double FlexClock::rotateY(double angle)
{
    return sin(angle * 71 / 4068.0);
}
void FlexClock::drawLine(line l)
{
    _screen->drawLine(_l.stX, _l.stY, _l.spX, _l.spY);
}
void FlexClock::eraseLine(line l)
{
    // _screen->setColor(BLACK);
    // drawLine(l);
    // _screen->setColor(WHITE);
}

hourMinSec FlexClock::fetchCurrentTime(bool isDebug)
{
    if (FlexClock::FC_DEBUG_TO_SERIAL)
    {
        Serial.println();
        Serial.println("Current Time:");
        Serial.println();
    }

    if (isDebug)
        return _hrMinSec;

    if (WiFi.status() == WL_CONNECTED)
    {
        HTTPClient http;
        http.begin(FlexClock::TIME_URL);

        if (http.GET() > 0)
        {
            String payload = http.getString();
            String strTime = splitAndGet(payload, ',', 2);

            if (FlexClock::FC_DEBUG_TO_SERIAL)
            {
                Serial.println("   Payload: " + payload);
                Serial.println("   1st Split: " + strTime);
            }
            if (!strTime.isEmpty())
            {

                strTime = splitAndGet(strTime, 'T', 1);
                //Serial.println("2nd Split: " + strTime);
                if (!strTime.isEmpty())
                {
                    _hrMinSec.parseStr(strTime);

                    if (FlexClock::FC_DEBUG_TO_SERIAL)
                    {
                        Serial.println("   int: " + String(_hrMinSec.hr) + ":" + String(_hrMinSec.minute) + ":" + String(_hrMinSec.sec));
                        Serial.println("   str: " + String(_hrMinSec.hrStr) + ":" + String(_hrMinSec.minuteStr) + ":" + String(_hrMinSec.secStr));
                        Serial.println("   angle: " + String(_hrMinSec.hrAngle) + ":" + String(_hrMinSec.minuteAngle) + ":" + String(_hrMinSec.secAngle));
                    }
                }
            }
        }

        http.end();
    }
    return _hrMinSec;
}

String FlexClock::splitAndGet(String data, char separator, uint16_t index)
{
    uint16_t found = 0;
    int16_t strIndex[] = {0, -1};
    uint16_t maxIndex = data.length() - 1;

    for (uint16_t i = 0; i <= maxIndex && found <= index; i++)
    {
        if (data.charAt(i) == separator || i == maxIndex)
        {
            found++;
            strIndex[0] = strIndex[1] + 1;
            strIndex[1] = (i == maxIndex) ? i + 1 : i;
        }
    }
    return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void FlexClock::applyScale()
{

    _clockScale = clockScale;
    _radius = radius * _clockScale;
    _hrArmScale = hrArmScale * _clockScale;
    _minArmScale = minArmScale * _clockScale;
    _secArmScale = secArmScale * _clockScale;

    // max offset possible without cutting off the clock
    _maxOffsetX = _screen->getWidth() / 2 - _radius;
    _maxOffsetY = _screen->getHeight() / 2 - _radius;
    _offsetScaleX = offsetScaleX * (_maxOffsetX);
    _offsetScaleY = offsetScaleY * (_maxOffsetY);

    _originX = originX + _offsetScaleX;
    _originY = originY + _offsetScaleY;

    if (FlexClock::FC_DEBUG_TO_SERIAL)
    {
        Serial.println();
        Serial.println("Apply Scale:");
        Serial.printf("   Clock scale: %2.2f\n", _clockScale);
        Serial.printf("   Radius: %f\n", _radius);
        Serial.printf("   Min scale: %2.2f\n", _minArmScale);
        Serial.printf("   Sec scale: %2.2f\n", _secArmScale);
        Serial.printf("   Hr scale: %2.2f\n", _hrArmScale);
        Serial.printf("   Origin X: %d\n", _originX);
        Serial.printf("   Origin Y: %d\n", _originY);
        Serial.printf("   Offset X: %2.2f\n", _offsetScaleX);
        Serial.printf("   Offset Y: %2.2f\n", _offsetScaleY);
    }
}
