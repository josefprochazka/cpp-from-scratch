/*
 * vector_move.cpp – std::vector (kapacita, přestěhování dat) a move sémantika.
 *
 * C vs. C++: v C jsme si psali IntVector s ručním realloc, zdvojnásobením
 * kapacity a hlídáním, aby se při selhání realloc neztratil starý blok.
 * std::vector dělá přesně tohle. Navíc je generický (vector<cokoliv>),
 * prvky správně kopíruje nebo přesouvá a v destruktoru paměť uvolní.
 */
#include "vector_move.hpp"
#include "noisy.hpp"
#include "util.hpp"

#include <cstdint>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

/* Malá třída, která jen počítá kopie a přesuny. Parametr šablony říká,
 * jestli je move konstruktor noexcept. Za chvíli uvidíme, proč na tom záleží. */
template <bool NoexceptMove>
struct Counted {
    Counted() = default;
    Counted(const Counted&) { ++copies; }
    Counted(Counted&&) noexcept(NoexceptMove) { ++moves; }
    Counted& operator=(const Counted&) { ++copies; return *this; }
    Counted& operator=(Counted&&) noexcept(NoexceptMove) { ++moves; return *this; }
    ~Counted() = default;

    static inline int copies = 0;
    static inline int moves = 0;
};

template <bool NoexceptMove>
void count_reallocation_copies(std::string_view label)
{
    using Item = Counted<NoexceptMove>;
    Item::copies = 0;
    Item::moves = 0;

    std::vector<Item> items;
    for (int i = 0; i < 1000; ++i) {
        items.emplace_back();   // prvek vznikne rovnou ve vectoru, kopie/přesuny jsou jen z realokací
    }
    print("  {:<32} kopií {:>5}, přesunů {:>5}\n", label, Item::copies, Item::moves);
}

/* Návrat objektu z funkce. Od C++17 je tu zaručené "copy elision": objekt
 * se postaví rovnou v proměnné volajícího, nevolá se kopie ani přesun.
 * V C bys strukturu s ukazatelem uvnitř vracel opatrně, tady je to zadarmo. */
Noisy make_noisy(std::string name)
{
    return Noisy{std::move(name)};
}

}  // namespace

void demo_vector_move()
{
    print_header("3) std::vector a move sémantika");

    /* --- růst kapacity --- */
    std::vector<int> numbers;
    std::mt19937 rng{std::random_device{}()};   // C++ náhodná čísla místo rand()
    std::uniform_int_distribution<int> dist{0, 99};

    print("Přidávám 20 náhodných čísel, sleduj, kdy vector realokuje:\n");
    for (int i = 0; i < 20; ++i) {
        const auto old_capacity = numbers.capacity();
        /* Starou adresu si uložíme jako ČÍSLO, stejně jako v C verzi:
         * po realokaci už starý blok neexistuje. */
        const auto old_address = reinterpret_cast<std::uintptr_t>(numbers.data());

        numbers.push_back(dist(rng));

        if (numbers.capacity() != old_capacity) {
            const bool moved = reinterpret_cast<std::uintptr_t>(numbers.data()) != old_address;
            print("    kapacita {:>2} -> {:>2}  ({:>3} B)  blok {}\n",
                  old_capacity, numbers.capacity(), numbers.capacity() * sizeof(int),
                  old_capacity == 0 ? "první alokace" : (moved ? "se PŘESTĚHOVAL" : "zůstal na místě"));
        }
    }
    print("\n  MSVC zvětšuje kapacitu 1,5x, GCC a Clang 2x. Obojí dává v průměru O(1) na push_back.\n");

    print("\nObsah (size = {}, capacity = {}):\n  [", numbers.size(), numbers.capacity());
    for (std::size_t i = 0; i < numbers.size(); ++i) {
        print("{}{}", i == 0 ? "" : ", ", numbers[i]);
    }
    print("]\n");

    /* reserve = řekni předem, kolik toho bude. Žádné realokace. */
    std::vector<int> reserved;
    reserved.reserve(20);
    int reallocations = 0;
    for (int i = 0; i < 20; ++i) {
        const auto before = reserved.capacity();
        reserved.push_back(i);
        if (reserved.capacity() != before) {
            ++reallocations;
        }
    }
    print("\nS reserve(20) předem: {} realokací při 20x push_back.\n", reallocations);

    /* POZOR: realokace = přestěhování. Každý ukazatel, reference nebo iterátor
     * do vectoru, který sis uložil PŘED push_back, může ukazovat do uvolněné
     * paměti. Je to ta samá past jako po realloc v C a C++ tě před ní neochrání.
     * (Debug MSVC to ale u iterátorů umí odhalit a program zastaví.) */

    /* --- kopie vs. přesun --- */
    Noisy::verbose = true;
    print("\nKopie vs. přesun (sleduj výpisy konstruktorů):\n");
    {
        print("  Noisy a(\"a\");\n");
        Noisy a("a");

        print("  Noisy b = a;               // a má jméno (lvalue) -> KOPIE\n");
        Noisy b = a;

        print("  Noisy c = std::move(a);    // std::move = \"a už nepotřebuju\" -> přesun\n");
        Noisy c = std::move(a);
        print("  a.name() je teď \"{}\". Po přesunu je objekt platný, ale prázdný.\n", a.name());

        print("  b = c;                     // přiřazení z lvalue -> KOPIE\n");
        b = c;

        print("  b = std::move(c);          // přiřazení z rvalue -> přesun\n");
        b = std::move(c);

        print("  Noisy d = make_noisy(\"d\"); // návrat z funkce: jen 1 konstruktor, žádná kopie\n");
        Noisy d = make_noisy("d");

        print("  -- konec bloku: destruktory v opačném pořadí (d, c, b, a) --\n");
    }

    print("\nVkládání do vectoru:\n");
    {
        std::vector<Noisy> crowd;
        crowd.reserve(3);   // bez reserve by se navíc přesouvalo při realokacích
        Noisy e("e");

        print("  crowd.push_back(e);             // kopie\n");
        crowd.push_back(e);

        print("  crowd.push_back(std::move(e));  // přesun\n");
        crowd.push_back(std::move(e));

        print("  crowd.emplace_back(\"f\");        // objekt vznikne rovnou uvnitř vectoru\n");
        crowd.emplace_back("f");

        print("  -- konec bloku --\n");
    }

    /* --- co přesun dělá s pamětí --- */
    std::string source(100, 'x');
    const void* buffer_before = source.data();
    std::string target = std::move(source);
    print("\nstd::string se 100 znaky na haldě:\n");
    print("  buffer před přesunem {}, buffer cíle po přesunu {}\n",
          buffer_before, static_cast<const void*>(target.data()));
    print("  -> {}. source.size() je teď {}.\n",
          buffer_before == target.data() ? "ten samý blok, přesunul se jen ukazatel, 100 B se nekopírovalo"
                                         : "jiný blok",
          source.size());

    /* --- noexcept a realokace --- */
    print("\nProč na noexcept u move konstruktoru záleží (1000x emplace_back bez reserve):\n");
    count_reallocation_copies<true>("move konstruktor noexcept:");
    count_reallocation_copies<false>("move konstruktor BEZ noexcept:");
    print("  Bez noexcept vector při realokaci radši KOPÍRUJE. Kdyby přesun v půlce\n"
          "  vyhodil výjimku, staré prvky by už byly vykradené a nešlo by se vrátit.\n"
          "  S kopií zůstane původní blok netknutý (tzv. silná záruka).\n");
}
