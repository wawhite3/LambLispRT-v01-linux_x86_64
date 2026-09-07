// Copyright 2026 by Frobenius Norm LLC 2026-05-16
// Free for non-commercial use. Commercial use requires a license.
#ifndef LL_PLATFORM_GENERIC_H
#define LL_PLATFORM_GENERIC_H

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <setjmp.h>
#include <type_traits>   //!< std::enable_if / std::is_same -- guards AsciiConverter::dec(int), see there
#include "assert.h"
#include "unistd.h"

// Default all LL_ feature flags to 0 so they can be used as C++ values, not just #if guards.
//! TWO FLAGS, ONE UNDERSCORE APART, AND THEY MEAN OPPOSITE THINGS.  Read this before editing either.
//!   LL_ARDUINO = 1  -> build against ESPRESSIF'S Arduino core (framework-arduinoespressif32).
//!   LLARDUINO  = 1  -> build against LAMBLISP'S OWN Arduino implementation (P176, "LLArduino").
//! They are mutually exclusive backends for the same API, and the names differ only by that
//! underscore, so a typo silently selects the other backend and the failure shows up far away.
//! When grepping, match the exact token: a search for "LL_ARDUINO" will NOT find "LLARDUINO".
#ifndef LL_ARDUINO
#define LL_ARDUINO       0
#endif
//! LLARDUINO was called LL_FAKE_ARDUINO until 2026-09-04.  RENAMED because "fake" was wrong in the
//! direction that costs something: it reads as a toy, and readers treat toys as untested and
//! optional.  It is neither.  Every Linux and Jetson build runs on it, the whole host test suite
//! runs on it, and P174 (MinGW/Windows) and P175 (WASM) CANNOT use Espressif's core at all -- there
//! this is the only implementation there will ever be.  See w3_ai_exch/proposal_llarduino_P176.md.
#ifndef LLARDUINO
#define LLARDUINO  0
#endif
#ifndef LL_POSIX
#define LL_POSIX         0
#endif
//! P175: WebAssembly.  LL_WASM is the umbrella ("this is a wasm32 target"); LL_WASI and
//! LL_EMSCRIPTEN name the two HOSTS, which differ only in their I/O and filesystem shims.
//!
//! LL_WASM IMPLIES LL_POSIX HERE, AND THAT IS DELIBERATE: wasi-libc gives us open/read/write/
//! stat/opendir/readdir/clock_gettime, which is every LL_POSIX guard the interpreter core
//! actually needs.  What it does NOT give is the *rest* of what LL_POSIX has historically meant
//! on Linux -- sockets (netdb.h does not exist), termios (termios.h does not exist), and
//! mmap/mprotect with PROT_EXEC (there are no executable pages in wasm at all).  So every
//! LL_POSIX block that reaches for one of those is now spelled `LL_POSIX && !LL_WASM`.
//!
//! THE SYMPTOM IF YOU FORGET: it is NOT a link error.  wasi-libc ships sys/mman.h and
//! sys/socket.h as headers with no working implementation behind them, so the code COMPILES and
//! then fails at run time (or links to a stub that returns -1) -- which is the expensive kind of
//! failure because the build says SUCCESS.  Grep for LL_WASM before adding a POSIX call.
#ifndef LL_WASM
#define LL_WASM          0
#endif
#ifndef LL_WASI
#define LL_WASI          0
#endif
#ifndef LL_EMSCRIPTEN
#define LL_EMSCRIPTEN    0
#endif
//! P174: native Windows, cross-built with MinGW-w64.  Set by the windows_x86_64 board.
//!
//! LL_WINDOWS IMPLIES LL_POSIX AND SUBTRACTS, exactly as LL_WASM does -- MinGW's msvcrt gives us
//! stdio, open/read/write, stat, <dirent.h> and clock_gettime, which is every LL_POSIX guard the
//! interpreter core actually needs.  What it does NOT give is the rest of what LL_POSIX has meant
//! on Linux: <termios.h> (absent), BSD sockets (Winsock instead -- SOCKET not int, closesocket,
//! WSAPoll), mmap/mprotect with PROT_EXEC (VirtualAlloc instead), and /proc/self/exe.  So an
//! LL_POSIX block reaching for one of those is spelled `LL_POSIX && !LL_WINDOWS`.
//!
//! THE SYMPTOM IF YOU FORGET is a COMPILE error here, not a silent one -- MinGW simply lacks the
//! headers -- which makes this the mild half of the port.  The dangerous half is LLP64: Windows
//! x64 has 32-bit `long` with 64-bit pointers, so anything storing a pointer in a `long` compiles
//! clean and then CORRUPTS, with a GC walking garbage as the first symptom.  That is why Word_t is
//! uintptr_t (P174 Phase 0, e4c542d) and why it carries a static_assert: on this target that
//! assertion is the only thing standing between the cell layout and silent truncation.
#ifndef LL_WINDOWS
#define LL_WINDOWS       0
#endif
#ifndef LL_WIFI
#define LL_WIFI          0
#endif
#ifndef LL_LITTLEFS
#define LL_LITTLEFS      0
#endif
#ifndef LL_STACK_SIZE
#define LL_STACK_SIZE    0
#endif
//! Native C-stack budget for eval() recursion.  The B70 guard raises a catchable
//! "recursion too deep" error once eval has grown this many bytes past its outermost
//! frame -- before the real OS stack guard page is hit (which would SIGSEGV/crash the
//! whole process).  Sized below the platform stack: POSIX main thread = 8 MB; ESP32
//! Arduino loopTask = 96 KB (see getArduinoLoopTaskStackSize() in main.cpp).  Override
//! with -DLL_EVAL_STACK_BUDGET=<bytes> per env if a target uses a different stack.
#ifndef LL_EVAL_STACK_BUDGET
  #if LL_WASM
  //! WASM HAS NO 8 MB STACK.  The linear-memory shadow stack is exactly what the LINKER was told
  //! to reserve (`-Wl,-z,stack-size=`, default 64 KB), and overrunning it does not fault -- it
  //! silently walks into static data below __stack_pointer, so the first symptom is corrupted
  //! globals, not a crash at the overrun.  This budget MUST stay below the linker's stack-size in
  //! w3_pio/platforms/wasm32/builder/main.py; change one and you must change the other.
  #define LL_EVAL_STACK_BUDGET  (3UL * 1024 * 1024)   /*!< 3 MB of the 4 MB wasm stack (1 MB headroom) */
  #elif LL_POSIX
  #define LL_EVAL_STACK_BUDGET  (6UL * 1024 * 1024)   /*!< ~6 MB of the 8 MB POSIX stack (2 MB headroom) */
  #else
  #define LL_EVAL_STACK_BUDGET  (40UL * 1024)         /*!< ~40 KB of the 48 KB ESP32 loopTask stack (8 KB headroom); see getArduinoLoopTaskStackSize() in main.cpp -- keep budget < stack so the B70 guard raises a catchable "recursion too deep" before the real stack overflows */
  #endif
#endif
//! DEFINED HERE, ABOVE THE FIRST USE, ON PURPOSE: LL_NCG_DIRAM_POOL below tests LL_RISCV32,
//! and a #define that lands AFTER its own use reads as 0 -- the guard would silently take the
//! wrong arm on any target that does not pass -DLL_RISCV32 on the command line.
//! ISA flags -- P173 sec.4.4.  NAME THE ISA, NOT THE CHIP.
//! Until 2026-09-04 the Xtensa NCG backend was guarded `#if LL_XTENSA || LL_ESP32`, and
//! LL_XTENSA WAS DEFINED NOWHERE IN THE TREE -- so in practice LL_ESP32 alone selected it.
//! LL_ESP32 means "an Espressif SoC" (heap_caps, Arduino, the 32-bit Word_t typedef); it does
//! NOT mean Xtensa, and every Espressif part newer than the S3 is RISC-V.  An ESP32-C3/C6 env
//! must set -DLL_ESP32=1 for all of the above, and that would have compiled the XTENSA backend
//! into a RISC-V build -- 24-bit Xtensa opcodes fed to an RV32 core.
//! Both flags are now set explicitly per board in w3_pio/boards.json (chip_flags), the guards
//! read `#if LL_XTENSA` / `#if LL_RISCV32`, and a target matching NEITHER fails at LINK with
//! `undefined reference to ncg_make_backend` -- loudly, which is the point.
#ifndef LL_XTENSA
#define LL_XTENSA                         0
#endif
#ifndef LL_RISCV32
#define LL_RISCV32                        0
#endif

