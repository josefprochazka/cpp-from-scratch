# cpp-from-scratch 🧪

Malá interaktivní konzolová aplikace v moderním **C++ (C++20)**. Je to sourozenec projektu
[c-from-scratch](https://github.com/josefprochazka/c-from-scratch): stejná témata (paměť, ukazatele,
vlastní alokátor, bity, binární soubor), ale napsaná tak, jak se to dělá v C++.
Uvidíš, co C++ přidává: **RAII a destruktory**, chytré ukazatele, **move sémantiku**, šablony místo
`void*`, `constexpr`, lambdy, algoritmy a nástroje pro práci s bajty (`std::span`, `std::byte`,
`std::bit_cast`, `<bit>`).

Kód je okomentovaný česky. U každého dema najdeš v komentářích **„C vs. C++“**, tedy jak by to samé
vypadalo v C a proč je C++ verze bezpečnější nebo pohodlnější. V celém projektu není jediné `free`
ani `delete`. (Jedno `free` tam přece jen je, uvnitř unique_ptr, které obaluje `malloc` z C.)

---

## Co aplikace dělá

Po spuštění se ukáže menu. Každá položka je samostatné demo. Prvních 6 má stejné číslo jako
odpovídající demo v C verzi, takže je jde porovnávat vedle sebe:

| # | Demo | Co uvidíš | Soubor |
|---|------|-----------|--------|
| 1 | **Paměť pod lupou** | adresy na zásobníku, haldě a v `.data`/`.bss`/`.rdata`; `sizeof` základních typů i `std::vector`, `std::string`, `unique_ptr`…; endianitu přes `std::endian`; `double` rozebraný přes `std::bit_cast`; **SSO** (krátký `std::string` bydlí uvnitř objektu); `make_unique_for_overwrite` vs. `make_unique` (obdoba `malloc` vs. `calloc`) | `src/memview.cpp` |
| 2 | **Reference a objekty zevnitř** | reference vs. ukazatel, `swap` přes reference, **padding** a `offsetof`; `sizeof` prázdné třídy, třídy s metodami a s **virtuální funkcí**; skrytý **vptr** v hexdumpu; polymorfismus přes `unique_ptr<Shape>`; **layout dědičnosti** a posun ukazatele u vícenásobné dědičnosti | `src/objects.cpp` |
| 3 | **std::vector a move sémantika** | růst `capacity` a jestli se data **přestěhovala**; `reserve`; **kopie vs. přesun** (move konstruktor a přiřazení, `std::move`, copy elision); `push_back` vs. `emplace_back`; proč na `noexcept` u move konstruktoru záleží (1000 prvků: kopie vs. přesuny) | `src/vector_move.cpp` |
| 4 | **Šablony, arena a alokátory** | generický `FixedStack<T, N>` místo `void*`, **concept**, `constexpr` + `static_assert`; **arena** jako třída (RAII, `create<T>`, `create_array<T>`); `ArenaAllocator<T>` **kompatibilní s STL** (`std::vector<int, ArenaAllocator<int>>`); `std::pmr::monotonic_buffer_resource` nad polem bajtů **na zásobníku** | `src/arena.cpp` |
| 5 | **Bitové operace** | registr jako `std::bitset<8>`; `constexpr` šablony místo maker `SET_BIT`/`CLEAR_BIT`/`TOGGLE_BIT`; `enum class`; `std::popcount`, `has_single_bit`, `bit_width`, `bit_ceil`, `countr_zero`, `rotl`; `std::byte`; RGB přes `constexpr` a `bit_cast` na pole bajtů | `src/bits.cpp` |
| 6 | **Úkolníček** | `std::vector<Task>` místo spojového seznamu, `std::optional`, `std::ranges::find` s projekcí, **výjimky** (vlastní `FileFormatError`), `std::fstream` pro **binární soubor ve stejném formátu jako C verze** (`tasks.dat` jde přenášet mezi nimi) | `src/tasks.cpp` |
| 7 | **RAII a chytré ukazatele** | destruktory v opačném pořadí, `unique_ptr` (předání vlastnictví, „sink“ funkce, pole), **vlastní deleter** kolem `malloc`/`free`, `shared_ptr` a `use_count`, `weak_ptr`, obousměrný seznam bez cyklu, **úklid při výjimce** (stack unwinding) | `src/raii.cpp` |
| 8 | **Lambdy a algoritmy** | zachytávání hodnotou a odkazem, `mutable`, generická lambda, `sizeof` lambd, `std::function` s funkcí, funktorem i lambdou, lambda jako komparátor pro **`qsort` z C**; `std::sort`, `std::ranges::sort`, projekce, `count_if`, `minmax`, **views** (`filter`, `transform`, `reverse`, `take`, `iota`) | `src/lambdas.cpp` |

