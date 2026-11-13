# SIDERA

Samodzielne obserwatorium nieba dla **RP6502-PICO i RP6502-EMU**. Projekt
korzysta z cc65 i [RP6502-SDK](https://picocomputer.github.io/sdk.html),
tworząc jeden `sidera.rp6502` z kompletem danych i pięcioma nakładkami.
Nie wymaga repozytorium mtRAM ani systemu razemOS.

## Wymagania

- CMake 3.28+, Ninja, Python 3.12+, Git.
- Aktualny cc65 z obsługą RP6502, w tym `cl65`, `ca65`, `ld65`, `ar65`, `sim65`.
  Instalacja według [instrukcji SDK](https://picocomputer.github.io/sdk.html#installing-the-prerequisites).
- Do testów hosta: Windows — Visual Studio 2022 Build Tools z C++ i Windows SDK;
  Linux — GCC i Make (`build-essential` w Ubuntu).

Testy i generatory korzystają ze standardowej biblioteki Pythona. Dane odniesienia,
źródłowa grafika oraz przypięty pakiet tzdata są lokalne. Pierwsza konfiguracja
SDK może pobrać emulator 0.43; późniejsze kompilacje używają lokalnej kopii.
Nie jest pobierany emulator WEB.

## Budowanie i testy

Z katalogu projektu, w PowerShell lub powłoce Linuksa:

```text
cmake --preset cc65/Debug
cmake --build --preset cc65/Debug
ctest --preset cc65/Debug

cmake --preset cc65/Release
cmake --build --preset cc65/Release
ctest --preset cc65/Release
```

Wyniki: `build/cc65/debug/sidera/sidera.rp6502` i
`build/cc65/release/sidera/sidera.rp6502`. Obok są `.map`, `.lbl`, `.dbg`,
`.sym`, rdzeń i pięć `.ovl`. Plików z różnych kompilacji nie wolno mieszać.
Kompilacja automatycznie kontroluje strukturę ROM-u, CRC, identyfikator kompilacji,
BSS i granice pamięci. Każda konfiguracja buduje również natywne programy testowe.

Oba presety zachowują oryginalną optymalizację `-O`, ponieważ rozmiary nakładek
są częścią architektury. Debug używa `-g`, Release `-DNDEBUG`; SDK dostarcza
informacje do debuggera również dla plików wynikowych Release. Natywne testy
mają aktywne asercje w obu konfiguracjach.

Windows i WSL muszą używać osobnych katalogów wynikowych. Na WSL dla tego
samego checkoutu:

```text
cmake --preset cc65/Debug -B build/linux/cc65/debug
cmake --build build/linux/cc65/debug
ctest --test-dir build/linux/cc65/debug --output-on-failure
```

Dla Release użyj presetu `cc65/Release` i katalogu `build/linux/cc65/release`.
SDK wybiera w WSL emulator Windows; test korzysta z lokalnego adaptera ścieżek
`scripts/emu_wsl.py`. Na zwykłym Linuksie SDK wybiera emulator Linuksa.

## Uruchamianie

W VS Code wybierz preset, cel `sidera` oraz konfigurację `RP6502-EMU` lub
`RP6502-PICO`. F5 dla PICO **wysyła program na sprzęt** — uruchamiaj ją świadomie.
Ustawienia połączenia PICO (`device`, `key`, `workdir`, `args`) znajdują się
w lokalnym, ignorowanym `.rp6502`, zgodnie z SDK. Nie zapisuj kluczy w repozytorium.

Na sprzęcie domyślna konfiguracja to `MSC0:/sidera.cfg`. `/config` pozwala
wybrać inną ścieżkę. Konfiguracja EMU w VS Code przekazuje
`/config SAVE:/sidera.cfg`, aby umożliwić trwały zapis w emulatorze.
Przy uruchamianiu z wiersza poleceń podaj te same argumenty po `--`.
Test emulatora używa własnego pustego katalogu zapisu i nie korzysta z urządzenia.

[Instrukcja użytkownika](src/rom/sidera-manual-en.txt) opisuje klawisze i opcje.
Pomoc i instrukcja są też zasobami `ROM:help` oraz `ROM:manual`.

## Struktura

| Katalog | Zawartość |
| --- | --- |
| `src/` | Aplikacja, lokalny kod wspólny i platformowy, linker, dane, zasoby |
| `scripts/` | Generatory, przygotowanie nakładek i adapter testów WSL |
| `cmake/` | Integracja nakładek i testów ze standardowym SDK |
| `tests/` | Testy hosta, sim65, całego ROM-u i renderery kontrolne |
| `tools/` | Narzędzia dostarczane i aktualizowane przez SDK |
| `docs/` | Architektura, testy, pochodzenie i raport weryfikacji |
| `build/` | Wszystkie wyniki kompilacji, generowania i testów |

## Odtwarzanie zasobów

```text
python scripts/regenerate.py --output build/generated --check
cmake --build --preset cc65/Debug --target sidera_assets
```

Pierwsze polecenie odtwarza zasoby offline i porównuje je z dostarczonymi
plikami. Drugie tylko je generuje. Zwykła kompilacja używa wersji z `src/rom/`
i `src/tables/`. Generatory zapisują wyniki do `build/`; aktualizacja źródeł
danych wymaga osobnego, świadomego skopiowania przejrzanych wyników do `src/`.
Opcjonalne importery i skrypty pobierania danych odniesienia zachowano,
ale nie są uruchamiane podczas kompilacji ani testów.

## SDK, CI i WEB

`cmake -P tools/rp6502.cmake` aktualizuje narzędzia SDK. Własne rozszerzenia
znajdują się poza `tools/`. Aktualizator może także zmieniać `.vscode/`,
`.gitignore` i główny CMake — po aktualizacji przejrzyj diff i uruchom testy.
W szczególności może ponownie dodać konfigurację uruchamiania WEB.

WEB jest wyłączony: brak wywołania `rp6502_web()`, paczki i aktywnej konfiguracji.
Oryginalne pliki zachowano jako `.github/workflows/web.yml.disabled` oraz
`.vscode/launch-web.json.disabled`. Nie przywracaj ich aktywnych nazw bez
osobnej decyzji o WEB. Workflow `ci.yml` buduje i testuje Windows/Linux ×
Debug/Release oraz zachowuje artefakty. Nie publikuje strony ani wydań.

## Dokumentacja i licencje

- [Architektura i pamięć](docs/architecture.md)
- [Testy, generatory i sześć widoków](docs/tests.md)
- [Pochodzenie źródeł](docs/provenance.md)
- [Raport lokalnej weryfikacji](docs/validation.md)
- [Informacje licencyjne](LICENSE-NOTICE.txt)

Wydzielenie nie nadaje nowej licencji własnemu kodowi autora. Zachowano
licencje i przypisania danych oraz narzędzi zewnętrznych.