//! Size of the NCG executable (D/IRAM) pool.  B95: this comes out of INTERNAL DRAM, the same scarce
//! region esp_flash_read needs for the DMA bounce buffer behind every LittleFS read into PSRAM.  On
//! the N8R2 (352 KB internal) the original 128 KB left 500 bytes free after setup -- measured with
//! (esp32-heapinfo) -- so every 512-byte FS read returned ESP_ERR_NO_MEM (err 257).  Override with
//! -DLL_NCG_EXEC_POOL_BYTES=<bytes> per env; larger only buys more simultaneously-resident NCG code.
//! LL_NCG_DIRAM_POOL -- NAMED FOR THE CAPABILITY, NOT THE CHIP.
//! The pool needs one property: a D/IRAM ALIAS REGION, where the same physical block is writable
//! through a DRAM address and executable through an IRAM address.  BOTH the ESP32 (LX6) and the
//! ESP32-S3 (LX7) have one, and the IDF exposes it identically via esp_ptr_in_diram_dram() /
//! esp_ptr_diram_dram_to_iram() in esp_memory_utils.h -- a COMMON component, not an S3 one.
//! This guard was `#if LL_ESP32S3` and that was wrong in a way that cost real time: the LX6 boards
//! (4WD, WROVER) got NO pool, every NCG compile fell through to heap_caps_malloc(MALLOC_CAP_EXEC)
//! -- which has ~368 bytes because the IRAM-only region is consumed by static code -- and so NCG
//! HAS NEVER RUN on those boards.  It presents as `alloc_exec: heap_caps_malloc(N) FAILED` and
//! then as tests that quietly SKIP because the driver could not be compiled.
//! Keying a capability off a chip model also means the only way to enable it on another chip is to
//! claim to BE that chip, which puts a false -DLL_ESP32S3 on a non-S3 board and breaks every other
//! thing that flag legitimately selects.  Name the capability; let each SoC declare whether it has it.

//! ESP32-C3 / C6 (RISC-V), P173 sec.4.2 -- WHY THEY ARE NOT IN THIS LIST YET.
//! The ADDRESSING is already solved for them: the C3 has the same aliased D/IRAM shape as the
//! S3 (soc.h SOC_DIRAM_DRAM_LOW 0x3FC80000 / IRAM_LOW 0x40380000, no SOC_DIRAM_INVERTED) and on
//! the C6 both LOWs are 0x40800000, so esp_ptr_diram_dram_to_iram() degenerates to the identity
//! and alloc_exec's per-word mapping is correct on both AS WRITTEN, with no #if.
//! What is NOT solved is PERMISSION, and it is a different question from addressing: the stock
//! Arduino sdkconfig ships CONFIG_ESP_SYSTEM_MEMPROT_FEATURE=y (+ _LOCK=y) on the C3 and
//! CONFIG_ESP_SYSTEM_PMP_IDRAM_SPLIT=y on the C6, and the IDF's own Kconfig says allocating with
//! MALLOC_CAP_EXEC is not possible while that is on.  The S3 works only because ITS shipped
//! config has memprot OFF -- a Kconfig default, NOT a capability the C-series lost
//! (SOC_CPU_IDRAM_SPLIT_USING_PMP is a switch; MALLOC_CAP_EXEC is still defined there).
//! So this is a BUILD-CONFIGURATION question with a known switch, not a hardware wall -- and it
//! is P173 Phase 0, which needs a real C3 and a real C6 and has NOT been run.  Turning the pool
//! on here without that measurement would claim a capability nobody has observed; leaving it off
//! costs only speed, because a failed alloc_exec makes ncg_compile fall back to the bytecode
//! interpreter, which is a supported state.  When Phase 0 passes, add the chip here AND give it
//! its own LL_NCG_EXEC_POOL_BYTES measured against a LittleFS read in the same run (B95).
//! The `&& !LL_RISCV32` is load-bearing, not tidiness: a C-series env sets -DLL_ESP32=1 (it needs
//! heap_caps, Arduino and the 32-bit Word_t), so without it the pool would switch ON for a chip
//! whose Phase 0 has never been run.  It would not fail politely either -- ncg_exec_pool_init()
//! only checks that the block is IN the D/IRAM range, which it is, so the pool would be USED and
//! the first call into it would fault on the PMP/PMS split, i.e. a crash instead of the graceful
//! "compile fell back to bytecode".
#ifndef LL_NCG_DIRAM_POOL
  #if (LL_ESP32S3 || LL_ESP32) && !LL_RISCV32
    #define LL_NCG_DIRAM_POOL 1
  #else
    #define LL_NCG_DIRAM_POOL 0
  #endif
#endif

//! Pool size is PER-SoC because the budget is, and B95 is the cautionary number: this comes out of
//! INTERNAL DRAM, the same scarce region esp_flash_read needs for its DMA bounce buffer.
//!   ESP32-S3 (LX7): 352 KB internal -> 32 KB pool, already proven on the N8R2.
//!   ESP32   (LX6): ~320 KB internal and a SMALLER D/IRAM alias window, and the classic ESP32 also
//!                   pays for the PSRAM cache workaround, so it gets 12 KB.  Smaller only limits
//!                   how much native code is simultaneously resident; ncg_exec_pool_init() verifies
//!                   the block really landed in D/IRAM and DISABLES the pool if not, so an
//!                   over-ask degrades to the old fallback rather than to a wrong mapping.
#ifndef LL_NCG_EXEC_POOL_BYTES
  #if LL_ESP32S3
    #define LL_NCG_EXEC_POOL_BYTES  (32UL * 1024)
  #elif LL_ESP32
    //! MEASURED, not guessed: at 12 KB the 4WD filled the pool (12228/12288) partway through
    //! ncg-tests.scm and every later compile fell back to heap_caps_malloc(EXEC), which offers
    //! ~700-800 bytes -- so the seam drivers could not compile and their tests SKIPped.  The whole
    //! subsystem tier still ran in 42 s with LittleFS healthy at 12 KB, so there is headroom below
    //! the B95 cliff; 24 KB is the next step, still well under the S3's proven 32 KB on a chip with
    //! less internal DRAM.  RAISE THIS FURTHER ONLY WITH A LITTLEFS READ TEST IN THE SAME RUN:
    //! B95's failure mode is not a pool error, it is every 512-byte FS read returning ESP_ERR_NO_MEM.
    //! 32 KB, and the number is MEASURED twice over, not guessed.  THE POOL IS A BUMP ALLOCATOR
    //! THAT NEVER RECLAIMS -- ncg_pool_used only ever increases, and finalize() deliberately sets
    //! alloc_base=nullptr for pool allocations so nothing is freed -- so a compile-heavy file has a
    //! HARD CEILING and a bigger pool only moves the failure later.  Observed on the 4WD running
    //! ncg-tests.scm: 12 KB filled at 12228/12288, 24 KB filled at 24548/24576, identical SKIPs.
    //! At 24 KB the still-wanted allocations were 1104 + 1160 + 168 + 156 ~= 2.6 KB, so 32 KB is
    //! the smallest size that clears them -- and it matches the S3's already-proven value on a chip
    //! with only slightly less internal DRAM (~320 KB vs 352 KB), keeping the same ~10%% share.
    //! DO NOT raise further without exercising a LittleFS READ in the same run: B95's failure is
    //! not a pool error, it is every 512-byte FS read returning ESP_ERR_NO_MEM, which presents as
    //! MISSING TEST FILES rather than as an allocation message.
    #define LL_NCG_EXEC_POOL_BYTES  (32UL * 1024)
  #else
    #define LL_NCG_EXEC_POOL_BYTES  (32UL * 1024)
  #endif
