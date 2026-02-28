#ifndef LL_LAMB_H
#define LL_LAMB_H

#include "ll_platform_generic.h"

/*! @file
  This file is the header file required to use LambLisp.

  The LambLisp abstract machine is built on a solution stack of several parts:
  -  Part 0 is in the generic platform header file, containing definitions for foundational element of the underlying system.
  -  Part 1 contains definitions for general-purpose low-level data types and utility functions.
  -  Part 2 defines the Cell structure and its accessors.
  -  Part 3 defines the LambLisp Virtual Machine.
*/

//! @name Scheme partially defines *ports*, but LambLisp tries to keep them abstracted as far as possible, traceable back to the RxRS specifications.
//!@{
class LL_Port;			//!<This is a placeholder for the underlying C++ port defined elsewhere,  Here we only need pointers to instances of it.
typedef LL_Port Port_t;		//!<An alias for the underlying port implementation, conforming to the LambLisp VM shared type nomenclature.
typedef Port_t *Portst_t;	//!<A pointer to the shared type representing a *port* as described in the RxRS specifications.
//!@}

//!Facility to automatically delete C++ objects from Lisp.
typedef void CPPDeleter(void *cpp_obj);
typedef CPPDeleter *CPPDeleterPtr;

class Cell;		//forward
typedef Cell *Sexpr_t;	//!<A symbolic expression is a pointer to a cell.

/*! @name These singletons exist outside the general cell population.
  Lisp implementations are notoriously variable on the type and details of NIL.  In some, NIL is a *well-known symbol*, whose type is *list* and
  whose *car* and *cdr* are also NIL (pointing to itself).

  In *Scheme*, there is no symbol NIL but the empty parenthesis are used as in ().  The empty list is its own type, which is is a list, and an atom, but not a pair.
  In LambLisp, the singleton Cell NIL is allocated statically with the appropriate initializers to fill the role.
  
  Several other statically allocated symbols are also defined in this group.  They are available at compile time.
*/
//!@{
extern Sexpr_t NIL;		//!<The empty list.
extern Sexpr_t HASHT;		//!<The canonical *true* value.
extern Sexpr_t HASHF;		//!<The canonical *false* value.
extern Sexpr_t OBJ_EOF;		//!<A single EOF object is used to indicate end-of-file on all ports.
extern Sexpr_t OBJ_UNDEF;	//!<A single UNDEF object is used to initialize a Cell having no immediate default value, or to indicate an unspecified value (in RxRS) that should not be encountered by the evaluator.
extern Sexpr_t OBJ_VOID;	//!<A silent version of OBJ_UNDEF.  It will not cause an error and will produce no output.  Use with care because it will hide potential error that would otherwise be obvious.
extern Sexpr_t OBJ_SYSERROR;	//!<A single instance of T_ERROR, statically alloocated with a fixed size buffer (also statically allocated ) for use by Cell-level errors
//!@}

/*! @class Cell
  The Cell class is the foundational class for the LambLisp Virtual Machine.

  Nearly all the Cell methods are *inline* for performance.
  A Cell has only getters and setters; there are no other side effects.
  Cell fields are changed only through explicit requests, and not as the result of any other mutations.
  This means that Cells do not participate in garbage collection, which is managed from outside the Cell class.
  Indeed, there is no concept within the Cell that there might be a plurality of them; only the behavior of a single Cell is defined.

  A Cell may be one of several types.  Historically, the number of types varied with the variant of Lisp.
  In LambLisp, there are Cell types that correspond directly to the types described in the *Scheme RxRS* specifications (integers, procedures etc),
  and there are additional types that implement underlying behavior to support the higher-level language (thunks, dictionaries).

  For example, LambLisp supports several types of *strings* internally, depending on whether the character are stored on the heap,
  in externally-provided memory, or immediately within the cell (for fast operations on short strings).  Each of these *string* types
  is compatible with the string type in the *Scheme RxRS* specifications.  Likewise, bytevectors have external and immediate variants.
  
  LambLisp also supports specialized *pair* types, such as the LambLisp *dictionary*.
  These act as *Lisp* pairs for purposes of allocation and garbage collection, but their specialized operators are executed by the underlying LambLisp virtual machine and therefore operate at C++ speed.

  The Cell type enumeration is purposefully ordered in such a way as to allow efficient integer comparisons instead of a C switch in most cases,
  enhancing runtime performance as well as garbage collection.

  | Cell enumeration characteristics:                                              |
  |--------------------------------------------------------------------------------|
  | Cells requiring submarking                                                     |
  | Cells requiring finalizing                                                     |
  | Simple atoms: immediate cell types like bool char int real etc                 |
  | External (non-GC) objects and pointers to native C++ code                      |
  | Cells that point to C++-allocated objects, paired with optional GC finalizers. |
  | "well-known singleton atoms" such as NIL, undefined, and void types.           |
  | Pair - the Cell type that responds to the Lisp *pair?* predicate.              |
  | Other pairs - specialized pair types                                           |

  With this ordering of Cell types, the solution to many common cases during expression evaluation can be obtained with an inequality,
  rather than a C++ *switch* statement.
  This provides a significant performance boost, because the most common cases are checked first, and the total number of cases is reduced.
  
  | Cell optimized type tests                                                                                                          |
  |-------------------------------------------------------------------------------------------------------------------------------|
  | (Types <  "simple atoms") need specialized GC marking and heap reclamation.                                                   |
  | (Types <= "cells requiring finalizing") need specialized GC heap reclamation.                                                 |
  | (Types >= T_NIL) are lists.                                                                                                   |
  | (Types >  T_NIL) are pairs (but not only *Lisp* pairs, extended pairs too).                                                   |
  | (Types != T_PAIR) are atoms, including numbers, strings, vectors, and singletons such as NIL.                                 |
  | (Types >  T_PAIR) are extended pairs, used for specialized lists known to LambVM, but receiving regular *pair* GC processing. |

  There are cases where the ordering does not help so much.  For example the LambLisp type system allows for several types of strings.
  Because the type system is ordered according to GC requirements, these are not adjacent in the enumeration and less amenable to inequality tests.
  For these cases, there is a cell feature matrix available to be queried by type and feature.
  This allows the determination to be made with an array lookup, which is faster than several sequential tests or a C *switch*,
  putting a permanent cap on the cost of a type check.  It also allows other details such as `is-immediate?` to be implemented inexpensively.

  The garbage collector states are ordered for similar reasons.  See the garbage collector chapter for details on the Cell life cycle.
*/
class Cell {
public:
  /*! \name Static Cell constructors, used only for "well-known" atomic cells (nilL, true, false etc).
    
    In general, it is best not to do anything complex at static construction time.  There is no guarantee that dependencies will be ready.
    In particular, use of the terminal may cause the system to crash.
    Most applications should leave any construction activities to more purposeful code at runtime, using only cells obtained through *cons*.
  */
  ///@{
  Cell() {}
  
  Cell(Word_t typ, Word_t  w1, Word_t  w2)	{ type(typ);  rplaca(w1);  rplacd(w2); }
  Cell(Word_t typ, Int_t   w1, Sexpr_t w2)	{ type(typ);  rplaca((Word_t) w1);  rplacd((Word_t) w2); }
  Cell(Word_t typ, Sexpr_t w1, Sexpr_t w2)	{ type(typ);  rplaca((Word_t) w1);  rplacd((Word_t) w2); }
  ///@}

  /*! \name Cell type enumeration

| Note the following properties of this enumeration:                                                                             |
|--------------------------------------------------------------------------------------------------------------------------------|
| NIL is a singleton, in the middle of the enumeration.                                                                          |
| Types <= NIL are atoms.                                                                                                        |
| Types >= NIL are lists.                                                                                                        |
| Types >  NIL are pairs.                                                                                                        |
| Types <= T_PORT_HEAP are heap-allocated objects, requiring specialized finalizing but not specialized marking.                 |
| Types <= T_ANY_HEAP_SVEC are heap allocated vectors requiring specialized marking in addition to finalizing.                   |
| For Scheme purposes, there is one `pair` type, but there are several other specialized `pair` types that facilitate execution. |
| These additional `pair` types are *atoms*, but list operations such as `car`, `cdr`, and `append` can operate on them.         |

  */
  //!@{
  enum {
    //Complex atoms first
    T_SVEC_HEAP=0,	//!<(Int_t Sexpr_t[])	A vector of Sexprs is a (len Sexpr_t*) pair.
    T_SVEC2N_HEAP,	//!<(Int_t Sexpr_t[]) 	Same as vector, but length must be a power of 2; useful for hash tables.
    
    T_ANY_HEAP_SVEC = T_SVEC2N_HEAP,	//!<Any types less than or equal to this are heap-stored vectors of S-expressions.  They need specialized marking at GC time.
    
