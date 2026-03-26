#include "LambLisp.h"
#include "ll_platform_generic.h"

LambPlatform lambPlatform;

Sexpr_t Platform_mop3_free_heap(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)	{ return lamb.mk_integer(lambPlatform.free_heap(), env_exec); }
Sexpr_t Platform_mop3_free_stack(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)	{ return lamb.mk_integer(lambPlatform.free_stack(), env_exec); }
Sexpr_t Platform_mop3_micros(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)		{ return lamb.mk_integer(micros(), env_exec);  }
Sexpr_t Platform_mop3_millis(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)		{ return lamb.mk_integer(millis(), env_exec);  }
Sexpr_t Platform_mop3_loop_elapsed_ms(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)	{ return lamb.mk_integer(lambPlatform.loop_elapsed_ms(), env_exec);  }
Sexpr_t Platform_mop3_loop_elapsed_us(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)	{ return lamb.mk_integer(lambPlatform.loop_elapsed_us(), env_exec);  }
Sexpr_t Platform_mop3_lamb_reboot(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)		{ lambPlatform.reboot(); return OBJ_UNDEF; }

Sexpr_t Platform_mop3_random_bytevector(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  ME("::Platform_mop3_random_bytevector()");
  Sexpr_t n_sx = lamb.car(sexpr);
  if (n_sx->type() != Cell::T_INT) throw NIL->mk_error("%s Bad type %s\n", me, n_sx->str().c_str());
  
  Int_t n    = lamb.car(sexpr)->as_Int_t();
  Sexpr_t bv = lamb.mk_bytevector(n, env_exec);
  Int_t nelems;
  ByteVec_t elems;
  bv->any_bvec_get_info(nelems, elems);
  lambPlatform.rand(elems, nelems);
  return bv;
}

Sexpr_t Platform_mop3_random_integer(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)	{ Int_t r = 0;  lambPlatform.rand((byte *) &r, sizeof(r));  return lamb.mk_integer(r, env_exec); }
Sexpr_t Platform_mop3_random_real(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  static long double rimax = 0;
  once({
      long long imax = 0x7f;
      Int_t n = sizeof(imax);
      for (int i=1; i<n; i++) imax = (imax << 8) | 0xff;	//skip sign byte already in sample
      rimax = ((long double) imax) + 1.0;
    });
  
  long long iran = 0;
  lambPlatform.rand((byte *) &iran, sizeof(iran));
  if (iran < 0) iran = -iran;
  
  long double rran = (long double) iran;
  return lamb.mk_real(rran / rimax, env_exec);
}

Sexpr_t __lamb_platform_generic_install_mop3(Lamb &lamb, Sexpr_t list_containing_env_target, Sexpr_t env_exec)
{
  ME("_lamb_platform_install_mop3()");

  ll_try {
    static const struct {
      Lamb::Mop3st_t func;
      const char *name;
    } hal_bindings[] = {

      Platform_mop3_micros, "micros",
      Platform_mop3_millis, "millis",
      Platform_mop3_micros, "Platform.micros",
      Platform_mop3_millis, "Platform.millis",
      Platform_mop3_lamb_reboot, "Platform.reboot",
      Platform_mop3_random_integer, "Platform.random-integer",
      Platform_mop3_random_real, "Platform.random-real",
      Platform_mop3_loop_elapsed_us, "Platform.loop-elapsed-us",
      Platform_mop3_loop_elapsed_ms, "Platform.loop-elapsed-ms",
      Platform_mop3_free_heap, "Platform.free-heap",
      Platform_mop3_free_stack, "Platform.free-stack",

      __lamb_platform_generic_install_mop3, "Platform.install-mop3"
      
    };
    
    const Int_t Nsyms = sizeof(hal_bindings)/sizeof(hal_bindings[0]);

    Sexpr_t env_target = lamb.car(list_containing_env_target);
    lamb.log("%s defining %d Mops\n", me, Nsyms);
    for (int i=0; i<Nsyms; i++) {
      auto p = hal_bindings[i];
      Sexpr_t sym  = lamb.mk_symbol(p.name, NIL);
      Sexpr_t proc = lamb.mk_Mop3_procst_t(p.func, sym);
      lamb.dict_bind_bang(env_target, sym, proc, env_exec);
    }

    return OBJ_UNDEF;
  }
  
  ll_catch();
}

