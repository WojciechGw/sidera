# Architektura SIDERY

Samodzielny ROM dla RP6502-PICO i RP6502-EMU. [Budowanie](../README.md).

## Okno i podział

Jedno ciągłe okno `$C800–$DFFF` ma 6144 B, zaczyna się na granicy 2048 B
i nie zmienia adresu pomiędzy nakładkami. `$C800–$DEFF` (5888 B) mieści
kod, stałe, zainicjalizowane dane i BSS aktywnej nakładki. `$DF00–$DFFF`
(256 B) to niezależny bufor roboczy, w tym 224 B konfiguracji.

| Zasób `ROM:` | Zawartość | Odczyt do okna | BSS | Wolne w 5888 B |
| --- | --- | ---: | ---: | ---: |
| `sidera-init.ovl` | Opcje, konfiguracja, otwarcie danych | 4517 B | 0 B | 1371 B |
| `sidera-cities.ovl` | Wyszukiwanie i wybór miasta | 3579 B | 303 B | 2006 B |
| `sidera-edit.ovl` | Edycja lokalizacji, daty i strefy | 3106 B | 112 B | 2670 B |
| `sidera-cover.ovl` | Splash i początkowe zasoby VGA | 829 B | 0 B | 5059 B |
| `sidera-solar.ovl` | Orbity i panel Układu Słonecznego pod X | 5672 B | 82 B | 134 B |

Rozmiary po bieżącej kompilacji cc65; każdy zasób ma dodatkowo 24 B nagłówka,
czytanego poza oknem. Kontrola podczas budowania odrzuca przekroczenie granic.

Kod rezydentny, jego stałe i BSS kończą się na `$B3F1` (adres wyłączny).
Po odjęciu całego okna pozostaje **11023 B niezajętego RAM** poniżej stosu C:
5135 B w `$B3F1–$C7FF` i 5888 B w `$E000–$F6FF`.
Stos C zachowuje `$F700–$FEFF`, 2048 B. Górny wolny obszar nie jest obecnie
segmentem linkera ani stertą; wykorzystanie go przez przyszły kod wymaga
osobnego segmentu i uwzględnienia go w pakowaniu obrazu.

Dla porównywanej oryginalnej kompilacji standalone koniec danych wynosi
`$D5B1`, a wolna przestrzeń 8527 B. Po dodaniu funkcji X i usunięciu warstwy RMT pozostaje **2496 B**
więcej wolnego RAM niż w oryginale, po odjęciu okna, loadera i obsługi panelu.
Wolne miejsca wewnątrz nakładek nie są dodawane do tego wyniku.

Samodzielna obsługa sprzętu znajduje się w `src/platform/`. Bufor argumentów
monitora ma 512 B i jest rezydentny. `argv.c` jest bezpośrednim źródłem celu
`sidera`, aby linker nie zastąpił go domyślną implementacją cc65.
Pliki nakładek i konfiguracja linkera są w `src/overlays/`; ich układ
pamięci nie zależy od dawnego systemu RMT.

## Wywołania i trwałość danych

Jeden przebieg ld65 linkuje program i pięć plików pod tym samym adresem.
Wywołania mają zwykłe adresy ustalone w tej kompilacji; plików `.ovl` nie
można mieszać między kompilacjami. Packer nadaje rdzeniowi i nagłówkom
wspólny identyfikator kompilacji i zapisuje CRC32 każdego ładunku.

Rezydentny `main` wybiera nakładkę przez `ovl_select()` przed wywołaniem jej
funkcji. Nakładka nie może przełączać własnego okna ani zachowywać wskaźników
do niego po powrocie. Start przebiega: INIT prepare, opcjonalnie CITIES dla
`/location`, INIT apply, COVER, widok nieba. Podczas obserwacji doładowania
występują przy wejściu w K, L/D/Z lub X. Obliczenia nieba i zapis S są rezydentne.

## Panel X

