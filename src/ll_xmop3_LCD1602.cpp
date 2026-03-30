#include "LambLisp.h"

#if LL_LCD1602

#include "LiquidCrystal_I2C.h"

typedef LiquidCrystal_I2C LCD1602;

LCD1602 *lcd1602 = 0;

Sexpr_t LCD1602_mop3_begin(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  Int_t addr = lamb.car(sexpr)->mustbe_Int_t();
  Int_t cols = lamb.cadr(sexpr)->mustbe_Int_t();
  Int_t rows = lamb.caddr(sexpr)->mustbe_Int_t();
  
  if (lcd1602) { delete lcd1602;  lcd1602 = 0; }
  lcd1602 = new LiquidCrystal_I2C(addr, cols, rows);
  //lcd1602->begin(cols, rows, LCD_5x8DOTS);
  lcd1602->init();
  lcd1602->backlight();
  lcd1602->setCursor(0, 0);
  return OBJ_UNDEF;
}

Sexpr_t LCD1602_mop3_clear(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)		{ lcd1602->clear();   return OBJ_UNDEF; }
Sexpr_t LCD1602_mop3_home(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)		{ lcd1602->home();    return OBJ_UNDEF; }

Sexpr_t LCD1602_mop3_display(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  Bool_t onoff = true;
  if (sexpr != NIL) onoff = lamb.car(sexpr) != HASHF;
  if (onoff) lcd1602->display(); else lcd1602->noDisplay();
  return OBJ_UNDEF;
}

Sexpr_t LCD1602_mop3_cursor(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  Bool_t onoff = true;
  if (sexpr != NIL) onoff = lamb.car(sexpr) != HASHF;
  if (onoff) lcd1602->cursor(); else lcd1602->noCursor();
  return OBJ_UNDEF;
}

Sexpr_t LCD1602_mop3_backlight(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  Bool_t onoff = true;
  if (sexpr != NIL) onoff = lamb.car(sexpr) != HASHF;
  if (onoff) lcd1602->backlight(); else lcd1602->noBacklight();
  return OBJ_UNDEF;
}

Sexpr_t LCD1602_mop3_write(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  const char *s = lamb.car(sexpr)->mustbe_any_str_t()->any_str_get_chars();
  lcd1602->print(s);
  return OBJ_UNDEF;
}

Sexpr_t LCD1602_mop3_print(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  const char *s = lamb.car(sexpr)->mustbe_any_str_t()->any_str_get_chars();
  lcd1602->setCursor(0, 0);
  lcd1602->print(s);
  return OBJ_UNDEF;
}

Sexpr_t LCD1602_mop3_setCursor(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  Int_t col = lamb.car(sexpr)->mustbe_Int_t();
  Int_t row = lamb.cadr(sexpr)->mustbe_Int_t();
  lcd1602->setCursor(col, row);
  return OBJ_UNDEF;
}

#endif

Sexpr_t LCD1602_install_mop3(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  ME("::LCD1602_install_mop3()");
  ll_try {
#if LL-LCD1602
  
    static struct { Lamb::Mop3st_t func;  const char *name; } std_procs[] = {

#define mk_dispatcher(__item__) { LCD1602_mop3_##__item__, "LCD1602."#__item__ }
      mk_dispatcher(begin),
      mk_dispatcher(clear),
      mk_dispatcher(home),
      mk_dispatcher(display),
      mk_dispatcher(backlight),
      mk_dispatcher(cursor),
      mk_dispatcher(setCursor),
      mk_dispatcher(write),
      mk_dispatcher(print),
#undef mk_dispatcher
      LCD1602_install_mop3, "LCD1602-install-mop3"
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
