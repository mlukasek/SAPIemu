# Beta: text pro testery

Podklad pro Martina k rozeslání bety 0.7.0-beta třem testerům alf (Libor, Petr, Michal) spolu se zipem z `python
tools\package.py --public` a souborem `.sha256` vedle něj (`package.py` ho píše, až zip projde kontrolami). Text pod
čarou jde poslat, jak je, jen se doplní termín v hranatých závorkách. E-mail ani jiný kontakt do repa nepíšeme,
testeři odpoví na zprávu, se kterou betu dostanou. Liborovi a Petrovi přidat žádost o souhlasy podle
`docs/vydani/souhlasy.md`, Liborovi i prosbu o aktuální verze jeho softwaru (MikroMon, MIKROBIOS, dema a programy
v `docs/vydani/sestavy.md`). Po jejich odpovědích přijde úprava a 0.7.1-beta jako první veřejná verze (cesta vydání:
`docs/vydani/plan-vzhled.md`, etapa 6 pod „→ BETA zip“). Soubor je v `docs/vydani/`, do veřejného zipu proto nejde.

---

## SAPIemu beta: prosba o vyzkoušení

Posílám betaverzi SAPIemu 0.7.0-beta, emulátoru Tesla SAPI-1. Po úpravách podle vašich připomínek z ní bude
0.7.1-beta, první veřejná verze na GitHubu.
Novinky: vzhled (Windows 11, světlý i tmavý), výběr sestav s náhledy, průvodci sestavami, nápověda a slovníček
v programu a bezpečnější ukládání (dodané sestavy se nepřepisují, změny jdou do Mých sestav).
Nejde mi o pochvalu, ale o to, kde se člověk, který program vidí poprvé, zasekne. Zabere to asi půl hodiny,
odpověď prosím do [datum].

**Spuštění:** zip rozbalte do **nové** složky a spusťte `sapiemu.exe`. Windows ukážou varování SmartScreen, pak
„Další informace“ → „Přesto spustit“. Nic se neinstaluje. Kontrolní součet zipu je v souboru `.sha256`
(`certutil -hashfile SAPIemu-….zip SHA256`). První kroky jsou v `PRVNI-KROKY.md`, anglicky v `GETTING-STARTED.md`.

### Pět úkolů

Zkuste je jen s tím, co ukáže program (okno Vybrat sestavu, průvodce sestavou, Nápověda). Když se zaseknete,
poznamenejte si kde, i když to pak vyřešíte.

1. **ZPS-1 a BASIC.** Po prvním spuštění se otevře okno Vybrat sestavu a za ním už běží ZPS-1. Napište program
   z průvodce (ručně, nebo tlačítkem Napsat do SAPI) a spusťte ho `RUN`. Pak přidejte vlastní řádek a vypište
   program příkazem `LIST`.
2. **APS-5.** Spusťte sestavu „Měřicí jednotka APS-5 (pece)“ a v okně APS-5 stiskněte DEMO. Asi za minutu pece
   žhnou. Pak zkuste tlačítko Výpadek sítě.
3. **Hra.** Spusťte „SAPI-1 V: hry a dema“, napište `3` a pak třeba `BADAPPLE` nebo `FLAPPY` (joystick
   z klávesnice zapne Ctrl+F8). Do CP/M se vrátíte klávesou ESC, nebo resetem F12.
4. **Soubor v CP/M.** Spusťte „ZPS-3 s CP/M 64K Tesla IMA“, napište `B:` a `MBASIC`, pak řádek `10 PRINT "AHOJ"`,
   `SAVE "POKUS"` a `SYSTEM`. `DIR` ukáže `POKUS.BAS`. Zavřete emulátor, spusťte ho znovu, zase ZPS-3 a `DIR B:`.
   Soubor tam má být, `B:` a `MBASIC POKUS` ho spustí. Navíc: Soubor → Uložit stav… a Načíst stav….
