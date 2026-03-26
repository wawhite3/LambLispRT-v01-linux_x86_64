#include "LambLisp.h"

#if LL_WS2812

#include "WS2812_ESP32.h"

Sexpr_t mop3_neopixelWrite(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  ME("::mop3_neopixelWrite()");
  ll_try {
    Int_t args[4];
    for (int i=0; i<4; i++) {
      Sexpr_t arg = lamb.car(sexpr);
      args[i]     = arg->mustbe_Int_t();
      sexpr       = lamb.cdr(sexpr);
    }
    neopixelWrite(args[0], args[1], args[2], args[3]);
    return OBJ_VOID;
  }
  ll_catch();
}

WS2812 *ws2812 = 0;

Sexpr_t WS2812_mop3_begin(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  if (ws2812) { delete ws2812;  ws2812 = 0; }
  ws2812 = new WS2812(lamb.car(sexpr)->as_Int_t(),			//number of LEDs
		      lamb.cadr(sexpr)->as_Int_t(),			//LED pin
		      lamb.caddr(sexpr)->as_Int_t(),			//LED channel
		      (LED_TYPE) lamb.cadddr(sexpr)->mustbe_Int_t()	//LED type
		      );
  Int_t success = ws2812->begin();
  return lamb.mk_integer(success, env_exec);
}

Sexpr_t WS2812_mop3_setBrightness(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  Int_t brightness = lamb.car(sexpr)->coerce_Int_t();
  ws2812->setBrightness(brightness);
  return OBJ_UNDEF;
}

Sexpr_t WS2812_mop3_setLedColorData(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  Int_t index = lamb.car(sexpr)->coerce_Int_t();
  sexpr       = lamb.cdr(sexpr);
  Int_t val   = lamb.car(sexpr)->coerce_Int_t();

  Int_t ires = -1;
  if (lamb.cdr(sexpr) == NIL) ires = ws2812->setLedColorData(index, val);
  else {
    Int_t r = val;
    Int_t g = lamb.cadr(sexpr)->coerce_Int_t();
    Int_t b = lamb.caddr(sexpr)->coerce_Int_t();
    ires = ws2812->setLedColorData(index, r, g, b);
  }
  return lamb.mk_integer(ires, env_exec);
}

Sexpr_t WS2812_mop3_Wheel(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  Int_t wh   = lamb.car(sexpr)->mustbe_Int_t();
  Int_t ires = ws2812->Wheel(wh);
  return lamb.mk_integer(ires, env_exec);
}

Sexpr_t WS2812_mop3_show(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  return lamb.mk_integer(ws2812->show(), env_exec);
}

#endif

Sexpr_t WS2812_install_mop3(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  ME("::WS2812_install_mop3()");
  ll_try {
#if LL_WS2812

    static struct { Lamb::Mop3st_t func;  const char *name; } std_procs[] = {

#define mk_dispatcher(__item__) { WS2812_mop3_##__item__, "WS2812."#__item__ }
      mk_dispatcher(begin),
      mk_dispatcher(setBrightness),
      mk_dispatcher(Wheel),
      mk_dispatcher(setLedColorData),
      mk_dispatcher(show),
#undef mk_dispatcher
      { mop3_neopixelWrite, "neopixelWrite" },
      
      WS2812_install_mop3, "WS2812-install-mop3"
    };

    const int Nstd_procs = sizeof(std_procs)/sizeof(std_procs[0]);

    lamb.log("%s defining %d Mops\n", me, Nstd_procs);
    Sexpr_t env_target = lamb.car(sexpr);
    for (int i=0; i<Nstd_procs; i++) {
      const auto &p = std_procs[i];
      Sexpr_t sym   = lamb.mk_symbol(p.name, env_exec);
      lamb.gc_root_push(sym);
      Sexpr_t proc  = lamb.mk_Mop3_procst_t(p.func, env_exec);
      lamb.gc_root_pop();
      lamb.dict_bind_bang(env_target, sym, proc, env_exec);
    }
#endif
    return NIL;
  }
  ll_catch();
}
