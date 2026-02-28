#include "LambLisp.h"

/*! @name These are the non-VM operators that are local to this application.

  To add new Lisp-compatible C++ operators, follow this recipe:

  1. Create your C++ function, having the "mop3" signature, as this:
  ```
  Sexpr_t my_function(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec);

  ```
  In this example *my_function()* will receive a Lamb virtual machine, a symbolic expression *sexpr* of type *Sexpr_t* to be evaluated,
  and an environment in which to execute (which is also of type *Sexpr_t*).
  All the LambLisp native operators share this signature, so there is no "foreign function interface" as in other Lisps.
  All the functions with the *mop3* signature are native and will run at full C++ speed.

  **Warning:* To preserve the tail-recursion feature, *mop3* operators should not call other *mop3* operators.
  Recursive calls (direct or indirect) to the LambLisp S-expression partial evaluator will cause stack overflow.
  If you encounter a situation in which this seems desirable, you should instead factor out the common part
  and have the 2 *mop* operators call the common part separately.
 */

//!@{
Sexpr_t CommonIO_install_mop3(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec);
Sexpr_t ESP32_install_mop3(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec);
Sexpr_t PCA9685_install_mop3(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec);
Sexpr_t WS2812_install_mop3(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec);
Sexpr_t WiFi_install_mop3(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec);
Sexpr_t Wire_install_mop3(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec);
Sexpr_t Sonar_install_mop3(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec);
Sexpr_t LCD1602_install_mop3(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec);
#if LL_CUDA
Sexpr_t Cuda_install_mop3(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec);
#endif
//!@}

Sexpr_t install_local_native_operators(Lamb &lamb)
{
  ME("::install_local_native_operators()");

  /*! @name The LambLisp virtual machine operates by executing Lisp language native functions written in C++.
    
    This provides high performance at runtime, and scalability at compile/link time.
    The LambLisp virtual machine operations are referred to generically as *mops* or *mop3* (because they all take 3 parameters).
    In the Lamb VM code the C++ type name for a LambLisp-compatible function is *Mop3st_t* ("mop3 star type", with the st standing in for an asterisk).
    The type Mop3st_t is a pointer to a LambLisp-compatible native function.

    Installers for native procedures are themselves Mop3st types, but so far they do not return any useful value.
    It should be possible to reinstall these native operators at runtime, a feature of doubtful utility.
    In Lisp the function signature for an installer is ```(installer env-target)```.
    The C++ signature for *Mop3st_t* is ```Sexpr_t installer(Lamb &lamb, Sexpr_t list_containing_1_element_env_target, Sexpr_t env_execution);```

    Dictionaries are first-class objects in LambLisp.
    Dictionaries are hierarchical, implemented as a list of frames, each of which may be an alist or a vector of alists (used as a hash table).
    It is possible to add a new dictionary child frame on top of an existing parent dictionary.
    When a dictionary is searched, the topmost child is checked first, and the first matching key/value pair is returned.

    The LambLisp execution environment is a dictionary.  Dictionary keys may be any type, but when used as environments dictionary keys are all symbols.
    the LambLisp object system is based on dictionaries, which directly support the concept of *object inheritance*.
    Object keys are also symbols.
  */
    
  /*!These are the installers for additional language primitives (written in C++) required for this particular application.
    They do not require any header file; for each additional group of functions there is an installer that places the functions into the runtime environment.
    The installer is a global function, not a member of any class.
    All the installers have a same signature, of the *Mop3st_t* type.
   */
  const Lamb::Mop3st_t func[] = {
    CommonIO_install_mop3,
    ESP32_install_mop3, 
    WiFi_install_mop3,
    Wire_install_mop3,
    Sonar_install_mop3,
    PCA9685_install_mop3,
    WS2812_install_mop3,
    LCD1602_install_mop3,
#if LL_CUDA
    , Cuda_install_mop3
#endif
  };
  const int Nfuncs = sizeof(func)/sizeof(func[0]);

  Sexpr_t env_exec      = lamb.r5_interaction_environment();
  Sexpr_t env_target_sx = lamb.cons(env_exec, NIL, env_exec);	//put env into a list for the Mop3st calling protocol

  lamb.gc_root_push(env_target_sx);
  for (int i=0; i<Nfuncs; i++)
    Sexpr_t ignore = func[i](lamb, env_target_sx, env_exec);
  lamb.gc_root_pop();
  
  return OBJ_UNDEF;
}

/*!
  Allocate Lamb virtual machines dynamically, not statically.
  The constructors may depend on serial port availability or other operating system facility that is not available at static construction time.
*/
Lamb *lamb = 0;

void setup()
{
  unsigned long t_start = millis();
  ME("::setup()");

  LambStdio.begin();
  global_printf("[%lu] %s LambLisp starting, 1st light @%lu ms\n", millis(), me, t_start);
  lamb = new Lamb;	//avoid static allocation due to possible lack of terminal at static construct time
  lamb->setup();
  
  lamb->log("%s Installing local native operators\n", me);
  install_local_native_operators(*lamb);

  const char *setup_file = "setup.scm";
  lamb->log("%s Loading %s\n", me, setup_file);
  Sexpr_t ignored = lamb->load(setup_file, lamb->r5_interaction_environment(), 0);

}

void loop()
{
  ME("::loop()");
  ll_try {
    lamb->loop();
  }
  
  catch (Sexpr_t err) {	//not ll_catch, this is the last catch
    global_printf("[%lu] %s %s\n", millis(), me, err->str().c_str());
  }
}

#if LL_POSIX

int main(int argc, const char **v)
{
  setup();
  while (true) loop();
}
#endif
