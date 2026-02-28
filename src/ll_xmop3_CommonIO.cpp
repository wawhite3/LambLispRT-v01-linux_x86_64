#include "LambLisp.h"

#if LL_COMMONIO

#if 0
Ethernet Class

Ethernet.begin()
Ethernet.dnsServerIP()
Ethernet.gatewayIP()
Ethernet.hardwareStatus()
Ethernet.init()
Ethernet.linkStatus()
Ethernet.localIP()
Ethernet.MACAddress()
Ethernet.maintain()
Ethernet.setDnsServerIP()
Ethernet.setGatewayIP()
Ethernet.setLocalIP()
Ethernet.setMACAddress()
Ethernet.setRetransmissionCount()
Ethernet.setRetransmissionTimeout()
Ethernet.setSubnetMask()
Ethernet.subnetMask()

IPAddress Class

IPAddress()

Server Class

Server
EthernetServer()
server.begin()
server.accept()
server.available()
server.write()
server.print()
server.println()

Client Class

Client
EthernetClient()
client.connected()
client.connect()
client.localPort()
client.remoteIP()
client.remotePort()
client.setConnectionTimeout()
client.write()
print()
client.println()
client.available()
client.read()
client.flush()
client.stop()

EthernetUDP Class

EthernetUDP.begin()
EthernetUDP.read()
EthernetUDP.write()
EthernetUDP.beginPacket()
EthernetUDP.endPacket()
EthernetUDP.parsePacket()
EthernetUDP.available()

UDP class

UDP.stop()
UDP.remoteIP()
UDP.remotePort()
      

#endif
//
//Hardware Abstraction Layer
//

Sexpr_t mop3_cio_digitalRead(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)	{  return lamb.mk_bool(digitalRead(lamb.car(sexpr)->mustbe_Int_t()) != 0, env_exec); }
Sexpr_t mop3_cio_digitalWrite(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)	{  digitalWrite(lamb.car(sexpr)->mustbe_Int_t(), lamb.cadr(sexpr) != HASHF);  return OBJ_VOID; }
Sexpr_t mop3_cio_pinMode(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)		{  pinMode(lamb.car(sexpr)->mustbe_Int_t(), lamb.cadr(sexpr)->mustbe_Int_t());  return OBJ_VOID; }
Sexpr_t mop3_cio_analogRead(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)	{  return lamb.mk_integer(analogRead(lamb.car(sexpr)->mustbe_Int_t()), env_exec); }
Sexpr_t mop3_cio_analogWrite(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)	{  analogWrite(lamb.car(sexpr)->mustbe_Int_t(), lamb.cadr(sexpr)->mustbe_Int_t());  return OBJ_VOID; }
Sexpr_t mop3_cio_delay_ms(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)		{  delay(lamb.car(sexpr)->mustbe_Int_t());  return HASHT; }
Sexpr_t mop3_cio_delay_us(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)		{  delayMicroseconds(lamb.car(sexpr)->mustbe_Int_t());  return HASHT; }

Sexpr_t mop3_cio_analogReadResolution(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)	{ return OBJ_UNDEF; }
Sexpr_t mop3_cio_analogReference(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)		{ return OBJ_UNDEF; }
Sexpr_t mop3_cio_analogWriteResolution(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)	{ return OBJ_UNDEF; }
Sexpr_t mop3_cio_noTone(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)			{ return OBJ_UNDEF; }
Sexpr_t mop3_cio_pulseIn(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)			{ return OBJ_UNDEF; }
Sexpr_t mop3_cio_pulseInLong(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)		{ return OBJ_UNDEF; }
Sexpr_t mop3_cio_shiftIn(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)			{ return OBJ_UNDEF; }
Sexpr_t mop3_cio_shiftOut(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)			{ return OBJ_UNDEF; }

Sexpr_t mop3_cio_tone(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  Int_t pin = lamb.car(sexpr)->mustbe_Int_t();
  Int_t freq = lamb.cadr(sexpr)->coerce_Int_t();
  if (lamb.cddr(sexpr) == NIL) tone(pin, freq);
  else {
    Int_t dur = lamb.caddr(sexpr)->coerce_Int_t();
    tone(pin, freq, dur);
  }
  return OBJ_UNDEF;
}

