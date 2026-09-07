;;; Copyright 2026 by Frobenius Norm LLC 2026-09-06 00:00:00
;;; Free for non-commercial use. Commercial use requires a license.
;;;
;;; idle-heap-tests.scm -- THE HEAP MUST COME BACK.
;;;
;;; A CORRECTNESS TEST, NOT A STRESS TEST.  Every allocation here is small and bounded; nothing
;;; races the collector and nothing measures a rate.  The single question is:
;;;
;;;     after allocating a thing and dropping it, does the memory return?
;;;
;;; and, for the idle loop:
;;;
;;;     does a loop that does NOTHING allocate anything?
;;;
;;; WHY THIS DID NOT EXIST, which is the reason it is worth having.  B288: the default idle `(loop)`
;;; ended each tick with `(set! t_loop (Stopwatch_ms))`, and Stopwatch_ms returns a CLOSURE over a
;;; `let` frame -- so the loop whose entire purpose is donating spare time to the collector handed
;;; it a fresh frame and closure to collect, every tick, forever.  That is WASTE, and worth
;;; removing -- but be clear about what it is not: at ~48-67 cells/tick and 4 Hz it is ~250
;;; cells/second, which does not strain a collector doing incremental work on every cons.  Four
;;; consecutive 100-tick idle windows measured ZERO heap growth and ZERO gc_urgent (2026-09-06).
;;; An earlier claim that idle produced two expansions was a startup artifact sampled just after
;;; boot; see the CORRECTION note on B288/B291 in the registry.
;;; Nothing caught the allocation because (a) `setup.scm` skips the loop definition entirely on Linux
;;; -- `(if linux? #f (define loop ...))` -- so the cheapest and most frequent tests never ran the
;;; code at all, and (b) no tier asserts anything about the heap at rest.  conform tests meaning,
;;; gcpause tests pause DURATION under load, perf tests throughput.  A slow leak at 4 Hz shows up
;;; in none of them.
;;;
;;; WHAT IT ASSERTS AGAINST, and why two counters rather than one:
;;;   (Platform.gc-diag) index 13 = free cells, 14 = total cells  -- the CELL pool.
;;;   (Platform.sysmem-inuse)                                     -- the SYSTEM allocator.
;;; They answer different questions.  A bytevector keeps its handle in a cell and its bytes in a
;;; malloc'd block, so a backing block that is never freed when its cell is collected is a leak
;;; that every cell-based counter calls clean.  Both must return.
;;;
;;; RETURN TO BASELINE, NOT MAGNITUDE.  Measured 2026-09-06: a 4,000,000-byte bytevector moves
;;; `sysmem-inuse` by 256 bytes, because glibc serves a block that large with mmap and mallinfo2's
;;; uordblks does not count mmap'd space.  So the SIZE of an allocation is not observable here and
;;; no assertion depends on it -- only that the number comes back to where it started.  That is
;;; exactly what a correctness test wants and it is robust to the allocator's internal strategy.
;;;
;;; -1 FROM sysmem-inuse MEANS UNKNOWN, NEVER ZERO.  mallinfo2 is glibc-only, so wasm, MinGW and
;;; the ESP32 cannot answer.  Those targets SKIP the system-heap checks and say so; they must never
;;; silently report a pass for a question the build cannot ask.
;;;
;;; Run: (load "ll_tests/idle-heap-tests.scm" 0)

;;; Under (quiet): the collect calls below emit gc_pass lines, and unsuppressed they bury the
;;; results.  Returns the previous setting so loading this file does not change the session.
(define idle-was-quiet (quiet))

(define *idle-pass* 0)
(define *idle-fail* 0)
(define *idle-skip* 0)

(define (ichk name ok detail)
  (if ok
      (begin (set! *idle-pass* (+ *idle-pass* 1)) (display "PASS ") (display name) (newline))
      (begin (set! *idle-fail* (+ *idle-fail* 1))
             (display "FAIL ") (display name) (display ": ") (display detail) (newline))))

(define (iskip name why)
  (set! *idle-skip* (+ *idle-skip* 1))
  (display "SKIP ") (display name) (display " -- ") (display why) (newline))

;;; ---- observables -------------------------------------------------------------------------