A ještě pár drobností navíc:
- **Menu je `std::vector` lambd** (`main.cpp`). Každá položka je lambda vyrobená jinou lambdou,
  která navíc počítá, kolik dem jsi spustil.
- Třída **`Noisy`** (`noisy.hpp`) hlásí každý konstruktor, kopii, přesun i destruktor. Díky ní je vidět,
  co C++ dělá „za tebe“.
- Vlastní **`print(...)`** nad `std::format`. Formátovací řetězec se kontroluje už při překladu
  (v C++23 je hotové `std::print`).
- V Debug buildu ve Visual Studiu se zapne **detekce memory leaků** (`_CrtSetDbgFlag`), stejně jako v C verzi.
- Bezpečné čtení vstupu: `std::getline` + `std::from_chars`, výsledek je `std::optional<int>` (`util.cpp`).

## C vs. C++

Stejná témata vedle sebe. C verze: **https://github.com/josefprochazka/c-from-scratch**

| Téma | c-from-scratch (C11) | cpp-from-scratch (C++20) |
|------|----------------------|---------------------------|
| Paměť na haldě | `malloc` + kontrola `NULL` + `free` | `std::make_unique` / `make_shared`, úklid v destruktoru, při nedostatku paměti výjimka `std::bad_alloc` |
| Pohled na bajty | `const void *data, size_t size` | `std::span<const std::byte>`, `hexdump_of(x)` |
| Bity typu double | `union` nebo `memcpy` | `std::bit_cast` (i v `constexpr`) |
| Endianita | test za běhu přes `(unsigned char *)&x` | `std::endian::native` při překladu |
| Řetězce | `char *`, `strlen`, `str_duplicate` + `free` | `std::string` (SSO), `std::string_view` |
| Předání odkazem | `swap(&x, &y)` s ukazateli | reference `swap(x, y)` |
| Dynamické pole | vlastní `IntVector` + `realloc` | `std::vector<T>`, move sémantika, `reserve` |
| Generický kód | `void *` + `element_size`, `qsort` | šablony, concepts, `std::sort` s lambdou |
| Arena | `arena_init`/`arena_destroy`, `_Alignof` | třída s RAII, `create<T>`, STL alokátor, `std::pmr` |
| Bitová makra | `#define SET_BIT(...)` | `constexpr` šablony, `std::bitset`, `<bit>` |
| Spojový seznam | ruční `malloc`/`free` uzlů, `Task **link` | `std::vector<Task>`, `std::ranges::find` |
| Chyby | návratové hodnoty, `goto cleanup` | výjimky + RAII (úklid proběhne vždycky) |
| Soubor | `fopen`/`fwrite`/`fclose` | `std::ofstream` s `exceptions()`, zavře se sám |
| Menu | pole ukazatelů na funkce + `MENU_COUNT` | `std::vector<MenuItem>` se `std::function` a lambdami |
| Polymorfismus | ruční tabulka ukazatelů na funkce | `virtual`, překladač vyrobí vtable sám |

