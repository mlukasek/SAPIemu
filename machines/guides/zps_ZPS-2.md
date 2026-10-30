# ZPS-2 s MIKOSem V4

ZPS-2 (1985) přidal k ZPS-1 paměť RAM-1 a kazetový operační systém MIKOS: monitor s povely pro práci s pamětí a
soubory na kazetě magnetofonu.

## Vyzkoušejte

1. Klikněte do okna obrazovky. Na obrazovce je `MIKOS-1 V4.0F +`, `SAPI-1 V4.0` a výzva tečka.
2. Povel monitoru je jedno písmeno, parametry se oddělují mezerou (na obrazovce je místo mezery `=`):
   - `Q` ukáže přiřazení zařízení: `C=C R=1 P=1 L=L`;
   - `D0 1F` a Enter vypíše paměť od 0000 do 001F (začátek monitoru).

```
D0 1F
```

3. `K` a další písmeno pracuje s kazetou: `.K_D` adresář, `.K_L` čtení programu, `.K_S` zápis.

## Dál

- Povely MIKOSu a práce s kazetou: [MIKOS](../../docs/software/MIKOS.md).
- Tato sestava má magnetofon s prázdnou kazetou, programy z kazet jdou vložit v okně Média (WAV, MP3).
