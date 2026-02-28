#ifndef LL_PLATFORM_GENERIC_H
#define LL_PLATFORM_GENERIC_H

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include "assert.h"
#include "unistd.h"

#if LL_ARDUINO
#include "Arduino.h"
#endif

#if LL_FAKE_ARDUINO
#include <string.h>

unsigned long millis();
unsigned long micros();
void delay_ms(unsigned long ms);

static void delay(unsigned long ms) { delay_ms(ms); }

typedef uint8_t byte;

class LL_String {
public:

  LL_String()				{ ptr = 0;  set(""); }
  LL_String(const char *other)		{ ptr = 0;  set(other);  }
  LL_String(const char *other, int n)	{ ptr = 0;  set(other, n);  }
  LL_String(const LL_String &other)	{ ptr = 0;  set(other.ptr);  }
  
  ~LL_String()	{ if (ptr) { delete[] ptr;  ptr = 0; } }

  void set(const char *s) { set(s, strlen(s)); }

  void set(const char *s, int n) {
    if (ptr) { delete[] ptr;  ptr = 0; }
    char *p = ptr = new char[n + 1];
    while (n--) *p++ = *s++;
    *p = '\0';
  }
  
  LL_String concat(const char *s1, const char *s2) {
    int n1 = strlen(s1);
    int n2 = strlen(s2);
    char *s3 = new char[n1 + n2 + 1];

    char *p = s3;
    while (n1--) *p++ = *s1++;
    while (n2--) *p++ = *s2++;
    *p = '\0';
    return s3;
  }

  LL_String &append(const char *s1) {
    const char *s0 = ptr;
    int n0 = s0 ? strlen(s0) : 0;
    int n1 = strlen(s1);
    char *s2 = new char[n0 + n1 + 1];

    char *p = s2;
    while (n0--) *p++ = *s0++;
    while (n1--) *p++ = *s1++;
    *p = '\0';
    
    if (ptr) { delete[] ptr;  ptr = 0; }
    ptr = s2;
    return *this;
  }

  LL_String substring(int start, int len)	{ return LL_String(&(ptr[start]), len); }
  
  LL_String &operator=(const char *other)	{ set(other); return *this; }
  LL_String &operator=(const LL_String &other)	{ set(other.ptr); return *this; }

  bool operator==(const char *other)		{ return strcmp(ptr, other) == 0; }
  bool operator!=(const char *other)		{ return strcmp(ptr, other) != 0; }
  bool operator==(const LL_String &other)	{ return strcmp(ptr, other.ptr) == 0; }
  bool operator!=(const LL_String &other)	{ return strcmp(ptr, other.ptr) != 0; }
  char &operator[](int ix)			{ return ptr[ix]; }

  LL_String operator+(const char *other)	{ return concat(ptr, other); }
  LL_String operator+(const LL_String &other)	{ return concat(ptr, other.ptr); }
  LL_String operator+(char c)			{ char cstr[2];  cstr[0] = c;  cstr[1] = '\0';  return concat(ptr, cstr); }

  LL_String &operator+=(const char *other)	{ return append(other); }
  LL_String &operator+=(const LL_String &other)	{ return append(other.ptr); }
  LL_String &operator+=(char c)			{ char cstr[2];  cstr[0] = c;  cstr[1] = '\0';  return append(cstr); }
  
  char *c_str() const	{ return ptr; }
  int length()	{ return strlen(ptr); }  
  
private:
  char *ptr;
};

typedef LL_String String;
#endif

//! @name Handy directives, macros and utility functions.
//!@{
#define NOTUSED __attribute__((__unused__))			/*!<GCC extension to suppress individual "not used" warnings. */
#define INLINE __attribute__((__inline__))			/*!<GCC extension to encourage inlining a function. */
#define NOINLINE __attribute__((__noinline__))			/*!<GCC extension to avoid inlining a function. */
#define CHECKPRINTF __attribute__((format(printf, 1, 2)))	/*!<GCC extension, turn on checking of printf() const format strings, if they are known at compile time. */
#define CHECKPRINTF_pos2 __attribute__((format(printf, 2, 3)))	/*!<GCC extension, turn on checking of printf() const format strings, if they are known at compile time. */
#define CHECKPRINTF_pos3 __attribute__((format(printf, 3, 4)))	/*!<GCC extension, turn on checking of printf() const format strings, if they are known at compile time. */
#define CHECKPRINTF_pos4 __attribute__((format(printf, 4, 5)))	/*!<GCC extension, turn on checking of printf() const format strings, if they are known at compile time. */

