# Ekosystem — koncepcje współpracy

> 2026-09-30 · Notatka z brainstormu. Możliwe kierunki, nie opis gotowych integracji ani zlecenie ich wdrożenia.

## 1. Wspólny opis ekosystemu

Projekty rozwijamy jako ekosystem wzajemnie użytecznych zdolności, a nie sztywny łańcuch aplikacji o rozłącznych rolach. Każdy może udostępniać innym wiedzę, narzędzia, sposoby interakcji i doświadczenia, a także z nich korzystać. Współpraca nie wymaga rezygnacji z samodzielnej użyteczności projektów.

**Najbardziej bezpośredni kierunek to ChatADHD jako interfejs do AGEDS oraz do części lub całości WatchDoga.** Nie tylko do zadawania pytań o wyniki, ale również do pracy z materiałem i kierowania zadaniami. Dalej można rozważać ChatADHD jako interfejs do pozostałych projektów. Szersza filozofia dopuszcza wszelkie możliwe powiązania interfejsów z danymi i sterowaniem: rozmowę, głos, graf, widok przestrzenny, gesty czy urządzenia. To horyzont koncepcyjny, nie obecny plan implementacji; czat nie musi zastępować innych interfejsów.

**Przepływ jest dwukierunkowy.** iOmatrix nie jest wyłącznie wejściem i wyjściem: może korzystać ze struktur wiedzy i pamięci rozwijanych w ekosystemie, np. do zapisywania nauczonych rzeczy o użytkowniku, jego słowniku, preferencjach, otoczeniu i sposobach działania. Wiedza ta nie musi należeć do interfejsu ChatADHD. Analogicznie inne projekty mogą wzajemnie korzystać ze swoich metod i doświadczeń.

Wspólne obszary do rozważenia to pamięć i kontekst pracy, schowek dla grafów i innych struktur, zaznaczanie nieostre, przechodzenie między reprezentacjami, pamięć korekt i niedokończonych zadań, pochodzenie informacji oraz przenoszenie nauczonych procedur. Obserwacja, wypowiedź użytkownika, hipoteza modelu i wynik działania pozostają rozróżnialne. Współdzielenie nie oznacza automatycznego dostępu do wszystkich danych ani uprawnień do działania.

Mapa obejmuje ChatADHD, iOmatrix (repozytorium Custom-Keyboard-Pro), Loom, AGEDS, WatchDog, program LEM i jego Workbench oraz PixelSpace AR. Otwarta pozostaje także na książkę/meta-książkę, wątki Legal Flow, narzędzia multimedialne i rekonstrukcję scen, agentów i avatar, DevBox i środowiska pracy, analizę archiwów oraz programy badawcze takie jak RCH. Dawny projekt może wrócić jako samodzielny produkt, współdzielona zdolność albo źródło metod; nie oznacza to automatycznego wznowienia wszystkich prac.

Nie rozstrzygamy tutaj jednej aplikacji, bazy, technologii, podziału repozytoriów ani ostatecznego modelu danych. Różne grafy nie muszą mieć tej samej semantyki. Zachowujemy alternatywy i szukamy rzeczywistych korzyści współpracy zamiast łączyć wszystko na siłę. Ta notatka nie zmienia bieżących priorytetów, kontraktów ani kryteriów gotowości funkcji.

## 2. Znaczenie dla Loom

Punkt wyjścia: wydzielanie współużywalnych zdolności silnika ChatADHD z zachowaniem istniejącej kompatybilności. Szersza rola w ekosystemie pozostaje kierunkiem, nie opisem gotowego uniwersalnego silnika.

- **Struktury dostępne poza czatem:** pamięć relacji, kontekstu i historii zmian może służyć iOmatrix, AGEDS, WatchDogowi i innym narzędziom bez zależności od interfejsu ChatADHD.
- **Pamięć użytkownika i działania:** preferencje, korekty, intencje, nauczone procedury oraz ich zastosowania mogą być powiązaną wiedzą zamiast osobnymi zbiorami ustawień w każdej aplikacji.
- **Wspólne operacje na przedmiocie pracy:** zaznaczanie nieostre, schowek strukturalny i przechodzenie między reprezentacjami są kandydatami do współużywalnych zdolności, przy zachowaniu różnic między grafem rozmowy, dowodów, transformacji i fabuły.
- **AGEDS, WatchDog i LEM:** możliwa współpraca przy pochodzeniu, niepewności, sprzecznościach i wersjach wiedzy. LEM dostarcza hipotez do sprawdzenia, a zastosowania praktyczne mogą dostarczać pytań badawczych.
- **Archiwa, książka i PixelSpace:** genealogia pomysłów, relacje między scenami i perspektywami oraz różne sposoby oglądania tych samych obiektów mogą korzystać ze wspólnego warsztatu bez narzucania jednej dziedzinowej ontologii.

Loom jest możliwym miejscem współdzielenia silnika, nie zadekretowanym właścicielem wszystkich danych. Nie zamrażamy obecnego modelu ChatADHD jako ostatecznego modelu ekosystemu i nie uzależniamy każdej integracji od ukończenia całego Loom lub LEM.
