# PINK TROMBONE for the M-VAVE FM-1

Neil Thapen's [Pink Trombone](https://dood.al/pinktrombone/) (a glottis and a 44-segment vocal tract with a nose)
as a firmware for the FM-1. It is a [fm1-dock](https://github.com/joakimeriksson/fm1-dock) module built solo:
fm1-dock with PINK TROMBONE as its only synth, so USB, the installers and the rescue keep working.

- `pt_engine.h`: the voice, single-precision float, no hardware, no libm. Builds on any computer. The waveguide runs at
  half the original's resolution (22 segments at 44.1 kHz, the same 17 cm tract) so it fits the FM-1 with room to spare
  (load 28 % in fm1-emu at 100 MHz); the shape is still made in the original's 44 segments.
- `pinktrombone_module.c`: the FM-1 side (keys, knobs, buttons, screen). `pt_gfx.h`: the picture, drawn in software.
- `dock/module.cfg`, `build.sh`: the build. `host/`: tests on the computer.

## Playing it

| | |
|---|---|
| keys / MIDI | the pitch (monophonic, last note first). F3 sounds an F2, as the original's keyboard; OCT+ goes up |
| KNOB1 / KNOB2 | tongue position / tongue height |
| KNOB3 / KNOB4 | lips (rounded .. open) / tenseness (a breath .. a pressed voice) |
| PRESETS | a vowel: AH AE EH IH EE UH AW OH OO |
| SELECT / ALGORITHM | vibrato / the nose (nasal sound) |
| hold SEQ, SEL, ENV | the fricatives S, SH, F |
| FX | the effects page (toggle, the LED shows it): KNOB1 chorus, KNOB2 delay, KNOB3 reverb, KNOB4 the delay's time |
| hold LFO, EDIT, GLO | the stops T/D, K/G, P/B (the burst comes when you let go) |
| hold ARP | M / N: the nose opens |
| PLAY | the voice drones (toggle) |
| SAVE | keeps the settings |

MIDI CC: 1 vibrato, 74 tongue position, 71 tongue height, 72 lips, 73 tenseness, 76 nose, 91 reverb, 92 delay, 93 chorus,
123 all notes off.

The effects (`pt_fx.h`, stereo): a chorus of two swept taps (what makes the mono voice wide), a ping-pong delay and a small
reverb (Freeverb's combs). Chorus and reverb are on a little at power-on, the delay off; SAVE keeps the settings.

The screen shows the throat as a bent tube (`pt_gfx.h`): from the vocal folds up the back of the throat and out through
the lips, with the tongue's body, teeth, lips, the nose with its velum, the folds opening and closing in time with the
voice, a white arrow where a consonant pinches, and a scope of the sound. The air glows with the voice.

## Build

```sh
./build.sh            # -> build/pinktrombone.fwsc
```

Needs [fm1-dock](https://github.com/joakimeriksson/fm1-dock) next to this directory (`FM1_DOCK=...` otherwise), JieLi's
toolchain (`fm1-dock/tools/get_toolchain.sh`, to `~/.jieli/toolchain`) and three files of the AC79 SDK
(`~/fw-AC79_AIoT_SDK`, see fm1-dock's README). Off Linux x86-64 the toolchain runs in Docker (colima is enough).
The package identity is `FM-1_944`.

## Try it in the emulator

Open <https://joakimeriksson.github.io/fm1-emu/?fw=felucca&mhz=100>, press **Open .fwsc…** (or drop the file on the
synth) and pick `build/pinktrombone.fwsc`. The computer keyboard plays the keys, the knobs are on the page.

## On the computer

```sh
host/fxtest.c             # (cc it) the effects' tails must die
host/vowels.sh             # F1 / F2 of each vowel preset against male targets
python3 host/tune_vowels.py AH OO     # search tongue / lip settings for a vowel's formants
cc -O2 -DHB_DESC='const hb_module_t hb_module' -I. -I../fm1-dock/firmware/dock -o build/host/modtest host/modtest.c -lm
./build/host/modtest       # the module with a stub host: build/host/phrase.wav and the tract's screen
```

## Status

Tried only in [fm1-emu](https://github.com/joakimeriksson/fm1-emu) (100 MHz, about 38 % load with the effects on),
never on a real FM-1. The video in `video/` is rendered from the emulator; see `video/youtube.md`.

## Licence

GPL-3.0-only (the module and fm1-dock); `pt_engine.h` is a port of Pink Trombone, MIT, (c) 2017 Neil Thapen. See
[LICENSING.md](LICENSING.md). Not affiliated with M-VAVE. Installing firmware is at your own risk.
