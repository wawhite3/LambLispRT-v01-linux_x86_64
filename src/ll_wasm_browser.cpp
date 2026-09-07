// Copyright 2026 by Frobenius Norm LLC 2026-09-04 00:00:00
// Free for non-commercial use. Commercial use requires a license.
/*! @file ll_wasm_browser.cpp
  P175 Phase 2 -- the browser entry points.

  A browser page cannot call main() and let it sit in `while (true) loop();`: that would peg the
  tab's only thread and never return to the event loop, so nothing would ever be drawn and no
  keystroke would ever be delivered.  The env therefore links with -sINVOKE_RUN=0 (main is never
  called) and JavaScript drives the VM one step at a time through the three functions below.

  These are the ONLY wasm-specific entry points in the tree.  Everything they call --
  ::setup(), ::loop(), LL_Term -- is the same code every other target runs; see
  LL_Term::push_line() for why the input path is inverted rather than made asynchronous.
*/
#if LL_EMSCRIPTEN

#include <emscripten.h>
#include "LambLisp.h"
#include "ll_vm_term.h"

//! Defined in main.cpp.  Declared rather than #included because main.cpp has no header.
void setup();
void loop();

extern "C" {

/*! Boot the VM: construct Lamb, install the native operators, load setup.scm.

  CALL THIS EXACTLY ONCE, and expect it to take a moment: it loads the whole staged `.scm` library
  from the preloaded MEMFS image.  It is a straight call to ::setup(), so a failure reports itself
  through the ordinary log path rather than through a return code -- the page should show the
  terminal output, not a status number.
*/
EMSCRIPTEN_KEEPALIVE void ll_wasm_boot(void)
{
  setup();
  //! QUIET BY DEFAULT IN THE BROWSER, and only in the browser.
  //!
  //! This page is a CUSTOMER-FACING DEMO, and the VM's diagnostics are written for us, not for a
  //! visitor: `LambMemoryManager::gc_pass() (Ngc 9) (Amax 11017) (Yuasa_MN 41 11094)` and a
  //! `Lamb::loop() Input:` echo of every line.  On a real workload that is hundreds of lines --
  //! measured 2026-09-06 in headless Chrome: one `(churn 40000 ...)` produced 336 gc_pass lines
  //! and 22 expand() reports.  Three consequences, all of which read as a broken product:
  //!   * the visitor's own answer is pushed out of the viewport by log lines;
  //!   * the flood LOOKS like the runtime struggling, when the same expression on linux_x86_64
  //!     produces MORE expansions (27 vs 22) and is simply how this allocation pattern paces;
  //!   * it advertises internals a customer has no use for.
  //!
  //! `(verbose)` turns it back on from the REPL, so nothing is hidden from anyone who wants it --
  //! the page's footer says so.
  //!
  //! NOT DONE FOR wasm32_wasi: that build is what `w3 test conform LL wasm` drives, and the
  //! harness PARSES this output.  Silencing it there would break the tier, so this lives in the
  //! Emscripten-only entry point rather than in setup().
  ll_term.quiet = true;
}

/*! Evaluate one line of REPL input.

  `line` is UTF-8, WITHOUT a trailing newline; JS is responsible for line editing, because the
  browser terminal (xterm.js) already does it far better than a raw-mode line editor could.
  Output arrives on stdout/stderr, which Emscripten routes to Module.print / Module.printErr.
*/
EMSCRIPTEN_KEEPALIVE void ll_wasm_eval(const char *line)
{
  ll_term.push_line(line ? line : "");
  loop();
}

/*! Run one idle iteration of the VM with NO input.

  The page should call this on an interval (or from requestAnimationFrame).  It is not
  decoration: Lamb::loop() is where the Scheme-level `(loop)` procedure runs, where the deferred
  GC status line is emitted, and where the incremental collector gets its idle quantum.  A page
  that only ever calls ll_wasm_eval() does all its collection inside the user's keystrokes.
*/
EMSCRIPTEN_KEEPALIVE void ll_wasm_tick(void)
{
  loop();
}

}  // extern "C"

#endif // LL_EMSCRIPTEN
