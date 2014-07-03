//Serial Data Logger by Jack Christensen is licensed under CC BY-SA 4.0,
//http://creativecommons.org/licenses/by-sa/4.0/

//buffer class

const uint16_t BUFSIZE = 256;            //serial receive buffer size

class buffer
{
    public:
        buffer(void);
        void init(void);
        void assignLEDs(int8_t writeLED, int8_t overrunLED);
        int write(SdFile* f, bool flush = false);
        uint8_t buf[BUFSIZE];
        uint8_t* p;
        uint16_t nchar;
        bool writeFlag;
        static bool ovrFlag;

    private:
        static int8_t _writeLED;
        static int8_t _overrunLED;
};

bool buffer::ovrFlag = false;
int8_t buffer::_writeLED = -1;
int8_t buffer::_overrunLED = -1;

//constructor
buffer::buffer(void)
{
    init();
}

//initialize the buffer
void buffer::init(void)
{
    p = buf;                //point at the first byte
    nchar = 0;              //buffer is empty
    writeFlag = false;      //does not need to be written to SD
    ovrFlag = false;        //no overruns
}

void buffer::assignLEDs(int8_t writeLED, int8_t overrunLED)
{
    _writeLED = writeLED;
    _overrunLED = overrunLED;
    pinMode(_writeLED, OUTPUT);
    pinMode(_overrunLED, OUTPUT);
}

//if the flush flag is set, write the buffer to SD if it is not empty
//else write it only if the writeFlag is set
int buffer::write(SdFile* f, bool flush)
{
    int ret = 0;

    if ( (flush & (nchar > 0)) || writeFlag ) {
        if (_writeLED >= 0 ) digitalWrite(_writeLED, HIGH);
        writeFlag = false;             //reset the flag
        ret = f -> write(buf, nchar);
        nchar = 0;                     //the buffer is empty/available again
        if (_writeLED >= 0 ) digitalWrite(_writeLED, LOW);
    }
    digitalWrite(_overrunLED, ovrFlag);
    return ret;
}

//poor man's Serial.println() substitute
void writeUSART0(char* buf)
{
    char c;

    uint8_t ucsr0b = UCSR0B;

    while ( (c = *buf++) != 0 ) {
        loop_until_bit_is_set(UCSR0A, UDRE0);
        UDR0 = c;
    }
    loop_until_bit_is_set(UCSR0A, UDRE0);
    UDR0 = '\r';
    loop_until_bit_is_set(UCSR0A, UDRE0);
    UDR0 = '\n';
    UCSR0B = ucsr0b;
}
