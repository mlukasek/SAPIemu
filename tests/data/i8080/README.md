# Testy procesoru 8080

Testovací programy pro CP/M z repozitáře jádra https://github.com/superzazu/8080 (`cpu_tests/`), popis v `README.TXT`:

- `TST8080.COM`: diagnostika Microcosm Associates (Kelly Smith).
- `8080PRE.COM`, `8080EXM.COM`: cvičení instrukční sady (Ian Bartholomew, Frank Cringle; GPL jako ZEXDOC).
- `CPUTEST.COM`: SuperSoft Associates, Diagnostic II.

Spouští je `tests/test_i8080.cpp` s minimálním CP/M BDOS (funkce 2 a 9). 8080EXM jen v optimalizovaném buildu
(ctest label `slow`).
