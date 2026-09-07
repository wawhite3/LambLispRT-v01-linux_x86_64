// Copyright 2026 by Frobenius Norm LLC 2026-05-16
// Free for non-commercial use. Commercial use requires a license.
#include "ll_platform_generic.h"

#if LL_POSIX

//#include <stdio.h>
//#include <sys/select.h>
#if LL_TERMIOS
#include <termios.h>
#include <sys/ioctl.h>
#endif
//#include <stropts.h>

void LambStdioClass::begin(unsigned long baudrate)
{
  // Byte-at-a-time I/O.  stdout MUST be unbuffered: write() -> putchar() is otherwise
  // block-buffered when stdout is a PIPE (not a tty), so output only flushes at ~4KB or exit
  // -- a host driving the REPL over a pipe (llip_test_runner subprocess transport) would see
  // nothing until EOF.  This runs unconditionally at startup, unlike the interactive termios
  // setup in available() which the batch-mode (piped-stdin) input path never reaches.
  setbuf(stdout, NULL);
  setbuf(stdin, NULL);
}
void LambStdioClass::end() {}

int LambStdioClass::setTxBufferSize(int n) { return 1; }
int LambStdioClass::setRxBufferSize(int n) { return 1; }

int LambStdioClass::availableForWrite()	{ return true; }      
int LambStdioClass::read(void)		{ return getchar(); }
int LambStdioClass::write(uint8_t c)	{ putchar(c);  return 1; }  
void LambStdioClass::flush(void)	{ fflush(stdout); }

#if !LL_TERMIOS
/*! P175: no-termios stdin (wasm32-wasi, and any future host without a raw terminal mode).

  THERE IS NO WAY TO ASK "IS A BYTE WAITING?" HERE.  wasi-libc has no termios.h and no
  FIONREAD ioctl, so the kbhit() trick below cannot be ported -- and there is no partial
  version of it either: a poll() on fd 0 under WASI reports the *file* as readable, not the
  tty as having a keystroke, so it answers "yes" forever and the caller spins.

  So this says "a byte is available" unconditionally and lets read() -> getchar() BLOCK.
  That is correct for the two ways this build is actually driven:
    - piped stdin (the conform/test path), where LL_Term::monitor_commandline() takes its
      line-accumulating branch and never calls available() at all;
    - an interactive wasmtime REPL, where blocking in getchar() at the prompt is exactly
      what a line-oriented REPL should do.
  THE CONSEQUENCE TO KNOW: the line EDITOR (arrow keys, Esc-B/Esc-F, history) needs raw mode
  to see keystrokes one at a time, and cannot work here.  The host terminal stays in cooked
  mode, so LambLisp receives a whole line at a time, already echoed by the terminal.  That is
  a missing feature of this target, not a bug to hunt.
*/
int LambStdioClass::available(void) { return 1; }
#else
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
      setbuf(stdin, NULL);    // stdout is unbuffered in begin() (runs for batch mode too)
      initialized = true;
    }

    int bytesWaiting;
    ioctl(STDIN, FIONREAD, &bytesWaiting);
    return bytesWaiting;
  }
#endif // LL_TERMIOS

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
