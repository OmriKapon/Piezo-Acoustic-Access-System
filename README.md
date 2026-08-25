[READ_ME.md](https://github.com/user-attachments/files/31431267/READ_ME.md)
# Piezo Acoustic Access System

A tap-based physical access-control system built on an Arduino, using four
piezo discs as pressure-sensitive "keys." Instead of a traditional 1-to-1
key-to-digit mapping, the system works as a **chorded keypad**: a valid
digit is determined by *how many* piezo discs are struck simultaneously
within a short temporal window, regardless of *which* specific discs are
hit. This spreads mechanical wear evenly across all discs and makes
shoulder-surfing significantly harder, since watching hand position no
longer reveals the code.

## Why this exists

Standard numeric keypads leak information: worn keys reveal which digits
are used, and an observer watching your hand can often read the code
directly. This project explores whether the input method itself can be
obfuscated through spatial chords — counting concurrent strikes rather
than tracking specific key identities — and whether *how* a correct code
is entered (its rhythmic signature) can add a second, low-friction layer
of security.

## How it works

### Hardware

- Four piezo discs, each wired to its own analog input (A0–A3)
- Each channel has a 1MΩ pull-down resistor (discharges the piezo's
  self-capacitance after a strike, preventing a floating pin from
  producing false readings) and a 5.1V zener diode clipper (protects the
  ADC input from voltage spikes on a hard strike)
- Green/red LEDs and a piezo buzzer for feedback

### Digit determination (chorded input)

The firmware groups near-simultaneous strikes into a single logical
chord using a tunable temporal window (e.g. 150ms). To enter the digit -> 3
 a user strikes any three of the four discs concurrently — which
three doesn't matter. Because the system counts overlapping impacts
instead of tracking individual sensor identity, an observer can't deduce
the secret sequence just by watching hand placement.

### Two-layer authentication

1. **Hard gate — the code.** The sequence of logical digits must match
   the secret code exactly. This is the only thing that can deny access.
2. **Soft gate — entry rhythm.** The system also learns the timing
   between presses (mean + standard deviation per gap, from a real
   enrollment sample) and compares new attempts against it. A correct
   code entered at an unusual tempo still grants access — it just raises
   a short warning chirp. This was a deliberate choice: a purely
   behavioral signal like tap rhythm is known to drift with fatigue,
   stress, or illness, so it flags anomalies rather than ever locking
   out a legitimate but tired user.

## Engineering process

**Attempt 1 — amplitude windowing.** Originally tried classifying
presses by the *peak voltage* of the piezo strike (e.g. only 2.0–2.2V
counts). This failed in practice: human press force isn't repeatable
enough, and a piezo impact produces a damped oscillating signal, not one
stable value — there's no reliable window to threshold against.

**Attempt 2 — timing instead of amplitude.** Pivoted to inter-press
*timing* as the distinguishing signal, with a hard/soft gate split so
timing could never lock out a correct code.

**Debugging cross-talk with real data.** Calibration logs revealed
electrical/mechanical cross-talk between piezo channels — initially
small and fixed (~1–2 ADC counts, negligible), but a second, harder
round of testing revealed a *proportional* pattern instead: a hit on one
channel consistently produced roughly 10% of that value on a
neighboring channel. Because the leakage scaled with strike strength, no
fixed threshold could fully separate it from a real light tap on the
neighbor.

**Next — adapting detection for chorded input.** The strike-detection
logic built to solve the problem above (a global refractory window plus
picking the single strongest pin per event) was designed to identify
*one* winning pin — the opposite of what chord counting needs. Chord
detection instead has to catch a *burst* of near-simultaneous strikes
within a fixed window and count how many distinct piezos crossed
threshold, not pick a single winner. The proportional cross-talk found
during calibration also becomes more consequential here: a hard strike
bleeding onto a neighbor could inflate the count itself, so the
threshold will need re-tuning specifically for this mode.
This was solved in software by assigning independent state-tracking to each piezo,
though precise threshold calibration remains essential to mitigate false counts from physical cross-talk.

## Firmware modes

The firmware runs as a simple state machine with three modes:

- `R` — normal run mode
- `C` — calibration mode: streams raw ADC values (only printed on
  change) to pick a strike threshold and check for cross-talk
- `E` — enrollment mode: collects N correct-code entries and computes
  the timing mean/stdev used by the soft gate

## Validation

Tested against three cases: wrong code (always denied, regardless of
timing), correct code at natural rhythm (clean grant), and correct code
at a deliberately altered rhythm (granted, with the timing-anomaly flag
raised) — confirming the hard gate and soft gate behave independently as
designed. (These runs validated the per-pin digit-mapping version;
chord-based detection is not yet re-validated end-to-end.)

## Possible extensions

- Persist calibration to EEPROM instead of re-flashing after enrollment


## Built with

Arduino Uno (or compatible) · C++ / Arduino IDE · 4× piezo disc elements
· 1MΩ pull-down resistors · 5.1V zener diodes · LEDs + buzzer for
feedback
