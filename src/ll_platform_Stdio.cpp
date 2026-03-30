#include "ll_platform_generic.h"

#if LL_POSIX

//#include <stdio.h>
//#include <sys/select.h>
#include <termios.h>
#include <sys/ioctl.h>
//#include <stropts.h>

void LambStdioClass::begin(unsigned long baudrate) { }
void LambStdioClass::end() {}

int LambStdioClass::setTxBufferSize(int n) { return 1; }
int LambStdioClass::setRxBufferSize(int n) { return 1; }

int LambStdioClass::availableForWrite()	{ return true; }      
int LambStdioClass::read(void)		{ return getchar(); }
int LambStdioClass::write(uint8_t c)	{ putchar(c);  return 1; }  
void LambStdioClass::flush(void)	{ fflush(stdout); }

int LambStdioClass::available(void) //Credit to: Morgan McGuire, morgan@cs.brown.edu, originally as _kbhit()
{
    static const int STDIN = 0;
    static bool initialized = false;

    if (! initialized) {
      // Use termios to turn off input line buffering
      termios termio;
      tcgetattr(STDIN, &termio);
      termio.c_lflag &= ~ICANON;
      termio.c_lflag &= ~ECHO;
      tcsetattr(STDIN, TCSANOW, &termio);
      setbuf(stdin, NULL);
      initialized = true;
    }

    int bytesWaiting;
    ioctl(STDIN, FIONREAD, &bytesWaiting);
    return bytesWaiting;
  }

#endif

#if LL_ESP32
void LambStdioClass::begin(unsigned long baudrate)
{
  ME("LambStdioClass::begin()");
  Serial.begin(baudrate);
  Serial.printf("[%lu] %s started OK\n", millis(), me);
}

void LambStdioClass::end()			{ Serial.end(); }
int LambStdioClass::setTxBufferSize(int n)	{ return 1; }
int LambStdioClass::setRxBufferSize(int n)	{ return 1; }
int LambStdioClass::available(void)		{ return Serial.available(); }
int LambStdioClass::availableForWrite()		{ return Serial.availableForWrite(); }
void LambStdioClass::flush(void)		{ Serial.flush(); }

int LambStdioClass::write(uint8_t c)		{ Serial.write(c);  return 1; }
int LambStdioClass::read(void)			{ return Serial.read(); }
#endif

LambStdioClass LambStdio;
