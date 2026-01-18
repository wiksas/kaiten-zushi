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
- **Symulacja Restauracji (C):** Aplikacja symuluje pełny cykl życia lokalu gastronomicznego w środowisku wieloprocesowym.
- **Model Hybrydowy:** Główny proces (`main`) zarządza generowaniem klientów (procesy), wewnątrz których działają poszczególne osoby (wątki `pthread`).
- **Logika Zajmowania Miejsc:** - Algorytm decyzyjny wybiera między **Ladą** (szybka konsumpcja, 1-os) a **Stolikami** (1, 2, 3 lub 4-osobowymi).
    - Obsługa priorytetów dla **VIP** (brak kolejki, napiwki).
    - Walidacja grup: wymagany 1 dorosły na każde rozpoczęte 3 dzieci.
- **Bezpieczeństwo Systemu:** Wbudowany limit **10 000 procesów** (ochrona przed *fork bomb*) oraz procedura bezpiecznego zamykania (`SIGINT`) z oczekiwaniem na opróżnienie lokalu.

---

## Technologies

<p align="center">
<img src="https://img.shields.io/badge/C-00599C?style=for-the-badge&logo=c&logoColor=white" alt="C" />
<img src="https://img.shields.io/badge/Pthreads-0078D4?style=for-the-badge&logo=gnu&logoColor=white" alt="Pthreads" />
<img src="https://img.shields.io/badge/Processes-555555?style=for-the-badge&logo=linux&logoColor=white" alt="Linux Processes" />
<img src="https://img.shields.io/badge/SystemV_IPC-FF4500?style=for-the-badge&logo=linux&logoColor=white" alt="IPC" />
</p>

---

## Architecture & IPC

Projekt wykorzystuje zaawansowane mechanizmy komunikacji międzyprocesowej (System V IPC) oraz synchronizację wątków. Konfiguracja lokalu jest zdefiniowana centralnie w pliku `common.h`.

### 1. Pamięć Współdzielona (Shared Memory)
Wspólny obszar pamięci przechowuje stan świata dostępny dla wszystkich procesów:
* **`BeltSlot belt[P]`**: Taśma z posiłkami (dostęp chroniony mutexem).
* **`table_capacity` / `current_occupancy`**: Tablice monitorujące obłożenie każdego stolika/miejsca.
* **`stats_*`**: Globalne statystyki finansowe (sprzedaż, koszty produkcji, napiwki).
* **Flagi sterujące**: `open` (czy lokal otwarty), `emergency_exit` (ewakuacja).

### 2. Semafory (System V Semaphores)
System wykorzystuje zestaw **8 semaforów** do sterowania dostępem i synchronizacji:

| ID | Rola | Opis działania |
| :--- | :--- | :--- |
| **0** | **Mutex Taśmy** | Binarny (0/1). Blokuje dostęp do edycji taśmy podczas nakładania/zdejmowania dań. |
| **1** | **Sygnał Kucharza** | Kucharz oczekuje na tym semaforze (wartość 0). Klient podbija go (+1), by zlecić zamówienie. |
| **2** | **Licznik 2-os** | Sem. licznikowy dla stolików 2-osobowych. |
| **3** | **Licznik 3-os** | Sem. licznikowy dla stolików 3-osobowych. |
| **4** | **Licznik 4-os** | Sem. licznikowy dla stolików 4-osobowych. |
| **5** | **Mutex Kasjera** | Binarny (0/1). Zapewnia atomowość operacji dodawania utargu do statystyk globalnych. |
| **6** | **Licznik Lady** | Sem. licznikowy. Liczba wolnych miejsc przy barze (Lada). |
| **7** | **Licznik 1-os** | Sem. licznikowy dla stolików 1-osobowych. |

### 3. Kolejki Komunikatów (Message Queues)
* **Cel:** Obsługa asynchronicznych **Zamówień Specjalnych**.
* **Działanie:** Klient wysyła strukturę `SpecialOrder` (PID + cena). Wykorzystana flaga `IPC_NOWAIT` zapobiega blokowaniu klienta w przypadku przepełnienia kuchni.

