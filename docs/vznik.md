# Vznik projektu: původní zadání

Původní zadání projektu z jeho prvního dne (3. 10. 2026), jak ho napsal Martin Lukášek, ponechané beze změny jako
doklad, že cílem od začátku bylo ověřovat emulaci proti skutečným deskám.

---

Emulátor počítače Tesla SAPI-1. Počítač je 8-bitový založený na procesoru 8080 a Z80. Je velmi modulární, do sběrnice si uživatel naskládal libovolné karty z nabídky a podle toho spustil systém MIKOS nebo CP/M. Emulátor by měl být stejně modulární a konfigurovatelný, aby šlo vybrat sestavu a jednotlivé desky a podle toho by se choval.



Zdroje informací:

https://www.sapi.cz/sapi/sapi.php - základní poměrně hodně důvěryhodný informační zdroj, kompletní popisy všech desek (Technické prostředky), sestav (DPS), programového vybavení a další dokumentace

https://www.8bity.cz/category/ceskoslovenske-pocitace/sapi-1/ - moje vlastní články na blogu, jak jsem si stavěl některé SAPI desky

https://www.pmd85.cz/ - stránky Libora Lasoty, hodně projektů se týká počítače SAPI-1, viz. např. https://www.pmd85.cz/?page\_id=1858 (seznam není úplně aktualizovaný, je potřab projí i ostatní, případně doplním, mám část víc informací, než je na stránkách)

https://martin.hinner.info/iccmap/cz/cs/sapi\_1.html

http://webserver.ics.muni.cz/bulletin/articles/579.html

http://www.osmibitovemuzeum.cz/cssr/sapi.html

http://www.osmibitovemuzeum.cz/fotogalerie/sapi-1\_2.html

http://www.kompjutry.cz/soubory/pocitacecssr\_html/tesla\_sapi1.html

https://en.wikipedia.org/wiki/SAPI-1 - velmi základní informace

https://github.com/Stan6314/SAPI-1-Emulator - jednoduchý emulátor základní konfigurace, netuším jak kvalitní

https://pmd85.borik.net/wiki/Blog:Postavte\_si\_PMD\_32-SD - informace k emulátoru disketových jednotek PMD32-SD



Jako první ty bych rád implementoval ty desky, co jsem si stavěl, protože můžeme ověřit emulaci proti reálnému hardwaru:

ARB-1C\_8slot - deska sběrnice s 8 sloty, počet slotů může v emulátoru být teoreticky libovolný, pouze první slot má specifickou funkci pro procesorové karty až na pozdější PKT-1, který se zapojoval vedle ostatních desek a budiče na adresové sběrnici se odstavil vypnutím AEN

EKL-1 - emulátor klávesnice, umožňuje připojení PS/2 klávesnice k JPR-1Z a podobným kartám

ZRMD-1 - zálohovaný RAM disk, implementoval bych jako permenentní s ukládáním dat do souboru a volbou datového souboru, její modernější reinkarnace má označení ZRD-1V

AND-1Z-89 - deska video výstupu na monitor

IDE-1 - deska připojení LBA IDE disku a speciálního emulátoru disketových mechanik PMD32-SD

DMR-1 - deska sériového interface a RAM kombinující desky DSM-1 a RAM-1Z

JPR-1ZI - procesorová deska s CPU Z80, její modernější reinkarnace je označená JPR-1V a je velmi podobná

PMD32-SD - emulátor disketového řadiče s disketovými mechanikami

MikroMon - toto není deska, ale software, rád bych ho použil na první testy, stručný popis na https://www.pmd85.cz/?page\_id=2286 a návod v navod-k-MikroMon.pdf, kód v Intel HEX v souboru MikroMon1\_3\_f000.txt



Všechny výše zmíněné desky mají zdroje o adresář výše, nechtěl jsem je dělat součástí projektu, postupně budu doplňovat ostatní.