//! @name LambLisp imposes a limit on the length of strings, to reduce opportunities for runaway in an embedded system.
//!@{
const unsigned long toString_MAX_LENGTH = 8192;		//!<Global limit on Lamb-generated strings.
String toString(const char *fmt, ...) CHECKPRINTF;	//!<Produce a new string from the format and arguments, respecting the global limit on string length.
//!@}


#define ME(_me_) NOTUSED const char me[] = _me_			/*!<Declare an identifier "const char me[]" without causing "unused variable" warnings and/or code clutter to suppress them. */
#define isdef(sym) (#sym[0])					//!<Determine (cheaply) at runtime if a preprocessor symbol is defined.

void global_printf(const char *fmt, ...);		//!<This function enforces the limit on generated strings.

//!@}

/*! @name These primitive types are shared by LambLisp and the underlying VM.

  Sizing requirements for LambLisp Cells:

  | Each cell contains 3 fields: tag, car, cdr.                                     |
  | The fields are equal in size and sequential in memory.                          |
  | The bytes within each LambLisp Cell are individually addressable.               |
  | Each field can hold a generic computer "word", a signed integer, or an address. |
  | An integer fills the car field, and may also fill the cdr field.                |
  | A real number also fills the car field, and may also fill the cdr field.        |

  Since the beginning of time (Jan 1 1970) the specific organization of these have been platform-dependent.
  There is a (mostly) obvious correspondence between the shared type name and the underlying C++ type.

  Note the difference between *Charst_t* and *CharVec_t*; one is mutable, the other not.  The immutable version may go away.
  The same applies to *Bytest_t* and *ByteVec_t*.
*/
//!@{

typedef unsigned char Byte_t;	//!<Universally known byte type.
typedef bool Bool_t;		//!<Boolean type.
typedef char Char_t;		//!<Character type.

typedef Char_t const *Charst_t;		//!<Pointer to immutable character array.
typedef Byte_t const *Bytest_t;		//!<Pointer to immutable byte array
typedef Char_t *CharVec_t;		//!<Pointer to mutable char array
typedef Byte_t *ByteVec_t;		//!<Pointer to mutable byte array

#if LL_AMD64
typedef unsigned long Word_t;	//!<This is a generic computer word, used only for setting and retrieval.  It is not an *unsigned int* used for arithmetic.
typedef int Int_t;		//!<Integer type
typedef double Real_t;		//!<Real type - bug in gcc "long double" type
typedef void *Ptr_t;		//!<Generic pointer in C++, 
#endif

#if LL_ARM64
typedef unsigned long Word_t;	//!<This is a generic computer word, used only for setting and retrieval.  It is not an *unsigned int* used for arithmetic.
typedef int Int_t;		//!<Integer type
typedef float Real_t;		//!<Real type - not double to keep compatible with nvidia on jetson orin dev kit.
typedef void *Ptr_t;		//!<Generic pointer in C++.
#endif

#if LL_ESP32
typedef unsigned Word_t;	//!<This is a generic computer word, used only for setting and retrieval.  It is not an *unsigned int* used for arithmetic.
typedef int Int_t;		//!<Integer type
typedef double Real_t;		//!<Real type
typedef void *Ptr_t;		//!<Generic pointer in C++.
#endif

static_assert(sizeof(Word_t) == sizeof(Ptr_t), "Word_t size must be == Ptr_t size\n");
static_assert(sizeof(Word_t) >= sizeof(Int_t), "Word_t size must be >= Int_t size\n");
static_assert(sizeof(Real_t) <= 2*sizeof(Word_t), "Real_t size must be <= 2 * Word_t size\n");
//!@}

//The AsciiConverter class mitigates printf formatting problems caused by different int sizes on different processors, when using printf-style format string (%u, %d %lu %ld etc).
class AsciiConverter {
public:

  char *dec(Word_t n) {
    Int_t ix  = buffer_size - 1;
    char *ptr = &(buffer[ix]);

    *ptr = '\0';
    do {
      Word_t d = n % 10;
      n /= 10;
      char c = d + '0';
      *--ptr = c;
    } while (n);
    return ptr;
  }

