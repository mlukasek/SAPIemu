# ROM a PROM dumpy

Dumpy pocházejí z autorových repozitářů desek (`..\<deska>\ROM_PROM`, `..\DMR-1\PROM`, `..\MikroMon`).
Byly rozbaleny ze ZIP archivů. Emulátor používá PROM dumpy přímo jako dekodéry adres, jak je to na skutečné desce.

| Soubor | Deska / pozice | Popis |
|---|---|---|
| `jpr-1zi/JPR-1ZI_D3C_2k.bin` | JPR-1ZI D3C (74S287) | Paměťový dekodér. Při BOOT=1 je ROM na 0000–07FF. Odpovídá původnímu D14 z JPR-1Z. |
| `jpr-1zi/JPR-1ZI_D3C_8k.bin` | JPR-1ZI D3C | Varianta s ROM na 0000–1FFF při BOOT=1. |
| `jpr-1zi/JPR-1ZI_D6B.bin` | JPR-1ZI D6B (74S287) | I/O dekodér portů 00–03h. |
| `jpr-1zi/JPR-1ZI_ECMON_2k_Martin.bin/.HEX` | EPROM JPR-1ZI | Monitor ECMON 2k „JPR-1Z 62K MON CZ“. |
| `jpr-1zi/JPR-1ZI_ECMON_2k_IDE-1_Martin.bin` | EPROM JPR-1ZI | ECMON s podporou IDE-1 (porty 5C–5Fh). |
| `jpr-1zi/JPR-1ZI_ZZRMD_epr2k.bin`, `_epr8k.bin` | EPROM JPR-1ZI | Zavaděč CP/M z RAM disku ZRMD-1. |
| `jpr-1zi/JPR-1ZI_PROM_Info.txt` | | Poznámka o označení: D3C = D14, D6B = D10 (původně). |
| `and-1z-89/AND-1Z_CharGen.bin/.hex` | AND-1Z-89 (2732) | Znakový generátor, 4 KB, 16 B na znak. |
| `and-1z-89/AND-1Z_CharGen_upr10-1F.bin/.hex` | AND-1Z-89 | Varianta s upravenými znaky 10–1Fh. |
| `and-1z-89/PROM_D1.hex.bin`, `PROM_DA1-D1.hex` | AND-1Z-89 D1/DA1 (74S287) | Adresový dekodér VRAM (F800–FFFF). |
| `and-1z-89/PROM_DD4-D24.hex` | AND-1Z-89 DD4/D24 | Horizontální časování. |
| `and-1z-89/PROM_DE6_D35.hex` | AND-1Z-89 DE6/D35 (74S571) | Vertikální časování. |
| `dmr-1/DMR-1_D2C.bin/.hex` | DMR-1 D2C (74S571) | Dekodér RAM, 8 konfigurací (JP3). |
| `mikromon/MikroMon1_3_f000.txt` | EPROM JPR-1ZI | MikroMon 1.3 (Intel HEX, zavádí se od 0000, běží od F000). Autor: Libor Lasota, použito s jeho souhlasem. |
| `mikromon/navod-k-MikroMon.pdf` | | Návod k MikroMonu. |

Zdrojové kódy MikroMonu se do tohoto repozitáře nedávají.