    T_SYM_HEAP,		//!<(Int_t Charst_t)	Symbol is a (hash char*) pair.
    T_BVEC_HEAP,	//!<(Int_t Bytest_t)	Bytevector is a (len byte*) pair.
    T_STR_HEAP,		//!<(reserved Charst_t)	The reserved field may be used in future to store the string length (not possible with immediate strings, so some details to be worked).
    T_CPP_HEAP,		//!<(ptr-to-cpp-deleter ptr-to-cpp-object)   Deleter is a function ```void f(void *cpp_obj)``` of the appropriate type T of the C++ object, and performes ```delete (T *) cpp_obj;```

    T_PORT_HEAP,	//!<(reserved  Ptr_t)	car is unused, cdr is ptr to underlying C++ port instance
    T_NEEDS_FINALIZING = T_PORT_HEAP,	//!<Any types less than or equal to this have data stored in the heap, and need specialized finalizing at GC time.
    
    T_BVEC_EXT,		//!<(Int_t Bytest_t)	Same as T_BYTEVEC but externally allocated; the byte array is not freed at GC time.
    T_STR_EXT,		//!<(reserved Charst_t)	Same as T_STR but externally allocated; the character array is not freed at GC time.

    T_BVEC_IMM,		//!<Special format: type and flag bytes as usual, byte 2 is vector length, remaining bytes are vector elements.
    T_STR_IMM,		//!<Special format: type and flag bytes as usual, remaining bytes are 0-terminated string embedded in the cell.
    T_GENSYM,		//!<Runtime symbol generation with no heap operations.

    //Simple atoms next
    T_BOOL,		//!<(Bool_t reserved)	Boolean atom
    T_CHAR,		//!<(Char_t reserved)	Character atom
    T_INT,		//!<(Int_t reserved)	Integer atom
    T_REAL,		//!<(Special format: Real_t uses double word)	Real number atom
    T_RATIONAL,		//!<(Int_t Int_t)	The fields are numerator and denominator of a rational number.

    //Interface atoms to C++ functions.  No garbage collection finalization required.
    T_MOP3_PROC,	//!<(reserved *Mop3st_t)	pointer to native function - args are evaluated before calling
    T_MOP3_NPROC,	//!<(reserved *Mop3st_t)	pointer to native macro processor - args are not evaluated before calling

    //T_VOID and T_UNDEF are singletons
    T_VOID,		//!<(don't care) VOID has its own type and a singleton instance OBJ_VOID
    T_UNDEF,		//!<(ERROR ERROR) UNDEF has its own type and a singleton instance OBJ_UNDEF

    /*!
      - The NIL singleton is both a list and an atom, but not a pair.
      - Types <= T_NIL are atoms, types >= T_NIL are lists, types > T_NIL are pairs, but only T_PAIR responds to the Scheme *pair?* predicate.
    */
    T_NIL,		//!<(ERROR ERROR) NIL has its own type and a singleton instance.

    /*!
      - Pair types.  Types > NIL and types >= T_PAIR are pairs, but only T_PAIR responds to Scheme (pair?).
      - All pair types can be garbage collected without type-specific GC marking or finalizing.
    */
    T_PAIR,		//!<(Sexpr_t Sexpr_t) Normal untyped cons cell.  All C++ types < T_PAIR are atoms.
    T_SVEC_IMM,		//!<(Sexpr_t Sexpr_t) Vector of 0, 1 or 2 elements.
    T_PROC,		//!<(Sexpr_t Sexpr_t) Procedure pair; car is lambda (formals + body containing free variables), cdr is environment.
    T_NPROC,		//!<(Sexpr_t Sexpr_t) Non-evaluating procedure pair; car is nlambda (formals + body containing free variables), cdr is environment.
    T_MACRO,		//!<(Sexpr_t Sexpr_t) Macro; car is transformer (proc of 1 arg) and cdr is env.
    T_DICT,		//!<(Sexpr_t Sexpr_t) Dictionary pair; car is local frame, cdr is list of parent frames, used for environments & object instances.
    T_THUNK_SEXPR,	//!<(Sexpr_t Sexpr_t) S-expression thunk pair; car is sexpr, cdr is environment with all variable bindings.
    T_THUNK_BODY,	//!<(Sexpr_t Sexpr_t) Code body thunk pair; car is body (i.e., list of sexprs), cdr is environment with all variable bindings.
    T_ERROR,		//!<(Sexpr_t Sexpr_t) An error cell; car is a T_STRING, cdr is a pointer to *irritants*.

    Ntypes	//!<Number of LambLisp virtual machine types
  };
  //!@}
  
  //! \name Cell flags, including gc states for multi-pass incremental gc, tail marker, and spares.
  ///@{
  static const int F_GC01 = 0x01;	//!<gc flag
  static const int F_GC02 = 0x02;	//!<gc flag
  static const int F_GC04 = 0x04;	//!<gc flag
  static const int F_TAIL = 0x08;	//!<trampoline tail marker
  static const int F_0x10 = 0x10;	//!<spare
  static const int F_0x20 = 0x20;	//!<spare
  static const int F_0x40 = 0x40;	//!<spare
  static const int F_0x80 = 0x80;	//!<spare
  static const int GC_STATE_MASK  = F_GC01 | F_GC02 | F_GC04;	//!<Mask for obtaining garbage collection state bits from Cell flag byte.
  ///@}
  
  /*! \name Low-level cell field manipulation.

    Note that the type field occupies all of byte 0, although not all bits are required.
    This speeds up the very common type-checking operation, because no mask is required to extract the type bits.
    
    The flags are in byte 1, and a mask operation is required to access the individual flag fields.
  */
  //!@{
#define _type_		(_contents._byte[0])
#define _flags_		(_contents._byte[1])
#define _car_		(_contents._word[1])
#define _cdr_		(_contents._word[2])
#define _car_addr_	(&(_car_))
#define _int_ptr_	((Int_t *) _car_addr_)
#define _real_ptr_	((Real_t *) _car_addr_)
#define _byte2_ptr_	(&(_contents._byte[2]))

  void  zero()			{ _contents._word[0] = _contents._word[1] = _contents._word[2] = 0; }	//!<Set all cell bits to zero.
  Int_t type(void)		{ return _type_; }	//!<Return the type of the cell as a small integer.
  void  type(Int_t t)		{ _type_  = t; }	//!<Set the type of this cell.

  Int_t flags(void)		{ return _flags_; }	//!<Return the entire set of cell flags.
  void  flags_set(Int_t f)	{ _flags_ |= f; }	//!<Set the selected flags.
  void  flags_clr(Int_t f)	{ _flags_ &= ~f; }	//!<Clear the selected flags.

  void  rplaca(Word_t p)	{ _car_ = p; }		//!<Replace the cell car field.  Called rplaca for historical reasons, and to distinguish it from set-car!, which must respect the GC flags.
  void  rplacd(Word_t p)	{ _cdr_ = p; }		//!<Replace the cell cdr field.  Called rplacd for historical reasons, and to distinguish it from set-cdr!, which must respect the GC flags.
  //!@}

  /*! \name Cell type testing

    The cell type enumeration has been designed to group together cell types according to their most common operations.
    
    The main groups are those needing special sweep and finalizing during garbage collection, those that are treated as pairs, and simple atoms.
    There is a subgroup of those needing special sweep, that also need specialised marking during the garbage collection mark phase.

    There are some types that have optimized subtypes (such as **immediate** types), and they differ in their processing at GC time.
    Type-testing functions below have `is_any_x()` predicates that group all subtypes together (e.g., `is_any_str_atom()` returns **true** for any type of string (heap, immediate, external).
    There are also functions further down that will return the contents of complex cells, and throw an error if called with incorrect type.
    Therefore it is often not necessary to use the predicates before accessing the atom internals with `any_x_get_info()`.

    Note also: it is sometimes faster to check the types directly, rather than check the cell feature table.
    Preliminary testing indicates more than 2 type tests should use the table instead.
  */
  //!@{

  typedef struct {
    Int_t typ;
    bool is_any_pair, is_any_svec, is_any_svec2n, is_any_str, is_any_sym, is_any_bvec;
    const char *type_name;
  } CellFeatures;
  
  static const CellFeatures features[Ntypes];

  void init_static_data();
  
  Bool_t is_atom(void)			{ return _type_ <= T_NIL; }				//!<Return true if the cell is an atom.
  Bool_t is_pair(void)			{ return _type_ == T_PAIR; }				//!<Return true if the cell is a cons pair.

  //Note sometimes faster to check the features, other times faster to check the type directly.  For 3 checks the feature table wins, but for 2 it is not always obvious.
  Bool_t is_any_pair(void)		{ return _type_ > T_NIL; }						//!<Return true if the cell is any pair type.
  Bool_t is_any_svec_atom()		{ return features[_type_].is_any_svec; }				//!<Return true if the cell is any kind of Sexpr_t vector.
  Bool_t is_any_svec2n_atom()		{ return (_type_ == T_SVEC2N_HEAP) || (_type_ == T_SVEC_IMM); }		//!<Return true if the cell is Sexpr_t vector of size 2^n.
  Bool_t is_any_str_atom(void)		{ return features[_type_].is_any_str; }					//!<Return true if the cell is any kind of string.
  Bool_t is_any_sym_atom(void)		{ return (_type_ == T_SYM_HEAP) || (_type_ == T_GENSYM); }		//!<Return true if the cell is any kind of symbol
  Bool_t is_any_bvec_atom(void)		{ return features[_type_].is_any_bvec; }				//!<Return true if the cell is any kind of bytevector.
  
