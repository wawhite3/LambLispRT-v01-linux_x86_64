#include "LambLisp.h"

#if LL_WIFI

WiFiClass *LL_WiFi;

Sexpr_t WiFi_mop3_begin(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  ME("::WiFi_mop3_begin()");
  ll_try {
    Sexpr_t ssid = lamb.car(sexpr);
    Sexpr_t pwd  = lamb.cadr(sexpr);
    Charst_t id  = ssid->any_str_get_chars();
    Charst_t pw  = pwd->any_str_get_chars();
    Int_t status = LL_WiFi->begin(ssid->any_str_get_chars(), pwd->any_str_get_chars());
    return lamb.mk_integer(status, env_exec);
  }
  ll_catch();
}

Sexpr_t WiFi_mop3_disconnect(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  Sexpr_t res = OBJ_UNDEF;
  Bool_t wifioff = false;
  Bool_t eraseap = false;
  
  if (sexpr != NIL) {
    if (lamb.car(sexpr) != HASHF) wifioff = true;
    sexpr = lamb.cdr(sexpr);
  }
  if (sexpr != NIL)
    if (lamb.car(sexpr) != HASHF) eraseap = true;
  return lamb.mk_bool(LL_WiFi->disconnect(wifioff, eraseap), env_exec);
}

Sexpr_t WiFi_mop3_config(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  ME("::WiFi_mop3_config()");
  ll_try {
    Sexpr_t sx_ip      = OBJ_UNDEF;
    Sexpr_t sx_dns     = OBJ_UNDEF;
    Sexpr_t sx_gateway = OBJ_UNDEF;
    Sexpr_t sx_subnet  = OBJ_UNDEF;
    if (sexpr == NIL) throw lamb.mk_syserror("%s IP address required", me);
    sx_ip = lamb.car(sexpr);
    sexpr = lamb.cdr(sexpr);
    if (sexpr != NIL) {	sx_dns     = lamb.car(sexpr);  sexpr = lamb.cdr(sexpr); }
    if (sexpr != NIL) { sx_gateway = lamb.car(sexpr);  sexpr = lamb.cdr(sexpr); }
    if (sexpr != NIL) { sx_subnet  = lamb.car(sexpr);  sexpr = lamb.cdr(sexpr); }
    //DEBT what to do with those things
    return OBJ_UNDEF;
  }
  ll_catch();
}

Sexpr_t WiFi_mop3_setDNS(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  Sexpr_t res = OBJ_UNDEF;
  return res;
}

Sexpr_t WiFi_mop3_SSID(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)		{ return lamb.mk_string(env_exec, LL_WiFi->SSID().c_str()); }
Sexpr_t WiFi_mop3_BSSID(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)		{ return lamb.mk_string(env_exec, LL_WiFi->BSSIDstr().c_str()); }
Sexpr_t WiFi_mop3_RSSI(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)		{ return lamb.mk_integer(LL_WiFi->RSSI(), env_exec); }
Sexpr_t WiFi_mop3_scanNetworks(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)	{ return lamb.mk_integer(LL_WiFi->scanNetworks(), env_exec); }
Sexpr_t WiFi_mop3_localIP(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)	{ return lamb.mk_string(env_exec, LL_WiFi->localIP().toString().c_str()); }
Sexpr_t WiFi_mop3_subnetMask(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)	{ return lamb.mk_string(env_exec, LL_WiFi->subnetMask().toString().c_str()); }
Sexpr_t WiFi_mop3_gatewayIP(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)	{ return lamb.mk_string(env_exec, LL_WiFi->gatewayIP().toString().c_str()); }
Sexpr_t WiFi_mop3_getSocket(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)	{ return lamb.mk_string(env_exec, "<LL_WiFi->getSocket() unsupported>"); }

Sexpr_t WiFi_mop3_encryptionType(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  ME("::WiFi_mop3_encryptionType()");
  ll_try {
    byte ifc = 0;
    if (sexpr != NIL) {
      Sexpr_t sx_ifc = lamb.car(sexpr);
      if (sx_ifc->type() != Cell::T_INT) throw lamb.mk_syserror("%s bad type %d", me, sx_ifc->type());
      ifc = (byte) sx_ifc->as_Int_t();
    }
    return lamb.mk_integer(LL_WiFi->encryptionType(ifc), env_exec);
  }
  ll_catch();
}

Sexpr_t WiFi_mop3_macAddress(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  byte mac[6];
  LL_WiFi->macAddress(mac);

  String rstr = "";
  String colon = "";
  for (int i=0; i<6; i++) {
    rstr = toString("%s%s%02x", rstr.c_str(), colon.c_str(), mac[i] & 0xff);
    colon = ":";
  }
  return lamb.mk_string(env_exec, rstr.c_str());
}

Sexpr_t WiFi_mop3_status(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  Int_t stat   = LL_WiFi->status();
  Charst_t msg = "WL_UNKNOWN";
  switch (stat) {
  case WL_NO_SHIELD:		msg = "WL_NO_SHIELD";		break;
  case WL_IDLE_STATUS:		msg = "WL_IDLE_STATUS";		break;
  case WL_NO_SSID_AVAIL:	msg = "WL_NO_SSID_AVAIL";	break;
  case WL_SCAN_COMPLETED:	msg = "WL_SCAN_COMPLETED";	break;
  case WL_CONNECTED:		msg = "WL_CONNECTED";		break;
  case WL_CONNECT_FAILED:	msg = "WL_CONNECT_FAILED";	break;
  case WL_CONNECTION_LOST:	msg = "WL_CONNECTION_LOST";	break;
  case WL_DISCONNECTED:		msg = "WL_DISCONNECTED";	break;
  }

  Sexpr_t res = NIL;
  Sexpr_t sx_msg  = lamb.mk_string(env_exec, msg);
  res = lamb.cons(sx_msg, res, env_exec);

  lamb.gc_root_push(res);
  Sexpr_t sx_stat = lamb.mk_integer(stat, env_exec);
  lamb.gc_root_pop();

  res = lamb.cons(sx_stat, res, env_exec);
  return res;
}

#endif

//Install all WiFi symbols in base environment.
//
Sexpr_t WiFi_install_mop3(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec)
{
  ME("::WiFi_install_mop3()");
  ll_try {    
#if LL_WIFI  
    static const struct { Lamb::Mop3st_t func;  const char *name; } std_procs[] = {

#define mk_dispatcher(__item__) { WiFi_mop3_##__item__, "WiFi."#__item__ }
      mk_dispatcher(begin),
      mk_dispatcher(disconnect),
      mk_dispatcher(config),
      mk_dispatcher(setDNS),
      mk_dispatcher(SSID),
      mk_dispatcher(BSSID),
      mk_dispatcher(RSSI),
      mk_dispatcher(encryptionType),
      mk_dispatcher(scanNetworks),
      mk_dispatcher(status),
      mk_dispatcher(getSocket),
      mk_dispatcher(macAddress),
      mk_dispatcher(localIP),
      mk_dispatcher(subnetMask),
      mk_dispatcher(gatewayIP),
#undef mk_dispatcher
      
      { WiFi_install_mop3, "WiFi.install-mop3" }

    };
    
    const int Nstd_procs = sizeof(std_procs)/sizeof(std_procs[0]);
  
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
