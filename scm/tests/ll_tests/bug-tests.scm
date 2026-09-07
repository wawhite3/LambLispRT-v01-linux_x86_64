;;; Copyright 2026 by Frobenius Norm LLC 2026-09-05 15:05:00
;;; Free for non-commercial use. Commercial use requires a license.
;;;
;;; bug-tests.scm -- ONE REGRESSION PER FIXED BUG, each named by its B-number.
;;;
;;; WHY THIS FILE EXISTS SEPARATELY FROM THE TOPICAL SUITES.  A bug is fixed in whatever file owns
;;; the feature, and its regression test lands in whatever suite owns that feature -- so the answer
;;; to "is B229 still fixed?" is spread across five files and, for several bugs, was nowhere at all.
;;; A fix with no test is a fix that survives exactly until someone edits near it.  This file is
;;; indexed BY BUG, so the question can actually be asked.
;;;
;;; WHAT BELONGS HERE: a check that REPRODUCES THE ORIGINAL SYMPTOM and fails if it comes back.
;;; Not a feature test -- the feature suites have those.  Each check names the bug and states the
;;; symptom, because "FAIL b262-limit-raises" must tell the next reader what broke without a git
;;; archaeology session.
;;;
;;; WHAT DOES NOT BELONG HERE: CODE BUGS ONLY -- defects in the VM, runtime and Scheme library.
;;; Test-harness, build, release, packaging, documentation and lab-hardware bugs are out of scope:
;;; their symptom is not reachable from inside the interpreter, so a check written here for one
;;; could only be a check that cannot fail, and a test that cannot fail is worse than an absent one
;;; because it reports coverage that does not exist.  See the scope note at the end.
;;;
;;; Run: (load "ll_tests/bug-tests.scm" 0)

;;; RUN UNDER (quiet).  Several checks below DELIBERATELY provoke VM error messages -- that is the
;;; B167/B267 symptom -- and unsuppressed they bury the PASS/FAIL lines in thousands of log lines,
;;; which is the very flood B167 is about.  (quiet) gates the log channel, not `display`, so the
;;; results still print.  It returns the PREVIOUS setting: restore it at the end rather than forcing
;;; verbose, so loading this file does not silently change the session it was loaded into.
(define bug-was-quiet (quiet))

(define *bug-pass* 0)
(define *bug-fail* 0)
(define (bchk name expected got)
  (if (equal? expected got)
      (begin (set! *bug-pass* (+ *bug-pass* 1)) (display "PASS ") (display name) (newline))
      (begin (set! *bug-fail* (+ *bug-fail* 1))
             (display "FAIL ") (display name) (display ": expected ") (write expected)
             (display " got ") (write got) (newline))))

