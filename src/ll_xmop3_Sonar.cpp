#include "LambLisp.h"

#if LL_SONAR

class Sonar {

public:

  const Real_t range_meters = 4.0;		//limit of sensor range, determines echo wait timeout

  const Real_t Vs_est_mps = 343.0;		//initial value w/out temp compensation
  const unsigned long timeout_us = range_meters * 1000000.0 / Vs_est_mps;	//changes only slightly with temperature, so no temp compensation

  const Real_t gamma  = 1.40;			//Gas constant for air
  const Real_t Tc0_k  = 273.15;			//Temperature in K at 0 C.
  const Real_t M      = 0.0289645;		//kg/mol
  const Real_t R      = 8.31446261815324;	//Joules/(mol * K);
  const Real_t factor = gamma * R / M;		//constant part of formula calculated once

  Real_t us_to_distance(Int_t t_ping_us, Real_t Tc = 20.0) {
    static Real_t Tc_prev = 20.0;
    static Real_t Vs_mps  = Vs_est_mps;
    if (Tc != Tc_prev) {
      Vs_mps  = sqrt(factor * (Tc0_k + Tc));	//speed of sound in meters per second
      Tc_prev = Tc;
    }
    return Vs_mps * t_ping_us / 2000000.0;	//Convert usec to sec and count only half of the round-trip time.
  }

  void begin(int _pin_trig, int _pin_echo) {
    ME("Sonar::begin()");
    pin_trig = _pin_trig;
    pin_echo = _pin_echo;

    pinMode(pin_trig, OUTPUT);
    pinMode(pin_echo, INPUT);
    digitalWrite(pin_trig, LOW);
  }

  unsigned long ping() {
    digitalWrite(pin_trig, HIGH);
    delayMicroseconds(12);
    digitalWrite(pin_trig, LOW);
    return pulseIn(pin_echo, HIGH, timeout_us);
  }

  Real_t poke() { return us_to_distance(ping()); }
  
private:
  Int_t pin_trig, pin_echo;
  
};

Sonar *sonar = 0;	//!<Sonar singleton.

//(begin pin-trig trig-echo)
Sexpr_t Sonar_mop3_begin(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  if (sonar) { delete sonar;  sonar = 0; }
  if (!sonar) sonar = new Sonar;
  Int_t pin_trig = lamb.car(sexpr)->mustbe_Int_t();
  Int_t pin_echo = lamb.cadr(sexpr)->mustbe_Int_t();
  sonar->begin(pin_trig, pin_echo);
  return OBJ_UNDEF;
}

Sexpr_t Sonar_mop3_ping(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec) { Int_t t_ping = sonar->ping();  return (t_ping == 0) ? HASHF : lamb.mk_integer(t_ping, env_exec); }
Sexpr_t Sonar_mop3_poke(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec) { Int_t t_ping = sonar->ping();  return (t_ping == 0) ? HASHF : lamb.mk_real(sonar->us_to_distance(t_ping), env_exec); }

Sexpr_t Sonar_mop3_us_to_distance(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  Int_t us    = lamb.car(sexpr)->coerce_Int_t();
  Real_t temp = (lamb.cdr(sexpr) != NIL) ? lamb.cadr(sexpr)->coerce_Real_t() : 20.0;
  return lamb.mk_real(sonar->us_to_distance(us, temp), env_exec);
}

#endif

//Install all Sonar symbols in base environment.
//
Sexpr_t Sonar_install_mop3(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  ME("::Sonar_install_mop3()");
  ll_try {
#if LL_SONAR

    if (sonar) { delete sonar; sonar = 0; }
    if (sonar == 0) sonar = new Sonar;

    static const struct { Lamb::Mop3st_t func;  const char *name; } std_procs[] = {

#define mk_dispatcher(__item__) { Sonar_mop3_##__item__, "Sonar."#__item__ }
      mk_dispatcher(begin),
      mk_dispatcher(ping),
      mk_dispatcher(poke),
#undef mk_dispatcher

      { Sonar_mop3_us_to_distance, "Sonar.us->distance" },
      { Sonar_install_mop3, "Sonar.install-mop3" }
    };

    const int Nstd_procs = sizeof(std_procs)/sizeof(std_procs[0]);
  
    lamb.log("%s defining %d Mops\n", me, Nstd_procs);
    Sexpr_t env_target = lamb.car(sexpr);
    for (int i=0; i<Nstd_procs; i++) {
      Sexpr_t sym  = lamb.mk_symbol(std_procs[i].name, env_exec);
      lamb.gc_root_push(sym);
      Sexpr_t proc = lamb.mk_Mop3_procst_t(std_procs[i].func, env_exec);
      lamb.gc_root_pop();
      lamb.dict_bind_bang(env_target, sym, proc, env_exec);
    }
#endif
    
    return OBJ_UNDEF;
  }
  ll_catch();
}