  //!@}

  /*! @name Flag testing & setting for garbage collection and tail recursion.

    The garbage collection algorithm is based on the *tricolor abstraction* described in *Dijkstra  1978*.
    The original set of 3 colors was enlarged to 4 with *Kung and Song 1977*, with their correctness proof of the incremental GC algorithm.
    In LambLisp, is is useful to have a fifth state, and to think of the state or color as a stage in the Cell life cycle.
    When GC is in progress, Cells may advance in their life cycle, but may also be moved back to an earlier as the result of an assignment.
    This enumeration allows some tests to be combined in an inequality rather than a sequence of equality tests or s C `switch`.

    *LambLisp* uses a **trampoline technique** to implement tail recursion.
    The **tail** of a series of expressions is the last one evaluated; this is the expression that will return the value of the series.

    In the *trampoline*, instead of evaluating the last expression in the series and returning the value (as in C/C++),
    the expression is returned unevaluated, with the **tail flag** set.
    The evaluator checks every result to see if it is a *tail* that needs additional evaluation, or is a final result to be returned.

    This removes the need for an additional stack frame during recursion.
   */
  //!@{
  enum { gcst_idle, gcst_issued, gcst_stacked, gcst_marked, gcst_free, Ngcstates };
  
  Int_t   gc_state(void)	{ return _flags_ & GC_STATE_MASK; }							//!<Return the garbage collection stat eof this cell.
  Sexpr_t gc_state(Int_t st)	{ _flags_ =  (_flags_ & ~GC_STATE_MASK) | (st & GC_STATE_MASK);  return this; }		//!<Set the garbage collection state of this cell, and return the cell.
  Int_t   tail_state(void)	{ return _flags_ & F_TAIL; }								//!<Return the taill state of this cell.
  Sexpr_t tail_state_set(void)	{ _flags_ |=  F_TAIL;  return this; }							//!<Set the tail state flag on this cell, and return the cell.
  Sexpr_t tail_state_clr(void)	{ _flags_ &= ~F_TAIL;  return this; }							//!<Clear the tail state flag on this cell, and return the cell.
  //!@}
  
  //! @name Cell value extractors, dependent on Cell type.
  //!@{
  Ptr_t  get_car_addr()		{ return _car_addr_; }			//!<Return the address of the cell car.
  Word_t get_car(void)		{ return _car_; }			//!<Return the value of the cell car.
  Word_t get_cdr(void)		{ return _cdr_; }			//!<Return the value of the cell cdr.

  Bool_t as_Bool_t()		{ return (Bool_t) _car_; }		//!<Return the value of this cell as a boolean.
  Char_t as_Char_t()		{ return (Char_t) _car_; }		//!<Return the value of this cell as a character.
  Int_t  as_Int_t()		{ return *_int_ptr_; }			//!<Return the value of this cell as an integer.
  Real_t as_Real_t()		{ return *_real_ptr_; }			//!<Return the value of this cell as a real number.

  Ptr_t     as_Ptr_t()		{ return (Ptr_t)     _cdr_; }		//!<Return the value of this cell as a generic pointer (i.e., void*).
  Charst_t  as_Charst_t()	{ return (Charst_t)  as_Ptr_t(); }	//!<Return the value of this cell as a pointer to a zero-terminated character array.
  Bytest_t  as_Bytest_t()	{ return (Bytest_t)  as_Ptr_t(); }	//!<Return the value of this cell as a pointer to an array of bytes.
  CharVec_t as_CharVec_t()	{ return (CharVec_t) as_Ptr_t(); }	//!<Return the value of this cell as a pointer to a zero-terminated character array.
  ByteVec_t as_ByteVec_t()	{ return (ByteVec_t) as_Ptr_t(); }	//!<Return the value of this cell as a pointer to an array of bytes.
  Portst_t  as_Portst_t()	{ return (Portst_t)  as_Ptr_t(); }	//!<Return the value of this cell as a pointer to an instance of the system underlying "port" implementation.
  
  Int_t as_numerator()		{ return (Int_t) _car_; }		//!<Return the value of this cell as the numerator of a rational number.
  Int_t as_denominator()	{ return (Int_t) _cdr_; }		//!<Return the value of this cell as the denominator of a rational number.

  //!@}

  /*! @name Cell hash value

    Hashing is used extensively throughout *LambLisp*.  Each Cell has a hash value, calculated as follows:
    - If the Cell is a symbol, the Cell's hash value is the hash value of the symbol.
    - Otherwise, the address of the Cell is hashed, and that is the result.
    
  */
  //!@{
  Int_t hash_sexpr(void);	//!<For symbols, the hash of its characters; for numbers,  the hash of the number; otherwise the hash of the S-expression itself (i.e., the address of a cell).
  Int_t hash_contents(void);	//!<For symbols, the hash of its characters; for numbers, strings, vectors, and bytevectors, the hash of the contents; otherwise the hash of the S-expression itself.
  Int_t hash(void)	{ return (type() == T_SYM_HEAP) ? prechecked_sym_heap_get_hash() : hash_sexpr(); }	//!<Return the hash value of this cell.
  //!@}
  
  //! @name Cell setters converting the C types into S-expressions.
  //!@{
  Sexpr_t set(Int_t typ, Word_t w1, Word_t w2)	{ _type_ = typ;  _car_ = w1;  _cdr_ = w2;  return this; }			//!<This is the lowest-level generic "set" function.
  Sexpr_t set(Int_t typ, Int_t a,   Sexpr_t b)	{ _type_ = typ;  _car_ = (Word_t) a;  _cdr_ = (Word_t) b;  return this; }	//!<This is a convenience function for common cases.
  Sexpr_t set(Int_t typ, Sexpr_t a, Sexpr_t b)	{ _type_ = typ;  _car_ = (Word_t) a;  _cdr_ = (Word_t) b;  return this; }	//!<This is a convenience function for common cases.
  
  Sexpr_t set(Bool_t b)				{ return set(T_BOOL, (Word_t) b, (Word_t) 0); }		//!<Set cell as boolean
  Sexpr_t set(Char_t c)				{ return set(T_CHAR, (Word_t) c, (Word_t) 0); }		//!<Set cell as character
  Sexpr_t set(Int_t i)				{ _type_ = T_INT;  _car_ = _cdr_ = 0;  *_int_ptr_  = i;  return this; }	//!<Set cell as integer
  Sexpr_t set(Real_t r)				{ _type_ = T_REAL; _car_ = _cdr_ = 0;  *_real_ptr_ = r;  return this; }	//!<Set cell as real
  Sexpr_t set(Port_t &p)			{ return set(T_PORT_HEAP, (Word_t) 0, (Word_t) &p); }	//!<Set cell as port
  //Sexpr_t set(Sexpr_t a, Sexpr_t b)		{ return set(T_PAIR, a, b); }				//!<Set cell as pair
  
  Sexpr_t set(Int_t typ, Int_t a, Charst_t b)	{ return set(typ, (Word_t) a, (Word_t) b); }	//!<Set cell as a immutable string
  Sexpr_t set(Int_t typ, Int_t a, Bytest_t b)	{ return set(typ, (Word_t) a, (Word_t) b); }	//!<Set cell as a immutable bytevector
  Sexpr_t set(Int_t typ, Int_t a, CharVec_t b)	{ return set(typ, (Word_t) a, (Word_t) b); }	//!<Set cell as a mutable string 
  Sexpr_t set(Int_t typ, Int_t a, ByteVec_t b)	{ return set(typ, (Word_t) a, (Word_t) b); }	//!<Set cell as a mutable bytevector
  //!@}
  
  Sexpr_t mk_error(const char *fmt, ...) CHECKPRINTF_pos2;			//!<Fills and returns the single Cell-level T_ERROR object.
  Sexpr_t mk_error(Sexpr_t irritants, const char *fmt, ...) CHECKPRINTF_pos3;	//!<Fills and returns the single Cell-level T_ERROR object.
  
  /*! @name Accessors for use when the type is known.
    
    If the cell type is already known, then these accessors can be used to efficiently access the cell contents.
  */
  //!@{
  Sexpr_t   prechecked_anypair_get_car()		{ return (Sexpr_t) get_car(); }
  Sexpr_t   prechecked_anypair_get_cdr()		{ return (Sexpr_t) get_cdr(); }
  
  Int_t     prechecked_sym_heap_get_hash()				{ return as_Int_t(); }
  Charst_t  prechecked_sym_heap_get_chars()				{ return (Charst_t) as_Ptr_t(); }
  void      prechecked_sym_heap_get_info(Int_t &hsh, Charst_t &chars)	{ hsh = as_Int_t();  chars = (Charst_t) as_Ptr_t(); }

