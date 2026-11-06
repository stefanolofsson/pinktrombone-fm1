# Licensing

- `pinktrombone_module.c`, `pt_gfx.h`, `pt_fx.h`, `dock/`, `build.sh`, `video/`, `host/`: GPL-3.0-only (see LICENSE),
  to match [fm1-dock](https://github.com/joakimeriksson/fm1-dock), which the firmware is built with.
- `pt_engine.h`: a port of Pink Trombone, MIT, Copyright (c) 2017 Neil Thapen
  (<https://dood.al/pinktrombone/>). The voice model (the glottis, the Kelly-Lochbaum tract and nose, the noise
  and the constriction shapes) is his; the port to C, the half-resolution waveguide and the libm-free maths are new.
  The MIT notice must stay with that file:

  > Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
  > documentation files (the "Software"), to deal in the Software without restriction, including without limitation the
  > rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
  > permit persons to whom the Software is furnished to do so, subject to the following conditions: The above copyright
  > notice and this permission notice shall be included in all copies or substantial portions of the Software. THE
  > SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND.

The firmware package built from this source includes fm1-dock (GPL-3.0-only, Joakim Eriksson, after Dinghy by zvenson,
on the platform of Felucca by Leo Kuroshita, Hugelton Instruments) and an SPL and tables from JieLi's AC79 SDK
(Apache-2.0); see fm1-dock's LICENSING.md. Not affiliated with M-VAVE.
