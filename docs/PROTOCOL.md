# Enviracom Protocol


## Message Format
```
[H|M|L]_XXXX_YY_[R|Q|C]_BB_(ZZ_)x8_CC

[H|M|L] = high, med, or low priority
XXXX    = device or message type
YY      = instance or zone
[R|Q|C] = request, query or change 
BB      = bytes of message
CC      = checksum
_       = ascii [space] character.
```

## Client Commands

```
F[an] [On[n]|Of[f]|A[uto] - control the fan
H[old] - hold the current setpoints.
Q[uery] - get a fixed report in cooked form.
L[ine] - get a fixed report in field form
M[ode] [H[eat]|C[ool]|O[ff]|A[uto]] - control the system switch
R[un] - run program - return to current setpoints, and execute
S[et] [H[eat]|C[ool]] temp - create new setpoint, and execute
```
Temperatures can be in F or C.


## Other possible commands:
```
A[llowed] modes?
T[ime]?
Current: heating|cooling|off?
```