# ImBUS9 a ImBUS12 – sběrnice Libora Lasoty

Stav: **v emulátoru** jako typy sběrnice `ImBUS9` (9 pozic) a `ImBUS12` (12 pozic) od 2026-10-08.

## Podklady

- ImBUS9 rev. 1.1: `H:\SAPI-1\LiborLasota\ImBUS9 rev. 1.1\` (EAGLE `ImBus9.sch`, `.brd`, knihovna `ImBUS.lbr`, rozpiska
  `ImBus9.csv`, popis `DESCRIPTION`). ImBUS12 je podle autora stejná, jen na 12 karet.

## Zapojení (ImBUS9 rev. 1.1)

- „Imperial BUS pro novou sestavu SAPI standardní velikosti“, verze pro 19" rack: **1 slot CPU + 8 pracovních**,
  konektory FRB 62 (TX 518 6212) K1–K9, bez impedančního přizpůsobení.
- **Adresy:** K1 (procesorová karta) budí AB0–AB15, čtyři **MH3216** (IC1–IC4) je pouštějí jako A0–A15 do K2–K9.
  /CS obvodů 3216 = **/AEN** (kolík 13, pull-up R5), takže při /AEN = 1 jsou adresy od CPU odpojené (DMA).
  Propojka pro vypnutí bufferů (jako JP3 u ARB-1C) není.
- **Data, řídicí signály, přerušení** vedou přímo do všech pozic (D0–D7, /MR, /IOR, /RES, /INTA…). Kolík 14 je /DEN
  (doplněný v rev. 1.1), signály AEN a AEN2 jsou spojené.
- **Napájení:** konektor ATX (`/PS_ON`), konektor SAPI_PWR / SPN-1V, stabilizátor −5 V (7905), LED napětí.
  JP1 = S4 (dřív +5VSB), JP2 = /RTL (kolík 1, také na SAPI_PWR a SPN).
- Změny proti rev. 1.0: přidaný 9. slot, popis S4, úprava −5 V, /DEN, spojené AEN a AEN2 (podrobně `DESCRIPTION`).

## Emulace

- Typ sběrnice v sestavě: `"bus": { "type": "ImBUS9" }` nebo `"ImBUS12"`, v konfigurátoru v seznamu sběrnic.
- **X1 je jen pro procesorovou kartu** (karta jinde než v K1 by adresy neviděla). JP3 není.
- Sestava `machines/libor_doma.sapi` (ImBUS12, desky V Libora Lasoty).