  char *dec(Int_t n) {
    if (n >= 0) return dec((Word_t) n);
    n = -n;
    char *ptr = dec((Word_t) n);
    *--ptr = '-';
    return ptr;
  }

  char *hex(Word_t n) {
    Int_t ix   = buffer_size - 1;
    char *ptr  = &(buffer[ix]);
    Int_t nibs = sizeof(Word_t) * 2;

    *ptr = '\0';
    while (nibs--) {
      Word_t d = n & 0x0f;
      n >>= 4;
      char c = (d < 10) ? (d + '0') : (d - 10 + 'a');
      *--ptr = c;
    }
    return ptr;

  }

  char *dec(Real_t n) {
    snprintf(buffer, buffer_size, "%f", (double) n);
    return buffer;
  }

private:
  static const Int_t buffer_size = 128;
  char buffer[buffer_size];
};

extern AsciiConverter ascii;
  
class LambPlatform {
public:

  //! @name Interaction with the underlying runtime platform.
  //!@{
  LambPlatform() {}
  ~LambPlatform() { end(); }

  void begin(void);
  void loop(void);			//!<Perform any platform-specific activity needed at loop() time.  Call only once per loop at beginning main loop().
  void end(void);
  
  void reboot(void);
  
  const char *name();			//!<Return a pointer to a chacter array containing a description of the runtime platform.
  void identification(void);		//!<Emit a string with the complete detailed description of the platform.

  Int_t free_heap();			//!<Return the unused space available for LambLisp expansion.  Whether this is *total* space or *largest* space is platform-dependent.  Accuracy is specifically not guaranteed.
  Int_t free_stack();			//!<Return the unused execution stack space available.  Accuracy is specifically not guaranteed.
  Bool_t heap_integrity_check(Bool_t complain=false);	//!<Run intensive heap check; print errors if found; return true if errors found.

  //!Return a real number between -1.0 and +1.0.  May include -1.0 but not +1.0.
  Real_t rand11() {
    const int max_int = (~((unsigned int) 0)) >> 1;
    const int min_int = -max_int - 1;
    const Real_t min  = (Real_t) min_int;
    
    Int_t n;
    rand((byte *) &n, sizeof(n));
    return (n / min);
  }
  
  Real_t rand01()			{ return (rand11() + 1.0) / 2.0; }

  void rand(byte *buf, Int_t len);	//!<Fill a buffer with the highest-quality random numbers available on this platform.
  void rand11(Real_t *buf, Int_t n)	{ while (n--) *buf++ = rand11(); }
  void rand01(Real_t *buf, Int_t n)	{ while (n--) *buf++ = rand01(); }

  Int_t loop_elapsed_ms()	{ return millis() - loop_start_ms; }	//!<Return the time elapsed since the beginning of the current loop().
  Int_t loop_elapsed_us()	{ return micros() - loop_start_us; }	//!<Return the time elapsed since the beginning of the current loop().

  //!@}

private:
  Int_t loop_start_ms;
  Int_t loop_start_us;
};

extern LambPlatform lambPlatform;

//! @name Elide the differences between platform "files" with this typedef.
//!@{
#if LL_SPIFFS
#include "SPIFFS.h"
typedef File File_Native;
#endif

#if LL_POSIX
#include <stdio.h>
typedef FILE* File_Native;
#endif
//!@}

/*! \class LL_File
  
  The *file* type is ultimately provided by the underlying operating system, not by LambLisp.
  This class elides the differences between file types on different platforms, providing a POSIX-like interface.

  Note that there is no *open* operstion on files.
  A file is opened by the *file system* and then a *file* is returned.
  After a *file* is closed, the same file object cannot be opened again; instead a new file must be requested from the *file system*.
*/
//!@{
class LL_File {
public:

  LL_File();
  ~LL_File();

  bool isOpen() { return _path != ""; }
  int read(void);
  int write(byte b);
  int seek(unsigned long target, int whence=SEEK_SET);
  int tell();
  int size();
  int close();

  int read(byte *b, int n) {
    Int_t nread = 0;
    while (n--) {
      int ch = read();
      if (ch == EOF) return nread;
      b[nread++] = ch;
    }
    return nread;
  }
  
