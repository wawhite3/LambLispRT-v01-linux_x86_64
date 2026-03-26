#if LL_AMD64

#include "ll_platform_generic.h"
#include <sys/time.h>

unsigned long micros(void)
{
  static unsigned long start_us = 0;
  struct timeval te;
  gettimeofday(&te, NULL);
  unsigned long now = te.tv_sec * 1000000LL + te.tv_usec;
  if (start_us == 0) start_us = now;
  return now - start_us;
}

unsigned long millis(void)
{
  static unsigned long start_ms = 0;
  struct timeval te;
  gettimeofday(&te, NULL); // get current time
  unsigned long now = te.tv_sec * 1000LL + te.tv_usec / 1000; // calculate milliseconds
  if (start_ms == 0) start_ms = now;
  return now - start_ms;
}

void delay_ms(unsigned long ms)	{ unsigned long end = millis() + ms;  while (millis() < end) /*wait*/; }
void delay_us(unsigned long ms)	{ unsigned long end = micros() + ms;  while (micros() < end) /*wait*/; }

Int_t  LambPlatform::free_stack()			{ return 1<<10; /*just say 1k left*/ }
Int_t  LambPlatform::free_heap()			{ return 1<<20; /*just say 1 meg free */ }
void   LambPlatform::rand(byte *buf, Int_t len)		{}
Bool_t LambPlatform::heap_integrity_check(bool foo)	{ return 1; }
void   LambPlatform::reboot()				{}

void LambPlatform::identification()
{
  ME("LambPlatform::identification()");
  global_printf("%s This is x86-64\n", me);
}

void LambPlatform::begin()
{
  loop_start_ms = millis();
  loop_start_us = micros();
  identification();
}

void LambPlatform::end() {}

void LambPlatform::loop(void)	//call this 1st thing in Lamb::loop().
{
  loop_start_ms = millis();
  loop_start_us = micros();
}

#endif