(define (heap-free)   (list-ref (Platform.gc-diag) 13))
(define (heap-total)  (list-ref (Platform.gc-diag) 14))
(define (heap-urgent) (list-ref (Platform.gc-diag) 9))
(define (sysmem)      (Platform.sysmem-inuse))
(define sysmem-known? (>= (Platform.sysmem-inuse) 0))

;;; SETTLE, THEN MEASURE.  One (gc!) is not enough: the first collect frees what the SETUP of the
;;; measurement itself allocated, and the second is the one whose result is stable.  Without this
;;; the baseline drifts and every check becomes a coin toss -- the failure mode that makes a
;;; flaky test worse than no test.
(define (settle) (gc!) (gc!) (gc!))

;;; TOLERANCES.  Deliberately generous, because this test must not fail for being alive: reading
;;; the diagnostics allocates a list, `display` allocates, and the malloc arena moves under its own
;;; bookkeeping.  A LEAK is unbounded and repeated `reps` times below, so it clears these by orders
;;; of magnitude; the tolerance only has to exclude measurement noise, not to be tight.
(define cell-tolerance 400)
(define sys-tolerance  65536)

;;; Run `thunk` `reps` times, dropping each result, and report whether both heaps came back.
;;; The result is deliberately stored into a variable and then overwritten, so the value is
;;; genuinely dead rather than merely unreferenced-by-luck.
;;;
;;; THE WARMUP PASS IS NOT OPTIONAL, and leaving it out produced a FALSE ACCUSATION on this file's
;;; first run.  glibc grows its arena on demand and does NOT return freed memory to the OS, so the
;;; first allocator to run heavily absorbs that one-time growth and looks like the leaker.
;;; Measured 2026-09-06: `list->vector` was reported as leaking 5,997,520 bytes, and in isolation
;;; it grows the arena by 6,304 -- while `iota`, which passed in that same run, showed 1,474,720
;;; when IT went first.  The number was real; the attribution was noise.
;;;
;;; So: run the whole workload once to settle the arena, then measure the SECOND identical pass.
;;; A genuine leak grows on both passes and still fails; arena warmup grows only on the first and
;;; no longer accuses anyone.  This is why the test measures a REPEAT rather than a first use.
(define (round-trip name reps thunk)
  ;;; TWO warmup passes, not one -- measured, not guessed.  Running `(iota 256)` 200 times per
  ;;; pass and watching the system heap: pass1 +294,960 bytes, pass2 +1,573,120, pass3 0, pass4 0.
  ;;; glibc's arena keeps growing into the SECOND pass and only then reaches steady state, so a
  ;;; single warmup left `iota/sysmem` failing with 3.2 MB of growth that was not a leak at all.
  ;;; If a future allocator needs more, raise this -- but confirm it CONVERGES first (a real leak
  ;;; grows on every pass and must not be warmed away).
  (let ((warm 0))
    (let pass ((p 0))
      (if (< p 2)
          (begin (let loop ((i 0))
                   (if (< i reps) (begin (set! warm (thunk)) (set! warm 0) (loop (+ i 1)))))
                 (settle)
                 (pass (+ p 1))))))
  (settle)
  (let ((c0 (heap-free))
        (s0 (sysmem))
        (u0 (heap-urgent))
        (sink 0))
    (let loop ((i 0))
      (if (< i reps)
          (begin (set! sink (thunk))
                 (set! sink 0)
                 (loop (+ i 1)))))
    (settle)
    (let* ((c1 (heap-free))
           (s1 (sysmem))
           (u1 (heap-urgent))
           (cell-lost (- c0 c1))
           (sys-lost  (- s1 s0)))
      (ichk (string-append name "/cells")
            (<= cell-lost cell-tolerance)
            (string-append "lost " (number->string cell-lost) " cells over "
                           (number->string reps) " rounds (tolerance "
                           (number->string cell-tolerance) ")"))
      ;;; CONVERGENCE, NOT A FIXED CEILING -- because the two are what actually distinguish a leak
      ;;; from an allocator arena.  glibc grows its arena on demand and never returns it, and in a
      ;;; MIXED workload it keeps shifting for several passes: with two warmups `iota` still showed
      ;;; 196,640 bytes, having shown 3,244,560 with one.  Chasing that with more warmups or a
      ;;; bigger tolerance only moves the number; a leak of 200 rounds could hide under either.
      ;;;
      ;;; The property that separates them: ARENA GROWTH CONVERGES, A LEAK DOES NOT.  Measured on
      ;;; `(iota 256)` x200 per pass -- pass1 +294,960, pass2 +1,573,120, pass3 0, pass4 0.  So run
      ;;; the workload twice more and require the SECOND of those to be small: a leak allocates the
      ;;; same amount every pass and fails, while an arena that has settled reports ~0 however much
      ;;; it grew getting there.
      (if sysmem-known?
          (let ((again 0))
            (let ((s2 (sysmem)) (sink2 0))
              (let loop ((i 0))
                (if (< i reps)
                    (begin (set! sink2 (thunk)) (set! sink2 0) (loop (+ i 1)))))
              (settle)
              (set! again (- (sysmem) s2)))
            (ichk (string-append name "/sysmem")
                  (<= again sys-tolerance)
                  (string-append "system heap STILL grew " (number->string again)
                                 " bytes on a settled repeat pass of " (number->string reps)
                                 " rounds (first pass grew " (number->string sys-lost)
                                 "; a settled arena reports ~0, a leak repeats) -- tolerance "
                                 (number->string sys-tolerance))))
          (iskip (string-append name "/sysmem") "no mallinfo2 on this build"))
      ;; A bounded, dropped allocation must never need the SYNCHRONOUS catch-up collector.  If it
      ;; does, the mutator outran the incremental collector on a workload this small, which is a
      ;; pacing defect even when nothing leaked.
      (ichk (string-append name "/no-urgent")
            (= u0 u1)
            (string-append "gc_urgent fired " (number->string (- u1 u0)) " time(s)")))))

