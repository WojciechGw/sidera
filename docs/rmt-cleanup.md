# Usunięcie pozostałości RMT

Zakres uzgodniony 13 listopada 2026: SIDERA działa samodzielnie na RP6502-PICO
i RP6502-EMU. Z czynnego kodu usunięto obsługę RMT/razemOS oraz dawne nazwy.
Informacje o pochodzeniu, manifest importu i licencje zachowano.

## Zmiany

- Usunięto API zadań, sygnałów, przełączania aplikacji i wspólnej konsoli,
  wariant `RPMT_BARE`, puste blokady oraz `yield`.
  Usunięto też nieużywane resetowanie katalogu gwiazd po przełączeniu zadania;
  odświeżanie po zmianie daty lub obserwatora nadal korzysta z `cs_begin()`.
- Z biblioteki `rpc_common` pozostawiono dekoder klawiatury w `src/platform/`.
  Nieużywane moduły ekranów tekstowych, przekazywania stanu między programami,
  katalogów i współdzielonego XRAM usunięto.
- Operacje plikowe korzystają bezpośrednio z cc65. Mała warstwa platformowa
  obsługuje zegar 125 Hz, oczekiwanie, UART i wykrywanie VGA.
- `src/platform/argv.c` zapewnia 512-bajtowy bufor argumentów monitora.
  Jest bezpośrednim źródłem programu: umieszczenie go wyłącznie w archiwum
  pozwala linkerowi wybrać domyślną implementację cc65 bez bufora.
- `src/startup.c` współdzieli start aplikacji z testami. Testy nie wycinają
  już starego wariantu inicjalizacji z tekstu `main.c`.
- `src/bare/` zastąpiono `src/overlays/`. Pięć nakładek, loader, sprawdzanie
  CRC, identyfikator kompilacji, układ XRAM i formaty danych pozostają.
- Zachowano wszystkie opcje startowe, w tym `/assets` i `/config`, zapis
  ustawień, klawisze, LIVE/SIM, astronomię, miasta, strefy i panel X.

## Porównanie z punktem odniesienia

| Miara | Przed | Po |
| --- | ---: | ---: |
| Pliki `.c`, `.h`, `.inc` w `src/` | 55 | 33 |
| Linie tych plików, włącznie z komentarzami i tabelami | 7357 | 4823 |
| Rozmiar `sidera.rp6502` | 1 226 243 B | 1 225 825 B |
| Koniec danych rezydentnych, adres wyłączny | `$B4B1` | `$B3F1` |
| Wolny RAM poniżej stosu po odjęciu okna nakładek | 10 831 B | 11 023 B |

Ubyło 2534 linii (34,4%), ROM zmniejszył się o 418 B, a wolnego RAM przybyło
192 B. Znaczna część usuniętych źródeł nie trafiała wcześniej do ROM-u,
dlatego redukcja kodu źródłowego jest znacznie większa niż redukcja obrazu.
W nakładce CITIES usunięto dodatkowo 160-bajtowy bufor zmiennej ścieżki.

Wszystkie cztery ROM-y (Windows/WSL, Debug/Release) są identyczne bajtowo.
SHA-256 wersji po refaktoryzacji:

```text
770902ca485a5e645273283514be52e1ee37e4bf18a9588e069c95baed3209df
```

## Weryfikacja automatyczna

Wyniki końcowe z 13 listopada 2026:

| Środowisko | Debug | Release |
| --- | --- | --- |
| Windows, cc65 + MSVC | PASS | PASS |
| Ubuntu 24.04 / WSL 2, cc65 + GCC | PASS | PASS |

Każdy przebieg obejmuje 24 testy hosta/sim65/obrazów oraz kontrolę ROM-u,
test rzeczywistego emulatora i odtworzenie zasobów: 27 testów i dwie fixtures SDK.
WSL uruchamia emulator Windows przez istniejący adapter projektu; nie jest to
test natywnego emulatora Linuksa ani test sprzętu PICO.

Sześć obrazów kontrolnych zgadza się ze wzorcami piksel po pikselu.
Obejrzano też zrzuty widoku nieba i panelu X z rzeczywistego ROM-u w emulatorze.
Porównanie bajtowe potwierdziło brak zmian w 36 plikach danych, zasobów,
licencji, manifestu i podstawowych modułów astronomicznych. Generatory
odtwarzają dostarczone pliki bez zmiany drzewa źródeł.

Dodany test `sidera_platform_6502` obejmuje rzeczywisty kod czasu i dekodera
klawiatury. Emulator potwierdza działanie argumentów `/config` i `/date`,
przełączanie nakładek, zapis S oraz wyjście Q. Zachowane testy sprawdzają
również uszkodzone nakładki, sprzątanie po błędach i wstrzymanie symulacji
w panelu X. Usunięto wyłącznie scenariusze dotyczące zadań i sygnałów RMT.

Przeszukanie czynnego kodu, testów, generatorów i CMake nie wykazuje odwołań
do `rpmt`, `rpc_common` ani `RPMT_BARE`. `git diff --check` przechodzi.
Punkt odniesienia i kopia stanu roboczego są w lokalnym
`build/audit/pre-rmt-cleanup-20261113-074717/`, a logi w
`build/audit/cleanup-*.log` i katalogach `Testing/Temporary/`.
Nie wykonywano commitów, publikacji ani transferu na PICO.

## Kontrola na fizycznym PICO

Test na urządzeniu wykonuje właściciel. Automatyczna weryfikacja nie wysyła
ROM-u na PICO. Plik do przeniesienia:
`build/cc65/release/sidera/sidera.rp6502`.

1. Uruchom ROM z osobną konfiguracją, aby łatwo rozpoznać wynik zapisu:

   ```text
   LOAD MSC0:/sidera.rp6502 /config MSC0:/sidera-check.cfg /date "2026-11-12 20:00:00"
   ```

2. Sprawdź około pięciosekundowy ekran powitalny, widok nieba i wskazaną datę
   w trybie SIM/PAUSE. Poczekaj na zakończenie obliczeń wschodów/zachodów.
3. Sprawdź strzałki, Shift+strzałki, G/T/C/P oraz edycję L/D/Z: Enter zatwierdza,
   Esc anuluje; Home/End/Backspace/Delete działają w edytorze.
4. W K wyszukaj miasto, zatwierdź i sprawdź lokalizację oraz strefę. W X
   sprawdź panel planet, zamknięcie X i powrót do aktualnego widoku nieba.
5. Sprawdź Space, `+`, `-`, `[` i `]`, a następnie N i aktualizację zegara LIVE.
6. Naciśnij S, zakończ Q i uruchom ROM ponownie z samym
   `/config MSC0:/sidera-check.cfg`. Ustawienia powinny zostać odtworzone.
7. Uruchom `/help` oraz błędną opcję, np. `/fov 55`: pomoc lub błąd powinny
   wrócić do monitora. Jeśli używasz zasobów zewnętrznych, sprawdź również
   `/assets MSC0:/sidera.dat` z niezmienionym plikiem danych.

Wynik testu na PICO pozostaje do potwierdzenia przez właściciela.
