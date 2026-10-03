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
| `mikrobasic/MIKROBASIC_2.4_ZPS-1.bin` | REM-1 EPROM 0000–0FFF (4× 2708) | MIKROBASIC 2.4 s Mikromonitorem (TESLA ELTOS 1983), verze ZPS-1, RAM na REM-1 4000–47FF. Zdroj `H:\SAPI-1\_ROM\vypis_programu_MBASIC.BIN`, MD5 a9dde20b…, shodný s dumpy čtyř skutečných 2708 (`SAPI_DOKU_2011-09-10\EPROM_od_PEPAX\SAPI\MBASIC_v24_0..3.BIN`) a s HEX na sapi.cz. |
| `mikrobasic/MIKROBASIC_AR1983_1k.bin` | EPROM 0000–0FFF | Starší MIKROBASIC z AR/B 1983, 1 KB RAM na JPR-1 (2000–23FF). Zdroj `Foto\Jiri_Slansky\SW\JPR-1 MicroBASIC 1k`, MD5 6d42ad34…. |
| `mikos/MIKOS_V4.0F_ANK-1.bin` | REM-1 EPROM 0000–0FFF | Monitor V4.0 + MIKOS-1 V4.0F pro ANK-1 (ZPS-2). Zdroj `H:\SAPI-1\_ROM\MIKOS v4\SAPI_V4_0_MIKOS-1_V4_0F.BIN`, MD5 0da74a65…, shodný s výpisy `m40f`/`mi40f` na sapi.cz. První 1 KB je shodný s dumpem 2708 `SAPI_DOKU_2011-09-10\EPROM_od_PEPAX\SAPI\SAPI_V4_0.BIN`. |
| `mikos/MIKOS_V4.0D_ANK-1.bin` | REM-1 EPROM 0000–0FFF | Monitor V4.0 (C) + MIKOS-1 V4.0D. Zdroj `_ROM\MIKOS v4\SAPI_V4_0_MIKOS-1_V4_0D.BIN`, MD5 e7f3f09a…. |
| `mikos/MIKOS_V4.1F_Consul.bin` | EPROM 0000–0FFF | Monitor V4.1 + MIKOS-1 V4.0F pro klávesnici Consul. Zdroj `_ROM\MIKOS v4\SAPI_V4_1_MIKOS-1_V4_0F.BIN`, MD5 52936e84…. Další varianty (V4.1H, dumpy 2716 `Klasika\MIKOS4\36.BIN`, `37.BIN`) nejsou v repu. |
| `and-1/MHB2501.bin` | AND-1 znakový generátor MHB 2501 (latinka) | **Překreslený**, ne dump (p-MOS ROM, dump neexistuje). Podle tabulky v katalogu MHB250x (`Foto\Jiri_Slansky\Dokumentace\AND-1 a 1A\MHB250x.png`), automaticky převedeno, několik vad skenu opraveno ručně. 64 znaků × 8 řádků, bity 4–0 = sloupce zleva, řádek 8 nevyužit. |
| `and-1/MHB2502.bin` | AND-1 MHB 2502 (azbuka) | Překreslený stejně. Kódy 20–3Fh převzaté z MHB 2501. |
| `and-1/AND-1_D3C_74188.bin` | AND-1 D3C (74188) | Kurzor, blikání a dvojitá šířka, 32 B z výpisu na sapi.cz (`and-1.php`). |

Zdrojové kódy MikroMonu se do tohoto repozitáře nedávají.