X otwiera nieruchomy obraz Układu Słonecznego dla aktualnego `moment` (UTC).
Ponowne X zamyka go; kolejne otwarcie pobiera nowy czas. Stan panelu nie jest
zapisywany klawiszem S. Kwadrat **317×317 px** zajmuje x=301..617, y=93..409:
10 pustych pikseli dzieli go od prawej granicy wykresu, ostatniego znacznika
górnego kompasu i najwyższego znacznika dolnego kompasu (y=420).

Osiem orbit to koncentryczne okręgi o promieniach 30, 42, 54, 66, 83, 100,
117 i 134 px. Układ jest schematyczny, bez skali AU. Odstępy zapewniają brak
nakładania sprite'ów 8×8 px także przy zbliżonych kierunkach planet. Słońce
używa oryginalnego sprite'a 32×32 px, bez skalowania.
Angielskie nazwy ośmiu planet leżą bezpośrednio przy sprite'ach, bez linii
łączących. Rozmieszczenie uwzględnia obszar tekstu z tłem: omija Słońce,
wszystkie planety, pozostałe podpisy i granice panelu. Przy ciasnych układach
próbuje kolejnych położeń przy krawędziach sprite'ów, z odstępem do 10 px.
Księżyc to biała kropka 2×2 px bez podpisu, w schematycznej odległości 8 px
od środka Ziemi (po zaokrągleniu do pikseli). Kierunek wynika z geocentrycznej
długości ekliptycznej dla czasu snapshotu, obliczanej istniejącym modelem
Księżyca. Czarna obwódka o grubości 1 px oddziela kropkę od orbit.
Podpisy, w tym `Earth`, omijają cały obszar 4×4 px wraz z obwódką.

Kierunki heliocentryczne są obliczane dla zapamiętanej daty z
[elementów Schlytera](https://stjarnhimlen.se/comp/ppcomp.html#4), bez poprawek
perturbacyjnych. Widok jest od północy ekliptyki; kierunek równonocy jest
w prawo, 90° długości ekliptycznej w górę. Odległości nie są odwzorowane.
Nagłówek zawiera ` SOLAR SYSTEM ` czarnymi literami na białym tle po lewej,
obniżone o 8 px (tekst na y=105). `snapshot YYYY-MM-DD HH:MM:SS UTC` pozostaje
na y=97, wyrównane do prawej z marginesem 2 px.
U dołu są `Schematic view (not in AU scale)` oraz
`X: close`, wyrównane do prawej z marginesem 2 px. Oba wiersze skrótów
klawiszowych pod głównym wykresem zaczynają się na x=12 (przesunięcie o 8 px).

Sześć planet korzysta z istniejących obrazów i palet. Dodatkowy zasób
`ROM:sidera-solar.dat` zawiera Ziemię i Neptuna.
Deskryptory panelu zajmują XRAM `$B096–$B0EF`, obrazy `$E160–$E1DF`,
a dwie palety `$E200–$E5FF`, w dotychczasowych wolnych obszarach.

Przy otwartym panelu działają tylko X (zamknięcie) i Q (zakończenie programu).
Obliczenia, rysowanie nieba i symulacja są wstrzymane; w LIVE odświeżają się
wyłącznie zegary UTC/LOCAL na górnym pasku. Sprity nieba nachodzące na panel
są ukrywane do jego zamknięcia. X uruchamia ponowne przeliczenie i odrysowanie
wykresu dla bieżącego czasu. Symulacja wraca do wcześniejszego stanu PLAY/PAUSE.

Loader otwiera zasób przez `open/read/close`, sprawdza nagłówek, granice,
długość i CRC32, następnie zeruje BSS. Ostatnia poprawna nakładka pozostaje
w oknie; ponowny wybór tego samego ID pomija odczyt i zachowuje jej BSS.
Funkcje wejścia resetują dane ekranów jak wcześniej. S używa końcowych
256 B i nie unieważnia nakładki.

Błąd odczytu lub weryfikacji unieważnia okno i kończy program: pliki są
zamykane, konsola przywracana, a komunikat podaje zasób i przyczynę.
Nie ma ponowienia ani uruchomienia częściowo załadowanego kodu.

## Weryfikacja

Zestaw testów i obrazy kontrolne opisano w [tests.md](tests.md).
