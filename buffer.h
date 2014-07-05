//Serial Data Logger by Jack Christensen is licensed under CC BY-SA 4.0,
//http://creativecommons.org/licenses/by-sa/4.0/

const int NBUF = 2;                     //number of receive buffers
const uint16_t BUFSIZE = 256;           //serial receive buffer size

/*-------- buffer class --------*/
class buffer
{
    public:
        buffer(void);
        void init(void);
        int putch(uint8_t c);
        int write(SdFile* f, bool flush = false);

        uint8_t buf[BUFSIZE];           //the buffer
        uint8_t* next;                  //pointer to next available position in buffer
        uint16_t nchar;                 //number of characters in the buffer
        bool writeFlag;                 //buffer is full and needs to be written
};

//constructor
buffer::buffer(void)
{
    init();
}

//initialize the buffer
void buffer::init(void)
{
    next = buf;                         //point at the first byte
    nchar = 0;                          //buffer is empty
    writeFlag = false;                  //does not need to be written
}

int buffer::putch(uint8_t c)
{
}

//write the buffer if full (i.e. writeFlag is set),
//or if the flush flag is set, and the buffer is not empty.
//passes the return code from SdFile.write() back to the caller.
int buffer::write(SdFile* f, bool flush)
{
    int ret = 0;

    if ( (flush & (nchar > 0)) || writeFlag ) {
        writeFlag = false;             //reset the flag
        ret = f -> write(buf, nchar);
        nchar = 0;                     //the buffer is empty/available again
    }
    return ret;
}

/*-------- bufferPool class --------*/
class bufferPool
{
    public:
        bufferPool(uint8_t writeLED = -1);
        void begin(void);
        int putch(uint8_t c);
        int write(SdFile* f, bool flush = false);

        buffer buf[NBUF];
        bool overrun;
        
    private:
        int8_t _writeLED;
};

//constructor
bufferPool::bufferPool(uint8_t writeLED)
{
    _writeLED = writeLED;
}

void bufferPool::begin(void)
{
    if (_writeLED >= 0) pinMode(_writeLED, OUTPUT);
    overrun = false;    
}

int bufferPool::putch(uint8_t c)
{
}
int bufferPool::write(SdFile* f, bool flush)
{
    if (_writeLED >= 0 ) digitalWrite(_writeLED, HIGH);
    if (_writeLED >= 0 ) digitalWrite(_writeLED, LOW);
}

/*
//handle the incoming characters
ISR(USART_RX_vect)
{
    static bool overrun;
    static uint16_t lost;                          //number of lost characters due to overrun
    static uint8_t bufIdx;                         //index to the current buffer
    buffer* bp = &buf[bufIdx];                     //pointer to current buffer
    uint8_t c = UDR0;                              //get the received character

    if ( !overrun ) {
        *(bp -> p++) = c;                          //put the character into the buffer
        if ( ++(bp -> nchar) >= BUFSIZE ) {        //buffer full?
            bp -> writeFlag = true;                //yes, set writeFlag for mainline code
            if ( ++bufIdx >= NBUF ) bufIdx = 0;    //increment index to next buffer
            bp = &buf[bufIdx];                     //point to next buffer
            bp -> p = bp -> buf;                   //initialize the character pointer
            if ( bp -> nchar != 0 ) {              //if mainline code has not zeroed the character count,
                overrun = true;                    //then we have an overrun situation
                bp -> ovrFlag = true;
            }
        }
    }
    else if ( bp -> nchar == 0 ) {                 //has previous overrun cleared?
        overrun = false;
        lost = 0;
        *(bp -> p++) = c;                          //put the character into the buffer
    }
    else {
        ++lost;                                    //count lost characters
    }
}
*/

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
