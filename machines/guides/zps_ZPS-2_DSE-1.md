# Programátor EPROM DSE-1

Deska DSE-1 (Tesla Elstroj) programuje EPROM 2716 a 2732 a ve 4 KB své RAM je umí simulovat. Program DSE od
Ing. P. Navrátila je v EPROM desky REM-1 od 1000h. V patici programátoru je prázdná 2716. Okno Programátor EPROM
DSE-1 ukazuje krabičku kabelu se zelenou paticí, EPROM v ní a LED PROG a MAP, pod tím programovací napětí Vpp a délku
pulzu.

## Vyzkoušejte

1. Klikněte do okna obrazovky a za tečku napište `G1000` a Enter. Naběhne nabídka programu DSE.
2. Kontrola smazání: `K`, na `VLOZ EPROM` libovolná klávesa, na `2732? A=ANO N` klávesa `N`, pak rozsah `0` Enter
   a `7FF` Enter. Program vypíše `CISTA  0000 - 07FF`.
3. Data do RAM: `F` a `5000,50FF,55` Enter vyplní 5000–50FF bajtem 55h.
4. Programování: `P`, `5000` Enter, `50FF` Enter, adresa v EPROM `0` Enter, libovolná klávesa a `N`. Během
   `PROGRAMUJI` svítí v okně programátoru červená LED PROG, Vpp je 25 V a pulz trvá 50 ms. Asi za 13 s program
   vypíše `SHODNY OBSAH  0000 - 00FF` a na okénko EPROM se přilepí štítek se jménem souboru.
5. Naprogramovaná EPROM je pracovní kopie `work\dse-1\eprom_2716.bin`. UV lampa „UV · SMAZAT“ vpravo v okně
   programátoru ji smaže (štítek zmizí, okénko chvíli svítí fialově), Nová 2716 vloží další prázdnou.

## Dál

- Návod krok za krokem i se simulátorem (S a R): [návod DSE-1](../../docs/navod-dse1.md), deska:
  [DSE-1](../../docs/desky/DSE-1.md).
