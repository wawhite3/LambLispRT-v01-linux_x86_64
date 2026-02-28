#include "LambLisp.h"

#if LL_PCA9685

#include "PCA9685.h"

PCA9685 *pca9685 = 0;

//!(define PCA9685.begin . addr)
Sexpr_t PCA9685_mop3_begin(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  ME("::PCA9685_mop3_begin()");
  Int_t addr = lamb.car(sexpr)->mustbe_Int_t();
  LL_Wire->beginTransmission(addr);
  LL_Wire->write((uint8_t) 0x00);
  LL_Wire->write((uint8_t) 0x00);
  Int_t res = LL_Wire->endTransmission();
  if (res) lamb.log("%s No device at %d, LL_Wire->endTransmission == %d\n", me, addr, res);
  else {
    lamb.log("%s I2C device found at %d\n", me, addr);  
    pca9685->setupSingleDevice(Wire, addr, false);
    pca9685->setSingleDeviceToFrequency(addr, 50);
    lamb.log("%s PCA freq readback %d\n", me, pca9685->getSingleDeviceFrequency(0x5f));
  }
  return lamb.mk_integer(res, env_exec);
}

//!(PCA9685_mop3_setupSingleDevice addr)
Sexpr_t PCA9685_mop3_setupSingleDevice(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  Int_t addr = lamb.car(sexpr)->mustbe_Int_t();
  pca9685->setupSingleDevice(Wire, addr, false);
  return OBJ_UNDEF;
}

//!(PCA9685_mop3_setFrequency addr freq)
Sexpr_t PCA9685_mop3_setSingleDeviceToFrequency(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  ME("::PCA9685_mop3_setSingleDeviceToFrequency()");
  Int_t addr = lamb.car(sexpr)->mustbe_Int_t();
  Int_t freq = lamb.cadr(sexpr)->coerce_Int_t();
  lamb.log("%s (%d, %d)\n", me, addr, freq);
  
  pca9685->setSingleDeviceToFrequency(addr, freq);
  return OBJ_UNDEF;
}

Sexpr_t PCA9685_mop3_getSingleDeviceFrequency(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  ME("::PCA9685_mop3_getSingleDeviceFrequency()");
  Int_t addr = lamb.car(sexpr)->mustbe_Int_t();
  Int_t freq = pca9685->getSingleDeviceFrequency(addr);
  lamb.log("%s (%d) ==> %d\n", me, addr, freq);
  return lamb.mk_integer(freq, env_exec);
}

//!(PCA9685_mop3_setDutyCycle addr pin on_fraction)
Sexpr_t PCA9685_mop3_setChannelDutyCycle(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  Int_t chan  = lamb.car(sexpr)->mustbe_Int_t();
  Real_t frac = lamb.cadr(sexpr)->coerce_Real_t();
  Real_t phsh = lamb.caddr(sexpr)->coerce_Real_t();
  pca9685->setChannelDutyCycle(chan, frac, phsh);
  return OBJ_UNDEF;
}

Sexpr_t PCA9685_mop3_setChannelPulseWidth(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  Int_t chan = lamb.car(sexpr)->as_Int_t();
  Int_t pw   = lamb.cadr(sexpr)->coerce_Int_t();
  pca9685->setChannelPulseWidth(chan, 0, pw);
  return OBJ_UNDEF;
}

Sexpr_t PCA9685_mop3_setChannelServoPulseDuration(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  Int_t chan = lamb.car(sexpr)->as_Int_t();
  Int_t dur  = lamb.cadr(sexpr)->coerce_Int_t();
  pca9685->setChannelServoPulseDuration(chan, dur);
  return OBJ_UNDEF;
}

Sexpr_t PCA9685_mop3_setChannelOnAndOffTime(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  Int_t chan = lamb.car(sexpr)->as_Int_t();
  Int_t t_on = lamb.cadr(sexpr)->coerce_Int_t();
  pca9685->setChannelOnAndOffTime(chan, 0, t_on);
  return OBJ_UNDEF;
}

#endif

Sexpr_t PCA9685_install_mop3(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  ME("::PCA9685_install_mop3()");
  ll_try {    
#if LL_PCA09685
  
    static const struct { Lamb::Mop3st_t func;  const char *name; } std_procs[] = {

#define mk_dispatcher(__item__) { PCA9685_mop3_##__item__, "PCA9685."#__item__ }
      mk_dispatcher(begin),
      mk_dispatcher(setupSingleDevice),
      mk_dispatcher(setSingleDeviceToFrequency),
      mk_dispatcher(getSingleDeviceFrequency),
      mk_dispatcher(setChannelDutyCycle),
      mk_dispatcher(setChannelOnAndOffTime),
      mk_dispatcher(setChannelPulseWidth),
      mk_dispatcher(setChannelServoPulseDuration),
#undef mk_dispatcher

      { PCA9685_install_mop3, "PCA9685.install-mop3" }

    };
    
    const int Nstd_procs = sizeof(std_procs)/sizeof(std_procs[0]);

    if (pca9685) { delete pca9685;  pca9685 = 0; }
    pca9685 = new PCA9685;
    
    lamb.log("%s defining %d Mops\n", me, Nstd_procs);
    Sexpr_t env_target = lamb.car(sexpr);
    for (int i=0; i<Nstd_procs; i++) {
      Sexpr_t sym = lamb.mk_symbol(std_procs[i].name, env_exec);
      lamb.gc_root_push(sym);
      Sexpr_t proc = lamb.mk_Mop3_procst_t(std_procs[i].func, env_exec);
      lamb.gc_root_pop();
      lamb.dict_bind_bang(env_target, sym, proc, env_exec);
    }
#endif
    
    return NIL;
  }
  ll_catch();

}