  CharVec_t prechecked_str_heap_get_chars()				{ return as_CharVec_t(); }		//!<Return a pointer to the character array in the heap.
  CharVec_t prechecked_str_ext_get_chars()				{ return as_CharVec_t(); }		//!<Return a pointer to the character array located outside the heap.
  CharVec_t prechecked_str_imm_get_chars()				{ return (CharVec_t) _byte2_ptr_; }	//!<Return a pointer to the character array embedded in this cell.

  void      prechecked_bvec_imm_set_length(Int_t l)			{ _contents._byte[2] = l; }
  
  Charst_t  prechecked_gensym_get_chars()				{ static String s = "";  s = toString("G%s", ascii.hex(_cdr_));  return s.c_str(); }
  void      prechecked_gensym_get_info(Int_t &hsh, Charst_t &chars)	{ hsh = as_Int_t();  chars = prechecked_gensym_get_chars(); }
  
  Sexpr_t   prechecked_error_get_irritants()				{ return (Sexpr_t) get_cdr(); }
  Sexpr_t   prechecked_error_get_str()					{ return (Sexpr_t) get_car(); }
  Charst_t  prechecked_error_get_chars()				{ return prechecked_error_get_str()->any_str_get_chars(); }
  //! @}

  /*! @name Accessors for use when the cell type is unverified.
    The **any** and **mustbe** accessors will perform type checking and throw an error if an improper access is attempted.
    The **coerce** operators will perform C coercion on its operand if possible, otherwise throw an error.
  */
#define THROW_BAD_TYPE { throw mk_error("%s Bad type %s", me, dump().c_str()); }
  //!@{
  Bool_t  mustbe_Bool_t()	{ ME("Cell::mustbe_Bool_t()");    if (type() == T_BOOL)  return as_Bool_t(); THROW_BAD_TYPE; }		//!<Return the value of this cell as a boolean.
  Char_t  mustbe_Char_t()	{ ME("Cell::mustbe_Char_t()");    if (type() == T_CHAR)  return as_Char_t(); THROW_BAD_TYPE; }		//!<Return the value of this cell as a character.
  Int_t   mustbe_Int_t()	{ ME("Cell::mustbe_Int_t()");     if (type() == T_INT)   return as_Int_t();  THROW_BAD_TYPE; }		//!<Return the value of this cell as an integer.
  Real_t  mustbe_Real_t()	{ ME("Cell::mustbe_Real_t()");    if (type() == T_REAL)  return as_Real_t(); THROW_BAD_TYPE; }		//!<Return the value of this cell as a real number.

  Sexpr_t mustbe_any_str_t()	{ ME("Cell::mustbe_any_str_t()"); if (is_any_str_atom()) return this;        THROW_BAD_TYPE; }		//!<Return this cell if it is a string.
  Sexpr_t mustbe_cppobj_t()	{ ME("Cell::mustbe_cppobj_t()");  if (type() == T_CPP_HEAP) return this;     THROW_BAD_TYPE; }		//!<Return this cell if it is a CPP object.

  CPPDeleterPtr prechecked_cppobj_get_deleter()		{ return (CPPDeleterPtr) _car_; }	//!<Return the function to be called at garbage collection time to recycle the C++ object.
  Ptr_t prechecked_cppobj_get_ptr()			{ return as_Ptr_t(); }			//!<Return a pointer to a C++ object obtained earlier.

  CPPDeleterPtr any_cppobj_get_deleter()		{ return mustbe_cppobj_t()->prechecked_cppobj_get_deleter(); }
  Ptr_t any_cppobj_get_ptr()				{ return mustbe_cppobj_t()->prechecked_cppobj_get_ptr(); }
  void any_cppobj_get_info(CPPDeleterPtr &d, Ptr_t &p)	{ Sexpr_t o = mustbe_cppobj_t();  d = o->prechecked_cppobj_get_deleter();  p = o->prechecked_cppobj_get_ptr(); }

  Real_t coerce_Real_t()	//!<Coerce the value of numeric cell into a real number.
  {
    ME("Cell::coerce_Real_t()");
    Int_t typ = type();
    if (typ == T_REAL) return as_Real_t();
    else if (typ == T_INT) return (Real_t) as_Int_t();
    else if (typ == T_CHAR) return (Real_t) as_Char_t();
    THROW_BAD_TYPE;
  }

  Real_t coerce_Int_t()		//!<Coerce the value of numeric cell into an integer.
  {
    ME("Cell::coerce_Int_t()");
    Int_t typ = type();
    if (typ == T_INT) return as_Int_t();
    else if (typ == T_REAL) return (Int_t) as_Real_t();
    else if (typ == T_CHAR) return (Int_t) as_Char_t();
    THROW_BAD_TYPE;
  }

  Sexpr_t   anypair_get_car()		{ ME("Cell::anypair_get_car()");      if (is_any_pair()) return prechecked_anypair_get_car(); THROW_BAD_TYPE; }
  Sexpr_t   anypair_get_cdr()		{ ME("Cell::anypair_get_cdr()");      if (is_any_pair()) return prechecked_anypair_get_cdr(); THROW_BAD_TYPE; }

  Sexpr_t   error_get_str()		{ ME("Cell::error_get_str()");        if (_type_ == T_ERROR) return prechecked_error_get_str(); THROW_BAD_TYPE; }
  Sexpr_t   error_get_irritants()	{ ME("Cell::error_get_irritants()");  if (_type_ == T_ERROR) return prechecked_error_get_irritants(); THROW_BAD_TYPE; }
  Charst_t  error_get_chars()		{ ME("Cell::error_get_chars()");      if (_type_ == T_ERROR) return prechecked_error_get_chars(); THROW_BAD_TYPE; }

  Int_t any_sym_get_hash()		{ ME("any_sym_get_hash()");	      if (is_any_sym_atom()) return as_Int_t(); THROW_BAD_TYPE; }
  
  Charst_t any_sym_get_chars() {
    ME("Cell::any_sym_get_chars()");
    if (_type_ == T_SYM_HEAP) return prechecked_sym_heap_get_chars();
    else if (_type_ == T_GENSYM) return prechecked_gensym_get_chars();
    else THROW_BAD_TYPE;
  }
  
  void any_sym_get_info(Int_t &hsh, Charst_t &chars) {
    ME("Cell::any_sym_get_info()");
    if (_type_ == T_SYM_HEAP) prechecked_sym_heap_get_info(hsh, chars);
    else if (_type_ == T_GENSYM) prechecked_gensym_get_info(hsh, chars);
    else THROW_BAD_TYPE;
  }
  
  //!@}

  /*! @name Operations on strings

    LambLisp supports several subtypes of **strings**.  At the time of writing, there are heap-allocated strings (read-write),
    strings whose characters outside of LambLisp's managed memory, (aka external or EXT strings),
    and short **immediate** strings that are contained completely within a single cell.
    Additional subtypes (such as load-on-demand strings) may be added in future.
  
    There are functions of the form any_xxx() that can be used with any subtype, and there are type-specific functions of the form
    any_xxx_yyy() for use where the type is already known, with yyy being a code hint.
  */
  //!@{
  
  //!If the cell is any kind of string, return a pointer to the zero-terminated character array.
  CharVec_t any_str_get_chars() {
    ME("Cell::anystring_get_chars()");
    if (_type_ == T_STR_IMM) return prechecked_str_imm_get_chars();
    else if (_type_ == T_STR_HEAP) return prechecked_str_heap_get_chars();
    else if (_type_ == T_STR_EXT) return prechecked_str_ext_get_chars();
    else THROW_BAD_TYPE;
  }

  Int_t any_str_get_length()				{ return strlen(any_str_get_chars()); }
  void any_str_get_info(Int_t &len, CharVec_t &chars)	{ len = strlen(chars = any_str_get_chars()); }
  //!@}

  /*! \name Operations on vectors and sub-types of vectors

    Within the LambLisp virtual machine, a *Lisp vector* is referred to as *svec*.  This is an array of S-expressions having a fixed dimension.
    There are also *bytevectors*; these are an array of bytes, also of fixed dimension.
    
    As with *strings*, there are several subtypes of S-expression vectors.
    There is a heap-allocated vector, which may be of any size.
    There is a second type of heap-allocated vector, that is always sized to be a power of 2.  These are provided to support efficient hash tables.
    There are immediate vectors, which may be of 0, 1, or 2 elements.
    The 2-element immediate vector can also be used as a hash table.
    This can reduce search time by half without requiring any heap allocation, at the cost of 1 extra cell allocation.

    Bytevectors are also diverse, having heap, external, and immediate variants.
    Heap bytevectors are allocated, obviously, on the system heap, and the heap space is freed when the bytevector is garbage-collected.

    External bytevectors operate on bytes provided externally to LambLisp's memory manager.
    This space may heve been dynamically allocated from outside LambLisp, or may be located in read-only memory.
    When a C++ object os injected into *LambLisp*, it can optionally be provided with a garbage collector callback;
    in that case the external object can be automatically garbage collected when no it's longer used in the *Lisp* program.

    Immediate bytevectors are contained within a LambLisp Cell.  The maximum size is limited by the word size of the underlying platform.
    
    Within the *LambLisp* virtual machine, there are generic functions of the form any_xvec_xxx() that can operate on any xvec (svec or bvec) subtype,
    as well as type-specific functions of the form xvec_yyy_xxx(), where yyy is a code hint for the cell storage type (heap, immediate, ROM).
  */
  