  int read(char *s, int n) { return read((byte *) s, n); }  
  int write(const byte *b, int n) { while (n--) write(*b++); return n; }
  int write(const char *s, int n) { while (n--) write((byte) *s++); return n; }
  int peek();
  
  File_Native _theFile;
  String _path;
  String _mode;

private:

};
//!@}

/*! \class LL_File_System

  The "file system" type elides the differences between different underlying platforms.
  For example, it will deal with the leading '/' required by SPIFFS.
  This minimal file system interface is platform-independent.
*/
class LL_File_System {
public:
  LL_File *open(const char *path, const char *mode);
  int rm(const char *path);
  int mv(const char *from, const char *to);  

private:
};

extern LL_File_System ll_file_system;

/*! \name WiFi and Wire, in case we need to rationalize conflicting implementations.
 */
//!@{
#if LL_WIRE
#include "Wire.h"
extern TwoWire *LL_Wire;
#endif

#if LL_WIFI
#include "WiFi.h"
extern WiFiClass *LL_WiFi;
#endif
//!@}

/*! \class LambStdioClass
  
  A wrapper around the underlying stdin/stdout implementation.
  On an embedded system, this class will use the primary serial in/out (`Serial` on Arduino-compatibles).
  On Linux, this class will set the terminal to byte-at-a-time mode (called "non-CANONICAL" mode).
*/
class LambStdioClass {
public:
  int setTxBufferSize(int n);
  int setRxBufferSize(int n);
  
  void begin(void) { begin(115200); }
  void begin(unsigned long baudrate);
  void end();
  int available(void);
  int availableForWrite(void);
  int read(void);
  int write(uint8_t c);
  int write(char c) { return write((uint8_t) c); }
  
  void flush(void);

  int read(byte *buf, int max) {
    int nread = 0;
    while (max--) {
      int b = read();
      if (b == EOF) break;
      *buf++ = b;
    }
    return nread;
  }

  int read(char *s, int max)		{ return read((uint8_t *) s, max); }
  int write(const char *s, int n)	{ return write((uint8_t *) s, n); }
  int write(const byte *b, size_t n)	{ int i=n;  while (i--) write(*b++);  return n; }
  int write(const char *s)		{ int i=0;  while (*s) { write(*s++); i++; }  return i; }
  
  operator bool() { return true; }
};

extern LambStdioClass LambStdio;

typedef byte uuid_t[16];

//!Macro to do something once after the system awakens.
#define once(_once_something) do {		\
    static bool _visited_ = false;		\
    if (!_visited_) {				\
      _visited_ = true;				\
      { _once_something; }			\
    }						\
  } while (0)					\
    //
//

//!Macro to do something every so often.
#define every(_every_so_often_ms, _every_something_to_do) do {	\
    static unsigned long _every_next = 0;			\
    unsigned long _every_now = millis();			\
    if (_every_now >= _every_next) {				\
      { _every_something_to_do; }				\
      _every_next = _every_now + (_every_so_often_ms);		\
    }								\
  } while (0)							\
    //
//

/*!
  The embedded debug catcher ensures that an address is available to be set as a breaskpoint for a hardware debugger.
  This useful in cases where the generated code has been heavily inlined.
  Undefine this symbol if not using a hardware debugger, or redefine it to point to a different breakpoint target.
*/
void embedded_debug_catcher();
#define ll_debug_catcher embedded_debug_catcher()
//#define ll_debug_catcher ll_term.flush()

//! @name C++ *try* and *catch* are used to process code faults detected by the LambLisp VM.
//! To cleanly unwind after an error is detected, each *catch* has uniform behavior, which is captured in this macro.
//!@{
#define ll_try try

#define ll_catch(__code_before_rethrow__)				\
  catch (Sexpr_t __err__) {						\
    if (__err__->type() != Cell::T_ERROR)				\
      throw NIL->mk_error("ll_catch() BUG in %s bad type %s", me, __err__->dump().c_str()); \
    									\
    global_printf("\r[%d] %s ll_catch(): %s\n", millis(), me, __err__->error_get_chars()); \
									\
    ll_debug_catcher;							\
    __code_before_rethrow__;						\
    throw __err__;							\
  }									\
  //
//!@}


#endif
