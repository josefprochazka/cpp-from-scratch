/*
 * raii.cpp – RAII a chytré ukazatele: proč v C++ nepíšeš free ani delete.
 *
 * RAII = Resource Acquisition Is Initialization. Zdroj (paměť, soubor,
 * zámek, socket) patří objektu. Konstruktor ho získá a DESTRUKTOR ho uvolní.
 * Destruktor C++ zavolá automaticky, když objekt zanikne: na konci bloku,
 * při return i při výjimce. Na úklid se tak nedá zapomenout.
 */
#include "raii.hpp"
#include "noisy.hpp"
#include "util.hpp"

#include <cstdlib>    // std::malloc, std::free
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

/* Funkce, která může uprostřed práce selhat výjimkou. */
void risky_work(bool fail)
{
    Noisy local{"lokální v risky_work"};
    auto on_heap = std::make_unique<Noisy>("na haldě v risky_work");

    if (fail) {
        throw std::runtime_error{"něco se pokazilo"};
    }
    print("    risky_work doběhla normálně\n");
}   // tady se oba objekty uklidí, ať funkce skončí jakkoliv

/* "Sink": funkce PŘEBÍRÁ vlastnictví (unique_ptr předaný hodnotou). */
void take_ownership(std::unique_ptr<Noisy> owned)
{
    print("    take_ownership: teď vlastním \"{}\"\n", owned->name());
}   // owned tady zanikne a objekt se smaže

/* Uzel obousměrného seznamu: dopředu silný odkaz, dozadu slabý. */
struct Node {
    explicit Node(std::string node_name) : name(std::move(node_name)) {}
    ~Node() { print("    ~Node {}\n", name); }

    std::string name;
    std::shared_ptr<Node> next;   // silný odkaz: drží další uzel naživu
    std::weak_ptr<Node> prev;     // slabý odkaz: nedrží (jinak by vznikl cyklus)
};

}  // namespace