  //!@{
  void any_svec_get_info(Int_t &Nelems, Sexpr_t *&elems) {
    ME("Cell::any_svec_get_info()");
    Int_t typ = _type_;
    if (typ <= T_SVEC2N_HEAP) {
      Nelems = as_Int_t();
      elems  = (Sexpr_t *) as_Ptr_t();
    }
    else if (typ == T_SVEC_IMM) {	//could be 0, 1 or 2 elems
      Nelems = 2;	//assume
      if (prechecked_anypair_get_cdr() == OBJ_VOID) Nelems--;
      if (prechecked_anypair_get_car() == OBJ_VOID) Nelems--;
      elems  = (Sexpr_t *) _car_addr_;
    }
    else THROW_BAD_TYPE;
  }

  Sexpr_t *any_svec_get_elems()
  {
    ME("Cell::any_svec_get_elems()");
    Sexpr_t *res = 0;
    Int_t typ    = _type_;

    if (typ <= T_SVEC2N_HEAP) res = (Sexpr_t *) as_Ptr_t();
    else if (typ == T_SVEC_IMM) res  = (Sexpr_t *) _car_addr_;
    else THROW_BAD_TYPE;

    return res;
  }
  
  Int_t any_bvec_get_length()
  {
    ME("Cell::any_bvec_get_length()");
    Int_t Nelems = -1;
    if (_type_ == T_BVEC_IMM) {
      ByteVec_t b = (ByteVec_t) _byte2_ptr_;
      Nelems = b[0];
    }
    else if ((_type_ == T_BVEC_HEAP) || (_type_ == T_BVEC_EXT)) Nelems = as_Int_t();
    else THROW_BAD_TYPE;

    return Nelems;
  }
  
  ByteVec_t any_bvec_get_elems()
  {
    ME("Cell::any_bvec_get_elems()");
    ByteVec_t res = 0;
    if (_type_ == T_BVEC_IMM) {
      ByteVec_t b = (ByteVec_t) _byte2_ptr_;
      res = (ByteVec_t) &(b[1]);
    }
    else if ((_type_ == T_BVEC_HEAP) || (_type_ == T_BVEC_EXT)) res  = as_ByteVec_t();
    else THROW_BAD_TYPE;
    return res;
  }
  
  void any_bvec_get_info(Int_t &Nelems, ByteVec_t &elems)
  {
    ME("Cell::any_bvec_get_info()");
    if (_type_ == T_BVEC_IMM) {
      ByteVec_t b = (ByteVec_t) _byte2_ptr_;
      Nelems      = b[0];
      elems       = (ByteVec_t) &(b[1]);
    }
    else if ((_type_ == T_BVEC_HEAP) || (_type_ == T_BVEC_EXT)) {
      Nelems = as_Int_t();
      elems  = as_ByteVec_t();
    }
    else THROW_BAD_TYPE;
  }
  //!@}
    
#undef THROW_BAD_TYPE
#undef _type_
#undef _flags_
#undef _car_
#undef _cdr_
#undef _car_addr_
#undef _int_ptr_
#undef _real_ptr_
#undef _byte2_ptr_
    
  /*! @name Cell conversions to printable representation
    
    These functions convert a Cell (or parts of a Cell) to a printable representation of the S-expression contents of the Cell.
    Because environments are often included in the descendants of the Cell being printed, the depth of environment recursiveness is limited.
  */
  //!@{
  String   cell_name(void);		//!<A convenience feature to produce a string name for cells which are "well known" like NIL.  Otherwise the name is the hex representation of the cell address.

  Charst_t type_name(Int_t typ);	//!<Return a pointer to the C string corresponding to the cell type.
  Charst_t type_name(void) { return type_name(this->type()); }
  Charst_t gcstate_name(void);		//!<Return a pointer to a C string corresponding to the given GC state.

  String dump();			//!<Return a printable representation of the Cell internals.

  String str(Sexpr_t sx, Bool_t as_write_or_display, Int_t env_depth, Int_t max_depth);
  String str(Bool_t as_write_or_display, Int_t env_depth, Int_t max_depth)	{ return str(this, as_write_or_display, env_depth, max_depth); }
  String str(Bool_t as_write_or_display, Int_t env_depth)			{ return str(this, as_write_or_display, env_depth, 10); }
  String str(Bool_t as_write_or_display)					{ return str(this, as_write_or_display, 1, 10); }
  String str(Int_t env_depth) 							{ return str(this, true, env_depth, 10); }
  String str(void)								{ return str(this, true, 1, 10); }
  //!@}
  
private:
  union {
    Byte_t _byte[3 * sizeof(Word_t)];
    Word_t _word[3];
  } _contents;

};

static_assert(sizeof(Cell) == 3*sizeof(Word_t), "Cell size != 3*Word_t size\n");
static_assert(sizeof(Cell) == 3*sizeof(Ptr_t),  "Cell size != 3*Ptr_t size\n");

//! @name The LambLisp VM can accept S-expression generated externally, evaluate them, and produce the result as an S-expression.
//!@{
extern Sexpr_t LAMB_INPUT;	//!<If the variable is non-NIL, LambLisp will evaluate it and put the results in LAMB_OUTPUT;
extern Sexpr_t LAMB_OUTPUT;	//!<The result of evaluating LAMB_INPUT is placed here.
//!@}

class LambMemoryManager;

/*! \class Lamb
  
  The Lamb class represents a single Lisp virtual machine.
  To closely match the Arduino-style of loop-based control, LambLisp provides begin(), loop(), and end().

  To initialize the LambLisp VM, begin() should be called once before any other call.  Generally LambLisp loop() will be called one time for every main loop(), but it is not necessary.
  If end() is called, all LambLisp resources are freed; Lamb::setup() can be called again to restart that LambLisp VM.

  The LambLisp VM functions are grouped this way:
  - Logging functions
  - Build and version informational functions.
  - Cell constructors, getters & setters
  - Base data structures for the interpreter: alist, hash tables, and environments, along with their getters & setters.
  - Printers
  - Partial evaluation and application
  - Port access from *Lisp*
  - Vector, list, and dictionary utilities, used internally and also available in *Lisp*.
  - GC interface functions to protect critical sections.
  - Bindings
  - Makers
*/
class Lamb {
public:

  Lamb();
  
  /*!This is a pointer to a C++ native function that interacts directly with the S-expression in the given environment.
    Every LambLisp native function shares this signature.
    New external functions that conform to this signature can be used from within LambLisp and will run at full speed.
   */
  typedef Sexpr_t (*Mop3st_t)(Lamb &lamb, Sexpr_t sexpr, Sexpr_t env_exec);

  //! @name Arduino-compatible loop-based control interface.
  //!@{
  Sexpr_t setup(void);	//!<Run once after base platform has started.  In particular, *Serial* should be initialized before calling setup().
  Sexpr_t loop(void);	//!<Run often to maintain control.  The main responsibility of C++ Lamb::loop() is to call the LambLisp function of the same name (loop).
  void end(void);	//!<Release resources used by the Lamb virtual machine.
  //!@}

  //! @name A few foundational functions for embedded debugging.
  //!@{
  void log(const char *fmt, ...) CHECKPRINTF_pos2;	//!<C-level printf feature with a limit on the length of strings produced.  Takes care of log prompt.
  void printf(const char *fmt, ...) CHECKPRINTF_pos2;	//!<C-level printf feature with a limit on the length of strings produced.
  bool debug(void);					//!<Return the state of the Lamb internal debug flag.
  void debug(bool onoff);				//!<Set the state of the Lamb debug flag.
  //!@}

  //! @name Information about the current build.
  //!@{
  bool build_isDebug();			//!<Return true if this build is not checked in.  This is unrelated to the runtime `debug` flag.
  unsigned long build_version();	//!<Return the build version as a long integer (implemented as a UTC time stamp).
  unsigned long build_UTC();		//!<Return the UTC time of this build.
  unsigned long build_pushUTC();	//!<Return the UTC time this repo was last pushed.
  const char *build_buildRelease();	//!<Return a pointer to a character array containing the release description.
  const char *build_buildDate();	//!<Return a pointer to a character array containing the build date.
  const char *build_pushDate();		//!<Return a pointer to a character array containing the date this repo was last pushed.
  //@}

  /*! \name Cell constructors
    
    Garbage collection may happen at any time during the execution of the Cell constructors.
    The cell constructors that accept S-expression arguments will protect those arguments from GC during the operation, using the gc root push/pop functions.
  */
  //!@{
  void expand();							//!<Add another block of cells to the population.
  Sexpr_t tcons(Int_t typ, Word_t a,  Word_t  b, Sexpr_t env_exec);	//!<Generic cell constructor with no GC protection for its arguments.
  Sexpr_t tcons(Int_t typ, Sexpr_t a, Sexpr_t b, Sexpr_t env_exec);	//!<Constructor for any pair type; protects both S-expression arguments.