5. **Konfigurátor.** V ZPS-1 otevřete Zobrazení → Konfigurátor sestavy…, klikněte na kartu AND-1 (slot X3)
   a přepněte Generátor znaků na „MHB 2502 azbuka“. Tlačítko „Restartovat s touto sestavou“: obrazovka píše
   azbukou. Pak program zavřete. Zeptá se na neuložené změny, vyberte, co vám dává smysl, a napište proč.
   Uložit dodanou ZPS-1 nepřepíše, vznikne vaše kopie „ZPS-1 … (moje)“ ve skupině Moje sestavy v okně Vybrat
   sestavu a program ji příště spustí. Původní ZPS-1 píše latinkou dál, kopii smaže tlačítko Smazat… (u jiné
   běžící sestavy). Po Ponechat pro příští spuštění stiskněte v konfigurátoru Vrátit změny.

**Na konec čtyři otázky:**
- Co vás mátlo?
- Kde jste se zasekli, i jen na chvíli?
- Co byste vyhodili nebo schovali?
- Co je SAPIemu? Jednou větou, podle README nebo okna Vybrat sestavu.

### Co hlásit a jak

- **Nápověda → Nahlásit chybu… → Kopírovat diagnostiku** dá do schránky verzi, sestavu, karty, složky, systém
  a konec logu. Vložte to do odpovědi. Jsou v tom cesty na vašem PC, často i se jménem uživatele Windows.
- K tomu napište sestavu, co jste dělali, co jste čekali a co se stalo.
- Hodně pomůže:
  - snímek obrazovky Ctrl+F5 (uloží se do `work\snimky`);
  - stav uložený těsně před chybou (Soubor → Uložit stav…);
  - soubor sestavy, kterou jste změnili (vaše kopie jsou ve `work\machines`).
- **Po pádu** spusťte SAPIemu znovu a pošlete `sapiemu.prev.log` (Nahlásit chybu → Otevřít složku logu). Je to log
  spuštění, které spadlo. Další spuštění ho přepíše, pošlete ho proto hned.
- Odpovídejte na zprávu, se kterou jste betu dostali, česky nebo anglicky. GitHub Issues budou až po zveřejnění.

### Známá omezení

- Jen Windows 10 a 11, 64 bit. Exe není podepsané, proto SmartScreen. Linux jen překladem ze zdrojů.
- Podrobná dokumentace je česky. Anglicky je program, průvodci sestavami a `GETTING-STARTED.md`.
- Karty AND-30C, RAM-64 a RPD-1Z se zatím neemulují, v konfigurátoru jsou označené.
- Měřením na skutečném SAPI-1 jsou ověřené desky V. Původní desky Tesla vycházejí z dokumentace, schémat a dumpů.
  Otevřené otázky jsou v `docs/hw-otazky.md`.
- Chyby původního softwaru zůstávají věrně (CKM, MIKOS/K, BIOS ECBIM8, MTCOPY). Návody ukazují, jak je obejít.
- Sestavy s ECMONem (sestava V, hry): volby 0–2 čekají na řadič disket, který sestava nemá, stejně jako skutečná
  deska. Mačkejte `3`.
- Zvuk hraje jen při rychlosti 100 %.
- Disketa otevřená mimo složku `media` se používá přímo, bez pracovní kopie. Zápis změní originál.
- Stavy `.sapistate` z alf od 0.3.0 by měly jít načíst. Kdyby ne, je to chyba k nahlášení. Stavy z alf 0.1.0
  a 0.2.0 program odmítne („Stav uložila jiná verze SAPIemu (starší alfa)…“) a běžící sestava zůstane.
- Kdo měl alfu: když jsou okna jinde, než má být, pomůže Zobrazení → Výchozí rozložení oken. **F9 už není reset,**
  reset je jen F12. F9 teď nastavuje breakpoint v debuggeru jako ve Visual Studiu.
- Čtečky obrazovky s programem nefungují (knihovna Dear ImGui). Text obrazovky jde zkopírovat Ctrl+Shift+C.
- BomberNet po síti potřebuje internet. Spojení přes relay `api.mzpico.com` není šifrované, relay vidí IP adresu
  a kód místnosti.