Co C++ **neopraví**: dangling pointer po realokaci `vector`u, čtení neinicializované paměti
nebo cyklus `shared_ptr`. Pod kapotou je to pořád stejný počítač.

## Jak to spustit ve Visual Studiu

### 0. Jednorázově: nainstaluj C++ kompilátor
Visual Studio ve výchozím stavu kompilátor C/C++ neobsahuje.
1. Spusť **Visual Studio Installer**.
2. U své verze VS (2022 nebo 2026) klikni na **Modify**.
3. Zaškrtni workload **„Desktop development with C++“** (Vývoj desktopových aplikací pomocí C++).
   Přidá se i MSVC kompilátor a *C++ CMake tools for Windows*.
4. Klikni na **Modify** / **Install**.

Potřebuješ MSVC, které umí C++20 včetně `<format>`. To splňuje VS 2022 17.2 a novější.

### 1. Otevři projekt
Projekt používá **CMake**. Visual Studio ho umí otevřít rovnou, bez `.sln`:

1. `File > Open > Folder...` a vyber složku s tímto projektem (tu, kde je `CMakeLists.txt`).
2. Chvíli počkej, než VS dokončí konfiguraci CMake (dole v okně *Output* se objeví „CMake generation finished“).
3. Nahoře v liště vyber jako spouštěcí položku **`cpp-from-scratch.exe`**.
4. Zmáčkni **F5** (spuštění s debuggerem) nebo **Ctrl+F5** (bez debuggeru).

### Alternativa: příkazová řádka
V **Developer PowerShell for VS** (najdeš ho ve Start menu):

```powershell
cmake -S . -B build
cmake --build build --config Debug
.\build\Debug\cpp-from-scratch.exe
```

Příkaz `cmake -S . -B build` ti zároveň vygeneruje klasické `build\CppFromScratch.sln`, takže ho jde otevřít i „postaru“.

Na Linuxu / s GCC nebo Clangem (potřebuješ GCC 13+ nebo Clang 17+ kvůli `std::format`):

```bash
cmake -S . -B build && cmake --build build && ./build/cpp-from-scratch
```

## Tipy, co si zkusit

- **Kdo volá destruktor?** Dej breakpoint (F9) do `Noisy::~Noisy` v `noisy.hpp`, spusť demo 7 (F5)
  a dívej se do okna `Debug > Windows > Call Stack`. U výjimky uvidíš, že destruktor volá
  runtime při „odmotávání“ zásobníku, ne tvůj kód.
- **Okno Memory:** v demu 1 nebo 2 dej breakpoint za `hexdump_of(...)`, otevři
  `Debug > Windows > Memory > Memory 1` a vlož adresu, kterou program vypsal. U `Rectangle`
  uvidíš vptr. Když jeho hodnotu vložíš do okna Memory, dostaneš se na vtable s adresami funkcí.
- **Watch a „Raw View“:** ve Watch okně u `std::vector` nebo `std::string` VS ukazuje hezký pohled
  (Natvis). Rozbal **[Raw View]** a uvidíš skutečné členy: ukazatele `_Myfirst`, `_Mylast`, `_Myend`
  a u stringu `_Bx._Buf` (SSO buffer) vs. `_Bx._Ptr`.
- **Vyrob memory leak v C++:** v `raii.cpp` změň v `struct Node` řádek `std::weak_ptr<Node> prev;`
  na `std::shared_ptr<Node> prev;`. Vznikne cyklus, `~Node` se nezavolá a po ukončení programu
  v Debug buildu uvidíš v okně *Output* `Detected memory leaks!`. Druhá možnost: napiš
  `new Noisy{"leak"}` bez `delete`.
- **Smaž `noexcept`** z move konstruktoru `Noisy` v `noisy.hpp` a pusť demo 3 bez `crowd.reserve(3)`.
  Při realokaci se místo přesunů objeví KOPIE.
