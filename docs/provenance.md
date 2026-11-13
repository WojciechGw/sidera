# Pochodzenie

Podstawą wydzielenia jest zatwierdzony przez właściciela bieżący stan roboczy
projektu w repozytorium mtRAM, w tym pliki niezatwierdzone i nieśledzone.
HEAD źródła: `b4654cd`. Sam commit nie odtwarza tego stanu roboczego.

[source-manifest.json](source-manifest.json) zapisuje oryginalne ścieżki,
docelowe miejsca i SHA-256 plików przed dostosowaniem. Ścieżki źródłowe są
informacją historyczną; kompilacja i testy używają wyłącznie tego repozytorium.
Przeniesiono aplikację, wszystkie jej testy i generatory, dane, instrukcje,
wzorce obrazów, źródłowy BMP oraz lokalną kopię `rpc_common`, `rpmt_bare.c`,
`rpmt.h`, `rpmt_os_file.c` i `rpmt_os_dir.c`. Biblioteki sieciowe i system mtRAM
nie są potrzebne do linkowania SIDERY.

Narzędzia w `tools/` pochodzą z RP6502-SDK (`picocomputer/rp6502`). Były już
pobrane w repozytorium docelowym przed migracją; nie wprowadzono do nich
własnych zmian. Ich licencję zachowano w [LICENSES/RP6502-SDK.txt](../LICENSES/RP6502-SDK.txt).
Emulator jest przypięty do wersji 0.43. Oficjalne instalatory cc65 wskazuje
[dokumentacja SDK](https://picocomputer.github.io/sdk.html).

Własność aplikacji i przeniesionego kodu wspólnego została potwierdzona przez
autora. Nie nadano nowej licencji temu kodowi. Licencje danych i przypisania
zachowano; indeks znajduje się w [LICENSE-NOTICE.txt](../LICENSE-NOTICE.txt).

Istotne dostosowania: lokalne ścieżki, standardowe cele i pakowanie SDK,
generowanie do `build/`, przenośne adaptery testów oraz użycie `int32_t` dla
czterobajtowego pola przesunięcia w pliku stref czasowych. Ta ostatnia zmiana
usuwa założenie, że `long` ma cztery bajty; na cc65 nie zmienia formatu danych.
