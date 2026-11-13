# Weryfikacja SIDERY

`ctest --preset cc65/Debug` i `ctest --preset cc65/Release` uruchamiają:

- `sidera_host_suite`: testy hosta i sim65 oraz porównanie pikseli
  sześciu obrazów ze wzorcami (łącznie 24 testy). Obejmują astronomię,
  katalogi, strefy czasowe, wschody/zachody, konfigurację, grafikę, podpisy,
  loader i pełną pętlę aplikacji z nakładkami oraz scenariusze błędów.
  `sidera_platform_6502` sprawdza kod obsługi zegara 125 Hz, przepełnienie
  16-bitowego licznika, oczekiwanie, wykrywanie VGA, konsolę, bufor argumentów
  monitora i dekoder klawiatury (sekwencje VT, modyfikatory, Esc i timeout).
  Rejestry RIA i czas są kontrolowane przez test, kod wykonuje sim65.
- `sidera_rom`: kontrolę faktycznie zbudowanego ROM-u: tylko rdzeń i wektor
  resetu w pamięci początkowej, pięć nakładek jako zasoby, nagłówki, CRC32,
  wspólny identyfikator, BSS, adresy rezydentne i stos. Ta kontrola jest też
  obowiązkowym krokiem pakowania.
- `sidera_emu`: rzeczywisty ROM w RP6502-EMU, z ustaloną datą i ziarnem
  losowego wypełnienia RAM. Start z ekranem powitalnym, widok nieba, panel X,
  edycja L, miasta K, zapis S do `SAVE:` i wyjście Q. Sprawdzane są stany
  aplikacji, brak błędu nakładki i obecność zapisanego pliku.
  Zapisany czas musi odpowiadać opcji `/date`, co dodatkowo sprawdza odbiór
  argumentów monitora przez rzeczywisty ROM.
- `sidera_generated_assets`: odtworzenie katalogu, tabel, grafik, miast
  i stref offline oraz porównanie z dostarczonymi plikami. Dla tekstów
  normalizowane są wyłącznie końce wierszy.

SDK dodaje dwie pomocnicze fixtures: `rp6502.build` i `rp6502.save`.
Wszystkie asercje testów pozostają aktywne również w Release.
Testy nie wysyłają niczego na PICO i nie pobierają danych astronomicznych.

Start aplikacji i testy opcji korzystają ze wspólnego `src/startup.c`
oraz `src/overlays/boot.c`. Adaptery hosta podstawiają pliki, czas i RIA,
bez emulowania systemu zadań lub blokad. `SIDERA_OVERLAYS` rozróżnia
rozmieszczenie kodu w ROM-ie i natywne testy; nie wybiera wariantu systemowego.

## Obrazy

Test hosta zapisuje stan XRAM rzeczywistych algorytmów renderowania.
`reference_views.py` tworzy sześć PNG i trzy konfiguracje w
`build/cc65/<debug|release>/host-native/views/`:

- `sidera-testA.png`, `sidera-testB.png`, `sidera-testC.png`;
- `sidera-solarA.png`, `sidera-solarB.png`, `sidera-solarC.png`.

Oryginalne wzorce znajdują się w `src/assets/`. Są to renderingi testów
hosta; nie należy przedstawiać ich jako zrzutów z fizycznego urządzenia.
Osobne zrzuty rzeczywistego emulatora (`sky.png`, `solar.png`) powstają
w `build/cc65/<debug|release>/test/sidera_emu/`.

## Diagnostyka

Szczegółowy wynik pojedynczego testu hosta:

```text
ctest --test-dir build/cc65/debug/host-native -C Debug -R sidera_timezone --output-on-failure
```

Logi CTest znajdują się w `Testing/Temporary/` głównego oraz natywnego
katalogu budowania. Nie mieszaj katalogów Windows i WSL. Test stref czasowych
kopiuje plik danych do natywnego katalogu tymczasowego, aby setki tysięcy
małych odczytów nie przechodziły przez montowanie dysku Windows w WSL.

Podczas debugowania pamiętaj, że pięć nakładek ma te same adresy wykonania.
Niektóre symbole mogą być niejednoznaczne, a breakpoint pod adresem nakładki
odnosi się do aktualnie wczytanego kodu. Testy loadera i mapy nie zastępują
weryfikacji czasu przełączania i urządzeń na fizycznym PICO.

## Generatory

`scripts/regenerate.py --output build/generated --check` uruchamia lokalne
generatory `constellations.py`, `assets.py`, `solar_assets.py`, `cities.py`
i `timezones.py`. Bez `--check` tylko generuje wyniki. Importery katalogów
przyjmują `--import-dir`; wynik importu pozostaje w katalogu wynikowym do
ręcznego przeglądu. `tests/fetch_reference.py` i `fetch_planets.py` zachowano
do świadomego odświeżania danych odniesienia; wymagają sieci i nie są częścią CI.
Domyślnie zapisują wyniki w `build/references/`; `--output` zmienia miejsce
zapisu. Wymiana wzorców w `tests/` wymaga przeglądu i jawnego skopiowania.