  Sexpr_t cons(Sexpr_t a,  Sexpr_t b, Sexpr_t env_exec)	{ return tcons(Cell::T_PAIR, a, b, env_exec); }	//!<Cell constructor for normal pairs.

  Sexpr_t gensym(Sexpr_t env_exec);	//!<Produce a new unique symbol.
  
  void  gc_root_push(Sexpr_t p);	//!<Preserve the cell given from GC, until popped.
  void  gc_root_pop(Int_t n=1);		//!<Release the preserved cells for normal GC processing.
  //!@}
  
  //! @name A subset of the car/cdr accessors for list cells.  This is the subset used within LambLisp.
  //!@{
  Sexpr_t car(Sexpr_t l);
  Sexpr_t cdr(Sexpr_t l);
  Sexpr_t caar(Sexpr_t l)	{ return car(car(l)); }
  Sexpr_t cadr(Sexpr_t l)	{ return car(cdr(l)); }
  Sexpr_t cdar(Sexpr_t l)	{ return cdr(car(l)); }
  Sexpr_t cddr(Sexpr_t l)	{ return cdr(cdr(l)); }
  Sexpr_t caddr(Sexpr_t l)	{ return car(cddr(l)); }
  Sexpr_t cdddr(Sexpr_t l)	{ return cdr(cddr(l)); }
  Sexpr_t cadddr(Sexpr_t l)	{ return car(cdddr(l)); }
  Sexpr_t cddddr(Sexpr_t l)	{ return cdr(cdddr(l)); }
  //!@}
  
  //! @name The "bang" functions are the "mutators" of GC literature.
  //!@{
  void set_car_bang(Sexpr_t c, Sexpr_t val);			//!<Replace the car field in the cell with *val*.  GC flags will be maintained as required.
  void set_cdr_bang(Sexpr_t c, Sexpr_t val);			//!<Replace the cdr field in the cell with *val*.  GC flags will be maintained as required.
  void vector_set_bang(Sexpr_t vec, Int_t k, Sexpr_t val);	//!<Replace the specified vector element with *val*.  GC flags will be maintained as required.
  Sexpr_t reverse_bang(Sexpr_t l);				//!<Reverse the list in-place and return the new list head (the former list tail).
  //!@}
  
  //! @name Equivalence tests and sequential search
  //!@{
  Sexpr_t eq_q(Sexpr_t obj1, Sexpr_t obj2);	//!<Return true if 2 cells are the same cell, or are atoms with the same value.
  Sexpr_t eqv_q(Sexpr_t obj1, Sexpr_t obj2);	//!<Return true if 1 cells are the same cell, or are atoms with the same value.
  Sexpr_t equal_q(Sexpr_t obj1, Sexpr_t obj2);	//!<Returns true when obj1 and obj2 are eqv?, and also all their descendants.
  Sexpr_t assq(Sexpr_t obj, Sexpr_t alist);	//!<Search an association list for a matching key, and return the `(key . value)` pair, or **false** if not found.
  //!@}

  /*!
    \name Interned symbols

    There are just a few operations on symbols:
    - Remember a new symbol.
    - Check if symbol has already been seen.
    - Compare 2 symbols for equality.
    - At evaluation time, lookup the symbol in the *current environment*.

    For best performance, symbols in LambLisp are *interned*.
    This means a single copy of the symbol's characters are stored in a data structure containing the set of all *interned symbols*.
    Once interned, symbols can be tested for equality by address rather than character-by-character.
    
    Traditionally, the data structure was called *oblist* if an association list was used, or called *obarray* if an array was used.
    Arrays allow for a hash table implementation, which greatly reduces search time, and can be further optimized if the array size is a power of 2 (2^n).

    LambLisp uses the term *oblist* for the interned symbol table, implemented using a 2^n hash table.
    Symbol hashes are computed only once and are stored with the symbol, speeding runtime lookups.
  */
  
  //!@{
  Sexpr_t oblist_query(Sexpr_t oblist, const char *identifier, Bool_t force, Sexpr_t env_exec);
  Sexpr_t oblist_test(Sexpr_t oblist, const char *identifier, Sexpr_t env_exec)		{ return oblist_query(oblist, identifier, false, env_exec); }
  Sexpr_t oblist_intern(Sexpr_t oblist, const char *identifier, Sexpr_t env_exec)	{ return oblist_query(oblist, identifier, true, env_exec); }
  Sexpr_t oblist_analyze(Sexpr_t oblist, Int_t verbosity, Sexpr_t env_exec);
  //!@}

  /*! \name Hierarchical Dictionary type
    
    LambLisp's high-performance hierarchical dictionary implementation is used internally to represent the runtime environment.
    Dictionaries are also directly usable in the Lisp applications, and the dictionary data type is the basis for the LambLisp Object System (LOBS).
  */
  //!@{
  Sexpr_t dict_new(Int_t framesize, Sexpr_t env_exec)				{ return dict_add_empty_frame(NIL, framesize, env_exec); }	//!<Return a new empty dictionary with the given top frame size.
  Sexpr_t dict_new(Sexpr_t env_exec)						{ return dict_add_empty_frame(NIL, 0, env_exec); }		//!<Return a new empty dictionary with alist top frame.

  Sexpr_t dict_add_empty_frame(Sexpr_t dict, Int_t framesize, Sexpr_t env_exec);	
  Sexpr_t dict_add_empty_frame(Sexpr_t dict, Sexpr_t env_exec)			{ return dict_add_empty_frame(dict, 0, env_exec); }	//!<Returns a new dictionary with an empty top frame, and an existing dictionary as parent.
  
  Sexpr_t dict_add_keyval_frame(Sexpr_t dict, Sexpr_t keys, Sexpr_t vals, Sexpr_t env_exec);						//!<Returns a new dictionary with a new top frame containing the keys bound to the values.
  Sexpr_t dict_add_alist_frame(Sexpr_t dict, Sexpr_t alist, Sexpr_t env_exec)	{ return mk_dict(alist, dict, env_exec); }		//!<Returns a new dictionary with the alist bindings added in a new frame on top of the base dictionary.
  
  void    dict_bind_bang(Sexpr_t dict, Sexpr_t key, Sexpr_t value, Sexpr_t env_exec);	//!<Modify the target dictionary; `value` is re-assigned to `key`if found, else created in the top frame.
  void    dict_rebind_bang(Sexpr_t dict, Sexpr_t key, Sexpr_t value, Sexpr_t env_exec);	//!<Modify the target dictionary; `value` is assigned to `key` wherever first found in env, else error if not found.
  void    dict_bind_alist_bang(Sexpr_t dict, Sexpr_t alist, Sexpr_t env_exec);		//!<Modify the target dictionary; binding keys in the alist to their corresponding values.
  void    dict_rebind_alist_bang(Sexpr_t dict, Sexpr_t alist, Sexpr_t env_exec);	//!<Modify the target dictionary; rebinding keys in the alist to their corresponding values.

  Sexpr_t dict_ref_q(Sexpr_t dict, Sexpr_t key);					//!<Returns (key value) pair, or **false** if key is unbound.
  Sexpr_t dict_ref(Sexpr_t dict, Sexpr_t key);						//!<Returns the value associated with the key in the given dictionary; throws error if key is unbound.

  //!Note that keys and values are guaranteed to return in the corresponding order in dict_keys and dict_values, duplicate keys may occur.
  Sexpr_t dict_keys(Sexpr_t dict, Sexpr_t env_exec);				//!<Return a list of all the keys in this dictionary.  If the dictionary has parents, keys may appear multiple times.
  Sexpr_t dict_values(Sexpr_t dict, Sexpr_t env_exec);				//!<Return a list of all the values in this dictionary.  If the dictionary has parents, values may appear multiple times for each key.
  
  Sexpr_t dict_to_alist(Sexpr_t dict, Sexpr_t env_exec);	//!<Convert the dictionary to an alist, retaining only the top-level `(key . value)` pairs.
  Sexpr_t dict_to_2list(Sexpr_t dict, Sexpr_t env_exec);	//!<Convert the dictionary to a list of 2-element lists, retaining only the top-level `(key value)` sublists.
  Sexpr_t alist_to_dict(Sexpr_t alist, Sexpr_t env_exec);	//!<Convert an alist into a dictionary.  The resulting dictionary has no parent.
  Sexpr_t twolist_to_dict(Sexpr_t twolist, Sexpr_t env_exec);	//!<Convert a list of 2-element lists into a dictionary.  The resulting dictionary has no parent.

  Sexpr_t dict_analyze(Sexpr_t dict, Int_t verbosity = 0);	//!<Internal integrity check.
  //!@}

  //! \name Reading and writing
  //!@{
  Sexpr_t read(LL_Port &src, Sexpr_t env_exec)	{ return read_sexpr(src, env_exec, false); }

  Sexpr_t write_or_display(Sexpr_t sexpr, Bool_t do_write);
  Sexpr_t write_simple(Sexpr_t sexpr);