;;; ---- the allocators ----------------------------------------------------------------------
;;; Each keeps its payload OUTSIDE the cell that names it, which is the case a cell-counter alone
;;; cannot check.  Sizes are small on purpose: this is a correctness test, and a leak repeated
;;; `reps` times is just as visible at 1 KB as at 1 MB.

(round-trip "vector"      200 (lambda () (make-vector 256 0)))
(round-trip "bytevector"  200 (lambda () (make-bytevector 4096 0)))
(round-trip "string"      200 (lambda () (make-string 1024 (integer->char 120))))
(round-trip "iota"        200 (lambda () (iota 256)))
(round-trip "list->vector" 200 (lambda () (list->vector (iota 128))))
;;; open-output-string / get-output-string, NOT call-with-output-string -- the latter is not bound
;;; in this runtime, and on the first run of this file that check RAISED and vanished from the
;;; output entirely.  A check that disappears reports no coverage while looking like none was
;;; wanted, which is worse than one that fails; if a procedure used here is ever unbound the same
;;; thing will happen again, so keep these to names the C++ installer tables actually register.
(round-trip "string-port" 100 (lambda ()
                                (let ((p (open-output-string)))
                                  (write (iota 64) p)
                                  (get-output-string p))))
(round-trip "cons-chain"  200 (lambda ()
                                (let build ((n 128) (acc (list)))
                                  (if (= n 0) acc (build (- n 1) (cons n acc))))))

