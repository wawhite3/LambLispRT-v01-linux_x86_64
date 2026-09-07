;;; Copyright 2026 by Frobenius Norm LLC 2026-09-05 16:05:00
;;; Free for non-commercial use. Commercial use requires a license.
;;; proposal-tests.scm -- does the RUNTIME actually contain what PROPOSALS.md says it does?
;;;
;;; WHAT THIS IS FOR, AND WHY IT IS NOT JUST ANOTHER FEATURE SUITE.
;;; `w3_ai_exch/PROPOSALS.md` calls itself the authoritative status table, and every row makes a
;;; CLAIM ABOUT STATE: done, open, partial.  Nothing checked those claims against the running
;;; system.  They have been wrong in both directions, repeatedly and expensively:
;;;
;;;   * P174 Phase 0 read `OPEN` while the code (Word_t -> uintptr_t + the static_assert) had been
;;;     in master since e4c542d.
;;;   * B244 read `(OPEN)` for a day after af6d0f9 fixed it, and a session was within minutes of
;;;     implementing the fix a second time by a different mechanism.
;;;   * B95 read `FIXED` for 17 days while its commit sat on a branch, so nobody merged it.
;;;
;;; The lesson CLAUDE.md draws is the one this file mechanises: **a wrong record does not merely
;;; misinform, it removes the reason to act.**  `verify_truth.sh` audits the records against GIT.
;;; This audits them against the RUNNING VM, which is the thing git cannot tell you.
;;;
;;; SO THE INTERESTING OUTPUT IS THE DISAGREEMENTS, NOT THE PASSES:
;;;   REGRESSION   -- claimed `done`, feature is ABSENT or misbehaves.  Something broke, or the
;;;                   proposal was never really finished.
;;;   STALE-RECORD -- claimed `open`, feature is PRESENT.  The work is done and the row still
;;;                   invites someone to do it again.  This is the expensive one.
;;;
;;; SCOPE: CODE proposals with a Scheme-visible surface.  Deliberately NOT covered:
;;;   * doc / business / event / application proposals (P116, P118, P154, ...) -- nothing to probe;
;;;   * test-harness and build-tooling proposals (P13, P42, P44, P46b, P52, P84, P96, P99, P128,
;;;     P137, P166) -- they are about how tests RUN, so a test asserting them is circular;
;;;   * pure-internal refactors with no observable surface (P25, P27, P28, P29, P30, P32, P35,
;;;     P111 PIMPL, P152) -- absence of a symbol proves nothing about them.
;;; A proposal in none of those groups and missing here is an omission worth fixing.
;;;
;;; USAGE:  (load "ll_tests/proposal-tests.scm" 0)
;;; The `claimed` argument of each probe MUST be kept in step with PROPOSALS.md by hand -- that is
;;; the point: the two records are independent, and this file is where they are compared.

;;; -----------------------------------------------------------------------
;;; Harness
;;; -----------------------------------------------------------------------
(define *pt-ok*        0)   ;;;!< record and reality agree
(define *pt-regress*   0)   ;;;!< claimed done, absent  -- ACT
(define *pt-stale*     0)   ;;;!< claimed open, present -- ACT (update the record)
(define *pt-partial*   0)   ;;;!< claimed partial/board-only; reported, never counted as failure
(define *pt-notes*   (list))

