# PKT-1 – procesorová deska SAPI-1.5 (Procesor, Klávesnice, Tiskárna)

Stav: **emulovaná** (typ `PKT-1`, `src/cards/pkt1.*`) od 2026-10-07. Sestava `machines/zps/ZPS-6.sapi`, test
`test_sapi15`. Tesla Liberec 1990, 6XK 199 243, deska 6XB 007 53, návod 6XV 123 42.

## Podklady

- **Návod Tesla** (sken): `..\SAPI_DOKU\SAPI-1.5_dokumentace\PKT-1\TMPnn.TIF` (strana N = TMP(N+1), schéma TMP28/29).
  OCR a **dumpy PROM z desky** (INTELHEX) na sapi.cz: `..\SAPI.CZ\sapi\pkt-1.php`.
- **Repliky EC1045** (EAGLE, `..\SAPI-karty\PKT-1\`): `PKT-1` (1:1 i s chybou tiskárny), `PKT-1_funkcni` (BUSY
  přesunutý na PC3, zem na XP2/21 a XP3/21 jako u JPR-1), `PKT-1_u` (navíc propojka původní/opravené zapojení
  a +5 V na XP3/9). `Popis_PTK-1.txt`, `PKT-1_info.txt` (pole propojek patice EPROM), `DEK PKT-1 DD8.pdf`
  (upravený dekodér 8 KB z doby replik).
- **Software:** ROM BIOS MON-V100690 (dump `PKT1_ZPS-7.BIN` a zdroják `System\pkt1.asm`, opravená verze
  `pkt1-rombios.asm` = Melsoft `ROMBIOS.MAC`), `docs/software/SAPI-1.5.md`.
- Podrobný rozbor (netlisty, citace): `work/sapi15/report_pkt1_rsw1.md` a `report_software.md` (mimo git, jen
  na autorově PC).

## Zapojení

- **Z80A** (UA880D), krystal 16 MHz / 74LS93: **XT5 24-25 = CPU 2 MHz** (dodávka ZPS-8), 23-24 = 4 MHz. Takt
  sběrnice F2 (kolík 60) je vždy 2 MHz.
- **EPROM** DD9 (2716 až 27512, propojky XT2 jen přepojují vývody patice) se čte, když BOOT = L, podle PROM
  **DD8** (74S287, Tesla 6XN 902 54): vstupy A2 = linka BOOT (kolík 47, čte se zpět), A3–A7 = A11–A15, A0/A1 na
  zemi. **Dump z desky: EPROM jen 0000–07FF.** Čtení zůstane na desce, **zápisy jdou vždy na sběrnici**
  (v okně s vypnutým datovým bufferem, RAM pod EPROM dostane nebuzenou sběrnici, emulace FFh). Po resetu je
  BOOT = L a Z80 startuje v EPROM na 0000h. ROM BIOS se zkopíruje na F000h a EPROM vypne.
- **EPROM bez DBS-1:** `roms/pkt-1/PKT-1_ROMBIOS_AND.bin` je ROM BIOS Melsoft s ovladačem pro AND-1Z a AND-1V na F800h
  (`docs/software/SAPI-1.5.md`, sestavy `sapi15_and1z` a `sapi15_and1v`), na skutečné desce zatím nevyzkoušený.
- **XT3:** 6-7 = BOOT z registru (standard), 5-6 = EPROM trvale vypnutá.
- **Registr DD7, port 0Eh** (jen zápis, nuluje /RES): D0 = **BOOT** (0 = EPROM zapnutá, kolík 47, LED HL1
  svítí), D1 = **MAP2** (kolík 22), D2 = **MAP1** (kolík 21), přes XT1 na sběrnici. ROM BIOS píše 03h kolem
  každého zápisu do video RAM DBS-1 (MAP2) a 01h potom.
- **Porty** (PROM **DD12**, dump z desky): 00–03h 8255, **0Eh registr** (tabulka v návodu str. 16 0Eh vynechává,
  dump ji má). Plný 8bitový dekód, /IOR a /IOW jdou na sběrnici i pro vnitřní porty.
- **8255:** PA = data tiskárny (XP3), PB = klávesnice (XP2, **režim 1**: /KSTB = PC2 zachytí kód, IBF = PC1 →
  /KACK), PC4 /NUL (reset tiskárny), PC5 /ZVUK (bzučák klávesnice), PC7 /STROBE tiskárny. BUSY tiskárny:
  **PC3** na opravené desce (ROM BIOS s tím počítá), **PC6** (/ACK A) v původním zapojení Tesla, kde tiskárna
  bez úpravy nefunguje. INTR 8255 nevedou na CPU.
- **Klávesnice XP2** je vývodově shodná s X2 JPR-1Z/ZI (data 6,4,3,5,2,8,9,7, strobe 10 a 30, ACK 13, ZVUK 14,
  +5 V 22). Standard **Consul 262.3** (pulz 1 ms bez ACK; 8255 ho zachytí, úprava 7474 netřeba), přes KB-13
  i Consul 259.11. ROM BIOS data invertuje (CMA).
- **Tiskárna XP3** je shodná s X3 JPR-1Z/ZI (data, /STROBE 26, BUSY 15).
- **Přerušení:** /INT0 → /INT, /INT1 → /NMI (výpadek napájení, 0066h). Obvod XINTA pro IM 0 s 8259 (3bajtový
  CALL) se neemuluje.
- **/RTL** (tlačítko reset, watchdog RSW-1) → klopný obvod na konci M1 a monostabil ~15 µs → RESET Z80, 8255
  a sběrnice /RES. Emulace: reset celé sestavy, když některá karta drží /RTL.
- **XT4:** 20-21 čekání jen z RDY (standard), 21-22 jeden čekací stav v každém strojovém cyklu.
- **Sběrnice:** PKT-1 sama budí /AEN (inverze /BUSAK) a nemusí sedět v X1. V emulátoru proto ARB-1C s **JP3 =
  1-2** (buffery vypnuté, X1 volný) místo SBI-1/SBK-1 SAPI-1.5 (`docs/desky/ARB-1C.md`).

## Emulace

- Parametry: `rom` (výchozí dump EPROM, volba opravené verze Melsoft), `eprom` (XT2), `decoder` (DD8: Tesla,
  nebo 8 KB replika), `io_decoder` (DD12), `cpu_mhz` (XT5), `wait` (XT4), `boot` (XT3), `map1`/`map2` (XT1),
  `keyboard` (XP2; jen klávesnice s kódem a strobe: EKL-1, Consul), `printer` s `printer.busy` (`pc3`/`pc6`).
- 8255 je plná (režim 0 i 1, port B v režimu 1 jako vstup přibyl 2026-10-07). Tiskárna bere bajt při náběžné
  hraně PC7 (ROM ho strobuje bitovým nastavením), v režimu 1 portu A se netiskne.
- Klávesnice: náběžná hrana /KSTB zachytí kód do portu B; dokud program kód nepřečte (IBF), další klávesu
  nepošle (pohodlí: text se neztratí; `keyboard.strict` = věrně).

## Nejasnosti k ověření na HW

1. Polarita linek MAP1/MAP2/BOOT na sběrnici (replika: aktivní H; čísla vývodů DD7 v originálním schématu
   nesedí s replikou).
2. Blokuje se sběrnicové /MR při čtení vnitřní EPROM (text návodu ano, netlist repliky ne)?
3. Hodnota na datové sběrnici při zápisu do okna EPROM a při INTA bez odpovídající karty (emulace FFh).
4. XT4 a XT5 na skutečných ZPS-5/6/7 (ZPS-8: 2 MHz, čekání jen z RDY).
5. Zapojení BUSY tiskárny na dochovaných deskách (PC6 původně, nebo upravené na PC3).
6. Funguje EKL-1 s /KACK = IBF?
7. Kde se použil dekodér DD8 s oknem 8 KB a 2864 z fotky (`FOTO_poklady_petr`)?
8. **Jel Melsoft na 4 MHz (XT5 23-24)?** SIMPROG na disketě 7 (programátor DSE-1) je build pro 4 MHz: délka
   programovacího pulzu `LXI H,07CEh` dá na 4 MHz 49 ms, na 2 MHz 98 ms. Upravená kopie `SAPI15\SimProg_opr` má
   03E7h a poznámku „nemám rychlé SAPI (4MHz) ale normální JPR-1Z na 2MHz“. Emulace má ZPS-6 dál na 2 MHz
   (dodávka ZPS-8), SIMPROG z diskety 7 tam programuje pulzy 98 ms a log to poznamená (`docs/desky/DSE-1.md`).