void demo_raii()
{
    print_header("7) RAII a chytré ukazatele");
    Noisy::stats = {};
    Noisy::verbose = true;

    /* --- 1. objekty na zásobníku --- */
    print("1) Objekty na zásobníku: destruktory se volají automaticky, v opačném pořadí:\n");
    {
        Noisy first{"první"};
        Noisy second{"druhý"};
        print("    -- konec bloku --\n");
    }

    /* --- 2. unique_ptr --- */
    print("\n2) std::unique_ptr: jediný vlastník, sizeof = {} B (stejně jako holý ukazatel):\n",
          sizeof(std::unique_ptr<Noisy>));
    {
        /* C vs. C++:  Noisy *p = malloc(...); ... free(p);
         * new/delete v moderním C++ skoro nepíšeš. make_unique alokuje,
         * zavolá konstruktor a výsledek hned svěří vlastníkovi. */
        auto owner = std::make_unique<Noisy>("na haldě");
        print("    owner.get() = {}\n", static_cast<const void*>(owner.get()));

        // auto copy = owner;   // CHYBA překladu: unique_ptr nejde kopírovat (dva majitelé = double free)
        auto new_owner = std::move(owner);   // vlastnictví jde jen PŘEDAT
        print("    po std::move: owner = {}, new_owner = {}\n",
              static_cast<const void*>(owner.get()), static_cast<const void*>(new_owner.get()));

        take_ownership(std::move(new_owner));
        print("    po take_ownership je new_owner {}\n", new_owner ? "plný" : "prázdný (nullptr)");

        /* Pole: make_unique<T[]> zavolá na konci delete[] (ne delete). Plést si
         * delete a delete[] je v C++ klasická chyba, tady to hlídá typ.
         * V praxi je na pole skoro vždycky lepší std::vector. */
        auto squares = std::make_unique<int[]>(5);
        for (int i = 0; i < 5; ++i) {
            squares[static_cast<std::size_t>(i)] = i * i;
        }
        print("    make_unique<int[]>(5): {} {} {} {} {}\n",
              squares[0], squares[1], squares[2], squares[3], squares[4]);
        print("    -- konec bloku --\n");
    }

    /* --- 3. vlastní deleter --- */
    print("\n3) unique_ptr s vlastním deleterem: RAII obal kolem C API (malloc/free, FILE*, HANDLE):\n");
    {
        auto free_deleter = [](void* memory) {
            print("    deleter: volám std::free({})\n", memory);
            std::free(memory);
        };
        std::unique_ptr<int, decltype(free_deleter)> c_buffer{
            static_cast<int*>(std::malloc(4 * sizeof(int))), free_deleter};

        if (c_buffer) {
            c_buffer.get()[0] = 42;
            print("    paměť z C funkce malloc, ale free() za nás zavolá unique_ptr\n");
        }
        print("    -- konec bloku --\n");
    }

    /* --- 4. shared_ptr a weak_ptr --- */
    print("\n4) std::shared_ptr: sdílené vlastnictví s počítadlem odkazů, sizeof = {} B:\n",
          sizeof(std::shared_ptr<Noisy>));
    {
        std::weak_ptr<Noisy> observer;
        {
            /* make_shared = jedna alokace pro objekt i řídicí blok s počítadlem. */
            auto shared = std::make_shared<Noisy>("sdílený");
            observer = shared;
            print("    use_count = {}\n", shared.use_count());
            {
                auto another = shared;
                std::vector<std::shared_ptr<Noisy>> holders{shared, shared};
                print("    + 1 kopie + 2x ve vectoru: use_count = {}\n", shared.use_count());
            }
            print("    kopie zanikly: use_count = {}\n", shared.use_count());
            print("    weak_ptr expired? {}\n", observer.expired() ? "ano" : "ne");
            print("    -- poslední vlastník končí --\n");
        }
        print("    weak_ptr expired? {} (objekt už neexistuje a weak_ptr to pozná)\n",
              observer.expired() ? "ano" : "ne");
    }

    /* --- 5. seznam se shared_ptr a weak_ptr --- */
    print("\n5) Obousměrný seznam: shared_ptr dopředu, weak_ptr dozadu:\n");
    {
        auto head = std::make_shared<Node>("A");
        head->next = std::make_shared<Node>("B");
        head->next->prev = head;
        head->next->next = std::make_shared<Node>("C");
        head->next->next->prev = head->next;

        for (auto node = head; node; node = node->next) {
            const auto prev = node->prev.lock();   // weak_ptr -> shared_ptr (nebo prázdný)
            print("    {} (prev: {})\n", node->name, prev ? prev->name : std::string{"-"});
        }
        print("    -- konec bloku: head zanikne a uzly se smažou jeden za druhým --\n");
    }
    /* Kdyby prev byl taky shared_ptr, A a B by se držely navzájem, počítadlo
     * by nikdy nekleslo na 0 a destruktory by se nezavolaly. Memory leak
     * i v C++! Zkus to (viz README) a Debug build ti ho nahlásí. */

    /* --- 6. výjimky --- */
    print("\n6) Výjimka a úklid (stack unwinding):\n");
    risky_work(false);
    try {
        risky_work(true);
    } catch (const std::exception& e) {
        print("    zachycena výjimka \"{}\". Všimni si, že se oba objekty uklidily.\n", e.what());
    }
    /* C vs. C++: v C se tohle řeší "goto cleanup" nebo kontrolou každé návratové
     * hodnoty a ručním free() na každé chybové cestě. Na jednu se vždycky zapomene. */

    const NoisyStats& s = Noisy::stats;
    const int created = s.constructed + s.copied + s.moved;
    print("\nBilance Noisy: vzniklo {} objektů, zaniklo {} -> {}\n", created, s.destroyed,
          created == s.destroyed ? "sedí, nic neuniklo" : "NESEDÍ!");
}
