// Serial Data Logger
// https://github.com/JChristensen/serialLogger
// Copyright (C) 2018 by Jack Christensen and licensed under
// GNU GPL v3.0, https://www.gnu.org/licenses/gpl.html

// heartbeat LED with various blink modes

enum blinkMode_t {BLINK_IDLE, BLINK_RUN, BLINK_ERROR, BLINK_NO_CARD};

class heartbeat
{
    public:
        heartbeat(uint8_t pin);
        void begin(blinkMode_t m);
        void run();
        void mode(blinkMode_t m);

    private:
        uint8_t m_pin;
        uint8_t m_ledMask;
        volatile uint8_t* m_ledReg;
        uint32_t m_msOn;
        uint32_t m_msOff;
        uint32_t m_interval;
        bool m_state;
        uint32_t m_msLastChange;
};

// constructor
heartbeat::heartbeat(uint8_t pin)
{
    m_pin = pin;
    m_ledMask = digitalPinToBitMask(m_pin);     // save some cycles
    uint8_t port = digitalPinToPort(m_pin);
    m_ledReg = portOutputRegister(port);
}

// hardware init
void heartbeat::begin(blinkMode_t m)
{
    pinMode(m_pin, OUTPUT);
    m_state = false;
    *m_ledReg &= ~m_ledMask;
    mode(m);
}

void heartbeat::run()
{
    uint32_t ms = millis();
    if ( ms - m_msLastChange >= m_interval )
    {
        m_msLastChange = ms;
        if ( (m_state = !m_state) )
            *m_ledReg |= m_ledMask;
        else
            *m_ledReg &= ~m_ledMask;
        m_interval = m_state ? m_msOn : m_msOff;
    }
}

void heartbeat::mode(blinkMode_t m)
{
    switch (m)
    {
        case BLINK_IDLE:
            m_msOn = 50;
            m_msOff = 950;
            break;

        case BLINK_RUN:
            m_msOn = 500;
            m_msOff = 500;
            break;

        case BLINK_ERROR:
            m_msOn = 100;
            m_msOff = 100;
            break;

        case BLINK_NO_CARD:
            m_msOn = 2000;
            m_msOff = 2000;
            break;
    }
    m_state = false;
    m_msLastChange = 0;
    run();
}

