#include "LambLisp.h"

#if LL_WIRE

TwoWire *LL_Wire = 0;

/*!
  (Wire.setWireTimeout)
  OR
  (Wire.setWireTimeout timeout reset-on-timeout-flag)
  
  With no parameters, some generally useful (but as yet unknown) values are used.
*/

Sexpr_t Wire_mop3_setTimeout(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  if (LL_Wire == 0) LL_Wire = &Wire;
  if (sexpr == NIL) { LL_Wire->setTimeout(1000); /*LL_Wire->setWireTimeout();*/ }	//default 1000 ms according to arduino docs
  else {
    Int_t timeout           = lamb.car(sexpr)->mustbe_Int_t();
    Bool_t reset_on_timeout = lamb.cadr(sexpr) != HASHF;
    //LL_Wire->setWireTimeout(timeout, reset_on_timeout);	//platform-dependent
    LL_Wire->setTimeout(timeout);
  }
  return OBJ_UNDEF;
}

Sexpr_t Wire_mop3_setPins(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  if (LL_Wire == 0) LL_Wire = &Wire;
  Int_t sda = lamb.car(sexpr)->mustbe_Int_t();
  Int_t scl = lamb.cadr(sexpr)->mustbe_Int_t();
  Int_t res = LL_Wire->setPins(sda, scl);
  return lamb.mk_integer(res, env_exec);
}

/*!(begin [ addr ])
  If address is provided , it is our slave bus address.
  If no address is provided, then we are the bus controller.
 */
Sexpr_t Wire_mop3_begin(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  ME("::Wire_mop3_begin()");
  ll_try {
    if (LL_Wire == 0) LL_Wire = &Wire;
    Int_t addr = 0;
    if (sexpr != NIL) addr = lamb.car(sexpr)->mustbe_Int_t();
    Int_t res = 0;
    if (addr) res = LL_Wire->begin(addr);
    else {
      res = LL_Wire->begin();
      LL_Wire->setTimeout(50);	//esp doc says this is default
    }
    return lamb.mk_bool(res, env_exec);
  }
  ll_catch();
}
/*!
  (Wire.requestFrom addr quantity [ release-bus-after ])
  OR
  (Wire.requestFrom addr quantity)

  If not provided, release-bus-after defaults to #t.
*/
Sexpr_t Wire_mop3_requestFrom(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  ME("Wire_mop3_requestFrom()");
  ll_try {
    Int_t addr = lamb.car(sexpr)->mustbe_Int_t();
    sexpr      = lamb.cdr(sexpr);
    Int_t qty  = lamb.car(sexpr)->mustbe_Int_t();
    sexpr      = lamb.cdr(sexpr);
    Int_t stop = (sexpr == NIL) ? true : (lamb.car(sexpr) != HASHF);
    Int_t res  = LL_Wire->requestFrom(addr, qty, stop);
    
    return lamb.mk_integer(res, env_exec);
  }
  ll_catch();
}

Sexpr_t Wire_mop3_setClock(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)			{ LL_Wire->setClock(lamb.car(sexpr)->coerce_Int_t()); return OBJ_UNDEF; }
Sexpr_t Wire_mop3_beginTransmission(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)	{ LL_Wire->beginTransmission(lamb.car(sexpr)->mustbe_Int_t()); return OBJ_UNDEF; }
Sexpr_t Wire_mop3_available(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)		{ return lamb.mk_integer(LL_Wire->available(), env_exec); }
Sexpr_t Wire_mop3_read(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)			{ return lamb.mk_integer(LL_Wire->read(), env_exec); }
Sexpr_t Wire_mop3_end(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)			{ return lamb.mk_bool(LL_Wire->end(), env_exec); }

Sexpr_t Wire_mop3_onReceive(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)		{ return OBJ_UNDEF; }
Sexpr_t Wire_mop3_onRequest(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)		{ return OBJ_UNDEF; }
Sexpr_t Wire_mop3_clearWireTimeoutFlag(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)	{ return OBJ_UNDEF; }
Sexpr_t Wire_mop3_getWireTimeoutFlag(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)	{ return OBJ_UNDEF; }

Sexpr_t Wire_mop3_endTransmission(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  ME("Wire_mop3_endTransmission()");
  
  Bool_t stop = true;
  if (sexpr != NIL) stop = lamb.car(sexpr) != HASHF;
  Int_t res = LL_Wire->endTransmission(stop);
  Sexpr_t res_sx = OBJ_UNDEF;
  if (res == 0) res_sx = HASHF;
  else if (res == 1) res_sx = lamb.mk_string(env_exec, "%s xmit overfow", me);
  else if (res == 2) res_sx = lamb.mk_string(env_exec, "%s xmit addr NAK", me);
  else if (res == 3) res_sx = lamb.mk_string(env_exec, "%s xmit data NAK", me);
  else if (res == 4) res_sx = lamb.mk_string(env_exec, "%s other error", me);
  else if (res == 5) res_sx = lamb.mk_string(env_exec, "%s timeout", me);
  else res_sx = lamb.mk_string(env_exec, "%s unknown error %d", me, res);
  return res_sx;
}

Sexpr_t Wire_mop3_write(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  ME("Wire_mop3_write()");
  auto wiresend = [](Int_t n, uint8_t *buf) { return LL_Wire->write(buf, n); };
  
  Int_t ires = 0;
  Int_t typ = sexpr->type();
  
  if (sexpr == NIL) {}
  else if (typ == Cell::T_CHAR) ires = LL_Wire->write(sexpr->as_Char_t() & 0xff);
  else if (typ == Cell::T_INT)  ires = LL_Wire->write(sexpr->as_Int_t() & 0xff);
  else if (sexpr->is_any_str_atom()) ires = LL_Wire->write(sexpr->any_str_get_chars());
  else if (sexpr->is_any_bvec_atom()) {
    Int_t nelems;
    ByteVec_t elems;
    sexpr->any_bvec_get_info(nelems, elems);
    ires = LL_Wire->write(elems, nelems);
  }
  return lamb.mk_integer(ires, env_exec);
}
#endif

//Install all Wire symbols in base environment.
//
Sexpr_t Wire_install_mop3(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  ME("::Wire_install_mop3()");
  ll_try {
#if LL_WIRE
  
    static const struct { Lamb::Mop3st_t func;  const char *name; } std_procs[] = {

#define mk_dispatcher(__item__) { Wire_mop3_##__item__, "Wire."#__item__ }
      mk_dispatcher(begin),
      mk_dispatcher(end),
      mk_dispatcher(requestFrom),
      mk_dispatcher(beginTransmission),
      mk_dispatcher(endTransmission),
      mk_dispatcher(write),
      mk_dispatcher(available),
      mk_dispatcher(read),
      mk_dispatcher(setClock),
      mk_dispatcher(onReceive),
      mk_dispatcher(onRequest),
      mk_dispatcher(setPins),
      mk_dispatcher(setTimeout),
      //mk_dispatcher(setWireTimeout),
      mk_dispatcher(clearWireTimeoutFlag),
      mk_dispatcher(getWireTimeoutFlag),
#undef mk_dispatcher
      
      { Wire_install_mop3, "Wire.install-mop3" }
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
