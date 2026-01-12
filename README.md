# 🍣 kaiten-zushi

## Wiktor Sasnal

Zaawansowana symulacja restauracji sushi oparta na architekturze wieloprocesowej oraz mechanizmach komunikacji międzyprocesowej (IPC) w systemie Linux.

## 📂 Spis treści
- [Author](#author)
- [General Info](#general-info)
- [Technologies](#technologies)
- [Architecture & IPC](#architecture--ipc)
- [Pseudo-codes](#pseudo-codes)
- [Functional Tests](#functional-tests)
- [Setup](#setup)

# Author  
**Wiktor Sasnal**

---

## General info  
- Aplikacja symuluje cykl życia restauracji: od otwarcia o godzinie Tp do zamknięcia o Tk.
- System zarządza ruchem taśmy, produkcją dań przez kucharza oraz konsumpcją przez grupy klientów.
- Zaimplementowano logikę biletomatu, obsługę klientów VIP oraz restrykcyjne zasady opieki nad dziećmi.
- Po zakończeniu pracy generowany jest automatyczny raport finansowy uwzględniający sprzedaż i straty.

---

## Technologies  

<p align="center">
<img src="https://img.shields.io/badge/C++-00599C?style=for-the-badge&logo=cplusplus&logoColor=white" alt="C++" />
<img src="https://img.shields.io/badge/Multithreading-0078D4?style=for-the-badge&logo=googlecloud&logoColor=white" alt="Multithreading" />
<img src="https://img.shields.io/badge/Processes-555555?style=for-the-badge&logo=linux&logoColor=white" alt="Processes" />
<img src="https://img.shields.io/badge/IPC-FF4500?style=for-the-badge&logo=threadless&logoColor=white" alt="IPC" />
</p>

---

## Architecture & IPC
Symulacja wykorzystuje zdecentralizowany model procesów komunikujących się przez systemowe mechanizmy IPC:

* **Pamięć Współdzielona (Shared Memory):** Służy do przechowywania tablicy talerzyków na taśmie (`Plate belt[P]`), aktualnego czasu symulacji oraz statystyk sprzedaży.
* **Semafory (System V Semaphores):** * Zarządzanie biletomatem (Semafor 0).
    * Kontrola dostępności stolików dla grup 1, 2, 3 i 4-osobowych (Semafory 1-4).
    * Zapewnienie wyłącznego dostępu do taśmy (Mutex - Semafor 5).
* **Kolejki Komunikatów (Message Queues):** Obsługa zamówień specjalnych składanych przez klientów przy użyciu tabletów.
* **Sygnały Systemowe:** Sterowanie tempem pracy kucharza (`SIGUSR1/2`) oraz procedura natychmiastowej ewakuacji (`SIGRTMIN`).

---






## Functional Tests

| ID | Nazwa Testu | Cel i Scenariusz | Oczekiwany Rezultat (Brak błędu) |
| :--- | :--- | :--- | :--- |
| **T1** | **Weryfikacja Opieki nad Dziećmi (Odmowa Wstępu)** | **Scenariusz:** Uruchomienie grupy (np. 4-osobowej), w której losowanie przydzieliło same dzieci (wiek < 10 lat) lub zbyt mało dorosłych (np. 4 dzieci, 1 dorosły). <br> **Cel:** Sprawdzenie, czy system blokuje wejście takiej grupie. | Program wypisuje czerwony komunikat: **`[System] ODMOWA WSTĘPU! Za mało opiekunów.`** oraz szczegóły (ile dzieci, ile dorosłych). Proces klienta kończy się (`exit(0)`) i nie zajmuje stolika. |
| **T2** | **Logika Współdzielenia Stolików** | **Scenariusz:** Do stolika 4-osobowego (np. index 16) siada grupa 2-osobowa. Następnie wchodzi kolejna grupa 2-osobowa, która wylosuje ten sam typ stolika. <br> **Cel:** Sprawdzenie, czy semafory i pętla szukania pozwalają na dosiadanie. | Druga grupa otrzymuje ten sam numer stolika (np. 16) i komunikat **`DOSIADA SIĘ`**. W pamięci współdzielonej (podgląd np. przez debug) zajętość stolika wzrasta do 4. Obie grupy jedzą równolegle. |
| **T3** | **Losowość Przydziału Stref (1-os i 2-os)** | **Scenariusz:** Wpuszczenie serii klientów 1-osobowych i 2-osobowych. <br> **Cel:** Weryfikacja algorytmu losowania (`rand() % 100`) w `klient.c`. | Klienci 1-os trafiają losowo do stolików 2-os (40%), 3-os (30%) lub 4-os (30%). Klienci 2-os trafiają losowo do stolików 2-os (50%) lub 4-os (50%). Widać to po numerach stolików w logach. |
| **T4** | **Weryfikacja Celu Konsumpcji** | **Scenariusz:** Klient otrzymuje losowy cel zjedzenia dań (zmienna `target_to_eat`, np. 5 dań). <br> **Cel:** Sprawdzenie, czy klient opuszcza stolik po zjedzeniu wyznaczonej liczby posiłków. | W komunikacie końcowym klienta widnieje status **`Najedzeni`** (jeśli `eaten_total >= target`). W logach startowych widać: `>> Wielkosc: X os. CEL DO ZJEDZENIA: 5 DAN.` |
| **T5** | **Poprawność Raportu Finansowego** | **Scenariusz:** Klient VIP zamawia danie specjalne i zostawia napiwek. Zwykli klienci jedzą dania standardowe. <br> **Cel:** Weryfikacja sumowania przychodów przez `Main`. | W raporcie końcowym w sekcji "Przychody": suma sprzedaży dań standardowych + specjalnych + napiwków zgadza się z "Całkowitym Przychodem". Nie ma rozbieżności w księgowaniu. |

---

## Setup  
To run this project:
### 1. Kompilacja modułów
Projekt można skompilować ręcznie przy użyciu poniższych komend:

```bash
# Sklonuj repozytorium
git clone https://github.com/wiksas/kaiten-zushi.git
cd kaiten-zushi

# Kompilacja ról systemowych
gcc kucharz.c -o kucharz
gcc obsluga.c -o obsluga
gcc kierownik.c -o kierownik

# Klient wymaga biblioteki pthread
gcc klient.c -o klient -lpthread

# Główny program
gcc main.c -o main
```


# Uruchom symulację: `./main`.

### Krok 2: Wysyłanie poleceń
W nowym oknie terminala wyślij odpowiedni sygnał:

| Akcja | Sygnał | Komenda terminala |
| :--- | :--- | :--- |
| **Przyspieszenie** | SIGUSR1 | kill -USR1 <PID_KIEROWNIKA> |
| **Spowolnienie** | SIGUSR2 | kill -USR2 <PID_KIEROWNIKA> |
| **Ewakuacja / Wyjście** | SIGALRM | kill -ALRM <PID_KIEROWNIKA> |

[Portfolio](https://wiksas.github.io/UIPortfolio/)