Sexpr_t mop3_cio_bit(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)			{ return OBJ_UNDEF; }
Sexpr_t mop3_cio_bitClear(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)			{ return OBJ_UNDEF; }
Sexpr_t mop3_cio_bitRead(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)			{ return OBJ_UNDEF; }
Sexpr_t mop3_cio_bitSet(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)			{ return OBJ_UNDEF; }
Sexpr_t mop3_cio_bitWrite(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)			{ return OBJ_UNDEF; }
Sexpr_t mop3_cio_highByte(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)			{ return OBJ_UNDEF; }
Sexpr_t mop3_cio_lowByte(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)			{ return OBJ_UNDEF; }
Sexpr_t mop3_cio_attachInterrupt(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)		{ return OBJ_UNDEF; }
Sexpr_t mop3_cio_detachInterrupt(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)		{ return OBJ_UNDEF; }
Sexpr_t mop3_cio_digitalPinToInterrupt(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)	{ return OBJ_UNDEF; }
Sexpr_t mop3_cio_interrupts(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)		{ return OBJ_UNDEF; }
Sexpr_t mop3_cio_noInterrupts(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)		{ return OBJ_UNDEF; }

#endif

Sexpr_t CommonIO_install_mop3(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  ME("::CommonIO_install_mop3()");

  ll_try {
#if LL_COMMONIO

    static const struct {
      Lamb::Mop3st_t func;
      const char *name;
    } cio_bindings[] = {

#define _make_ciobinding_(_sym_) { mop3_cio_##_sym_, #_sym_ }
      _make_ciobinding_(digitalRead),
      _make_ciobinding_(digitalWrite),
      _make_ciobinding_(pinMode),
      _make_ciobinding_(analogRead),
      _make_ciobinding_(analogReadResolution),
      _make_ciobinding_(analogReference),
      _make_ciobinding_(analogWrite),
      _make_ciobinding_(analogWriteResolution),
      _make_ciobinding_(noTone),
      _make_ciobinding_(pulseIn),
      _make_ciobinding_(pulseInLong),
      _make_ciobinding_(shiftIn),
      _make_ciobinding_(shiftOut),
      _make_ciobinding_(tone),
      _make_ciobinding_(delay_ms),
      _make_ciobinding_(delay_us),
      _make_ciobinding_(bit),
      _make_ciobinding_(bitClear),
      _make_ciobinding_(bitRead),
      _make_ciobinding_(bitSet),
      _make_ciobinding_(bitWrite),
      _make_ciobinding_(highByte),
      _make_ciobinding_(lowByte),
      _make_ciobinding_(attachInterrupt),
      _make_ciobinding_(detachInterrupt),
      _make_ciobinding_(digitalPinToInterrupt),
      _make_ciobinding_(interrupts),
      _make_ciobinding_(noInterrupts),
#undef _make_ciobinding_      
    };
    
    const Int_t Nsyms = sizeof(cio_bindings)/sizeof(cio_bindings[0]);

    Sexpr_t env_target = lamb.car(sexpr);

    lamb.log("%s defining %d Mops\n", me, Nsyms);
    for (int i=0; i<Nsyms; i++) {
      auto p = cio_bindings[i];
      Sexpr_t sym  = lamb.mk_symbol(p.name, NIL);
      Sexpr_t proc = lamb.mk_Mop3_procst_t(p.func, sym);
      lamb.dict_bind_bang(env_target, sym, proc, env_exec);
    }
    
    static const struct { int val; const char *name; }
      INT_constants[] = {

#define _make_intbinding_(_sym_) { _sym_, #_sym_ }
      _make_intbinding_(HIGH),
      _make_intbinding_(LOW),
      _make_intbinding_(INPUT),
      _make_intbinding_(INPUT_PULLUP),
      _make_intbinding_(OUTPUT)
#undef _make_intbinding_

    };
    
    const Int_t Nconst = sizeof(INT_constants)/sizeof(INT_constants[0]);
    lamb.log("%s defining %d constants\n", me, Nconst);
    for (int i=0; i<Nconst; i++) {
      auto p = INT_constants[i];
      Sexpr_t sym = lamb.mk_symbol(p.name, NIL);
      Sexpr_t val = lamb.mk_integer(p.val, sym);
      lamb.dict_bind_bang(env_target, sym, val, env_exec);
    }
#endif
    
    return NIL;
  }
  
  ll_catch();
}