#endif
//! Hard cap on the GC rootstack (entries).  Each non-tail recursion level pushes ~5 roots, so this
//! sets the rootstack-bound recursion depth.  Since B72 (markstack now grows on demand,
//! GC_MarkStack::push), the rootstack is no longer coupled to a fixed 65536 markstack and can be sized
//! so the non-expandable **C stack** (LL_EVAL_STACK_BUDGET) is the binding limit: a POSIX 8 MB stack
//! reaches ~40 k depth (~215 k roots) before the C-stack guard fires, so 262144.  ESP32's 96 KB stack
//! caps depth at ~600 (far below 65536 roots), so the cap there is irrelevant -- keep it small.
//! Each entry is a Sexpr_t (8 B) -> POSIX 2 MB; the markstack mirrors it transiently during a mark.
#ifndef LL_ROOTSTACK_MAX
  #if LL_WASM
  //! Sized from LL_EVAL_STACK_BUDGET above (3 MB, not the POSIX 6 MB), same ~5 roots per level.
  #define LL_ROOTSTACK_MAX  131072
  #elif LL_POSIX
  #define LL_ROOTSTACK_MAX  262144
  #else
  #define LL_ROOTSTACK_MAX  65536
  #endif
#endif
//! Physical slack allocated above LL_ROOTSTACK_MAX so the overflow error path (which itself
//! allocates a T_ERROR and pushes a few roots) has real buffer instead of writing OOB (B71).
#ifndef LL_ROOTSTACK_SLACK
#define LL_ROOTSTACK_SLACK  256
#endif
//! Recursion guard's rootstack trip point -- a little below the cap, leaving room for the error path.
#ifndef LL_EVAL_ROOT_DEPTH_MAX
#define LL_EVAL_ROOT_DEPTH_MAX  (LL_ROOTSTACK_MAX - 536)
#endif
#ifndef LL_I2C
#define LL_I2C           0
#endif
#ifndef LL_COMMONIO
#define LL_COMMONIO      0
#endif
#ifndef LL_ESP32
#define LL_ESP32         0
#endif
#ifndef LL_COMPLEX
#define LL_COMPLEX       0
#endif
#ifndef LL_RATIONAL
#define LL_RATIONAL      0
#endif
#ifndef LL_BIGNUM
#define LL_BIGNUM        0
#endif
#ifndef LL_BIGNUM_LIMB_BITS
#define LL_BIGNUM_LIMB_BITS 32
#endif
#ifndef LL_BIGNUM_STRICT_RT
#define LL_BIGNUM_STRICT_RT 0
#endif
#ifndef LL_NDARRAY
#define LL_NDARRAY       0
#endif
#ifndef LL_MODBUS
#define LL_MODBUS        0
#endif
#ifndef LL_PROFIBUS
#define LL_PROFIBUS      0
#endif
#ifndef LL_PROFINET
#define LL_PROFINET      0
#endif
#ifndef LL_CUDA
#define LL_CUDA          0
#endif
#ifndef LL_HIP
#define LL_HIP           0
#endif

// Board-specific flags (set by platformio.ini build_flags per env)
#ifndef LL_ESP32_S3_DEVKIT_C
#define LL_ESP32_S3_DEVKIT_C              0
#endif
#ifndef LL_ESP32S3_N8R2
#define LL_ESP32S3_N8R2                   0
#endif
#ifndef LL_Freenove_4WD_Car_Kit_ESP32
#define LL_Freenove_4WD_Car_Kit_ESP32     0
#endif
#ifndef LL_Freenove_ESP32_WROVER
#define LL_Freenove_ESP32_WROVER          0
#endif
#ifndef LL_ESP32_N4R4
#define LL_ESP32_N4R4                     0
#endif
#ifndef LL_AMD64
#define LL_AMD64                          0
#endif
#ifndef LL_X86_64
#define LL_X86_64                         0
#endif
#ifndef LL_AARCH64
#define LL_AARCH64                        0
#endif
#ifndef LL_ARM64
#define LL_ARM64                          0
#endif
#ifndef LL_WASM32
#define LL_WASM32                         0
#endif

//! ===========================================================================
//! THE "DEMO PROFILE" SWITCHES -- factored here ONCE because P174 (MinGW/Windows) and P175
//! (WebAssembly) need byte-for-byte the same exclusion set, and two proposals each carrying
//! their own copy of the list is how the two drift apart.
//!
//!   LL_NETWORK  0 -> no BSD/WiFi sockets at all (TCP client/server ports, LLIP over TCP).
//!   LL_TERMIOS  0 -> no raw terminal mode; line-at-a-time stdio only.
//!   LL_NCG      0 -> no native code generator; the AST and bytecode tiers are the runtime.
//!
//! Each DEFAULTS to what the platform historically had, so no existing target changes; a port
//! that cannot provide one sets it to 0 in its build_flags and every guard follows.
//!
//! WHY THESE ARE SEPARATE FROM LL_POSIX: LL_POSIX has quietly meant three unrelated things --
//! "has open/read/write", "has sockets", and "has termios".  wasm32-wasi is the first target to
//! have the first without the other two, so the single flag stopped being able to express the
//! platform.  A guard that means "has a socket" must now SAY so.
//! ===========================================================================
#ifndef LL_NETWORK
  #if LL_WASM
  #define LL_NETWORK  0	//!< browsers have no raw TCP (WebSocket only); WASI sockets are incomplete
  #else
  #define LL_NETWORK  ((LL_WIFI) || (LL_POSIX))
  #endif
#endif
//! Sockets that specifically come from the BSD/POSIX implementation (ll_vm_port.cpp's
//! posix_fd paths), as opposed to the Arduino WiFi ones.
#define LL_POSIX_NET  ((LL_POSIX) && (LL_NETWORK))

#ifndef LL_TERMIOS
  #if LL_WASM
  #define LL_TERMIOS  0	//!< wasi-libc has no termios.h at all -- not a stub, the header is absent
  #else
  #define LL_TERMIOS  (LL_POSIX)
  #endif
#endif

//! LL_NCG -- is a native code generator COMPILED IN at all.
//! WASM IS A BUILD TARGET, NOT AN NCG BACKEND, and that is a design decision, not a gap to fill
//! later (P175 sec. "FIRST"): wasm is a stack machine with structured control flow and no
//! self-modifying code, the host engine already JITs it, and emitting wasm from our
//! register-machine backends would be a JIT feeding a JIT.  If you are here because you want to
//! "add a wasm NCG backend", read that section first.
#ifndef LL_NCG
  #if LL_WASM
  #define LL_NCG  0
  #else
  #define LL_NCG  1
  #endif
#endif


