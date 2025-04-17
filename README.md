


Áttérés a PlatformIO-ra!

- Arduino-Pico (Earle F. Philhower, III earlephilhower) kernel használata
  (https://github.com/earlephilhower/arduino-pico/blob/master/docs/platformio.rst#current-state-of-development)

A szükséges **platformio.ini** bejegyzések:
```
[env:pico]
    platform = __https://github.com/maxgerhardt/platform-raspberrypi.git__
    board = pico
    framework = arduino
    __board_build.core = earlephilhower__
```

 - Végre lehet használni a  C_Cpp.intelliSenseEngine": "Default"  beállítást!