### 4. Wątki (Pthreads)
* **Kontekst:** Działają wewnątrz procesu `./klient`.
* **Rola:** Symulują poszczególne osoby w grupie siedzące przy jednym stoliku.
* **Synchronizacja:** Lokalny `pthread_mutex_t group_lock` chroni wspólny rachunek grupy oraz licznik zjedzonych posiłków przed *race condition*.




## Functional Tests

Poniżej przedstawiono zestawienie testów weryfikujących kluczowe funkcjonalności systemu. Każdy test opatrzony jest dowodem działania.

### T1: Weryfikacja Opieki nad Dziećmi (Odmowa Wstępu)
* **Cel:** Sprawdzenie, czy system blokuje wejście grupie, która nie spełnia wymogu opieki (minimum 1 dorosły na każde rozpoczęte 3 dzieci).
* **Scenariusz:** Uruchomienie grupy , w której losowanie przydzieliło same dzieci (wiek < 10 lat).
* **Oczekiwany Rezultat:**
  1. Program wypisuje czerwony komunikat: `[System] ODMOWA WSTĘPU! Za mało opiekunów.`
  2. W komunikacie widać szczegóły grupy (wiek uczestników).
  3. Proces klienta kończy się natychmiast (`exit(0)`) i nie zajmuje zasobów.

> **Dowód działania:**
>
> ![T1 - Odmowa Wstępu](img/test_t.1.png)

---

### T2: Logika Współdzielenia Stolików
* **Cel:** Sprawdzenie, czy semafory i algorytm wyszukiwania pozwalają na dosiadanie się grup do częściowo zajętych stolików.
* **Scenariusz:**
  1. Do stolika 4-osobowego (np. o indeksie 16) siada pierwsza grupa 2-osobowa.
  2. Wchodzi kolejna grupa 2-osobowa, która wylosuje ten sam typ stolika/strefę.
* **Oczekiwany Rezultat:**
  1. Druga grupa otrzymuje ten sam numer stolika co pierwsza (np. nr 16).
  2. Pojawia się komunikat: `DOSIADA SIĘ do stolika nr 16`.
  3. Obie grupy jedzą równolegle, a zajętość stolika w pamięci współdzielonej wynosi 4/4.

> **Dowód działania:**
>
> ![T2 - Dosiadanie się](img/test_t_2.1.png)
> ![T2 - Dosiadanie się](img/test_t_2.2.png)

---



### T3: Weryfikacja Celu Konsumpcji
* **Cel:** Sprawdzenie, czy klient poprawnie realizuje cykl życia: jedzenie -> osiągnięcie celu -> zwolnienie stolika.
* **Scenariusz:** Klient otrzymuje losowy cel zjedzenia dań (zmienna `target_to_eat`, np. 4 dań).
* **Oczekiwany Rezultat:**
  1. W logu startowym widać: `>> Wielkosc: X os. CEL DO ZJEDZENIA: 5 DAN.`
  2. Klient zjada dokładnie tyle dań (lub więcej, jeśli kończył równocześnie z innymi).
  3. W komunikacie końcowym (po wyjściu) widnieje status `Najedzeni` oraz poprawna kwota rachunku.

> **Dowód działania:**
>
> ![T3 - Cel Konsumpcji](img/test_t.3.png)

---

### T4: Poprawność Raportu Finansowego
* **Cel:** Weryfikacja, czy proces `Main` poprawnie sumuje przychody ze wszystkich źródeł (sprzedaż standardowa, specjalna, napiwki).
* **Scenariusz:** Symulacja z udziałem klientów VIP (zamawiających dania specjalne i dających napiwki) oraz zwykłych klientów.
* **Oczekiwany Rezultat:**
  1. W raporcie końcowym wyświetlanym po zamknięciu lokalu sumy się zgadzają.
  2. Równanie: `Sprzedaż Dań Podstawowych` + `Sprzedaż Dań Specjalnych` + `Napiwki` = `CAŁKOWITY PRZYCHÓD`.

> **Dowód działania:**
>
> ![T4 - Raport Finansowy](img/test_t.4.png)

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
