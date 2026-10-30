# JPR-1S a grafika BGP-1S (8080)

Liborova procesorová karta JPR-1S (8080) s barevnou grafikou BGP-1S, která kreslí jako PMD-85. Klávesnice
ANK-1M2 má konektor joysticku. V paměti je MIKROBIOS 1.02, programy pro PMD-85 se nahrají jako Intel HEX.

## Manic Miner

1. Soubor → Nahrát program do paměti (Intel HEX)… → `media\jpr-1s\manic_miner.hex`.
2. Klikněte do okna obrazovky a napište:

```
JUMP 4400
```

3. Úvodní obrazovka ukáže ovládání: **O** vlevo, **P** vpravo, **SHIFT** skok, **M** melodie, **Enter** start
   hry.
4. Na hraní zapněte Stroj → Klávesnice → Režim pro hry (Ctrl+F7). Klávesy se pak drží podle polohy jako na
   skutečné klávesnici a Shift je SHIFT.

## Dál

- `media\jpr-1s\auto.hex` je závodní hra „auto“ (také `JUMP 4400`).
- Manic Miner: Vít Libovický a Daniel Jenne, ViLiSoft a DaJeSoft 1985 (port z PMD-85), na SAPI-1 s BGP-1S ho
  převedl Libor Lasota.
- Deska BGP-1S: [BGP-1S](docs/desky/BGP-1S.md).
