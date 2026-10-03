# Sestavení a spuštění

## Visual Studio 2022

1. Soubor → Otevřít → Složka, vybrat kořen repa.
2. Vybrat preset (`Windows x64 Debug`, případně lokální `Local x64 Debug`).
3. Jako Startup Item zvolit `sapiemu.exe` (GUI), nebo `sapiemu-cli.exe` (bez okna).

## Příkazová řádka

```
tools\build.cmd debug test          # preset windows-debug, výstup v out\build\windows-debug
tools\build.cmd local-debug test    # lokální preset (výstup mimo repo)
tools\build.cmd release             # release build, zahrnuje pomalé testy ZEXDOC/ZEXALL
```

Klávesy v GUI: F5 běh/pauza, F8 rychlost (takt CPU → 4 MHz → maximum, dokola), F10 krok přes, F11 krok, F12 nebo F9 reset (pod debuggerem jen F9).

## Lokální preset (necommituje se)

Když repo leží na síťovém disku, je rychlejší ukládat výstupy sestavení na lokální disk. Stačí v kořeni repa vytvořit
`CMakeUserPresets.json`:

```json
{
  "version": 6,
  "configurePresets": [
    { "name": "local-debug", "displayName": "Local x64 Debug (build on C:)", "inherits": "windows-debug",
      "binaryDir": "C:/SAPIemu-build/${presetName}" },
    { "name": "local-release", "displayName": "Local x64 Release (build on C:)", "inherits": "windows-release",
      "binaryDir": "C:/SAPIemu-build/${presetName}" }
  ],
  "buildPresets": [
    { "name": "local-debug", "configurePreset": "local-debug" },
    { "name": "local-release", "configurePreset": "local-release" }
  ],
  "testPresets": [
    { "name": "local-debug", "configurePreset": "local-debug", "output": { "outputOnFailure": true } },
    { "name": "local-release", "configurePreset": "local-release", "output": { "outputOnFailure": true } }
  ]
}
```

## Datová složka

Emulátor hledá složku s `roms/` a `machines/`: nejdřív od složky s exe směrem nahoru, pak od aktuální složky
a nakonec ve zdrojové složce, ze které byl sestaven. Jinou složku lze zadat parametrem `--data-root`.

## Příklady pro režim bez okna

```
sapiemu-cli --wait-text "V1.3" --type "?\r" --run-ms 300 --print-screen
sapiemu-cli --machine machines/mikromon.jsonc --run-ms 500 --screenshot obrazovka.png
sapiemu-cli --keyboard "Consul 262.3 (bez 7474)" --wait-text "V1.3" --type "M F000\r" --run-ms 500 --print-screen
```

Úplný seznam akcí vypíše `sapiemu-cli --help`.