//! LL_XTENSA / LL_RISCV32 -- NAME THE ISA, NOT THE CHIP.  (P172/P173 Phase 1.)
//!
//! An NCG backend is selected by INSTRUCTION SET, and until 2026-09-05 nothing in this tree said
//! so.  `LL_XTENSA` was referenced by ll_vm_ncg_xtensa.cpp's guard and DEFINED NOWHERE -- grep the
//! whole tree and it appears only inside `#if LL_XTENSA || LL_ESP32` -- so the Xtensa backend was
//! in practice selected by **LL_ESP32 alone**.
//!
//! THE CONSEQUENCE, and it is the reason this block exists rather than a comment: every Espressif
//! part newer than the ESP32-S3 is RISC-V (C2, C3, C5, C6, C61, H2, P4).  Such a target MUST set
//! -DLL_ESP32=1 -- it needs heap_caps, the Arduino core, and the ESP32 branch of the platform
//! layer -- and the moment it does, it compiles the XTENSA CODE GENERATOR into a RISC-V binary.
//! That is not a build error; it emits Xtensa opcodes into a buffer and jumps to them.
//!
//! This is the identical defect the LL_NCG_DIRAM_POOL note above documents -- keying a capability
//! off a chip model means the only way to enable it elsewhere is to CLAIM TO BE that chip -- and
//! the LX6 boards already lost NCG entirely to that mistake once.
//!
//! Both default to 0, and the existing Xtensa envs now pass -DLL_XTENSA=1 EXPLICITLY, so current
//! behaviour is preserved by statement rather than by omission.  A target that sets neither gets
//! `ncg_make_backend()` returning nullptr (ll_vm_ncg_core.cpp, the complement guard) and therefore
//! runs the bytecode interpreter -- a SUPPORTED state, not a broken one.  If you add an ISA here,
//! add it to that complement guard too, or you get a duplicate symbol instead of a missing one.
#ifndef LL_XTENSA
#define LL_XTENSA        0                //!<Tensilica Xtensa LX6/LX7 (ESP32, ESP32-S2/S3).
#endif
#ifndef LL_RISCV32
#define LL_RISCV32       0                //!<32-bit RISC-V (RV32IMC): ESP32-C/H/P series, and others.
#endif


//! Backend selection.  See the note at the flag definitions above: LL_ARDUINO is Espressif's core,
//! LLARDUINO is ours -- one underscore apart, opposite meanings.
#if LL_ARDUINO
#include "Arduino.h"
#endif

//! LLArduino (P176): LambLisp's own implementation of the Arduino API.  Not a stub and not a
//! fallback -- it is the ONLY backend on POSIX, WASM and Windows, and it is what the entire host
//! test suite exercises.  Anything added here is API SURFACE WE OWE ON EVERY TARGET.
#if LLARDUINO
#include <string.h>

unsigned long millis();
unsigned long micros();
void delay_ms(unsigned long ms);

static void delay(unsigned long ms) { delay_ms(ms); }

//! LLArduino GPIO surface (P176), implemented by a BACKEND -- ll_llarduino_null.cpp simulates it in
//! memory; a libgpiod backend drives real pins on a Pi or Jetson.  These are the eight functions
//! ll_xmop3_CommonIO.cpp actually calls; without them `commonio` cannot compile off-Arduino, which
//! is why 26 Scheme procedures were ESP32-only and untestable without a board.
//! Arduino spells the constants as macros, so match that rather than inventing an enum.
#ifndef HIGH
#define HIGH          1
#define LOW           0
#endif
#ifndef INPUT
#define INPUT         0x0
#define OUTPUT        0x1
#define INPUT_PULLUP  0x2
#endif

void pinMode(int pin, int mode);
void digitalWrite(int pin, int val);
int  digitalRead(int pin);
void analogWrite(int pin, int val);
int  analogRead(int pin);
void tone(int pin, unsigned int freq);
void tone(int pin, unsigned int freq, unsigned long dur);
void noTone(int pin);
void delayMicroseconds(unsigned long us);

//! Backend inspection -- lets a test assert on state the Arduino API cannot report (mode, duty,
//! tone) and name which backend answered.  See ll_llarduino_null.cpp.
int         ll_llarduino_pin_mode(int pin);
int         ll_llarduino_pin_duty(int pin);
int         ll_llarduino_pin_tone(int pin);
int         ll_llarduino_npins();
const char *ll_llarduino_backend();

typedef uint8_t byte;

class LL_String {
public:

  LL_String()				{ ptr = 0;  set(""); }
  LL_String(const char *other)		{ ptr = 0;  set(other);  }
  LL_String(const char *other, int n)	{ ptr = 0;  set(other, n);  }
  LL_String(const LL_String &other)	{ ptr = 0;  set(other.ptr);  }
  
  ~LL_String()	{ if (ptr) { delete[] ptr;  ptr = 0; } }

  void set(const char *s) { set(s, strlen(s)); }

  void set(const char *s, int n) {
    if (ptr) { delete[] ptr;  ptr = 0; }
    char *p = ptr = new char[n + 1];
    while (n--) *p++ = *s++;
    *p = '\0';
  }
  
  LL_String concat(const char *s1, const char *s2) {
    int n1 = strlen(s1);
    int n2 = strlen(s2);
    char *s3 = new char[n1 + n2 + 1];

    char *p = s3;
    while (n1--) *p++ = *s1++;
    while (n2--) *p++ = *s2++;
    *p = '\0';
    return s3;
  }

  LL_String &append(const char *s1) {
    const char *s0 = ptr;
    int n0 = s0 ? strlen(s0) : 0;
    int n1 = strlen(s1);
    char *s2 = new char[n0 + n1 + 1];

    char *p = s2;
    while (n0--) *p++ = *s0++;
    while (n1--) *p++ = *s1++;
    *p = '\0';
    
    if (ptr) { delete[] ptr;  ptr = 0; }
    ptr = s2;
    return *this;
  }

  LL_String substring(int start, int len)	{ return LL_String(&(ptr[start]), len); }
  
  LL_String &operator=(const char *other)	{ set(other); return *this; }
  LL_String &operator=(const LL_String &other)	{ set(other.ptr); return *this; }

  bool operator==(const char *other)		{ return strcmp(ptr, other) == 0; }
  bool operator!=(const char *other)		{ return strcmp(ptr, other) != 0; }
  bool operator==(const LL_String &other)	{ return strcmp(ptr, other.ptr) == 0; }
  bool operator!=(const LL_String &other)	{ return strcmp(ptr, other.ptr) != 0; }
  char &operator[](int ix)			{ return ptr[ix]; }

  LL_String operator+(const char *other)	{ return concat(ptr, other); }
  LL_String operator+(const LL_String &other)	{ return concat(ptr, other.ptr); }
  LL_String operator+(char c)			{ char cstr[2];  cstr[0] = c;  cstr[1] = '\0';  return concat(ptr, cstr); }

  LL_String &operator+=(const char *other)	{ return append(other); }
  LL_String &operator+=(const LL_String &other)	{ return append(other.ptr); }
  LL_String &operator+=(char c)			{ char cstr[2];  cstr[0] = c;  cstr[1] = '\0';  return append(cstr); }
  
  char *c_str() const	{ return ptr; }
  int length() const	{ return strlen(ptr); }
  
private:
  char *ptr;
};

typedef LL_String String;
#endif

//! @name Handy directives, macros and utility functions.
//!@{
#define NOTUSED __attribute__((__unused__))			/*!<GCC extension to suppress individual "not used" warnings. */
#define INLINE __attribute__((__inline__))			/*!<GCC extension to encourage inlining a function. */
#define NOINLINE __attribute__((__noinline__))			/*!<GCC extension to avoid inlining a function. */
#define CHECKPRINTF __attribute__((format(printf, 1, 2)))	/*!<GCC extension, turn on checking of printf() const format strings, if they are known at compile time. */
#define CHECKPRINTF_pos2 __attribute__((format(printf, 2, 3)))	/*!<GCC extension, turn on checking of printf() const format strings, if they are known at compile time. */

//! MOVED HERE 2026-09-04, and it must stay below this point: Print uses String (typedef at
//! `typedef LL_String String` above) and CHECKPRINTF_pos2 (defined just above).  Declared any
//! earlier in this header and the compiler reports "String does not name a type" -- which is a
//! header-ordering error, not a missing class.

