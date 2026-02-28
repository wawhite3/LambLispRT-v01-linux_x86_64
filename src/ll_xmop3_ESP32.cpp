#include "LambLisp.h"

#if LL_ESP32

#include "esp_system.h"
#include "esp_efuse.h"

Sexpr_t mop3_esp32_efuse_mac_get_default(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  byte mac[6];
  esp_efuse_mac_get_default(mac);
  return lamb.mk_bytevector(6, mac, env_exec);
}

Sexpr_t mop3_esp32_efuse_pkg_ver(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)	{ return lamb.mk_integer(esp_efuse_get_pkg_ver(), env_exec); }

Sexpr_t mop3_esp32_efuse_read_block(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  ME("::mop3_esp32_efuse_read_block()");
  ll_try {
    Sexpr_t blk        = lamb.car(sexpr);
    Sexpr_t bit_offset = lamb.cadr(sexpr);
    Sexpr_t Nbits_sx   = lamb.caddr(sexpr);

    Int_t Nbits  = Nbits_sx->mustbe_Int_t();
    Int_t Nbytes = Nbits / 8;
    if (Nbits & 7) Nbytes++;
    Sexpr_t res = lamb.mk_bytevector(Nbytes, 0);
    Int_t n;
    ByteVec_t elems;
    res->any_bvec_get_info(n, elems);
    
    auto err = esp_efuse_read_block((esp_efuse_block_t) blk->mustbe_Int_t(), elems, bit_offset->mustbe_Int_t(), Nbits);
    if (err != ESP_OK) res = lamb.mk_integer(err, env_exec);
    return res;
  }
  ll_catch();
}

  
Sexpr_t mop3_esp32_efuse_write_block(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  ME("::mop3_esp32_efuse_write_block");
  ll_try {
    Sexpr_t blk        = lamb.car(sexpr);
    Sexpr_t bits       = lamb.cadr(sexpr);
    Sexpr_t bit_offset = lamb.caddr(sexpr);
    Sexpr_t Nbits      = lamb.cadddr(sexpr);

    ByteVec_t p = 0;
    Int_t i = 0;
    if (bits->type() == Cell::T_INT) {
      i = bits->as_Int_t();
      p = (ByteVec_t) &i;
    }
    else bits->any_bvec_get_info(i, p);
    //DEBT all this is likely wrong
    auto err = esp_efuse_write_block((esp_efuse_block_t) blk->mustbe_Int_t(), bits, bit_offset->mustbe_Int_t(), Nbits->mustbe_Int_t());
    return lamb.mk_integer(err, env_exec);
  }
  ll_catch();
}

#endif

Sexpr_t ESP32_install_mop3(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  ME("::ESP32_install_mop3()");
  ll_try {
#if LL_ESP32

    static struct { Lamb::Mop3st_t func;  const char *name; } std_procs[] = {
      ESP32_install_mop3, "ESP32-install-mop3",
      mop3_esp32_efuse_mac_get_default, "esp32-efuse-mac-get-default",
      mop3_esp32_efuse_pkg_ver, "esp32-efuse-pkg-ver",
      mop3_esp32_efuse_read_block, "esp32-efuse-read-block",
      mop3_esp32_efuse_write_block, "esp32-efuse-write-block"
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

