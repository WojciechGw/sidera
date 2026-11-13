# Raport weryfikacji wydzielenia

Ten raport dokumentuje stan z 12 listopada 2026. Aktualne wyniki po usunięciu
warstwy RMT opisano w [raporcie refaktoryzacji](rmt-cleanup.md).

Weryfikacja lokalna: 12 listopada 2026, Windows i Ubuntu 24.04 w WSL 2.
Kompilator na obu systemach: cc65 V2.19, Git `6fd2669`. W WSL zainstalowany
oficjalnym instalatorem wskazanym w dokumentacji SDK. Emulator: RP6502 0.43.
Testy hosta: MSVC 2022 na Windows, GCC w Ubuntu. Python: 3.14.8 / 3.12.3.

| Środowisko | Konfiguracja | Czyste budowanie | Pełne testy |
| --- | --- | --- | --- |
| Windows | cc65/Debug | PASS | PASS |
| Windows | cc65/Release | PASS | PASS |
| Ubuntu 24.04 / WSL 2 | cc65/Debug | PASS | PASS |
| Ubuntu 24.04 / WSL 2 | cc65/Release | PASS | PASS |

Każdy przebieg obejmował 23 testy hosta/sim65/obrazów oraz kontrolę ROM-u,
rzeczywisty emulator i odtworzenie zasobów: **26 testów**, dodatkowo dwie
fixtures SDK. Po rozszerzeniu kontroli emulatora ponownie sprawdzono wszystkie
cztery konfiguracje: zakończenie obliczeń wschodów i zachodów, panel X,
edycję L, otwarcie/zamknięcie miast K, zapis S i wyjście Q.

WSL kompiluje i wykonuje testy hosta oraz sim65 jako programy Linuksa.
Zgodnie z wyborem SDK korzysta z emulatora Windows przez interop; adapter
projektu tłumaczy ścieżki systemu plików. Natywny emulator Linuksa jest
przewidziany dla joba Ubuntu w CI. Workflow GitHub Actions przygotowano,
ale nie uruchamiano go na GitHub — nie wykonano push ani publikacji.

## Wynik

Wszystkie cztery ROM-y są identyczne bajtowo, mają **1 226 243 B** i SHA-256:

```text
8ac123604cd3e2e630fd80288596b732e35d176e10a8bf6f830282ab1cc8baf1
```

Wersja Release Windows: `build/cc65/release/sidera/sidera.rp6502`.
Wyniki WSL są w `build/linux/cc65/<debug|release>/sidera/`.
Pakowanie standardowym SDK zachowuje rdzeń pod `$0200`, reset `$0200`,
pięć nazwanych zasobów nakładek, ich identyfikator kompilacji i CRC32.

| Nakładka | Ładunek | BSS | Zapas w 5888 B |
| --- | ---: | ---: | ---: |
| INIT | 4591 B | 0 B | 1297 B |
| CITIES | 3653 B | 463 B | 1772 B |
| EDIT | 3106 B | 112 B | 2670 B |
| COVER | 846 B | 0 B | 5042 B |
| SOLAR | 5678 B | 82 B | 128 B |

Każda nakładka ma dodatkowy nagłówek 24 B. Okno kodu i BSS:
`$C800–$DEFF`; bufor roboczy: `$DF00–$DFFF`; stos C: `$F700–$FEFF`.
Koniec danych rezydentnych: `$B4B1` (adres wyłączny).
Wolne RAM poniżej stosu po odjęciu okna: **10 831 B**.

## Obrazy i źródła

Wygenerowano i obejrzano A/B/C oraz solar A/B/C. Wszystkie sześć obrazów
Windows i WSL ma identyczne piksele jak zachowane wzorce `src/assets/`.
Obejrzano również zrzuty działającego ROM-u w emulatorze. Obrazy i konfiguracje
testowe są w `build/cc65/<debug|release>/host-native/views/`.

Generatory odtwarzają dane i tabele offline; ich wyniki zgadzają się z plikami
dostarczonymi w `src/`. Kontrola obejmuje także brak zmian drzewa źródłowego
podczas generowania. Sprawdzono SHA-256 wszystkich **126 plików wejściowych**
z manifestu — pliki źródłowego mtRAM nie zostały zmienione.

Pełne lokalne logi są w `build/audit/`, szczegółowe logi testów w
`Testing/Temporary/` odpowiednich katalogów budowania. `git diff --check`
nie zgłasza błędów. Nie wykonywano commitów, publikacji ani transferu na PICO.
Obsługa PICO jest skonfigurowana; nie stanowi to testu na fizycznym urządzeniu.