;;; A probe returns #t iff the feature is PRESENT AND BEHAVES.  Any exception counts as absent:
;;; an unbound symbol and a broken implementation are both "not implemented" for this purpose, and
;;; distinguishing them is the job of the suite that owns the feature, not of this audit.
(define (pt-live? thunk)
  (guard (e (#t #f))
    (if (thunk) #t #f)))

(define (pt-report id claimed name present)
  (cond
    ((eq? claimed 'partial)
     (set! *pt-partial* (+ *pt-partial* 1))
     (display "  PARTIAL      ") (display id) (display "  ") (display name)
     (display (if present "  [present]" "  [absent]")) (newline))
    ((and (eq? claimed 'done) present)
     (set! *pt-ok* (+ *pt-ok* 1)))
    ((and (eq? claimed 'open) (not present))
     (set! *pt-ok* (+ *pt-ok* 1)))
    ((and (eq? claimed 'done) (not present))
     (set! *pt-regress* (+ *pt-regress* 1))
     (set! *pt-notes* (cons (list 'REGRESSION id name) *pt-notes*))
     (display "  REGRESSION   ") (display id) (display "  claimed done, ABSENT: ")
     (display name) (newline))
    (else
     (set! *pt-stale* (+ *pt-stale* 1))
     (set! *pt-notes* (cons (list 'STALE-RECORD id name) *pt-notes*))
     (display "  STALE-RECORD ") (display id) (display "  claimed open, PRESENT: ")
     (display name) (newline))))

(define (probe id claimed name thunk)
  (pt-report id claimed name (pt-live? thunk)))

(define (section title)
  (newline) (display ";;; ") (display title) (newline))

(display "\n=== proposal-tests.scm -- PROPOSALS.md claims vs the running VM ===\n")

;;; -----------------------------------------------------------------------
(section "Numeric tower")
;;; -----------------------------------------------------------------------
(probe "P2"   'done "numeric scalars: exact/inexact are distinct"
       (lambda () (and (exact? 1) (inexact? 1.5) (inexact? (exact->inexact 1)))))
(probe "P23"  'done "embedded immediates survive car/cdr"
       (lambda () (and (= (car (cons 5 6)) 5) (= (cdr (cons 5 6)) 6)
                       (= (car (cons 1.5 2.5)) 1.5))))
(probe "P54"  'done "bignum: arbitrary-precision integers"
       (lambda () (and (= (expt 2 100) 1267650600228229401496703205376)
                       (> (expt 2 200) (expt 2 199)))))
(probe "P70"  'done "rationals built on bignum components"
       (lambda () (= (numerator (/ (expt 2 100) 3)) (expt 2 100))))
(probe "P9"   'done "R7RS primitives: exact-integer-sqrt / floor/ / truncate/"
       (lambda () (and (procedure? exact-integer-sqrt) (procedure? floor/) (procedure? truncate/))))
(probe "P9b"  'done "R7RS primitives: list-copy / string-map / vector-map"
       (lambda () (and (equal? (list-copy '(1 2 3)) '(1 2 3))
                       (equal? (vector-map (lambda (x) (* 2 x)) (vector 1 2)) (vector 2 4)))))

;;; -----------------------------------------------------------------------
(section "Reader, strings, vectors")
;;; -----------------------------------------------------------------------
;;; P61 is CHARACTER-indexed, and P100 (byte-indexed string ops) was WITHDRAWN as a deliberate
;;; embedded design decision -- so string-length counting CHARACTERS is the P61 claim under test.
(probe "P61"  'done "UTF-8: multi-byte chars read and count as one character"
       (lambda () (and (= (string-length "\x3b1;\x3b2;\x3b3;") 3)
                       (= (char->integer #\x3bb) 955))))
(probe "P3"   'done "typed vectors / bytevectors with native accessors"
       (lambda () (let ((b (make-bytevector 8 0)))
                    (bytevector-s32-native-set! b 0 -12345)
                    (= (bytevector-s32-native-ref b 0) -12345))))
(probe "P138" 'done "datum-label writer: `write` of a CYCLIC list terminates"
       (lambda () (let ((x (list 1 2)) (p (open-output-string)))
                    (set-cdr! (cdr x) x)          ;; genuinely circular
                    (write x p)
                    (> (string-length (get-output-string p)) 0))))

;;; -----------------------------------------------------------------------
(section "Macros and syntax")
;;; -----------------------------------------------------------------------
(probe "P141" 'done "syntax-rules expander (macro-based, the one that shipped)"
       (lambda () (begin (eval '(define-syntax pt-sw
                                  (syntax-rules () ((_ a b) (list b a)))))
                         (equal? (eval '(pt-sw 1 2)) '(2 1)))))
(probe "P206" 'done "syntax-rules: escaped ellipsis (... ...)"
       (lambda () (begin (eval '(define-syntax pt-esc
                                  (syntax-rules () ((_) (quote (... ...))))))
                         (eq? (eval '(pt-esc)) '...))))

;;; -----------------------------------------------------------------------
(section "Exceptions and continuations  (P168)")
;;; -----------------------------------------------------------------------
;;; The four groups P168 closed in pure Scheme.  Each is a BEHAVIOUR check, not a boundness check:
;;; before P168 every one of these symbols was BOUND -- to a stub that raised "not yet implemented"
;;; -- so `(procedure? dynamic-wind)` would have answered #t while nothing worked.  That is exactly
;;; the trap this file exists to avoid, and it is why every probe here computes a value.
(probe "P168a" 'done "raise-continuable resumes the raise point (handler value + pending work)"
       (lambda () (= 65 (with-exception-handler
                          (lambda (con) (if (string? con) 42 77))
                          (lambda () (+ (raise-continuable "should be a number") 23))))))
(probe "P168b" 'done "dynamic-wind runs before/after around a normal exit"
       (lambda () (begin (define pt-dw-log '())   ;; GLOBAL on purpose: B205 breaks the local idiom
                         (dynamic-wind
                           (lambda () (set! pt-dw-log (cons 'before pt-dw-log)))
                           (lambda () 'body)
                           (lambda () (set! pt-dw-log (cons 'after pt-dw-log))))
                         (equal? pt-dw-log '(after before)))))
(probe "P168c" 'done "dynamic-wind runs `after` on the ERROR unwind too"
       (lambda () (begin (define pt-dw2 #f)
                         (guard (e (#t #t))
                           (dynamic-wind (lambda () #t)
                                         (lambda () (raise 'boom))
                                         (lambda () (set! pt-dw2 #t))))
                         pt-dw2)))
(probe "P168d" 'done "call/cc ESCAPE works (escape-only is the shipped semantics)"
       (lambda () (= 41 (+ 1 (call/cc (lambda (k) (k 40) 999))))))
(probe "P168e" 'done "make-parameter / parameterize save, set and restore"
       (lambda () (let ((p (make-parameter 10)))
                    (and (= (p) 10)
                         (= (parameterize ((p 20)) (p)) 20)
                         (= (p) 10)))))
(probe "P168f" 'done "parameterize converter runs on ENTRY but not on RESTORE"
       (lambda () (let ((p (make-parameter 5 (lambda (x) (* 2 x)))))
                    (and (= (p) 10)                            ;; converted at construction
                         (= (parameterize ((p 5)) (p)) 10)     ;; converted on entry
                         (= (p) 10)))))                        ;; NOT converted again on restore
;;; The limit, asserted so it cannot regress into a false claim of full call/cc:
(probe "P12"  'open "RE-ENTRANT call/cc (invoke k after its extent exited) -- must NOT work"
       (lambda () (let ((k #f))
                    (call/cc (lambda (c) (set! k c) 1))
                    (guard (e (#t #f))       ;; raising here is the CORRECT outcome
                      (k 2)
                      #t))))

;;; -----------------------------------------------------------------------
(section "Compilation tiers")
;;; -----------------------------------------------------------------------
(probe "P1"   'done "bytecode compiler: procedure->bytecode produces an applicable object"
       (lambda () (begin (define (pt-bc v) (* v 2))
                         (= ((procedure->bytecode pt-bc) 21) 42))))
(probe "P266" 'open "procedure? recognises a bytecode-compiled procedure  (B266: it does NOT)"
       (lambda () (begin (define (pt-bc2 v) v)
                         (procedure? (procedure->bytecode pt-bc2)))))
;;; NCG'S INPUT LANGUAGE IS BYTECODE, NOT THE AST -- and getting this wrong is SILENT.
;;;   (ncg-compile f)                      on an AST procedure -> returns it UNCOMPILED,
;;;                                        ncg-compiled? #f, ncg-code-size #f, no error, no warning
;;;   (ncg-compile (procedure->bytecode f)) -> ncg-compiled? #t, ncg-code-size 264
;;; The first form looks like it worked: it returns a procedure, and calling it gives the right
;;; answer -- because it is still the interpreted one.  This probe was written the wrong way first
;;; and reported P129 as a REGRESSION on a tree where NCG was perfectly healthy, which is the exact
;;; false alarm this file must not produce.  Compile through bytecode.
(probe "P129" 'done "NCG: bytecode -> native, ncg-compiled? true and answers correctly"
       (lambda () (begin (define (pt-ncg n) (+ n 1))
                         (define pt-ncg-n (ncg-compile (procedure->bytecode pt-ncg)))
                         (and (ncg-compiled? pt-ncg-n)
                              (= (pt-ncg-n 41) 42)
                              (> (ncg-code-size pt-ncg-n) 0)))))
(probe "P165" 'done "NCG bytecode fusion entry point (ncg-fuse!) present"
       (lambda () (procedure? ncg-fuse!)))
(probe "P80"  'open "AST backing store: bytecode->procedure / bytecode-drop-ast!"
       (lambda () (and (procedure? (eval 'bytecode->procedure))
                       (procedure? (eval 'bytecode-drop-ast!)))))
(probe "P37+P41" 'open "runtime metrics: lamb-heap-stats / cell-type-stats / ncg-inventory"
       (lambda () (and (procedure? (eval 'lamb-heap-stats))
                       (procedure? (eval 'cell-type-stats))
                       (procedure? (eval 'ncg-inventory)))))

;;; -----------------------------------------------------------------------
(section "GC and integrity")
;;; -----------------------------------------------------------------------
(probe "P56"  'done "integrity_check: heap validator runs and reports clean"
       (lambda () (lamb-integrity-check)))
(probe "P96"  'done "GC worst-case pause benchmark entry point present"
       (lambda () (procedure? gc-pause-bench)))
;;; P169 is MEASUREMENT-GATED and NOT implemented: `float-alloc-stats` answers #f in every normal
;;; build and a LIST only in linux_x86_64_floatprof.  So "present" here means the CACHE shipped,
;;; which it has not -- and must not be inferred from the probe existing.
(probe "P169" 'open "float singleton cache installed (measurement env aside)"
       (lambda () (pair? (float-alloc-stats))))

;;; -----------------------------------------------------------------------
(section "Libraries and bindings")
;;; -----------------------------------------------------------------------
(probe "P105" 'done "JSON reader/writer round-trip"
       (lambda () (equal? (json-read (json-write (list 1 2 3))) (list 1 2 3))))
(probe "P140" 'done "CSV library present"
       (lambda () (and (procedure? csv-read) (procedure? csv-write))))
(probe "P108" 'done "Modbus bindings present"
       (lambda () (procedure? modbus-tcp-connect)))
(probe "P145" 'done "PROFIBUS/PROFINET bindings present"
       (lambda () (procedure? profibus-open)))
(probe "P106/P164" 'open "HTTPS client bindings (https-get)"
       (lambda () (procedure? (eval 'https-get))))
(probe "P163" 'open "socket bindings (socket-open)"
       (lambda () (procedure? (eval 'socket-open))))
(probe "P107" 'open "libplctag bindings (plctag-create)"
       (lambda () (procedure? (eval 'plctag-create))))
(probe "P109" 'open "BACnet bindings (bacnet-open)"
       (lambda () (procedure? (eval 'bacnet-open))))
(probe "P66"  'open "Bluetooth LE bindings (ble-init)"
       (lambda () (procedure? (eval 'ble-init))))

;;; -----------------------------------------------------------------------
(section "Board-dependent -- reported, never counted as failure on a host run")
;;; -----------------------------------------------------------------------
;;; These need real hardware, so on linux they are absent for a reason that says nothing about the
;;; proposal.  Reported so a device run can read them, and so their absence here is never mistaken
;;; for a regression.
(probe "P132" 'partial "LLIP tunnel entry points (device-side)"
       (lambda () (procedure? (eval 'llip-send))))
(probe "P136" 'partial "camera support (S3-EYE only)"
       (lambda () (procedure? (eval 'camera-init))))
(probe "P149" 'partial "ESP32 RSA/MPI accelerator (ESP32 only; host uses software bignum)"
       (lambda () (= (expt 2 100) 1267650600228229401496703205376)))

;;; -----------------------------------------------------------------------
;;; Summary
;;; -----------------------------------------------------------------------
(newline)
(display "=== PROPOSAL AUDIT SUMMARY ===") (newline)
(display "  agree with PROPOSALS.md : ") (display *pt-ok*)      (newline)
(display "  REGRESSIONS             : ") (display *pt-regress*) (newline)
(display "  STALE RECORDS           : ") (display *pt-stale*)   (newline)
(display "  partial / board-only    : ") (display *pt-partial*) (newline)
(if (null? *pt-notes*)
    (display "  no disagreements -- every claim checked matches the runtime\n")
    (begin
      (display "\n  ACT ON THESE -- the record and the runtime disagree:\n")
      (for-each (lambda (n)
                  (display "    ") (display (car n)) (display " ")
                  (display (cadr n)) (display " -- ") (display (caddr n)) (newline))
                (reverse *pt-notes*))))
(display "proposal-tests: ")
(display (+ *pt-ok* *pt-regress* *pt-stale* *pt-partial*))
(display " probes, ")
(display (+ *pt-regress* *pt-stale*))
(display " disagreements\n")
(display "--- proposal-tests done ---\n")
