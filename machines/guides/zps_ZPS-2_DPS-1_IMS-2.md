# Měřicí pracoviště na sběrnici IMS-2

Deska DPS-1 připojuje SAPI k přístrojové sběrnici IMS-2 (česká verze IEEE 488). Na sběrnici je voltmetr Metra
M1T 330 a k němu zdroj Tesla BS 525, jehož knoflíky se točí myší. Měří program BASIMS, BASIC s příkazem `IMS#`.

## Vyzkoušejte

1. Nad ostatními okny je Měřicí pracoviště IMS-2. Voltmetr sám měří napětí zdroje (asi `12.564 V`); otočte
   knoflíkem zdroje a hodnota se změní.
2. Nahrajte BASIMS: Soubor → Nahrát program do paměti (Intel HEX)… → `media\dps-1\BASIMS.hex`, klikněte do okna
   obrazovky a za tečku napište `G4400` a Enter. Naběhne `* BASIC - EX  V4.0 *`.
3. Programy jsou na kazetě v magnetofonu: `LOAD`, jméno `PRIKLAD`, v okně Média PŘEHRÁT, `Y`, pak `RUN` vypíše
   naměřené napětí.

## Dál

- Celý návod s měřením ve smyčce a příkazy `IMS#`: [návod DPS-1](../../docs/navod-dps1.md).
