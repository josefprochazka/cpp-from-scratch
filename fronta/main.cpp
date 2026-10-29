/*
 * main.cpp – program "fronta": interaktivní fronta lidí u pokladny.
 *
 * Samotná fronta je v queue.hpp, výpisy v queue_view.cpp.
 * Tady je jen menu a obsluha jednotlivých voleb.
 */
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <utility>

#include "queue.hpp"
#include "queue_view.hpp"
#include "util.hpp"

/* windows.h až na konec, jeho makra (min, max ...) by rozbila jiné hlavičky. */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

namespace {

void print_menu()
{
    print("\n");
    print("  +----------------------------------+\n");
    print("  |  f r o n t a   (FIFO)            |\n");
    print("  |  kdo dřív přijde, ten dřív mele  |\n");
    print("  +----------------------------------+\n\n");
    print("  1) Přidat člověka na konec (push)\n");
    print("  2) Obsloužit prvního (pop)\n");
    print("  3) Kdo je na řadě? (front)\n");
    print("  4) Vypsat frontu\n");
    print("  5) Vyprázdnit frontu\n");
    print("  6) Ukázka: přetočení přes konec a zvětšení bufferu\n");
    print("  0) Konec\n\n");
}

void add_person(Queue<std::string>& queue)
{
    auto name = read_line("Jméno: ");
    if (!name || name->empty()) {
        print("  Nic nepřidáno.\n");
        return;
    }

    const auto old_capacity = queue.capacity();
    print("  \"{}\" je teď na konci fronty.\n", *name);
    queue.push(std::move(*name));   // string se přesune do fronty, nekopíruje

    if (queue.capacity() != old_capacity) {
        print("  Buffer byl plný: zvětšen z {} na {} slotů, prvky přesunuty od indexu 0.\n",
              old_capacity, queue.capacity());
    }
    print_ring(queue);
}

void serve_next(Queue<std::string>& queue)
{
    const auto served = queue.pop();
    if (!served) {
        print("  Fronta je prázdná, není koho obsloužit.\n");
        return;
    }
    print("  Obslouženo: {}. Přišel/přišla první, tak jde první na řadu.\n", *served);
    print_ring(queue);
}

void show_front(const Queue<std::string>& queue)
{
    /* front() na prázdné frontě hází výjimku, tady si ji chytíme. */
    try {
        print("  Na řadě je: {}\n", queue.front());
    } catch (const std::out_of_range& e) {
        print("  Nikdo, {}.\n", e.what());
    }
}

/* Naplní frontu tak, aby bylo vidět, jak index "přeteče" přes konec
 * pole zpátky na začátek, a nakonec i jak se buffer zvětší. */
void wraparound_demo(Queue<std::string>& queue)
{
    queue.clear();
    print("  Fronta vyprázdněna (kapacita zůstala {}).\n", queue.capacity());

    print("\n  push: Anna, Bára, Cyril");
    queue.push("Anna");
    queue.push("Bára");
    queue.push("Cyril");
    print_ring(queue);

    print("\n  pop, pop: odchází Anna a Bára. Nic se neposouvá, jen head jde dopředu.");
    queue.pop();
    queue.pop();
    print_ring(queue);

    print("\n  push: David, Ema, Filip. Ema a Filip se nevejdou na konec,");
    print("\n  tak jdou na začátek pole (index dojde na konec a pokračuje od 0).");
    queue.push("David");
    queue.push("Ema");
    queue.push("Filip");
    print_ring(queue);
    print_fifo(queue);

    print("\n  push: Gustav. Pole je plné, takže se alokuje větší a prvky se srovnají.");
    queue.push("Gustav");
    print_ring(queue);
    print_fifo(queue);
}

}  // namespace

int main()
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);   // ať je v konzoli vidět čeština
    SetConsoleCP(CP_UTF8);
#endif

#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);   // hlášení leaků do Output
#endif

    /* Malá počáteční kapacita, ať je zvětšení bufferu brzo vidět. */
    Queue<std::string> queue{4};

    for (;;) {
        print_menu();

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

        switch (*choice) {
        case 1: add_person(queue); break;
        case 2: serve_next(queue); break;
        case 3: show_front(queue); break;
        case 4: print_fifo(queue); break;
        case 5:
            queue.clear();
            print("  Fronta je prázdná.\n");
            break;
        case 6: wraparound_demo(queue); break;
        default: print("Taková volba není.\n"); continue;
        }
        wait_for_enter();
    }

    print("Ahoj! Ve frontě zbývá: {}. Destruktor je uklidí sám.\n", queue.size());
    return EXIT_SUCCESS;
}
