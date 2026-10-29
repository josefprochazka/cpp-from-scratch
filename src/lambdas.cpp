/*
 * lambdas.cpp – lambdy, std::function a algoritmy ze standardní knihovny.
 *
 * C vs. C++: v C máš ukazatel na funkci, int (*fn)(int). Ukazuje jen na
 * obyčejnou funkci a nemůže si s sebou nést žádný stav (proto má qsort_s
 * nebo pthread_create navíc parametr void *context). Lambda je funkce
 * i s daty, která si "zachytila" z okolí.
 */
#include "lambdas.hpp"
#include "util.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>      // std::qsort
#include <functional>
#include <numeric>      // std::accumulate
#include <random>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

struct Person {
    std::string name;
    int age;
};

int twice(int x)
{
    return 2 * x;
}

/* Klasický "funktor": třída s operator(). Lambda je vlastně zkratka pro tohle:
 * překladač z každé lambdy vyrobí takovou anonymní třídu. */
struct AddN {
    int n;
    int operator()(int x) const { return x + n; }
};

/* Vypíše libovolný range: vector, pole, view ... Range se předává přes R&&,
 * protože některé views (filter) jde procházet jen jako ne-const. */
template <std::ranges::input_range R>
void print_range(std::string_view label, R&& range)
{
    print("  {:<38}", label);
    for (auto&& item : range) {
        print(" {}", item);
    }
    print("\n");
}

}  // namespace

void demo_lambdas()
{
    print_header("8) Lambdy, std::function a algoritmy");

    /* --- lambdy a zachytávání --- */
    int factor = 3;
    auto by_value = [factor](int x) { return x * factor; };       // kopie factor v okamžiku vytvoření
    auto by_reference = [&factor](int x) { return x * factor; };  // odkaz na původní proměnnou
    factor = 10;

    print("Zachytávání (capture), factor jsme po vytvoření lambd změnili z 3 na 10:\n");
    print("  [factor]  (2) = {:>2}   (lambda si factor zkopírovala, má pořád 3)\n", by_value(2));
    print("  [&factor] (2) = {:>2}   (lambda se dívá na původní proměnnou)\n", by_reference(2));

    /* init-capture + mutable: lambda má vlastní stav, který přežívá mezi voláními. */
    auto counter = [count = 0]() mutable { return ++count; };
    counter();
    counter();
    print("  [count = 0]() mutable: třetí volání vrátí {}\n", counter());

    /* Generická lambda (auto parametry) = šablona. Funguje pro cokoliv, co umí +. */
    auto add = [](const auto& a, const auto& b) { return a + b; };
    print("  [](auto a, auto b) {{ return a + b; }}: {} | {} | {}\n",
          add(1, 2), add(1.5, 2.25), add(std::string{"C"}, std::string{"++"}));

    auto plus_one = [](int x) { return x + 1; };
    print("\nsizeof: lambda bez zachycení {} B, [factor] {} B, [&factor] {} B, std::function {} B\n",
          sizeof(plus_one), sizeof(by_value), sizeof(by_reference), sizeof(std::function<int(int)>));
    print("  Lambda je jen struktura se zachycenými proměnnými. std::function je obal,\n"
          "  který umí uložit cokoliv volatelného, za cenu větší velikosti a nepřímého volání.\n");

    /* --- std::function: "cokoliv, co jde zavolat s int a vrátí int" --- */
    const std::vector<std::pair<std::string_view, std::function<int(int)>>> operations{
        {"twice (obyčejná funkce)", twice},
        {"AddN{100} (funktor)", AddN{100}},
        {"lambda [factor]", by_value},
        {"lambda bez zachycení", plus_one},
    };
    print("\nRůzné věci v jednom std::vector<std::function<int(int)>>:\n");
    for (const auto& [label, operation] : operations) {
        print("  {:<26} f(7) = {}\n", label, operation(7));
    }

    /* Lambda BEZ zachycení se umí převést na obyčejný ukazatel na funkci,
     * takže jde předat i do C funkce, třeba qsort: */
    std::array<int, 6> c_style{42, 7, 19, 3, 88, 1};
    std::qsort(c_style.data(), c_style.size(), sizeof(int), [](const void* left, const void* right) {
        const int l = *static_cast<const int*>(left);
        const int r = *static_cast<const int*>(right);
        return (l > r) - (l < r);
    });
    print("\nqsort z C s lambdou jako porovnávací funkcí:");
    for (const int value : c_style) {
        print(" {}", value);
    }
    print("\n");

    /* --- algoritmy --- */
    /* C vs. C++: qsort je generický přes void*, porovnání volá přes ukazatel na
     * funkci (nejde vložit do kódu) a o typech nic neví. std::sort je šablona,
     * lambdu si překladač vloží přímo do třídicí smyčky. Bývá proto rychlejší
     * a překladač hlídá typy. */
    std::vector<int> numbers(12);
    std::mt19937 rng{2024};   // pevné semínko -> pokaždé stejná "náhodná" čísla
    std::uniform_int_distribution<int> dist{1, 99};
    std::ranges::generate(numbers, [&rng, &dist] { return dist(rng); });

    print("\nAlgoritmy:\n");
    print_range("náhodná čísla:", numbers);

    std::sort(numbers.begin(), numbers.end());   // klasické STL s iterátory
    print_range("std::sort(begin, end):", numbers);

    std::ranges::sort(numbers, std::greater{});   // C++20 ranges: rovnou celý kontejner
    print_range("std::ranges::sort(v, std::greater{}):", numbers);

    const auto even_count = std::ranges::count_if(numbers, [](int n) { return n % 2 == 0; });
    const auto [lowest, highest] = std::ranges::minmax(numbers);
    const int total = std::accumulate(numbers.begin(), numbers.end(), 0);
    print("  sudých {}, min {}, max {}, součet {}\n", even_count, lowest, highest, total);

    /* Views jsou LÍNÉ: nic se nekopíruje ani nepočítá, dokud přes ně neiteruješ.
     * Roura | skládá kroky za sebe jako v shellu. */
    auto even_squares = numbers
                      | std::views::filter([](int n) { return n % 2 == 0; })
                      | std::views::transform([](int n) { return n * n; });
    print_range("sudá na druhou (filter | transform):", even_squares);
    print_range("3 nejmenší (reverse | take(3)):", numbers | std::views::reverse | std::views::take(3));
    print_range("std::views::iota(1, 11):", std::views::iota(1, 11));

    /* Projekce: řaď podle členu struktury, bez psaní porovnávací funkce. */
    std::vector<Person> people{{"Jana", 34}, {"Petr", 27}, {"Eva", 41}, {"Tomáš", 19}};
    std::ranges::sort(people, {}, &Person::age);
    print("\n  Lidé podle věku (projekce &Person::age):");
    for (const auto& person : people) {
        print(" {} ({})", person.name, person.age);
    }
    std::ranges::sort(people, {}, &Person::name);
    print("\n  Lidé podle jména (projekce &Person::name):");
    for (const auto& person : people) {
        print(" {}", person.name);
    }
    print("\n");
}
