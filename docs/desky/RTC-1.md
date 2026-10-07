# RTC-1 – deska reálného času

Stav: **emulovaná** (typ `RTC-1`, `src/cards/rtc1.*`) od 2026-10-07, test `test_rtc1`. Tesla Liberec, návod
6XV 122 84. Ověřeno programem J. Bíby `RTCZ.COM` (2011) pod CP/M. Autor desku nemá.

## Podklady

- **Návod Tesla** (OCR včetně tabulek propojek, adres a konektorů): `..\SAPI.CZ\sapi\rtc-1.php`.
- **Originální schéma Tesla** (sken J. Bíby): `..\SAPI_DOKU\SAPIMAN\RTC-1\TMP86.tif` (dekodéry, 8255, 8253, DD7,
  DD15), `TMP87.tif` (U114D, čítače 4029, DD14, hlídání napájení, záloha). Jeho poznámky v téže složce jsou jen
  programy (inicializace, čtení, zápis, watchdog, hodiny pro CP/M).
- **Replika EC1045** (EAGLE, PDF): `..\SAPI_DOKU\sapi_eagle\REPLIKY\RTC-1\`. Netlist se shoduje s originálem ve všech
  kontrolovaných bodech. Pozor na značky součástek: DD7 je nakreslený jako 7437, musí být **7438** (otevřený kolektor
  pro /RTL a /INT), DD3/DD4 jako 74S138 (= MH3205).
- Fotky: `..\SAPI_DOKU\FOTO_poklady_petr\SAPI\RTC-1_*.jpg`.
- **Software J. Bíby** na disketě `media/pmd32sd/b_rtc_zdp_zrmd.8sd` (a `a_…`): `RTCZ.COM` a `RTCZ.MAC` (zobrazení
  a nastavení času v týdenním cyklu, RTC-1 na A0h), `RTCREAL.MAC`, `RTCZK.MAC` (rutiny čtení, zápisu, přerušení
  a watchdogu). `RTC421.*` a `RTCZ80.*` na téže disketě jsou pro hodiny 72421 na ZRMD-1.

## Zapojení

- **Adresa:** 16 portů jen pro I/O. Dekodér DD3 (3205): A6–A4 vybírá přepínač **SA1–SA8** (sepnutý smí být jen
  jeden), A7 propojka: **1-2 + 3-4 = A7 = 1** (z výroby), 2-3 = A7 = 0. Základ = A7 · 80h + (n − 1) · 10h. Při /RES
  je dekodér vypnutý. DD4 dělí po čtyřech portech:

| Port | Čtení | Zápis |
|---|---|---|
| +0, +1, +2 | 8255 DD2: PA, PB, PC (čítač reálného času) | 8255 PA, PB, PC |
| +3 | FFh | řídicí slovo 8255 |
| +4..+6 | 8253 DD5 (uživatelský) | 8253 DD5 |
| +7 | FFh | řídicí slovo DD5 |
| +8..+10 | 8253 DD6 (systémový) | 8253 DD6 |
| +11 | FFh | řídicí slovo DD6 |
| +12..+15 | FFh | **zápis do čítače reálného času** (data se nepoužijí) |

- **Čítač reálného času:** pět 4029 (DD9–DD13) binárně nahoru, takt **0,5 Hz** z U114D (DD8, krystal 4,194304 MHz,
  výstup O1 přes VT5). Napájení DD8–DD14 je zálohované články na desce. Čítá asynchronně (proto program čte dvakrát,
  dokud se dvě čtení neshodují). Bity přes 100 kΩ na piny 8255: **bit 0–7 = PA (bit 0 = 2 s), 8–15 = PB, 16–18 =
  PC0–PC2, bit 19 = PC3** při propojce 9-11.
- **Zápis:** vstupy J čítačů jsou přímo na pinech 8255. PE = AND(napájení v pořádku, /IOW, **PC6**, CS portů
  +12..15) přes DD14 (4012). Program: 8255 vše výstupy (80h), PA, PB, PC, PC6 = 1 (0Dh), `OUT ADR+12`, zpět 0B3h
  (to PC6 zase vynuluje). Bity portů nastavených jako vstup si nechají hodnotu. Zápis fázi 2 s neruší.
- **Systémový 8253 DD6:** CLK2 = **Φ2 TTL** ze sběrnice (XC1/60, také na XC2/22), OUT2 = CLK0 = CLK1 (a XC2/29),
  všechna hradla v H. Příklad návodu při Φ2 = 2 MHz: čítač 2 mód 3 BCD 2000 = 1 kHz, čítač 1 mód 2 BCD 100 = takt
  100 ms, watchdog FFFFh = 65,5 s.
- **Přerušení** jde **jen na XC2/14** (/INT, otevřený kolektor, úroveň L). Na sběrnici (XC1) žádný vývod přerušení
  není, do počítače se vede kabelem. Propojky:
  - **5-6, 9-11 (z výroby, „asynchronní“):** /INT je v L, dokud je OUT1 v H. PC3 = bit 19 čítače;
  - **5-7, 6-8, 10-11 („pro systém reálného času“):** OUT1 = /STB A 8255 v módu 1 (zachytí PA), /INT = INTR A (PC3),
    smaže ho čtení PA. Bit 19 se zápisem nuluje (J4 na zemi), čítač má tedy prakticky 19 bitů;
  - 6-7: PC4 jako výstup budí /INT.
- **Watchdog:** /RTL (XC1/1) = NAND(OUT0 DD6, PC7) v DD7, úroveň. Čítač 0 v módu 0 musí program přepisovat dřív, než
  doběhne. /RES vynuluje 8255 (PC7 = 0) a tím /RTL uvolní. 8253 reset nemají.
- **Uživatelský 8253 DD5:** CLK, GATE a OUT (přes 7405, přímé i negované) jen na XC2, uvnitř nepropojený.
- PC5 je volný (XC2/30), PC6 a PC7 mají pull-down.

## Emulace

- **Parametry:** `base` (SA1–SA8 a propojka A7, výchozí **A0h** = SA3, jako programy J. Bíby), `mode` (`async`
  z výroby / `realtime`), `int` (kabel z XC2/14: `none` výchozí, `int0`, `int1`), `time`, `battery`, `file`.
- **Čas** (`time`):
  - `pc` (výchozí): čítač běží podle hodin PC jako RTC na ZRMD-1. **Po prvním zapnutí ukazuje čas od pondělní
    půlnoci** (ve dvousekundách), takže týdenní hodiny `RTCZ` hned ukážou správný den a čas. Zápis programu
    čítač přestaví a dál běží od nové hodnoty;
  - `emulated`: čítač běží podle času emulace, od nuly (v každém běhu stejně, pro testy).
- **Baterie** (`battery`, soubor `work/rtc-1/rtc.bin`): při vypnutí emulátoru se uloží hodnota čítače a čas PC.
  Po zapnutí v režimu `pc` čítač mezitím „běžel dál“ (jako na článcích), v režimu `emulated` pokračuje od uložené
  hodnoty.
- **DD6** běží z `Φ2` procesorové karty (JPR-1ZI, JPR-1V, JPR-1X: takt sběrnice, ostatní takt CPU). Watchdog drží
  /RTL a procesorové karty SAPI-1 (JPR-1, JPR-1A, JPR-1Z/ZI/V/S/X) i PKT-1 na něj odpoví resetem celé sestavy.
- **DD5** má jen registry: hodiny a hradla jsou na XC2, emulace na ně nic nepřipojuje.
- **8255 v módu 1 bez propojky na PC4** (z výroby, `async`): /STB A visí ve vzduchu. Emulace bere vstupní záchytný
  registr jako průchozí (čtení PA dává aktuální čítač), protože tak s RTC-1 počítá `RTCZ` J. Bíby (inicializace 0B3h).
  Je to předpoklad, viz Nejasnosti.
- Ověřeno: `RTCZ.COM` pod CP/M ze ZRMD-1 (ECMON, volba 1) s RTC-1 na A0h vypíše den v týdnu a čas PC.

## Nejasnosti k ověření na HW

1. Čtení PA v módu 1 (0B3h) s propojkami z výroby, kdy /STB A (PC4) nikam nevede: dává 8255 aktuální stav, nebo
   zamrzlou hodnotu? (Emulace: aktuální stav, podle funkčního `RTCZ`.)
2. Kterým kabelem a na který vstup (INT0, INT1, 3214 na JPR-1) se /INT z XC2/14 připojoval?
3. Které SA bylo sepnuté na deskách v sestavách (Bíba A0h; na Petrově fotce je jiný jen horní přepínač)?
4. Výstup XC2/29 je v originále i replice spojený s OUT2 DD6, návod ho uvádí jako vstup CLK.