  String  sprintf(Charst_t fmt, Sexpr_t sexpr, Sexpr_t env_exec);

  Sexpr_t printf(Sexpr_t args, LL_Port &outp);
  Sexpr_t printf(Sexpr_t args);
  //!@}
  
  //! \name Evaluation and function application
  //!@{
  Sexpr_t quasiquote(Sexpr_t sexpr, Sexpr_t env_exec);				//!<Partial quotation per RxRS.
  Sexpr_t eval(Sexpr_t sexpr, Sexpr_t env_exec);				//!<Evaluate the S-expression in the environment provided.
  Sexpr_t eval_list(Sexpr_t args, Sexpr_t env_exec);				//!<Evaluate the list of S-expressions in the environment provided, and return a lkist of results.
  Sexpr_t apply_proc_partial(Sexpr_t proc, Sexpr_t sexpr, Sexpr_t env_exec);	//!<Evaluate the function arguments and then apply the function to them.  The result may be a tail requiring further evaluation.
  Sexpr_t map_proc(Sexpr_t proc, Sexpr_t lists, Sexpr_t env_exec);		//!<Apply the procecure to the lists per R5RS.
  //!@}

  //! \name Querying underlying system features defined by Scheme.
  //!@{
  Sexpr_t r5_base_environment()		{ return _r5_base_environment; }
  Sexpr_t r5_interaction_environment()	{ return _r5_interaction_environment; }
  Sexpr_t lamb_oblist()			{ return _lamb_oblist; }
  
  Sexpr_t current_input_port()		{ return _r5_current_input_port; }
  Sexpr_t current_output_port()		{ return _r5_current_output_port; }
  Sexpr_t current_error_port()		{ return _r5_current_error_port; }
  //!@}
  
  Sexpr_t load(Charst_t name, Sexpr_t env_exec, Int_t verbosity = 0);		//!<Load and evaluate S-expressions from the named file.
  
  //! \name Useful list processing used internally by the LambLisp virtual machine.
  //!@{
  Sexpr_t append(Sexpr_t sexpr, Sexpr_t env_exec);
  Sexpr_t append2(Sexpr_t lis, Sexpr_t obj, Sexpr_t env_exec);	//!<Destructively attach *obj* to the end of *lis*, which must be a proper list.
  Sexpr_t list_copy(Sexpr_t sexpr, Sexpr_t env_exec);
  Sexpr_t list_analyze(Sexpr_t sexpr, Sexpr_t env_exec);	//!<Traverse the list  and return either false (if circular), the list length (if a proper list), ###
  Sexpr_t list_to_vector(Sexpr_t l, Sexpr_t env_exec);
  Sexpr_t vector_to_list(Sexpr_t v, Sexpr_t env_exec);
  Sexpr_t vector_copy(Sexpr_t from, Sexpr_t env_exec);
  //!@}

  /*! \name Sparse vectors
    The sparse vector representation is a sorted association list, in which the car of each pair is the vector index and the cdr is the vector value at that index.
  */
  //!@{
  Sexpr_t vector_to_sparsevec(Sexpr_t vec, Sexpr_t skip, Sexpr_t env_exec);
  Sexpr_t sparsevec_to_vector(Sexpr_t alist, Sexpr_t fill, Sexpr_t env_exec);
  //!@}

  /*! \name Makers for error types
    
    These functions will sprintf a message into a buffer, and return an error object containing the message.
    They differ in that one (mk_error) creates a new error object, and the other (mk_syserror) avoids use of the memory manager by reusing the system error singleton.
  */
  //!@{
  Sexpr_t mk_error(Sexpr_t env_exec, Sexpr_t irritants, const char *fmt, ...) CHECKPRINTF_pos4;	//!<Fill a new error object with the information given, and return it.
  Sexpr_t mk_error(Sexpr_t env_exec, const char *fmt, ...) CHECKPRINTF_pos3;			//!<Fill a new error object with the information given, and return it.  The *irritants* field is left NIL.
  Sexpr_t mk_syserror(const char *fmt, ...) CHECKPRINTF_pos2;					//!<Fill the system error object with the info given, and return it.
  Sexpr_t mk_syserror(Sexpr_t irritants, const char *fmt, ...) CHECKPRINTF_pos3;		//!<Fill the system error object with the info given, and return it.
  //!@}
  
  /*! @name Makers for simple compact types with embedded values, without external storage or dependencies

    These types all have their values embedded in the Cell.
    Note that other types may also hold immediate values, but are "less simple" because they are C++ subtypes that are variations on the *Lisp*-declared types.
  */
  //!@{
  Sexpr_t mk_bool(Bool_t b, Sexpr_t env_exec)				{ return tcons(Cell::T_PAIR, NIL, NIL, env_exec)->set(b);  }
  Sexpr_t mk_character(Char_t ch, Sexpr_t env_exec)			{ return tcons(Cell::T_PAIR, NIL, NIL, env_exec)->set(ch); }
  Sexpr_t mk_integer(Int_t i, Sexpr_t env_exec)				{ return tcons(Cell::T_PAIR, NIL, NIL, env_exec)->set(i);  }
  Sexpr_t mk_real(Real_t r, Sexpr_t env_exec)				{ return tcons(Cell::T_PAIR, NIL, NIL, env_exec)->set(r);  }

  Sexpr_t mk_number(Charst_t str, Sexpr_t env_exec);
  Sexpr_t mk_sharp_const(Charst_t name, Sexpr_t env_exec);
  //!@}
  
  /*! \name Makers for heap storage types

    In principle, these types require space from the system heap.  At GC time, they may also require specialized marking, and also specialized sweeping.

    As an optimization, *LambLisp* also implements **immediate** types.
    When the size required for the object is small, the internal bytes of the Cell can be used to hold the contents, avoiding heap operations.
    Using immediate types in this way has been common practice for integers and real numbers.  LambLisp makes some additional use of immediate types.
    Immediate types are a LambLisp internal optimization,  and they are all subtypes of the main *Lisp* type.

    The types that employ the *immediate* optimization are:
    - strings, providing up to 9 content bytes plus trailing 0 (32 bit word) or 22 content bytes (64 bit).
    - bytevectors a count plus up to 9 or 22 content bytes.
    - vectors of length 0, 1 or 2.

    There are also **EXTERNAL** versions of those data types.
    These data types can be used to accesss data that should not be garbage collected,
    such as data contained in the application image, or C++ objects provided by another application.

    A variant of the EXTERNAL types is also provided, so that objects created in C++ can be passed to LambLisp,
    along with an optional *deleter function*.
    In C++, the deleter is best implemented as a one-line C++ lambda function accepting 1 argument (a void* pointer to a C++ object).
    The deleter function casts the void* pointer to match the C++ object type, and applies the C++ *delete* or *delete[]* operators to free the object space and any dependent resources.

    By passing the C++ objects to LambLisp, they can be automatically garbage collected when LambLisp has finished with them.
  */
  //!@{
  Sexpr_t mk_string(Sexpr_t env_exec, const char *fmt, ...) CHECKPRINTF_pos3;
  Sexpr_t mk_string(Int_t k, Charst_t src, Sexpr_t env_exec);
  Sexpr_t mk_string(String s, Sexpr_t env_exec)			{ return mk_string(s.length(), s.c_str(), env_exec); }
  
  Sexpr_t mk_symbol_or_number(Charst_t str, Sexpr_t env_exec);
  Sexpr_t mk_symbol(Charst_t str, Sexpr_t env_exec)		{ return oblist_intern(_lamb_oblist, str, env_exec);  }
  
  Sexpr_t mk_bytevector(Int_t k, Sexpr_t env_exec);			//!<Simplest heap allocation with no initialization.
  Sexpr_t mk_bytevector(Int_t k, Bytest_t src, Sexpr_t env_exec);	//!<Heap allocation with initialization.
  Sexpr_t mk_bytevector(Int_t k, Int_t fill, Sexpr_t env_exec);		//!<Heap allocation with initialization.

  //!Injection of externally allocated memory, which will not be freed at GC time.
  Sexpr_t mk_bytevector_ext(Int_t k, Bytest_t ext, Sexpr_t env_exec)	{ return tcons(Cell::T_BVEC_EXT, (Word_t) k, (Word_t) ext, env_exec); }  

  //!Note that integer vectors and real vectors are just bytevectors underneath.
  Sexpr_t mk_intvector(Int_t k, Sexpr_t env_exec);		//!<Allocates a bytevector from the heap to ensure proper alignment (no IMM type).
  Sexpr_t mk_realvector(Int_t k, Sexpr_t env_exec);		//!<Allocates a bytevector from the heap to ensure proper alignment (no IMM type).
  Sexpr_t mk_intvector(Int_t k, Int_t fill, Sexpr_t env_exec);
  Sexpr_t mk_realvector(Int_t k, Real_t fill, Sexpr_t env_exec);

  Sexpr_t mk_vector(Int_t len, Sexpr_t fill, Sexpr_t env_exec);
  Sexpr_t mk_hashtbl(Int_t len, Sexpr_t fill, Sexpr_t env_exec);

