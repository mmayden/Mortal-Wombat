# `src/sim/math/`

Deterministic math for the simulation: lookup tables replacing anything that
would otherwise come from `<cmath>`.

Empty until something needs it. ADR 0002 forbids `<cmath>` below the sim
boundary because transcendental functions are not bit-identical across libm
implementations — so when trigonometry is first needed, the table goes here
rather than a `std::sin` call going into a sim file.
