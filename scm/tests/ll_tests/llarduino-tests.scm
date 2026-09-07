;;; =======================================================================
;;; llarduino-tests.scm -- conformance tests for LLArduino (P176)
;;;
;;; LLArduino is LambLisp's OWN implementation of the Arduino API, over
;;; per-target backends: `null` (in-memory, any host), the vendor Arduino
;;; core on ESP32, and in future libgpiod on a Pi/Jetson.  This suite is
;;; the same assertions run against WHICHEVER backend is present, because
;;; the value of the layer is that they agree.
;;;
;;; WHY THIS FILE EXISTS AT ALL.  Before it, the 26 commonio procedures
;;; could only be called with a board plugged in, so in practice they were
;;; tested rarely: on 2026-09-04, of the seven boards that have them,
;;; exactly ONE was reachable -- the rest shared with other sessions,
;;; unplugged to save power, or in a USB port that fails enumeration.
;;; B256 (an Arduino 3.x WiFi regression) reached customers as a boot loop
;;; for precisely that reason.
;;;
;;; TIERS, and the safety rule that shapes them:
;;;   Tier A  portable -- timing, bit ops, Print fidelity, backend identity.
;;;           Runs EVERYWHERE, drives nothing.
;;;   Tier B  pins -- loopback writes.  Runs ONLY on the `null` backend.
;;;           *** ON REAL HARDWARE A PIN WRITE MOVES SOMETHING. ***  The 4WD
;;;           is a car: its pins are motors and servos.  A test suite must
;;;           never drive an output on a board it cannot see, so Tier B is
;;;           gated on the backend being simulated, and the device tier
;;;           below only READS.
;;;   Tier C  device read-only -- safe on any board.
;;;
;;; Load with: (load "ll_tests/llarduino-tests.scm" 0)
;;; =======================================================================

(define *pass* 0)
(define *fail* 0)
(define *skip* 0)

(define (check name expected actual)
  (if (equal? expected actual)
    (begin (set! *pass* (+ *pass* 1)) (news "PASS ~a\n" name))
    (begin (set! *fail* (+ *fail* 1)) (warn "FAIL ~a  expected=~a  got=~a\n" name expected actual))))

(define (check-true name actual)
  (if actual
    (begin (set! *pass* (+ *pass* 1)) (news "PASS ~a\n" name))
    (begin (set! *fail* (+ *fail* 1)) (warn "FAIL ~a  expected=#t  got=~a\n" name actual))))

;;; A SKIP IS REPORTED, NEVER SILENT.  A test that quietly does not run reads
;;; exactly like a test that passed, and this tree has already paid for that
;;; (B257: a self-declared SKIP counted as a FAIL and was indistinguishable
;;; from a memory cliff).  Say which tier was skipped and why.
(define (skip name why)
  (set! *skip* (+ *skip* 1))
  (news "SKIP ~a  (~a)\n" name why))

(define backend (llarduino-backend))
(define simulated? (equal? backend "null"))

(display "\n=== LLArduino conformance: backend ") (display backend) (display " ===\n")

;;; -----------------------------------------------------------------------
;;; Tier A -- portable.  Drives nothing; must hold on every backend.
;;; -----------------------------------------------------------------------

(check-true "backend name is a string"            (string? backend))

;;; Timing.  millis/micros are the oldest part of the layer and the part every
;;; log line depends on, so assert the invariants rather than assume them.
(let* ((m0 (millis)) (u0 (micros)))
  (check-true "millis returns an integer"         (integer? m0))
  (check-true "micros returns an integer"         (integer? u0))
  (check-true "millis is non-negative"            (>= m0 0))
  (delay_ms 25)
  (let ((m1 (millis)) (u1 (micros)))
    (check-true "millis advances over delay_ms"   (>= m1 m0))
    (check-true "micros advances over delay_ms"   (>= u1 u0))
    ;;; The delay must actually elapse.  A backend whose delay returns
    ;;; immediately would still pass a monotonicity check, so bound it.
    (check-true "delay_ms(25) elapsed >= 20ms"    (>= (- m1 m0) 20))
    ;;; ...and must not sleep wildly long, which would mean the wrong units.
    (check-true "delay_ms(25) elapsed < 2000ms"   (< (- m1 m0) 2000))))

(let ((u0 (micros)))
  (delay_us 2000)
  (check-true "delay_us(2000) elapsed >= 1ms"     (>= (- (micros) u0) 1000)))