  Sexpr_t mk_serial_port(Sexpr_t env_exec);
  Sexpr_t mk_input_file_port(Charst_t name, Sexpr_t env_exec);
  Sexpr_t mk_output_file_port(Charst_t name, Sexpr_t env_exec);
  Sexpr_t mk_input_string_port(Charst_t inp, Sexpr_t env_exec);
  Sexpr_t mk_output_string_port(Sexpr_t env_exec);
  //!@}

  //! \name Makers for interface to native procedures and C++ objects.
  //!@{
  Sexpr_t mk_Mop3_procst_t(Mop3st_t f, Sexpr_t env_exec)		{ return tcons(Cell::T_MOP3_PROC,  (Word_t) NIL, (Word_t) f, env_exec);  }
  Sexpr_t mk_Mop3_nprocst_t(Mop3st_t f, Sexpr_t env_exec)		{ return tcons(Cell::T_MOP3_NPROC, (Word_t) NIL, (Word_t) f, env_exec); }
  Sexpr_t mk_cppobj(void *obj, CPPDeleterPtr deleter, Sexpr_t env_exec)	{ return tcons(Cell::T_CPP_HEAP,   (Word_t) deleter, (Word_t) obj, env_exec); }
  //!@}

  /*! \name Makers for pair types.

    These **pair** types all have the same structure for construction and garbage collection purposes, but are not *Scheme pair* types.
    *LambLisp*'s scalable type system presents opportunities for efficiency when implementing common operations on lists.
    These are executed by the *LambLisp* virtual machine at C++ speed, rather than at the slower speed of the *Lisp* evaluation loop.
    
    - procedures ... the evaluator must be able to identify these directly.
    - dictionaries ... often containing cycles when used as environments, so a depth limit is imposed when traversing.
    - thunks ... used in combination with the trampoline technique to implement tail recursion.
    
    Note that there is no need for mk_pair(), it is just cons().
  */
  //!@{
  Sexpr_t mk_macro(Sexpr_t nlam, Sexpr_t env_nlam, Sexpr_t env_exec)				{ return tcons(Cell::T_MACRO, nlam, env_nlam, env_exec); }
  Sexpr_t mk_procedure(Sexpr_t formals, Sexpr_t body, Sexpr_t env_proc, Sexpr_t env_exec)	{ return tcons(Cell::T_PROC, cons(formals, body, env_proc), env_proc, env_exec); }
  Sexpr_t mk_nprocedure(Sexpr_t formals, Sexpr_t body, Sexpr_t env_nproc, Sexpr_t env_exec)	{ return tcons(Cell::T_NPROC, cons(formals, body, env_nproc), env_nproc, env_exec); }
  Sexpr_t mk_thunk_sexpr(Sexpr_t sexpr, Sexpr_t env_thunk, Sexpr_t env_exec)	{ return tcons(Cell::T_THUNK_SEXPR, sexpr, env_thunk, env_exec); }
  Sexpr_t mk_thunk_body(Sexpr_t body, Sexpr_t env_thunk, Sexpr_t env_exec)	{ return tcons(Cell::T_THUNK_BODY, body, env_thunk, env_exec); }
  Sexpr_t mk_dict(Sexpr_t frame, Sexpr_t base, Sexpr_t env_exec)		{ return tcons(Cell::T_DICT, frame, base, env_exec); }
  Sexpr_t mk_dict(Int_t framesize, Sexpr_t env_exec);
  //!@}

  /*! \name Macro support

    These macro primitives behave as described in Steele's *Common Lisp*.
  */
  //!@{
  Sexpr_t macroexpand(Sexpr_t form, Sexpr_t env_exec);
  Sexpr_t macroexpand1(Sexpr_t form, Sexpr_t env_exec);

  Sexpr_t macroexpand(Sexpr_t proc, Sexpr_t args, Sexpr_t env_exec);
  Sexpr_t macroexpand1(Sexpr_t proc, Sexpr_t args, Sexpr_t env_exec);
  //!@{

  /* put this comment after the last documented member to start new page in doxygen detailed section.
     \latexonly \newpage \endlatexonly
  */
  
private:

  LambMemoryManager *mem;

  bool _debug_in_progress;
  Int_t _verbosity;
  
  //Symbols and bindings
  static const Int_t _lamb_oblist_size		= 2048;
  static const Int_t _r5_base_frame_size        = 1024;
  static const Int_t _r5_interaction_frame_size = 512;

  Sexpr_t _lamb_oblist;
  Sexpr_t _r5_base_environment;
  Sexpr_t _r5_interaction_environment;

  Sexpr_t _r5_current_input_port;
  Sexpr_t _r5_current_output_port;
  Sexpr_t _r5_current_error_port;
  
  //Reader
  const char *DELIMITERS = "()\";\f\t\v\n\r ";
  
  enum {
    TOK_EOF,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_DOT,
    TOK_ATOM,
    TOK_SQUOTE,
    TOK_DQUOTE,
    TOK_BQUOTE,
    TOK_COMMA,
    TOK_COMMA_AT,
    TOK_SHARP,
    TOK_SHARP_CONST,
    TOK_VECTOR,
    TOK_DCOLON,
    Ntokens
  };

  void report();
  
  Int_t produce_token(LL_Port &src);
  Sexpr_t consume_token(Int_t tok, LL_Port &src, Sexpr_t env_exec, bool quoted);

  //Tokens that the reader recognizes and converts directly to symbols.
  Sexpr_t sym_squote;		//single quote '
  Sexpr_t sym_qquote;		//backquote `
  Sexpr_t sym_unquote;		//comma ,
  Sexpr_t sym_uqsplice;		//comma-at ,@

  //Symbols inserted into the reader output to complete multi-step operations
  //e.g., a quoted vector '#(a b c) becomes (apply vector (quote (a b c)))
  Sexpr_t sym_apply;
  Sexpr_t sym_vector;

  //Magic reader tokens that turn into magic symbols.
  Sexpr_t sym_colon_hook;	//double colon ::
  Sexpr_t sym_sharp_hook;	//sharp # (not constant or vector)

  Sexpr_t sym_ellipsis;		//three dot ellipsis ...
  Sexpr_t sym_fatarrow;		//aka "feed through" =>
  
  //Loop-based control
  Sexpr_t sym_loop;
  
  Sexpr_t vector_eqv_q(Sexpr_t obj1, Sexpr_t obj2);
  Sexpr_t hashtbl_eqv_q(Sexpr_t obj1, Sexpr_t obj2);
  Sexpr_t bytevector_eqv_q(Sexpr_t obj1, Sexpr_t obj2);
  
  //Bindings: Symbols, Variables and Environments
  Sexpr_t dump_frame(Sexpr_t frame);
  Sexpr_t dump_env_stack(Sexpr_t env_stack);

  //!The reader
  const char *token2name(Int_t tok);

  Sexpr_t read_sexpr(LL_Port &src, Sexpr_t &env_exec, bool quoted);
  Sexpr_t read_atom(LL_Port &src, Sexpr_t &env_exec);

  Sexpr_t read_any_quote(Sexpr_t symbol, LL_Port &src, Sexpr_t &env_exec);
  Sexpr_t read_squote(LL_Port &src, Sexpr_t env_exec)	{ return read_any_quote(sym_squote, src, env_exec); }
  Sexpr_t read_qquote(LL_Port &src, Sexpr_t env_exec)	{ return read_any_quote(sym_qquote, src, env_exec); }
  Sexpr_t read_unquote(LL_Port &src, Sexpr_t env_exec)	{ return read_any_quote(sym_unquote, src, env_exec); }
  Sexpr_t read_uqsplice(LL_Port &src, Sexpr_t env_exec)	{ return read_any_quote(sym_uqsplice, src, env_exec); }

  Sexpr_t read_sharp_const(LL_Port &src, Sexpr_t &env_exec);
  Sexpr_t read_sharp(LL_Port &src, Sexpr_t &env_exec);
  Sexpr_t read_vector(LL_Port &src, Sexpr_t &env_exec);
  Sexpr_t read_quotedvector(LL_Port &src, Sexpr_t &env_exec);  
  Sexpr_t read_list(LL_Port &src, Sexpr_t &env_exec, bool quoted);
  Sexpr_t read_string(LL_Port &src, Sexpr_t &env_exec);  

};

//With error check, inlined for speed.
inline Sexpr_t Lamb::car(Sexpr_t c)	{
  ME("Lamb::car()");
  if (c->type() >= Cell::T_PAIR) return c->prechecked_anypair_get_car();
  embedded_debug_catcher();
  throw mk_syserror("%s Bad type %s", me, c->dump().c_str());
}

inline Sexpr_t Lamb::cdr(Sexpr_t c)	{
  ME("Lamb::cdr()");
  if (c->type() >= Cell::T_PAIR) return c->prechecked_anypair_get_cdr();
  embedded_debug_catcher();
  throw mk_syserror("%s Bad type %s", me, c->dump().c_str());
}

#endif
