#!/usr/bin/env python3
"""Writes film.json: the showcase's scenes, with their key scripts (fm1-live's FM1_BENCH_KEYS syntax)."""
import json, os
FWSC = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "build", "pinktrombone.fwsc"))
T0 = 2600   # ms after boot: the module is up and listening (checked on the first frames)

def hold(k, at, ms):
    return [f"{at}:{k}", f"{at + ms}:{k}^"]

def press(b, at, ms=120):
    return hold(b, at, ms)

scenes = []

# 1. intro: a drone, the voice and the picture
keys = hold("C4", T0, 300) + press("PLAY", T0 + 400)
scenes.append(dict(
    name="intro", fwsc=FWSC, ms=T0 + 5200, **{"from": 2.2}, dur=5.4, keys=",".join(keys),
    captions=[
        {"style": "title", "text": "PINK TROMBONE", "sub": "a singing voice synth for the M-VAVE FM-1", "from": 0.5, "to": 4.9},
    ]))

# 2. the vowels: PRESETS steps AH AE EH IH EE UH AW OH OO (it starts on UH)
keys = hold("C4", T0, 11800)
vow = ["AW", "OH", "OO", "AH", "AE", "EH", "IH", "EE"]
t = T0 + 900
caps = [{"text": "Vowels, not samples", "sub": "a throat and mouth modelled in real time", "from": 0.4, "to": 3.2}]
for i, v in enumerate(vow):
    keys.append(f"{t}:e6:1")
    t += 1350
keys += [f"{T0 + 9000}:e0:12"]     # SELECT: vibrato
caps += [{"text": "PRESETS", "sub": "AW  OH  OO  AH  AE  EH  IH  EE", "from": 3.4, "to": 8.2},
         {"text": "SELECT: vibrato", "from": 9.0, "to": 11.6}]
scenes.append(dict(name="vowels", fwsc=FWSC, ms=T0 + 12500, **{"from": 2.2}, dur=12.4, keys=",".join(keys), captions=caps))

# 3. a melody, with the tongue swept by the knobs ("talking")
tune = ["E4", "E4", "F4", "G4", "G4", "F4", "E4", "D4", "C4", "C4", "D4", "E4", "E4", "D4", "D4"]
beats = [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1.5, 0.5, 2.2]
beat = 480
keys, t = [], T0
for n, b in zip(tune, beats):
    keys += hold(n, int(t), int(b * beat - 70))
    t += b * beat
sweep_t = T0 + 600
for i in range(10):                               # KNOB1: the tongue forward and back, KNOB2 height
    keys.append(f"{sweep_t + i * 420}:e2:{6 if i % 2 == 0 else -6}")
    keys.append(f"{sweep_t + i * 420 + 200}:e3:{-8 if i % 2 == 0 else 8}")
scenes.append(dict(name="melody", fwsc=FWSC, ms=int(t) + 1800, **{"from": 2.2}, dur=round((t - T0) / 1000 + 1.6, 1), keys=",".join(keys),
    captions=[{"text": "Play it", "sub": "keys and MIDI, KNOB1-3: tongue, height, lips", "from": 0.3, "to": 4.2},
              {"text": "Ode to Joy, sung by a synth", "from": 4.4, "to": 8.2}]))

# 4. consonants: held buttons close the mouth in different places
keys = hold("C4", T0, 11000) + [f"{T0 + 200}:e6:-3"]       # AH -> back to UH... a vowel that opens the mouth
steps = [("GLO", "P"), ("LFO", "T"), ("EDIT", "K"), ("LFO", "T"), ("SCL", "SH"), (None, ""), ("SEQ", "S"), (None, ""),
         ("ENV", "F"), ("LFO", "T"), ("EDIT", "K"), ("GLO", "P"), ("ARP", "M"), (None, ""), ("ARP", "M"), (None, "")]
t = T0 + 1400
for b, _ in steps:
    if b:
        keys += press(b, t, 340 if b == "ARP" else 130)
    t += 260
for rep in range(1):
    pass
scenes.append(dict(name="consonants", fwsc=FWSC, ms=T0 + 11500, **{"from": 2.2}, dur=11.0, keys=",".join(keys),
    captions=[{"text": "Consonants", "sub": "hold a button to close the mouth: P T K  SH S F  M", "from": 0.4, "to": 8.2},
              {"style": "top", "text": "the white arrow shows where the mouth pinches", "from": 3.6, "to": 8.2}]))

# 5. effects: the FX page, then a phrase while chorus, ping-pong delay and reverb are turned up
keys = press("FX", T0, 200)
phrase = [("E4", 700, 600), ("G4", 1700, 600), ("C5", 2700, 700), ("G4", 3900, 500), ("E4", 4700, 500), ("C4", 5600, 1500)]
for n, at, ln in phrase:
    keys += hold(n, T0 + at, ln)
keys += [f"{T0 + 1100}:e2:12", f"{T0 + 2300}:e3:18", f"{T0 + 3400}:e4:12", f"{T0 + 4400}:e5:-8"]
keys += press("FX", T0 + 8300, 200)
scenes.append(dict(name="effects", fwsc=FWSC, ms=T0 + 11500, **{"from": 2.2}, dur=10.6, keys=",".join(keys),
    captions=[{"text": "FX", "sub": "chorus, ping-pong delay and reverb, in stereo", "from": 0.3, "to": 4.2},
              {"text": "KNOB1-3: how much", "sub": "KNOB4: the delay's time", "from": 4.4, "to": 8.4}]))

# 6. the end card, over a last note
keys = hold("C4", 600, 900)
scenes.append(dict(name="outro", fwsc=FWSC, ms=4200, **{"from": 0.2}, dur=3.6, keys=",".join(keys),
    card={"lines": [["PINK TROMBONE", 96], ["for the M-VAVE FM-1", 44], ["after Neil Thapen's Pink Trombone  -  on fm1-dock", 30]]}))

json.dump({"scenes": scenes}, open(os.path.join(os.path.dirname(__file__), "film.json"), "w"), indent=1)
print(len(scenes), "scenes,", round(sum(s["dur"] for s in scenes), 1), "s")
