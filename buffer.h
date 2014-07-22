//Serial Data Logger by Jack Christensen is licensed under CC BY-SA 4.0,
//http://creativecommons.org/licenses/by-sa/4.0/

#include <util/atomic.h>

const int NBUF = 2;                     //number of receive buffers
const uint16_t BUFSIZE = 512;           //serial receive buffer size

/*-------- buffer class --------*/
class buffer
{
    public:
        buffer(void);
        void init(int8_t writeLED = -1);
        int putch(uint8_t ch);
        int write(SdFile* f);
        int flush(SdFile* f);

        volatile uint8_t buf[BUFSIZE];  //the buffer
        volatile uint8_t* next;         //pointer to next available position in buffer
        volatile uint16_t nchar;        //number of characters in the buffer
        volatile bool writeMe;          //buffer is full and needs to be written

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
    writeMe = false;                    //does not need to be written

    _writeLED = writeLED;
    if (_writeLED >= 0) {
        pinMode(_writeLED, OUTPUT);
        _ledMask = digitalPinToBitMask(_writeLED);    //save some cycles by addressing port directly
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
        writeMe = true;
        return -1;
    }
    else {
        return ch;
    }
}
//write the buffer if it's full (i.e. if writeMe is set),
//passes the return code from SdFile.write() back to the caller.
int buffer::write(SdFile* f)
{
    int sdStat = 0;

    if ( writeMe ) {
        writeMe = false;
        *_ledReg |= _ledMask;
        sdStat = f -> write((const uint8_t *)buf, nchar);        //write the buffer to SD
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
        writeMe = false;
        *_ledReg |= _ledMask;
        sdStat = f -> write((const uint8_t *)buf, nchar);        //write the buffer to SD
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
        volatile bool overrun;
        
    private:
        buffer* _curBuf;                        //pointer to current buffer
        volatile uint8_t _bufIdx;               //index to the current buffer
        buffer* _writeBuf;                      //pointer to buffer to write
        uint8_t _writeIdx;                      //index to the buffer to write
        int8_t _writeLED;
        uint16_t _lost;
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
    _bufIdx = 0;
    _writeIdx = 0;
    _curBuf = &buf[_bufIdx];
    _lost = 0;
    overrun = false;    
}

//put a character into a buffer (Context: ISR)
//if the character filled the buffer, but if the next buffer was not
//yet written and cleared by the mainline code, returns -1,
//else returns the character.
int bufferPool::putch(uint8_t ch)
{
    uint8_t ret = ch;

    if ( !overrun ) {                               //room for the character?
        if ( _curBuf -> putch(ch) < 0 ) {           //yes, did the character fill the buffer?
            if ( ++_bufIdx >= NBUF ) _bufIdx = 0;   //yes, switch buffers
            _curBuf = &buf[_bufIdx];                //point to the next buffer
            _curBuf -> next = _curBuf -> buf;       //the next character goes in the first location
            if ( _curBuf -> nchar != 0 ) {          //if mainline code has not zeroed the character count,
                overrun = true;                     //then we have an overrun situation
                ret = -1;
            }
        }
    }
    else if ( _curBuf -> nchar == 0 ) {             //has previous overrun cleared?
        overrun = false;
        _curBuf -> putch('<');                      //insert a message
        _curBuf -> putch('L');
        _curBuf -> putch('O');
        _curBuf -> putch('S');
        _curBuf -> putch('T');
        _curBuf -> putch(' ');
        for (uint8_t i = 0; i < 4; i++) {           //convert count of lost chars to hex
            uint16_t n = ( _lost & 0xF000 ) >> 12;
            n = n + ( (n > 9) ? 'A' - 10 : '0' );
            _curBuf -> putch(n);
            _lost <<= 4;
        }
        _curBuf -> putch('>');
        _curBuf -> putch(ch);                       //yes, put the character into the buffer
        _lost = 0;
    }
    else {
        ++_lost;                                    //no, this character was lost
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
    _writeBuf = &buf[_writeIdx];                              //point to the buffer
    sdStat = _writeBuf -> write(f);                           //write the buffer to SD
    if ( ++_writeIdx >= NBUF ) _writeIdx = 0;                 //increment index to next buffer
    return sdStat;
}

//starting with the oldest buffer, write any that contain data.
//passes the return code from SdFile.write() back to the caller.
int bufferPool::flush(SdFile* f)
{
    int sdStat = 0;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        _writeIdx = ++_bufIdx;                          //index to oldest buffer
    }
    if ( _writeIdx >= NBUF ) _writeIdx = 0;

    for (uint8_t i = 0; i < NBUF; i++ ) {
        _writeBuf = &buf[_writeIdx];                              //point to the buffer
        sdStat = _writeBuf -> flush(f);                           //write the buffer to SD
        if (sdStat < 0) break;
        if ( ++_writeIdx >= NBUF ) _writeIdx = 0;                 //increment index to next buffer
    }
    return sdStat;
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