- **Iterator invalidation:** v demu 3 si ulož `auto it = numbers.begin();`, pak udělej pár `push_back`
  a přečti `*it`. Debug MSVC program zastaví hláškou *„vector iterator not dereferencable“*,
  v Release to tiše přečte uvolněnou paměť.
- **Debug vs. Release:** porovnej výpis `sizeof` v demu 1. V Debug MSVC mají kontejnery navíc ladicí
  ukazatel (`_ITERATOR_DEBUG_LEVEL`), takže `std::string` i `std::vector` jsou větší.
- **constexpr v akci:** v `arena.cpp` změň `static_assert(compile_time_sum == 6, ...)` na `== 7`.
  Program se nepřeloží, protože test proběhl už při překladu.
- **Přenes `tasks.dat`:** soubor uložený úkolníčkem v C verzi zkopíruj vedle `cpp-from-scratch.exe`
  (nebo do složky, ze které se spouští) a načti ho v C++ verzi. Formát je stejný.

## Úkoly na procvičení

1. Napiš vlastní `UniquePtr<T>`: zakázaná kopie, move konstruktor a přiřazení, destruktor,
   `operator*`, `operator->`, `release()`, `reset()`. Ověř, že `sizeof(UniquePtr<int>) == sizeof(int*)`.
2. Přidej do `FixedStack` metodu `top()` a konstruktor ze `std::initializer_list`
   (`FixedStack<int, 4> s{1, 2, 3};`). Jde to udělat `constexpr`?
3. Přidej do `Arena` značky `mark()` / `rewind(mark)` (stejný úkol jako v C) a pak z arény udělej
   potomka `std::pmr::memory_resource` (stačí přepsat `do_allocate`, `do_deallocate`, `do_is_equal`).
4. Přidej k úkolům termín (`std::chrono::sys_days`), zvyš `FileVersion` na 2 a zachovej čtení verze 1.
5. Přepiš `Shape` bez virtuálních funkcí pomocí `std::variant<Rectangle, Circle>` a `std::visit`
   a porovnej `sizeof`.
6. Přejdi na C++23 (`CMAKE_CXX_STANDARD 23`): v demu 8 nahraď `std::accumulate` za
   `std::ranges::fold_left` a naši funkci `print` z `util.hpp` za `std::print` z `<print>`.

## Struktura projektu

```
.
├── CMakeLists.txt       # build konfigurace (VS ji otevře přes Open Folder)
├── README.md
└── src/
    ├── main.cpp         # menu jako std::vector lambd
    ├── util.cpp/.hpp    # print (std::format), read_line, read_int (std::optional)
    ├── noisy.hpp        # třída, která hlásí konstruktory, kopie, přesuny a destruktory
    ├── memview.cpp/.hpp # hexdump (std::span<std::byte>), adresy, sizeof, bit_cast, SSO
    ├── objects.cpp/.hpp # reference, padding, vptr, layout dědičnosti
    ├── vector_move.cpp/.hpp  # std::vector, kapacita, move sémantika, noexcept
    ├── fixed_stack.hpp  # šablona FixedStack<T, N>
    ├── arena.cpp/.hpp   # Arena, ArenaAllocator<T>, std::pmr, constexpr
    ├── bits.cpp/.hpp    # bitset, <bit>, std::byte, constexpr místo maker
    ├── tasks.cpp/.hpp   # úkolníček: vector, optional, výjimky, fstream
    ├── raii.cpp/.hpp    # RAII, unique_ptr, shared_ptr, weak_ptr, výjimky
    └── lambdas.cpp/.hpp # lambdy, std::function, algoritmy, ranges
```

## Poznámka k češtině v konzoli

Program na Windows přepne konzoli do UTF-8 (`SetConsoleOutputCP`), aby byla vidět diakritika.
Ve starší konzoli (conhost) může nefungovat **zadávání** znaků s diakritikou.
V tom případě piš názvy úkolů bez háčků a čárek, nebo použij Windows Terminal.