//! GUARDED, AND IT MUST BE.  On ESP32 the VENDOR Arduino core defines its own Print and Stream
//! (cores/esp32/Print.h, Stream.h) via Arduino.h, so declaring these unconditionally is a
//! "redefinition of class Print" build failure on every ESP32 target.  It compiled clean on the
//! hosts because there is no Arduino core there to collide with -- which is exactly why the
//! mistake was invisible until an ESP32 build ran.  When this block was moved down here to sit
//! below the String typedef it left the LLARDUINO guard that ends further up; it now carries its
//! own.  LLArduino PROVIDES these where the vendor core is absent, and defers to it where present.
#if LLARDUINO
//! LLARDUINO Print / Stream / Serial (P176).  NOT STUBS, and they need not be: Print and Stream are
//! pure character I/O, and LambStdio ALREADY implements exactly that on POSIX (stdin/stdout in
//! non-canonical mode).  So this is an Arduino-SHAPED FACADE over working I/O, in the same class as
//! millis() -- genuinely implemented, not simulated.  It is what lets sketch-style code
//! (`Serial.println("hi")`) run on a host with no board, and on WASM (P175) and Windows (P174),
//! where Espressif's core cannot go at all.
//!
//! FIDELITY IS THE RISK HERE, not capability.  Arduino's Print has behaviours worth matching
//! DELIBERATELY rather than by accident, and each is a place a host test could pass while the
//! device disagrees: println() terminates CRLF ("\r\n") not "\n"; write() returns a BYTE COUNT;
//! print(double, digits) defaults to 2 decimal places; print(int, base) supports BIN/OCT/DEC/HEX.
//! Those are exactly what P176's success criterion 3 exists to check -- the same suite run against
//! the vendor core and against LLArduino must agree.
class Print {
public:
  virtual ~Print() {}
  virtual size_t write(uint8_t c) = 0;
  virtual size_t write(const uint8_t *buf, size_t n);
  size_t write(const char *s);
  size_t print(char c);
  size_t print(const char *s);
  size_t print(const String &s);
  size_t print(int n, int base = 10);
  size_t print(long n, int base = 10);
  size_t print(unsigned long n, int base = 10);
  size_t print(double v, int digits = 2);
  size_t println();                                  //!< CRLF, as Arduino does -- not "\n"
  size_t println(char c);
  size_t println(const char *s);
  size_t println(const String &s);
  size_t println(int n, int base = 10);
  size_t println(long n, int base = 10);
  size_t println(unsigned long n, int base = 10);
  size_t println(double v, int digits = 2);
  size_t printf(const char *fmt, ...) CHECKPRINTF_pos2;   //!< HardwareSerial::printf, used by LL itself
};

class Stream : public Print {
public:
  virtual int available() = 0;
  virtual int read()      = 0;
  virtual int peek()      = 0;
  virtual void flush()    {}
};

//! Serial over LambStdio.  peek() is implemented HERE because LambStdioClass has no peek: one byte
//! of pushback, which is all Stream promises.
class LLArduinoSerial : public Stream {
  int  _peeked;                                      //!< -1 = empty
public:
  LLArduinoSerial() : _peeked(-1) {}
  void begin(unsigned long baud = 115200);
  void end();
  int  available() override;
  int  availableForWrite();
  int  read() override;
  int  peek() override;
  void flush() override;
  size_t write(uint8_t c) override;
  using Print::write;
};

extern LLArduinoSerial Serial;


#endif // LLARDUINO -- Print/Stream/Serial

//! Render THROUGH whichever Print is compiled in -- LLArduino's or the vendor core's -- so Scheme
//! can assert what a device would actually emit and the SAME suite can be pointed at both.
//! Declared outside the guard above deliberately: see the note in ll_llarduino_print.cpp.
#if LLARDUINO || LL_ARDUINO
const char *ll_llarduino_fmt_int(long v, int base);
const char *ll_llarduino_fmt_real(double v, int digits);
const char *ll_llarduino_eol();
#endif

#define CHECKPRINTF_pos3 __attribute__((format(printf, 3, 4)))	/*!<GCC extension, turn on checking of printf() const format strings, if they are known at compile time. */
#define CHECKPRINTF_pos4 __attribute__((format(printf, 4, 5)))	/*!<GCC extension, turn on checking of printf() const format strings, if they are known at compile time. */

// On Arduino/ESP32, IRAM_ATTR and DRAM_ATTR are already defined by Arduino.h (esp_attr.h).
// On other platforms (Linux, desktop simulation) define them as empty so the same source compiles.
#if !defined(IRAM_ATTR)
#define IRAM_ATTR					/*!<Place function/data in fast on-chip IRAM (ESP32: ~1-cycle fetch). No-op on other platforms. */
#endif
#if !defined(DRAM_ATTR)
#define DRAM_ATTR					/*!<Place data in on-chip DRAM (ESP32: avoids PSRAM latency). No-op on other platforms. */
#endif

//! @name LambLisp imposes a limit on the length of strings, to reduce opportunities for runaway in an embedded system.
//!@{
const unsigned long toString_MAX_LENGTH = 8192;		//!<Global limit on Lamb-generated strings.
//! B167: how much of an offending form an error message may quote back.  Well under
//! toString_MAX_LENGTH so the surrounding message text cannot push the total over it.
const unsigned long LL_ERRFORM_MAX = 400;
//! B167: how many bindings of an ALIST environment frame a printed environment may expand before
//! it collapses to `#frame(N+)`.  An svec frame has always printed compactly; an alist frame did
//! not, and each of its values may be a closure that re-captures the same environment -- so
//! expanding one expands the program.  Measured on a chibi r7rs-tests run: single calls asking for
//! 13.7 MB, 14.8 MB and 31.8 MB of environment text, inside DIAGNOSTICS.  Chosen generously so
//! ordinary frames print in full and only unreadable ones are bounded.
const unsigned long LL_ENVFRAME_MAX = 32;
String toString(const char *fmt, ...) CHECKPRINTF;	//!<Produce a new string from the format and arguments, respecting the global limit on string length.
//!@}


#define ME(_me_) NOTUSED const char me[] = _me_			/*!<Declare an identifier "const char me[]" without causing "unused variable" warnings and/or code clutter to suppress them. */
#define isdef(sym) (#sym[0])					//!<Determine (cheaply) at runtime if a preprocessor symbol is defined.

void global_printf(const char *fmt, ...);		//!<This function enforces the limit on generated strings.

//!@}

/*! @name These primitive types are shared by LambLisp and the underlying VM.

  Sizing requirements for LambLisp Cells:

  | Each cell contains 3 fields: tag, car, cdr.                                     |
  | The fields are equal in size and sequential in memory.                          |
  | The bytes within each LambLisp Cell are individually addressable.               |
  | Each field can hold a generic computer "word", a signed integer, or an address. |
  | An integer fills the car field, and may also fill the cdr field.                |
  | A real number also fills the car field, and may also fill the cdr field.        |

  Since the beginning of time (Jan 1 1970) the specific organization of these have been platform-dependent.
  There is a (mostly) obvious correspondence between the shared type name and the underlying C++ type.

  Note the difference between *Charst_t* and *CharVec_t*; one is mutable, the other not.  The immutable version may go away.
  The same applies to *Bytest_t* and *ByteVec_t*.
*/
//!@{

typedef unsigned char Byte_t;	//!<Universally known byte type.
typedef bool Bool_t;		//!<Boolean type.
typedef char Char_t;		//!<Character type.
typedef int32_t ll_codepoint_t;  //!< Unicode scalar value (U+0000 – U+10FFFF)

typedef Char_t const *Charst_t;		//!<Pointer to immutable character array.
typedef Byte_t const *Bytest_t;		//!<Pointer to immutable byte array
typedef Char_t *CharVec_t;		//!<Pointer to mutable char array
typedef Byte_t *ByteVec_t;		//!<Pointer to mutable byte array