;;; Print fidelity.  Each of these is an Arduino behaviour that a host could
;;; plausibly get "nicely" wrong, and the divergence would be invisible until
;;; someone compared a serial monitor against a host run.
;;; UPPERCASE and no prefix: Arduino's Print::printNumber uses 'A'+d-10, so print(255, HEX) is
;;; "FF".  This assertion originally expected "ff" -- transcribed from the docs, which state the
;;; missing 0x but not the case -- and the ESP32 run caught it against the vendor core.
(check "print(255, HEX) is FF, no 0x prefix" "FF"     (llarduino-fmt-int 255 16))
(check "print(255, DEC)"                     "255"    (llarduino-fmt-int 255 10))
(check "print(5, BIN)"                       "101"    (llarduino-fmt-int 5 2))
(check "print(8, OCT)"                       "10"     (llarduino-fmt-int 8 8))
(check "negative signs in base 10"           "-42"    (llarduino-fmt-int -42 10))
(check "zero prints as 0"                    "0"      (llarduino-fmt-int 0 10))
(check "print(double) defaults to 2 places"  "1.50"   (llarduino-fmt-real 1.5 2))
(check "print(double, 0) has no point"       "3"      (llarduino-fmt-real 3.14159 0))
(check "print(double, 4)"                    "3.1416" (llarduino-fmt-real 3.14159 4))
;;; The one most likely to be "fixed" by a well-meaning edit: Arduino's
;;; println has ALWAYS emitted CRLF, and a host that emits bare \n looks
;;; right in a terminal while differing from every device.
(check "println terminator is CRLF"          "\\r\\n" (llarduino-eol))

;;; Bit helpers -- pure arithmetic, must be identical everywhere.
(check "bit(0)"            1   (bit 0))
(check "bit(7)"            128 (bit 7))
(check "bitRead(5,0)"      1   (bitRead 5 0))
(check "bitRead(5,1)"      0   (bitRead 5 1))
(check "bitSet(0,3)"       8   (bitSet 0 3))
(check "bitClear(255,0)"   254 (bitClear 255 0))
(check "highByte(#x1234)"  18  (highByte 4660))
(check "lowByte(#x1234)"   52  (lowByte 4660))

;;; -----------------------------------------------------------------------
;;; Tier B -- pin loopback.  SIMULATED BACKENDS ONLY.
;;; -----------------------------------------------------------------------

(if (not simulated?)
    (skip "Tier B pin writes"
          "real backend -- a pin write moves hardware; refusing to drive an unseen board")
    (begin
      (pinMode 13 1)                                  ; OUTPUT
      (digitalWrite 13 1)
      (check-true "digitalWrite HIGH reads back"      (digitalRead 13))
      (digitalWrite 13 0)
      (check-true "digitalWrite LOW reads back"       (not (digitalRead 13)))
      ;;; Scheme falsiness AND numeric zero both mean LOW -- the documented
      ;;; extension in ll_xmop3_CommonIO.cpp, worth pinning down.
      (digitalWrite 13 #t)
      (check-true "digitalWrite #t is HIGH"           (digitalRead 13))
      (digitalWrite 13 #f)
      (check-true "digitalWrite #f is LOW"            (not (digitalRead 13)))
      ;;; Pins are independent: a write to one must not disturb another.
      (digitalWrite 13 1)
      (digitalWrite 14 0)
      (check-true "pins are independent"              (and (digitalRead 13)
                                                           (not (digitalRead 14))))
      (analogWrite 5 200)
      (check "analogWrite/analogRead loopback"  200   (analogRead 5))
      (analogWrite 5 0)
      (check "analogWrite 0"                    0     (analogRead 5))
      ;;; Out-of-range pins must not corrupt anything.  The null backend
      ;;; range-checks; this asserts it does not crash or bleed into pin 0.
      (digitalWrite 0 0)
      (digitalWrite 99999 1)
      (check-true "out-of-range pin is harmless"      (not (digitalRead 0)))))

;;; -----------------------------------------------------------------------
;;; Tier C -- device, READ-ONLY.  Safe on any board, including the 4WD.
;;; -----------------------------------------------------------------------

(if simulated?
    (skip "Tier C device reads" "simulated backend -- nothing to read")
    (begin
      ;;; INPUT mode drives nothing, and digitalRead cannot move a motor.
      (pinMode 0 0)                                   ; INPUT
      (check-true "digitalRead returns a boolean"     (boolean? (digitalRead 0)))
      (check-true "millis on device is sane"          (> (millis) 0))))

(display "\n=== LLArduino TEST SUMMARY: ")
(display *pass*) (display " passed, ")
(display *fail*) (display " failed, ")
(display *skip*) (display " skipped ===\n")

(define llarduino-test-pass *pass*)
(define llarduino-test-fail *fail*)

; end of llarduino-tests.scm
