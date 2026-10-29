/*
 * main.cpp – vstupní bod programu a hlavní menu.
 *
 * cpp-from-scratch: sourozenec projektu c-from-scratch. Stejná témata
 * (paměť, ukazatele, alokátory, bity, binární soubor), ale v moderním C++20.
 * U každého dema je v komentářích "C vs. C++", tedy jak by to vypadalo v C.
 */
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <functional>
#include <string_view>
#include <vector>

#include "arena.hpp"
#include "bits.hpp"
#include "lambdas.hpp"
#include "memview.hpp"
#include "objects.hpp"
#include "raii.hpp"
#include "tasks.hpp"
#include "util.hpp"
#include "vector_move.hpp"

/* windows.h až na konec: definuje spoustu maker (min, max, small, RGB ...),
 * která by jinak mohla rozbít hlavičky vložené po něm. */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN   // z windows.h jen to nejnutnější
#define NOMINMAX              // jinak windows.h nadefinuje makra min/max a rozbije std::min
#include <windows.h>
#endif

/* Debug CRT od Microsoftu umí na konci programu vypsat neuvolněnou paměť.
 * Funguje i pro new/delete, protože MSVC je staví nad malloc/free. */
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

namespace {

/* C vs. C++: v C bylo menu pole struktur s ukazatelem na funkci
 *     typedef void (*DemoFn)(void);
 *     static const MenuItem MENU[] = { { "...", demo_memory_layout }, ... };
 * Tady je to std::vector a std::function. Do položky jde dát obyčejná funkce,
 * ale i lambda, která si s sebou nese stav (zachycené proměnné).
 * A vector zná svou velikost, takže makro MENU_COUNT se sizeof odpadá. */
struct MenuItem {
    std::string_view title;
    std::function<void()> run;
};

void print_menu(const std::vector<MenuItem>& menu)
{
    print("\n");
    print("  +----------------------------------+\n");
    print("  |  c p p - f r o m - s c r a t c h |\n");
    print("  |  pamet, RAII a sablony v C++20   |\n");
    print("  +----------------------------------+\n\n");

    for (std::size_t i = 0; i < menu.size(); ++i) {
        print("  {}) {}\n", i + 1, menu[i].title);
    }
    print("  0) Konec\n\n");
}

}  // namespace

int main()
{
#ifdef _WIN32
    // Ať se v konzoli Windows správně zobrazí čeština (UTF-8).
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

#if defined(_MSC_VER) && defined(_DEBUG)
    // Při ukončení vypíše do okna Output ve Visual Studiu všechny leaky.
    // V C++ se leak vyrábí hůř než v C. Návod je v README.
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    int demos_run = 0;

    /* Lambda, která vrací lambdu: obalí demo a navíc počítá spuštění.
     * [&demos_run] = zachycení odkazem (počítadlo je jedno, sdílené),
     * [demo]       = kopie ukazatele na funkci (každá položka má svůj). */
    auto counted = [&demos_run](void (*demo)()) {
        return [demo, &demos_run] {
            ++demos_run;
            demo();
        };
    };

    const std::vector<MenuItem> menu{
        {"Paměť pod lupou (adresy, sizeof, endianita, std::string zevnitř)", counted(demo_memory_layout)},
        {"Reference a objekty zevnitř (padding, vtable, dědičnost)",         counted(demo_objects)},
        {"std::vector a move sémantika (kopie vs. přesun)",                  counted(demo_vector_move)},
        {"Šablony, arena a alokátory (constexpr, std::pmr)",                 counted(demo_templates)},
        {"Bitové operace (bitset, <bit>, std::byte, bit_cast)",              counted(demo_bits)},
        {"Úkolníček (vector, optional, výjimky, fstream)",                   counted(demo_tasks)},
        {"RAII a chytré ukazatele (unique_ptr, shared_ptr, weak_ptr)",       counted(demo_raii)},
        {"Lambdy, std::function a algoritmy (ranges)",                       counted(demo_lambdas)},
    };

    for (;;) {
        print_menu(menu);

        const auto choice = read_int("Volba: ");
        if (!choice) {
            if (std::cin.eof()) {
                break;   // Ctrl+Z / konec vstupu
            }
            print("Zadej číslo z menu.\n");
            continue;
        }

        if (*choice == 0) {
            break;
        }
        if (*choice < 1 || static_cast<std::size_t>(*choice) > menu.size()) {
            print("Taková volba není.\n");
            continue;
        }

        /* Pojistka: výjimka, kterou demo samo neošetří, neshodí celý program. */
        try {
            menu[static_cast<std::size_t>(*choice - 1)].run();   // volání přes std::function
        } catch (const std::exception& e) {
            print("\nNeošetřená výjimka z dema: {}\n", e.what());
        }
        wait_for_enter();
    }

    print("Ahoj! Spuštěno dem: {}. Žádné free() ani delete, a paměť je stejně uklizená. :)\n",
          demos_run);
    return EXIT_SUCCESS;
}