/*! @name B95 payload allocator -- bulk VM payloads belong in PSRAM, not internal DRAM.
  String/symbol characters, vector and bytevector bodies, bignum digits and compiled bytecode are
  PURE DATA: the VM never DMAs out of them.  They must NOT come from plain new/malloc on an ESP32,
  because the IDF only diverts allocations LARGER than SPIRAM_MALLOC_ALWAYSINTERNAL to PSRAM -- so
  the many SMALL payloads a load chain creates (every symbol name, every string literal) all land
  in INTERNAL DRAM.  Internal DRAM is also the only memory a flash read can bounce through, so
  draining it makes every LittleFS read fail with ESP_ERR_NO_MEM (err 257) -- that is B95: the
  setup.scm load chain consumed ~115 KB of internal DRAM and left 1176 bytes, while 1.7 MB of
  PSRAM sat unused.  Allocating these payloads from PSRAM keeps internal DRAM for DMA.

  Falls back to malloc when there is no PSRAM (host builds, non-PSRAM parts) or PSRAM is full, so
  behaviour is unchanged everywhere else.  Pairs with ll_payload_free -- these blocks are freed by
  the GC sweep, so they must NEVER be released with delete[].
*/
//!@{
void *ll_payload_alloc_bytes(size_t nbytes);
void  ll_payload_free(void *p);

//! Typed convenience wrapper.  POD payloads only -- no constructors are run.
template <typename T> static inline T *ll_payload_new(size_t n)
{
  return (T *) ll_payload_alloc_bytes(n * sizeof(T));
}
//!@}

//! P174 Phase 0: ONE definition, not three per-platform ones, and spelled in the <stdint.h> idiom
//! so it states its ACTUAL requirement instead of satisfying it by accident.
//!
//! Word_t HOLDS POINTERS -- `rplacd((Word_t) val)`, the whole Cell payload -- so the requirement is
//! "wide enough for a pointer", which is exactly what uintptr_t means.  It was previously declared
//! per-platform as `unsigned long` (LL_AMD64, LL_ARM64) and `unsigned` (LL_ESP32).  Both were
//! CORRECT BY ACCIDENT rather than by construction: LP64 Linux and ILP32 Xtensa happen to make
//! those the same size as a pointer.  Windows x64 is LLP64 -- `long` is 32 bits while pointers are
//! 64 -- so the LL_AMD64 spelling TRUNCATES EVERY POINTER there, silently, at compile time, with
//! the first symptom being a GC walking garbage.  uintptr_t is 4 bytes on the ESP32 and 8 on every
//! 64-bit host, so one line is right on all of them and a fourth platform cannot reintroduce the
//! bug by copying the wrong branch.
//!
//! THE STATIC ASSERT IS THE POINT, not decoration.  Nothing else in the tree checks this property;
//! it is the only mechanism that catches a width regression at build time rather than in a
//! debugger.  Do not remove it, and do not replace uintptr_t with a fixed width (uint64_t would
//! waste half of every cell on the ESP32; uint32_t would truncate on a host).
//!
//! Related, and the reason the idiom matters beyond Windows: LL_int32 is int32_t, and the ESP-IDF 5
//! xtensa toolchain changed int32_t from `int` to `long` -- which silently made every
//! ascii.dec(someInt) call ambiguous (see the dec(int) forwarder below).  A type that NAMES its
//! width does not move under a toolchain upgrade; a type spelled `long` does.
typedef uintptr_t Word_t;	//!<A generic computer word, used only for setting and retrieval -- WIDE ENOUGH FOR A POINTER.  Not an *unsigned int* used for arithmetic.
typedef void *Ptr_t;		//!<Generic pointer in C++
static_assert(sizeof(Word_t) >= sizeof(void *),
              "Word_t must hold a pointer -- see the P174 note above; LLP64 (Windows x64) breaks "
              "the old `unsigned long` spelling because long is 32 bits there and pointers are 64.");

typedef int32_t  LL_int32;	//!<LambLisp 32-bit exact integer.

//! @name UTF-8 codepoint helpers
//! R7RS indexes strings by CHARACTER, not by byte.  LambLisp stores strings as NUL-terminated
//! UTF-8, so every index operation has to walk the encoding.  These are the one shared
//! implementation -- encoders were previously duplicated in ll_vm_cell.cpp, ll_vm_mop3_json.cpp
//! and ll_vm_port.cpp, which is how the write path came to encode real UTF-8 while the measure
//! path still counted bytes (B146: `(string-length (list->string (list (integer->char 945))))`
//! returned 2 for a one-character string).
//!@{

//! Bytes in the UTF-8 sequence that starts with lead byte b0.  A continuation or invalid lead
//! counts as 1 so a malformed string still advances and cannot hang a scan.
inline int ll_u8_seqlen(unsigned char b0)
{
  if (b0 < 0x80) return 1;
  if ((b0 & 0xE0) == 0xC0) return 2;
  if ((b0 & 0xF0) == 0xE0) return 3;
  if ((b0 & 0xF8) == 0xF0) return 4;
  return 1;
}

//! Decode the sequence at p; *nb receives its byte length.  Invalid input yields the lead byte
//! itself, matching the reader's pass-through behaviour rather than throwing mid-scan.
inline ll_codepoint_t ll_u8_decode(const char *p, int *nb)
{
  unsigned char b0 = (unsigned char) p[0];
  int n = ll_u8_seqlen(b0);
  if (nb) *nb = n;
  if (n == 1) return (ll_codepoint_t) b0;
  ll_codepoint_t cp = b0 & (0xFF >> (n + 1));
  for (int i = 1; i < n; i++) {
    unsigned char c = (unsigned char) p[i];
    if ((c & 0xC0) != 0x80) { if (nb) *nb = 1; return (ll_codepoint_t) b0; }
    cp = (cp << 6) | (c & 0x3F);
  }
  return cp;
}

//! Encode cp into out (at least 4 bytes); returns the byte count.  No NUL is written.
inline int ll_u8_encode(ll_codepoint_t cp, char *out)
{
  if (cp < 0x80)    { out[0] = (char) cp; return 1; }
  if (cp < 0x800)   { out[0] = (char)(0xC0 | (cp >> 6));  out[1] = (char)(0x80 | (cp & 0x3F)); return 2; }
  if (cp < 0x10000) { out[0] = (char)(0xE0 | (cp >> 12)); out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
                      out[2] = (char)(0x80 | (cp & 0x3F)); return 3; }
  out[0] = (char)(0xF0 | (cp >> 18));         out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
  out[2] = (char)(0x80 | ((cp >> 6) & 0x3F)); out[3] = (char)(0x80 | (cp & 0x3F));  return 4;
}

//! CHARACTER count of a NUL-terminated UTF-8 string (R7RS string-length).
inline LL_int32 ll_u8_strlen(const char *s)
{
  LL_int32 n = 0;
  for (const char *p = s; *p; n++) p += ll_u8_seqlen((unsigned char) *p);
  return n;
}

//! Byte pointer to character index k, or NULL if k is past the end.  k == length returns the
//! terminator, so a caller can use it as an exclusive range end.
inline const char *ll_u8_index(const char *s, LL_int32 k)
{
  const char *p = s;
  for (LL_int32 i = 0; i < k; i++) {
    if (!*p) return 0;
    p += ll_u8_seqlen((unsigned char) *p);
  }
  return p;
}
//!@}

typedef int64_t  LL_int64;	//!<LambLisp 64-bit exact integer.
typedef float   LL_float32;	//!<LambLisp IEEE 754 single-precision (32-bit).
typedef double  LL_float64;	//!<LambLisp IEEE 754 double-precision (64-bit).

static_assert(sizeof(Word_t)    == sizeof(Ptr_t),  "Word_t size must be == Ptr_t size\n");
static_assert(sizeof(Word_t)    >= sizeof(LL_int32), "Word_t size must be >= LL_int32 size\n");
static_assert(sizeof(LL_float32) == 4,              "LL_float32 must be 4 bytes\n");
static_assert(sizeof(LL_float64) == 8,              "LL_float64 must be 8 bytes\n");
//!@}

//The AsciiConverter class mitigates printf formatting problems caused by different int sizes on different processors, when using printf-style format string (%u, %d %lu %ld etc).
class AsciiConverter {
public:

  char *dec(Word_t n) {
    LL_int32 ix  = buffer_size - 1;
    char *ptr = &(buffer[ix]);

    *ptr = '\0';
    do {
      Word_t d = n % 10;
      n /= 10;
      char c = d + '0';
      *--ptr = c;
    } while (n);
    return ptr;
  }

  char *dec(LL_int32 n) {
    if (n >= 0) return dec((Word_t) n);
    if (n == INT32_MIN) return dec((LL_int64) n);   //!< B82: -INT32_MIN overflows int32; int64 path prints it in full
    char *ptr = dec(-n);
    *--ptr = '-';
    return ptr;
  }

  //! dec(int) -- EXISTS ONLY WHERE int32_t IS NOT int, AND THAT VARIES BY TOOLCHAIN.
  //! The xtensa toolchain shipped with ESP-IDF 5 typedefs int32_t as `long`; the one with IDF 4.4
  //! used `int`.  With int32_t == long the overload set below is {unsigned, long, long long,
  //! float} and a plain `int` argument converts to the first three at IDENTICAL rank, so every
  //! ascii.dec(someInt) in the tree stops compiling at once with
  //!     error: call of overloaded 'dec(int&)' is ambiguous
  //! -- which points at the CALL rather than at the typedef that moved, and looks like the call
  //! site is wrong when nothing about it changed.  Under IDF 4.4 int32_t == int made dec(LL_int32)
  //! an exact match, so the ambiguity simply could not arise and the calls were always fine.
  //! Adding this forwarder fixes every call site at once instead of casting at each one.
  //! THE enable_if IS LOAD-BEARING, NOT DECORATION: on hosts where int32_t IS int (x86-64 Linux,
  //! the IDF 4.4 toolchain) this would be a redeclaration of dec(LL_int32) and would not compile,
  //! so it must vanish there rather than be #ifdef'd on a chip or an IDF version -- the condition
  //! is a property of the TYPEDEF, and only the compiler knows it.
  template <typename T>
  typename std::enable_if<std::is_same<T, int>::value && !std::is_same<LL_int32, int>::value,
                          char *>::type
  dec(T n) { return dec((LL_int32) n); }

  char *dec(LL_int64 n) {
    // Full 64-bit signed decimal; avoids conflict with dec(Word_t) on 64-bit platforms.
    LL_int32 ix = buffer_size - 1;
    char *ptr = &(buffer[ix]);
    *ptr = '\0';
    bool neg = (n < 0);
    uint64_t uv = neg ? (n == INT64_MIN ? (uint64_t) 9223372036854775808ULL : (uint64_t) -n)
                      : (uint64_t) n;
    do { *--ptr = (char)('0' + (uv % 10)); uv /= 10; } while (uv);
    if (neg) *--ptr = '-';
    return ptr;
  }

  char *hex(Word_t n) {
    LL_int32 ix   = buffer_size - 1;
    char *ptr  = &(buffer[ix]);
    LL_int32 nibs = sizeof(Word_t) * 2;

    *ptr = '\0';
    while (nibs--) {
      Word_t d = n & 0x0f;
      n >>= 4;
      char c = (d < 10) ? (d + '0') : (d - 10 + 'a');
      *--ptr = c;
    }
    return ptr;

  }

  char *dec(LL_float32 n) {
    snprintf(buffer, buffer_size, "%f", (LL_float64) n);
    return buffer;
  }

private:
  static const LL_int32 buffer_size = 128;
  char buffer[buffer_size];
};

extern AsciiConverter ascii;
  
class LambPlatform {
public:

  //! @name Interaction with the underlying runtime platform.
  //!@{
  LambPlatform() {}
  ~LambPlatform() { end(); }

  void begin(void);
  void loop(void);			//!<Perform any platform-specific activity needed at loop() time.  Call only once per loop at beginning main loop().
  void end(void);
  
  void reboot(void);
  
  const char *name();			//!<Return a pointer to a chacter array containing a description of the runtime platform.
  void identification(void);		//!<Emit a string with the complete detailed description of the platform.

  LL_int32 free_heap();
  /*! @brief B129: free space in the pool cell BLOCKS are actually allocated from.
      expand() used to test free_heap(), which on ESP32 is esp_get_free_heap_size() -- the AGGREGATE
      of internal DRAM and PSRAM -- and then allocated specifically from MALLOC_CAP_SPIRAM.  It could
      therefore believe there was room when the space was in the wrong pool, let the cell heap grow
      until PSRAM was full, and starve the PAYLOADS (strings, bignum digits, vectors, bytecode) that
      share that pool.  A bignum payload then got NULL and the board died with StoreProhibited.
      On non-ESP32 targets this is just free_heap(). */
  LL_int32 cell_pool_free();			//!<Return the unused space available for LambLisp expansion.  Whether this is *total* space or *largest* space is platform-dependent.  Accuracy is specifically not guaranteed.
  LL_int32 free_stack();			//!<Return the unused execution stack space available.  Accuracy is specifically not guaranteed.
  Bool_t heap_integrity_check(Bool_t complain=false);	//!<Run intensive heap check; print errors if found; return true if errors found.

  //!Return a real number between -1.0 and +1.0.  May include -1.0 but not +1.0.
  LL_float32 rand11() {
    const int max_int = (~((unsigned int) 0)) >> 1;
    const int min_int = -max_int - 1;
    const LL_float32 min  = (LL_float32) min_int;
    
    LL_int32 n;
    rand((byte *) &n, sizeof(n));
    return (n / min);
  }
  
  LL_float32 rand01()			{ return (rand11() + 1.0) / 2.0; }

  void rand(byte *buf, LL_int32 len);	//!<Fill a buffer with the highest-quality random numbers available on this platform.
  void rand11(LL_float32 *buf, LL_int32 n)	{ while (n--) *buf++ = rand11(); }
  void rand01(LL_float32 *buf, LL_int32 n)	{ while (n--) *buf++ = rand01(); }

  LL_int32 loop_elapsed_ms()	{ return millis() - loop_start_ms; }	//!<Return the time elapsed since the beginning of the current loop().
  LL_int32 loop_elapsed_us()	{ return micros() - loop_start_us; }	//!<Return the time elapsed since the beginning of the current loop().

  //!@}

private:
  LL_int32 loop_start_ms;
  LL_int32 loop_start_us;
};

extern LambPlatform lambPlatform;

//! @name Elide the differences between platform "files" with this typedef.
//!@{
#if LL_LITTLEFS
#include "LittleFS.h"
typedef File File_Native;
#endif

#if LL_POSIX
#include <stdio.h>
typedef FILE* File_Native;
#endif
//!@}

/*! \class LL_File
  
  The *file* type is ultimately provided by the underlying operating system, not by LambLisp.
  This class elides the differences between file types on different platforms, providing a POSIX-like interface.

  Note that there is no *open* operation on files.
  A file is opened by the *file system* and then a *file* is returned.
  After a *file* is closed, the same file object cannot be opened again; instead a new file must be requested from the *file system*.
*/
//!@{
class LL_File {
public:

  LL_File();
  ~LL_File();

  bool isOpen() { return _path != ""; }
  int read(void);
  int write(byte b);
  int seek(unsigned long target, int whence=SEEK_SET);
  int tell();
  int size();
  int close();

  int read(byte *b, int n) {
    LL_int32 nread = 0;
    while (n--) {
      int ch = read();
      if (ch == EOF) return nread;
      b[nread++] = ch;
    }
    return nread;
  }
  
  int read(char *s, int n) { return read((byte *) s, n); }  
  int write(const byte *b, int n) { while (n--) write(*b++); return n; }
  int write(const char *s, int n) { while (n--) write((byte) *s++); return n; }
  int peek();
  
  File_Native _theFile;
  String _path;
  String _mode;

private:

};
//!@}

/*! \class LL_File_System

  The "file system" type elides the differences between different underlying platforms.
  For example, it will deal with the leading '/' required by LittleFS.
  This minimal file system interface is platform-independent.
*/
class LL_File_System {
public:
  LL_File *open(const char *path, const char *mode);
  bool exists(const char *path);
  int rm(const char *path);
  int mv(const char *from, const char *to);
  int mkdir(const char *path);

private:
};

