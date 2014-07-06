//Serial Data Logger by Jack Christensen is licensed under CC BY-SA 4.0,
//http://creativecommons.org/licenses/by-sa/4.0/

const int NBUF = 2;                     //number of receive buffers
const uint16_t BUFSIZE = 256;           //serial receive buffer size

/*-------- buffer class --------*/
class buffer
{
    public:
        buffer(void);
        void init(int8_t writeLED = -1);
        int putch(uint8_t ch);
        int write(SdFile* f);
        int flush(SdFile* f);

        uint8_t buf[BUFSIZE];           //the buffer
        uint8_t* next;                  //pointer to next available position in buffer
        uint16_t nchar;                 //number of characters in the buffer
        bool writeFlag;                 //buffer is full and needs to be written

    private:
        static int8_t _writeLED;
        static uint8_t _ledMask;
        static volatile uint8_t* _ledReg;
};

int8_t buffer::_writeLED;
uint8_t buffer::_ledMask;
volatile uint8_t* buffer::_ledReg;

//constructor
buffer::buffer(void)
{
    init();
}

//initialize the buffer
void buffer::init(int8_t writeLED)
{
    nchar = 0;                          //buffer is empty
    next = buf;                         //point at the first byte
    writeFlag = false;                  //does not need to be written

    _writeLED = writeLED;
    if (_writeLED >= 0) {
        pinMode(_writeLED, OUTPUT);
        _ledMask = digitalPinToBitMask(_writeLED);    //save some cycles
        uint8_t port = digitalPinToPort(_writeLED);
        _ledReg = portOutputRegister(port);
        *_ledReg &= ~_ledMask;
    }
}

//put a character into the buffer (Context: ISR)
//returns -1 if the character filled the buffer,
//else returns the character.
int buffer::putch(uint8_t ch)
{
    *next++ = ch;                       //put the character in the next available location
    if ( ++nchar >= BUFSIZE ) {
        writeFlag = true;
        return -1;
    }
    else {
        return ch;
    }
}
//write the buffer if it's full (i.e. writeFlag is set),
//passes the return code from SdFile.write() back to the caller.
int buffer::write(SdFile* f)
{
    int sdStat = 0;

    if ( writeFlag ) {
        writeFlag = false;
        *_ledReg |= _ledMask;
        sdStat = f -> write(buf, nchar);        //write the buffer to SD
        *_ledReg &= ~_ledMask;
        if (sdStat >= 0) nchar = 0;             //the buffer is empty/available again
    }
    return sdStat;
}

//write the buffer if it contains data
//passes the return code from SdFile.write() back to the caller.
int buffer::flush(SdFile* f)
{
    int sdStat = 0;

    if ( nchar > 0 ) {
        writeFlag = false;
        *_ledReg |= _ledMask;
        sdStat = f -> write(buf, nchar);        //write the buffer to SD
        *_ledReg &= ~_ledMask;
        if (sdStat >= 0) nchar = 0;             //the buffer is empty/available again
    }
    return sdStat;
}
/*-------- bufferPool class --------*/
class bufferPool
{
    public:
        bufferPool(int8_t writeLED = -1);
        void init(void);
        int putch(uint8_t ch);
        int write(SdFile* f);
        int flush(SdFile* f);

        buffer buf[NBUF];
        bool overrun;
        uint16_t lost;
        
    private:
        buffer* curBuf;                     //pointer to current buffer
        uint8_t bufIdx;                     //index to the current buffer
        buffer* writeBuf;                   //pointer to buffer to write
        uint8_t writeIdx;                   //index to the buffer to write
        int8_t _writeLED;
};

//constructor
bufferPool::bufferPool(int8_t writeLED)
{
    _writeLED = writeLED;
}

void bufferPool::init(void)
{
    for (uint8_t i = 0; i < NBUF; i++) {        //initialize the buffers
        buf[i].init(_writeLED);
    }
    bufIdx = 0;
    writeIdx = 0;
    curBuf = &buf[bufIdx];
    overrun = false;    
    lost = 0;
}

//put a character into a buffer (Context: ISR)
//if the character filled the buffer, but if the next buffer was not
//yet written and cleared by the mainline code, returns -1,
//else returns the character.
int bufferPool::putch(uint8_t ch)
{
    uint8_t ret = ch;

    if ( !overrun ) {                               //room for the character?
        if ( curBuf -> putch(ch) < 0 ) {            //yes, did the character fill the buffer?
            if ( ++bufIdx >= NBUF ) bufIdx = 0;     //yes, switch buffers
            curBuf = &buf[bufIdx];                  //point to the next buffer
            curBuf -> next = curBuf -> buf;         //the next character goes in the first location
            if ( curBuf -> nchar != 0 ) {           //if mainline code has not zeroed the character count,
                overrun = true;                     //then we have an overrun situation
                ret = -1;
            }
        }
    }
    else if ( curBuf -> nchar == 0 ) {              //has previous overrun cleared?
        overrun = false;
        curBuf -> putch('<');                       //insert a message
        curBuf -> putch('L');
        curBuf -> putch('O');
        curBuf -> putch('S');
        curBuf -> putch('T');
        curBuf -> putch(' ');
        for (uint8_t i = 0; i < 4; i++) {           //convert count of lost chars to hex
            uint16_t n = ( lost & 0xF000 ) >> 12;
            n = n + ( (n > 9) ? 'A' - 10 : '0' );
            curBuf -> putch(n);
            lost <<= 4;
        }
        curBuf -> putch('>');
        curBuf -> putch(ch);                        //yes, put the character into the buffer
        lost = 0;
    }
    else {
        ++lost;                                     //no, this character was lost
        ret = -1;                                   //still have overrun situation
    }
    return ret;
}

//write a buffer if it's full.
//moves to the next buffer on each call.
//passes the return code from SdFile.write() back to the caller.
int bufferPool::write(SdFile* f)
{
    int sdStat = 0;
    writeBuf = &buf[writeIdx];                                //point to the buffer
    sdStat = writeBuf -> write(f);                            //write the buffer to SD
    if ( ++writeIdx >= NBUF ) writeIdx = 0;                   //increment index to next buffer
    return sdStat;
}

//starting with the oldest buffer, write any that contain data.
//passes the return code from SdFile.write() back to the caller.
int bufferPool::flush(SdFile* f)
{
    int sdStat = 0;
    writeIdx = ++bufIdx;                                          //index to oldest buffer
    if ( writeIdx >= NBUF ) writeIdx = 0;

    for (uint8_t i = 0; i < NBUF; i++ ) {
        writeBuf = &buf[writeIdx];                                //point to the buffer
        sdStat = writeBuf -> flush(f);                            //write the buffer to SD
        if (sdStat < 0) break;
        if ( ++writeIdx >= NBUF ) writeIdx = 0;                   //increment index to next buffer
    }
    return sdStat;
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