;;; ---- the idle loop (B288) ----------------------------------------------------------------
;;; THE DIRECT REGRESSION FOR B288.  The default `(loop)` is defined only where `linux?` is false
;;; (setup.scm), i.e. on the ESP32 targets and wasm -- which is exactly why the per-tick allocation
;;; went unseen: it does not exist on the hosts where testing is cheap.  Where it IS defined, a
;;; tick must cost NOTHING: the loop's whole job is to hand spare time to the collector, so a loop
;;; that allocates is working against its own purpose.
;;; `interaction-environment`, NOT `r5-interaction-environment` -- the latter is the C++ member
;;; name and is not bound in Scheme.  Using it made this whole block RAISE, so the idle-loop check
;;; produced neither a PASS nor a SKIP and simply was not in the output: 21 pass / 0 fail / 0 skip,
;;; with the one check this file exists for silently missing.  That is the vanishing-check failure
;;; warned about above, committed in the same file that warns about it.  The totals are the guard:
;;; if pass+fail+skip does not account for every check, something disappeared.
(if (dict-ref? (interaction-environment) 'loop)
    (begin
      ;;; DO NOT COLLECT BETWEEN THE MEASUREMENTS.  B288 is allocation CHURN, not a leak: the
      ;;; per-tick closure is garbage and the collector reclaims it, so "did the heap come back
      ;;; after gc!" is a question the bug always answers correctly.  The first version of this
      ;;; check did exactly that and PASSED against the reverted, still-buggy loop -- a test that
      ;;; cannot fail, which is worse than no test because it reports coverage that is not there.
      ;;;
      ;;; Churn is only visible as CONSUMPTION: count free cells, tick, count again, with NO
      ;;; collection in between.  A loop that allocates draws the free list down; one that does not
      ;;; leaves it where it was.
      ;;;
      ;;; TICK COUNT IS BOUNDED BY THE FREE LIST ON PURPOSE.  If the ticks allocate enough to
      ;;; trigger a collection, that collection refills the free list and hides the very thing
      ;;; being measured.  200 ticks costs ~400 cells against a free list in the thousands, so the
      ;;; window stays open -- and 200 is still 50 seconds of real 4 Hz idle in the browser.
      ;;; TWO CALIBRATIONS ARE REQUIRED, and leaving either out makes this check lie.
      ;;;
      ;;; 1. SUPPRESS THE LOOP'S OWN COLLECTION.  The idle loop's job is
      ;;;    `(when (> spare-us 0) (Platform.gc-idle-task! ...))`, so it FREES cells in the same
      ;;;    tick it allocates them and a raw free-cell delta reports the collector's work instead
      ;;;    of the allocation.  Measured while writing this: the FIXED loop appeared to consume
      ;;;    17,684 cells and the BUGGY one 909 -- the reverse of the truth, decided by whether a
      ;;;    collection happened to fire in that window.  `loop-deadline-ms` at 0 makes spare-us
      ;;;    non-positive so the donation is skipped; restored immediately.
      ;;;
      ;;; 2. SUBTRACT THE HARNESS.  Calling ANY thunk 200 times costs cells -- measured at 13 per
      ;;;    call on wasm32 -- so an uncalibrated number is mostly this file measuring itself.
      ;;;    The budget below is per-tick cost ABOVE an empty lambda.
      ;;;
      ;;; WHAT THIS CATCHES, stated honestly because the number matters: a loop that starts
      ;;; allocating substantially more than it does today.  Measured 2026-09-06 on wasm32, the
      ;;; loop costs ~48 cells/tick over baseline, of which B288's discarded Stopwatch closure was
      ;;; ~9.  So a budget set here would NOT have caught B288 itself -- 9 cells is inside any
      ;;; sane tolerance.  It guards the order of magnitude, not the increment.  Driving the idle
      ;;; loop to genuinely zero allocation (the `let*` frame and the boxed arithmetic are the
      ;;; remainder) is open work; when that lands, lower this budget to match and it becomes a
      ;;; real ratchet.
      (settle)
      (let ((saved-deadline loop-deadline-ms)
            (ticks 200)
            (base 0) (cost 0) (u0 0))
        (set! loop-deadline-ms 0)
        ;; baseline: the cost of calling an empty thunk `ticks` times, through this same shape
        (settle)
        (let ((b0 (heap-free)))
          (let tick ((i 0)) (if (< i ticks) (begin ((lambda () #t)) (tick (+ i 1)))))
          (set! base (- b0 (heap-free))))
        (settle)
        (set! u0 (heap-urgent))
        (let ((c0 (heap-free)))
          (let tick ((i 0)) (if (< i ticks) (begin (loop) (tick (+ i 1)))))
          (set! cost (- c0 (heap-free))))
        (set! loop-deadline-ms saved-deadline)
        (let ((per-tick (quotient (- cost base) ticks)))
          (display "     idle loop costs ") (display per-tick)
          (display " cells/tick above an empty call (budget 90)") (newline)
          (ichk "idle-loop/allocation-budget"
                (<= per-tick 90)
                (string-append "the idle loop allocates " (number->string per-tick)
                               " cells per tick with no work to do -- budget is 90 (B288). "
                               "An idle loop exists to give the collector time, not garbage."))
          (settle)
          (ichk "idle-loop/no-urgent"
                (= u0 (heap-urgent))
                "an IDLE loop drove the collector into synchronous catch-up"))))
    (iskip "idle-loop" "no (loop) on this target -- linux? is true, see setup.scm"))

;;; ---- report ------------------------------------------------------------------------------

(display "idle-heap-tests: ")
(display *idle-pass*) (display " pass, ")
(display *idle-fail*) (display " fail, ")
(display *idle-skip*) (display " skip")
(newline)

(quiet idle-was-quiet)