extern LL_File_System ll_file_system;

/*! \name WiFi and Wire, in case we need to rationalize conflicting implementations.
 */
//!@{
#if LL_WIRE
#include "Wire.h"
extern TwoWire *LL_Wire;
#endif

#if LL_WIFI
#include "WiFi.h"
#include "WiFiClientSecure.h"
extern WiFiClass *LL_WiFi;
#endif
//!@}

/*! \class LambStdioClass
  
  A wrapper around the underlying stdin/stdout implementation.
  On an embedded system, this class will use the primary serial in/out (`Serial` on Arduino-compatibles).
  On Linux, this class will set the terminal to byte-at-a-time mode (called "non-CANONICAL" mode).
*/
class LambStdioClass {
public:
  int setTxBufferSize(int n);
  int setRxBufferSize(int n);
  
  void begin(void) { begin(115200); }
  void begin(unsigned long baudrate);
  void end();
  int available(void);
  int availableForWrite(void);
  int read(void);
  int write(uint8_t c);
  int write(char c) { return write((uint8_t) c); }
  
  void flush(void);

  int read(byte *buf, int max) {
    int nread = 0;
    while (max--) {
      int b = read();
      if (b == EOF) break;
      *buf++ = b;
    }
    return nread;
  }

  int read(char *s, int max)		{ return read((uint8_t *) s, max); }
  int write(const char *s, int n)	{ return write((uint8_t *) s, n); }
  int write(const byte *b, size_t n)	{ int i=n;  while (i--) write(*b++);  return n; }
  int write(const char *s)		{ int i=0;  while (*s) { write(*s++); i++; }  return i; }
  
  operator bool() { return true; }
};

extern LambStdioClass LambStdio;

typedef byte uuid_t[16];

//!Macro to do something once after the system awakens.
#define once(_once_something) do {		\
    static bool _visited_ = false;		\
    if (!_visited_) {				\
      _visited_ = true;				\
      { _once_something; }			\
    }						\
  } while (0)					\
    //
//

//!Macro to do something every so often.
#define every(_every_so_often_ms, _every_something_to_do) do {	\
    static unsigned long _every_next = 0;			\
    unsigned long _every_now = millis();			\
    if (_every_now >= _every_next) {				\
      { _every_something_to_do; }				\
      _every_next = _every_now + (_every_so_often_ms);		\
    }								\
  } while (0)							\
    //
//

/*!
  The embedded debug catcher ensures that an address is available to be set as a breaskpoint for a hardware debugger.
  This useful in cases where the generated code has been heavily inlined.
  Undefine this symbol if not using a hardware debugger, or redefine it to point to a different breakpoint target.
*/
void embedded_debug_catcher();
#define ll_debug_catcher embedded_debug_catcher()
//#define ll_debug_catcher ll_term.flush()

// ── Settings file paths ──────────────────────────────────────────────────────
#if LL_LITTLEFS
#define LAMB_SETTINGS_PATH  "/Settings-Lamb.scm"   //!< Pre-allocation settings (C++ pre-reader, before heap).
#define SETTINGS_PATH       "/Settings.scm"         //!< Runtime settings (Scheme reader, after startup).
#else
#define LAMB_SETTINGS_PATH  "Settings-Lamb.scm"   //!< cwd-relative (matches setup.scm's "Settings.scm"; program runs from data_staged/<env>)
#define SETTINGS_PATH       "Settings.scm"
#endif

// ── Pre-allocation settings ──────────────────────────────────────────────────
/*!
  Read from LAMB_SETTINGS_PATH before any cells exist.
  Default matches the hardcoded initial cell block size.
  Silent if file is missing (first-boot safe).
*/
struct LambPreSettings {
  LL_int32 cell_block_size      = 8 * 1024;   //!< Cells in the initial GC block (8K default; perf pushes 16K to match comps).
  LL_int32 extension_block_size = 4 * 1024;   //!< Cells per on-demand expansion block (4K).
  //! B247: the LIVE SET the GC last measured, persisted across reboots so the heap can come back
  //! already sized for the workload.  NOT a block size -- the initial block is derived from it
  //! (round up to an extension_block_size multiple, then double).  0 = nothing persisted yet.
  //! Deliberately a separate key from cell_block_size: the old code wrote a multi-block TOTAL into
  //! cell_block_size, which means "size of the initial single block", and the units mismatch
  //! bricked boards.  Keeping the quantities in distinct fields makes that error unrepresentable.
  LL_int32 gc_total_marked      = 0;
  LL_int32 ncg_frame_pool_size  = 16;        //!< NcgFrames pre-allocated into the free-list pool at startup.
  void load();               //!< Reads LAMB_SETTINGS_PATH_RAW via fopen; silent if missing.
};

//! @name C++ *try* and *catch* are used to process code faults detected by the LambLisp VM.
//! To cleanly unwind after an error is detected, each *catch* has uniform behavior, which is captured in this macro.
//!@{
#define ll_try try

//! B70: LOG ONCE PER ERROR, NOT ONCE PER FRAME.
//! This macro is on every unwinding frame, so a deep error printed one line PER eval FRAME: a
//! recursion-depth trip at ~40k deep emitted ~20,000 identical lines, which buries the actual error
//! and, on a serial console, takes longer to drain than the computation took to fail.  It is not
//! specific to B70 -- any error raised deep in a recursion did this -- but B70's guard made it
//! routine, because tripping the guard is precisely the deep case.
//! The SAME T_ERROR object propagates up the whole unwind, so frame 2..n are recognisable by
//! pointer: log the first, then one "suppressing" line, then stay quiet.  A genuinely NEW error
//! resets the counter and prints normally, so nothing is lost except the repeats.
//! Pointer-compared as void*, deliberately: this must not resurrect a cell or touch GC state while
//! an exception is in flight.  mk_syserror reuses a singleton, so two DISTINCT errors can share a
//! pointer -- but only when separated by a completed unwind, and the count resets on the next
//! distinct object, so the worst case is one suppressed duplicate line, never a lost first report.
extern const void *ll_catch_last_err;   //!< most recently logged error object (compared, never dereferenced)
extern int         ll_catch_repeats;    //!< frames seen for that same object during this unwind
#define LL_CATCH_REPEAT_LIMIT 4

#define ll_catch(__code_before_rethrow__)				\
  catch (Sexpr_t __err__) {						\
    if (__err__->type() != Cell::T_ERROR)				\
      throw NIL->mk_error("ll_catch() BUG in %s bad type %s", me, __err__->dump().c_str()); \
    									\
    if ((const void *) __err__ != ll_catch_last_err) {			\
      ll_catch_last_err = (const void *) __err__;			\
      ll_catch_repeats  = 0;						\
    }									\
    if (++ll_catch_repeats <= LL_CATCH_REPEAT_LIMIT)			\
      global_printf("\r[%d] %s ll_catch(): %s\n", millis(), me, __err__->error_get_chars()); \
    else if (ll_catch_repeats == LL_CATCH_REPEAT_LIMIT + 1)		\
      global_printf("\r[%d] %s ll_catch(): ... same error still unwinding; further frames suppressed\n", millis(), me); \
									\
    ll_debug_catcher;							\
    __code_before_rethrow__;						\
    throw __err__;							\
  }									\
  //

#define ll_catch_terminal						\
  catch (Sexpr_t __err__) {						\
    if (__err__->type() != Cell::T_ERROR)				\
      global_printf("\r[%d] %s ll_catch_terminal: non-error type %d\n",	\
                    millis(), me, __err__->type());			\
    else								\
      global_printf("\r[%d] %s ll_catch_terminal: %s\n",		\
                    millis(), me, __err__->error_get_chars());		\
    ll_debug_catcher;							\
  }									\
  //
//!@}


#endif
