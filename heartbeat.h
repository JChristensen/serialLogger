//Serial Data Logger by Jack Christensen is licensed under CC BY-SA 4.0,
//http://creativecommons.org/licenses/by-sa/4.0/

//heartbeat LED with various blink modes

enum blinkMode_t { BLINK_IDLE, BLINK_RUN, BLINK_ERROR, BLINK_NO_CARD };

class heartbeat
{
    public:
        heartbeat(uint8_t pin);
        void begin(blinkMode_t m);
        void run(void);
        void mode(blinkMode_t m);

    private:
        uint8_t _pin;
        uint32_t _msOn;
        uint32_t _msOff;
        uint32_t _interval;
        bool _state;
        uint32_t _msLastChange;
};

//constructor
heartbeat::heartbeat(uint8_t pin)
{
    _pin = pin;
}

//hardware init
void heartbeat::begin(blinkMode_t m)
{
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, _state = false);
    mode(m);
}

void heartbeat::run()
{
    uint32_t ms = millis();
    if ( ms - _msLastChange >= _interval ) {
        _msLastChange = ms;
        digitalWrite(_pin, _state = !_state);
        _interval = _state ? _msOn : _msOff;
    }
}

void heartbeat::mode(blinkMode_t m)
{
    switch (m)
    {
    case BLINK_IDLE:
        _msOn = 50;
        _msOff = 950;
        break;

    case BLINK_RUN:
        _msOn = 500;
        _msOff = 500;
        break;

    case BLINK_ERROR:
        _msOn = 100;
        _msOff = 100;
        break;

    case BLINK_NO_CARD:
        _msOn = 2000;
        _msOff = 2000;
        break;
    }
    _state = false;
    _msLastChange = 0;
    run();
}