;;; Did evaluating this thing RAISE?  Several of these bugs are "it must refuse, not silently
;;; return something plausible", so raising is the PASS condition and has to be a value we can test.
(define (raised? thunk)
  (guard (e (#t 'raised)) (thunk) 'no-raise))

;;; ---- B244 -- eq? on a NaN disagreed between execution modes -----------------------------------
;;; (let ((x (/ 0. 0.))) (eq? x x)) was #f interpreted and #t compiled.  An interpreted float
;;; lookup materialises a fresh cell, so eq? compared two cells with identical bits; NaN is the
;;; only value not equal to itself, so it is the only one that could expose it.
;;; THE TRAP, recorded in B244 and repeated here because it is easy to re-make: testing with 1.5
;;; returns #t and proves NOTHING -- #t is consistent with both "same cell" and "different cells,
;;; equal values".  The NaN case is the whole test.
(bchk "b244-eq-nan-self"      #t (let ((x (/ 0. 0.))) (eq? x x)))
(bchk "b244-eq-signed-zero"   #f (eq? 0.0 -0.0))
(bchk "b244-eqv-nan-self"     #t (let ((x (/ 0. 0.))) (eqv? x x)))   ;; B240 must stay fixed too
(bchk "b244-eqv-signed-zero"  #f (eqv? 0.0 -0.0))

;;; ---- B262 -- make-string truncated SILENTLY above the 8192-byte string limit -------------------
;;; The 8192 cap is DELIBERATE (fixed cell pool on a microcontroller).  The defect was the
;;; RESPONSE: (make-string 8192 #\a) returned a 41-character string -- the formatter's own overflow
;;; notice returned as data -- and (make-string 16384 #\a) returned 8191 characters, both silently.
;;; A limit enforced by quietly shortening the result is not a limit.
(bchk "b262-under-limit-exact" 8191 (string-length (make-string 8191 #\a)))
(bchk "b262-at-limit-raises"   'raised (raised? (lambda () (make-string 8192 #\a))))
(bchk "b262-over-limit-raises" 'raised (raised? (lambda () (make-string 16384 #\a))))
;;; THE CHECK IS ON BYTES, NOT CHARACTERS.  A multibyte fill multiplies the byte count, so a
;;; character-only test would let this straight back through: 4000 alphas is 8000 bytes and must
;;; still succeed, 5000 is 10000 bytes and must not.
(bchk "b262-multibyte-ok"      4000 (string-length (make-string 4000 (integer->char 945))))
(bchk "b262-multibyte-raises"  'raised (raised? (lambda () (make-string 5000 (integer->char 945)))))

;;; ---- B179 -- syntax-rules custom ellipsis form was unimplemented -------------------------------
;;; (syntax-rules <ellipsis> (lits) rules) raised an INTERNAL error -- Lamb::car() Bad type, with
;;; GC state and raw pointers -- at a user who had written legal R7RS 4.3.2.
(define-syntax bt-q  (syntax-rules ooo () ((_ x ooo) (list x ooo))))
(define-syntax bt-q2 (syntax-rules ::: () ((_ a b :::) (list 'first a 'rest b :::))))
(bchk "b179-custom-ellipsis"       '(1 2 3)              (bt-q 1 2 3))
(bchk "b179-custom-ellipsis-colon" '(first 1 rest 2 3)   (bt-q2 1 2 3))
;;; The DEGENERATE case from R7RS 4.3.2 itself: the chosen ellipsis is also declared a literal, so
;;; `...` must match ITSELF and the macro has no ellipsis at all.
(define-syntax bt-lit (syntax-rules ooo (...) ((_ x) '(x ...))))
(bchk "b179-ellipsis-as-literal"   '(100 ...)            (bt-lit 100))
;;; CONTROL: threading the ellipsis as a parameter must not disturb the default.  If this moves,
;;; every other ellipsis result in this file moves with it and the custom-ellipsis checks above
;;; would be passing for the wrong reason.
(define-syntax bt-std (syntax-rules () ((_ x ...) (list x ...))))
(bchk "b179-standard-unaffected"   '(7 8 9)              (bt-std 7 8 9))

;;; ---- B206 -- the (... ...) ellipsis ESCAPE, i.e. a macro that writes a macro --------------------
;;; B206 is the SCHEME expander's missing escape; the C++ expander (the one running here by
;;; default) has always had it.  This checks the C++ side stayed correct while B179 rewrote it to
;;; thread the ellipsis -- the Scheme side is covered by w3_ai_scripts/sr_ab.sh, which A/Bs the two
;;; implementations over scm/tests/ll_tests/sr-ab-battery.scm and is where a divergence shows up.
(define-syntax bt-blb
  (syntax-rules ()
    ((_ name) (define-syntax name (syntax-rules () ((_ e (... ...)) (begin e (... ...))))))))
(bt-blb bt-seq)
(bchk "b206-escape-macro-writes-macro" 9 (bt-seq 1 2 9))
(define-syntax bt-escpv (syntax-rules () ((_ x) '(... (x ...)))))
(bchk "b206-escape-substitutes-pvar"   '(42 ...) (bt-escpv 42))

;;; ---- B267 -- emsg() handed its expanded buffer to global_printf AS A FORMAT STRING --------------
;;; Any `%` reaching an emergency message was interpreted a second time against arguments that were
;;; never pushed -- a SIGSEGV in strlen inside glibc, five frames from the cause.  Reaching it needs
;;; a message big enough to hit the formatter's cap, which a wide VECTOR does (B167).
;;; THIS TEST PASSES BY NOT CRASHING.  There is no value to assert -- the failure mode is the
;;; PROCESS DYING, so the check is that the raise arrives and we are still here to record it.
;;; It goes through the ERROR path deliberately, not through `write`: formatting a datum INTO A
;;; DIAGNOSTIC is what reaches the capped formatter, and it needs no string-port support.
(bchk "b267-wide-vector-in-error"
      'raised
      (raised? (lambda () (bt-no-such-procedure-zzz (make-vector 4000 'aaaaaaaaaa)))))
(bchk "b267-wide-bytevector-in-error"
      'raised
      (raised? (lambda () (bt-no-such-procedure-zzz (make-bytevector 9000 65)))))

;;; ---- B167 -- error formatter flooded and stalled on deeply nested forms -------------------------
;;; Formatting an error whose offending form nests >= 10 deep emitted a buffer-overflow notice plus
;;; a spurious "possible tail recursion error" -- naming the wrong cause -- and under a load that
;;; produced many such errors the interpreter flooded stderr and did not terminate.  Depth 5 was
;;; clean, depth 10 was not.
;;; The nesting is BUILT, not written as a literal: forty hand-typed parens are unreadable, and a
;;; miscount would change the depth being tested without changing the test's name.
(define (bt-nest n)  (if (= n 0) 1 (list (bt-nest (- n 1)))))
(bchk "b167-depth-10-caught"  'raised (raised? (lambda () (bt-no-such-proc-zzz (bt-nest 10)))))
(bchk "b167-depth-40-caught"  'raised (raised? (lambda () (bt-no-such-proc-zzz (bt-nest 40)))))
(bchk "b167-depth-5-caught"   'raised (raised? (lambda () (bt-no-such-proc-zzz (bt-nest 5)))))
;;; And the interpreter must still be USABLE afterwards -- the B167 symptom that mattered was not
;;; the message, it was the run never reaching the end of the file.
(bchk "b167-still-alive-after" 42 (+ 40 2))

;;; ---- B229 -- sexpr_to_indices scanned the block chain linearly ----------------------------------
;;; Two halves.  The CORRECTNESS half: the linear index assumed every block was cell_block_size, so
;;; with mixed block sizes a cell could be mis-ordered against the sweep cursor and a LIVE cell
;;; freed (B154 class).  The COST half: worst-case GC pause scaled with BLOCK COUNT, 90us at 1
;;; block against 281us at 64, on an identical live set.
;;; WHAT CAN AND CANNOT BE ASSERTED FROM SCHEME: the pause numbers are a MEASUREMENT and belong to
;;; gc-pause-bench on real hardware, not here -- a timing assertion on a shared host is a flake
;;; generator.  What this file can do is force the heap to grow to MANY BLOCKS and then confirm the
;;; collector still returns correct data, which is the half that corrupts memory when it is wrong.
(define (bt-churn n)                       ;; allocate hard enough to force block expansion
  (let loop ((i n) (keep '()))
    (if (= i 0)
        keep
        (loop (- i 1)
              (if (= 0 (modulo i 97))      ;; keep ~1% live, discard the rest -> real sweep work
                  (cons (cons i (number->string i)) keep)
                  keep)))))
(define bt-live (bt-churn 200000))
(bchk "b229-live-set-survives-gc" #t (list? bt-live))
(bchk "b229-live-set-intact"      #t (let loop ((l bt-live))          ;; every kept pair still sane
                                       (cond ((null? l) #t)
                                             ((not (pair? (car l))) #f)
                                             ((not (string? (cdar l))) #f)
                                             ((not (string=? (cdar l) (number->string (caar l)))) #f)
                                             (else (loop (cdr l))))))
(bchk "b229-alloc-still-correct"  '(1 . 2) (cons 1 2))

;;; ---- summary -----------------------------------------------------------------------------------
;;; DECLARED TOTAL, for the reason sr-ab-battery.scm carries one: a check that raises OUTSIDE its
;;; guard removes itself from the run, and a suite that lost a check still prints "0 fail".  That is
;;; the B194/B211 failure -- a harness that cannot report failure.  If the count moves, say so.
(define bug-expected 24)
(display "BUG-TESTS ") (display *bug-pass*) (display " pass, ")
(display *bug-fail*) (display " fail, ")
(display (+ *bug-pass* *bug-fail*)) (display " ran, ") (display bug-expected) (display " declared")
(newline)
(if (not (= (+ *bug-pass* *bug-fail*) bug-expected))
    (begin (display "BUG-TESTS-FAIL checks vanished -- one raised outside its guard, or the declared total is stale")
           (newline)))
(if (> *bug-fail* 0)
    (begin (display "BUG-TESTS-FAIL ") (display *bug-fail*) (display " regression(s) came back") (newline))
    (display "--- bug-tests done ---\n"))
(quiet bug-was-quiet)          ;; restore the caller's logging, whatever it was

;;; ---- SCOPE: CODE BUGS ONLY ---------------------------------------------------------------------
;;; This file covers defects in the VM, runtime and Scheme library -- things a program can observe.
;;; Test-harness, build, release, packaging, documentation and lab-hardware bugs are OUT OF SCOPE by
;;; design, not merely absent: their symptom is not reachable from inside the interpreter, so any
;;; check written here for them could only be a check that cannot fail.  They are verified where
;;; they live -- a harness bug by running the harness, a release bug by cutting a release.
;;;
;;; Two halves of bugs that ARE in scope are verified elsewhere for a technical reason, noted so
;;; nobody records them as untested:
;;;   B206 -- the SCHEME expander half needs rxrs_syntax_rules.scm loaded explicitly (loading it at
;;;           startup exhausts the root stack), so it is A/B'd by w3_ai_scripts/sr_ab.sh.
;;;   B229 -- the PAUSE-SCALING half is a MEASUREMENT (1 block vs 64 on real hardware) and belongs
;;;           to gc-pause-bench; a timing assertion on a shared host is a flake generator.
