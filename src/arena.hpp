/*
 * arena.hpp – arena alokátor (bump allocator) jako třída a k němu alokátor
 * kompatibilní s STL.
 *
 * Princip je stejný jako v C verzi: jednou si řekneme o velký blok paměti
 * a pak z něj "ukrajujeme" posouváním offsetu. Uvolňuje se všechno najednou.
 */
#pragma once

#include <bit>        // std::has_single_bit
#include <concepts>   // std::unsigned_integral
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>     // std::unique_ptr, std::construct_at
#include <new>        // std::bad_alloc
#include <span>
#include <type_traits>
#include <utility>

/* Zarovnání nahoru na násobek align (align musí být mocnina dvojky).
 *
 * C vs. C++: constexpr funkce místo makra. Má typy, dá se do ní vkročit
 * debuggerem, argumenty se vyhodnotí jen jednou a jde ji zavolat i při
 * překladu. Concept std::unsigned_integral pustí jen bezznaménková čísla. */
template <std::unsigned_integral U>
constexpr U align_up(U value, U align) noexcept
{
    return (value + (align - 1)) & ~(align - 1);
}

/* static_assert = test, který proběhne při PŘEKLADU. Když neplatí, program
 * se nepřeloží. Příklad z C verze: adresa 13, align 8 -> 16. */
static_assert(align_up(13u, 8u) == 16u);
static_assert(align_up(16u, 8u) == 16u);
static_assert(align_up(0u, 16u) == 0u);

class Arena {
public:
    /* Konstruktor nahrazuje arena_init. Když dojde paměť, vyletí
     * std::bad_alloc, takže nikdy nevznikne "napůl hotová" aréna. */
    explicit Arena(std::size_t capacity);

    /* Aréna vlastní blok paměti. Kopie by znamenala dva majitele téhož bloku
     * (a dvojí uvolnění), proto kopírování zakážeme. */
    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;

    /* Destruktor psát nemusíme. Blok drží std::unique_ptr a ten ho uvolní sám
     * ("rule of zero"). C verze potřebovala ruční arena_destroy. */

    /* Vrací nullptr, když dojde místo nebo align není mocnina dvojky.
     * [[nodiscard]] = překladač varuje, když výsledek zahodíš (leak/chyba). */
    [[nodiscard]] void* allocate(std::size_t size, std::size_t align) noexcept;

    /* Typovaná alokace + zavolání konstruktoru (placement new).
     * Aréna nevolá destruktory, proto jen pro typy, které žádný úklid
     * nepotřebují (int, double, struktury bez std::string ...). */
    template <class T, class... Args>
    T* create(Args&&... args)
    {
        static_assert(std::is_trivially_destructible_v<T>,
                      "Aréna nevolá destruktory, použij ji jen pro jednoduché typy");
        void* memory = allocate(sizeof(T), alignof(T));
        if (memory == nullptr) {
            return nullptr;
        }
        return std::construct_at(static_cast<T*>(memory), std::forward<Args>(args)...);
    }

    /* Pole count prvků, inicializovaných na nulu. Vrací span (ukazatel + délka). */
    template <class T>
    std::span<T> create_array(std::size_t count)
    {
        static_assert(std::is_trivially_destructible_v<T>,
                      "Aréna nevolá destruktory, použij ji jen pro jednoduché typy");
        if (count > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
            return {};
        }
        void* memory = allocate(count * sizeof(T), alignof(T));
        if (memory == nullptr) {
            return {};
        }
        T* first = static_cast<T*>(memory);
        std::uninitialized_value_construct_n(first, count);
        return {first, count};
    }

    /* Zapomene všechny alokace. O(1). */
    void reset() noexcept { offset_ = 0; }

    std::size_t used() const noexcept { return offset_; }
    std::size_t capacity() const noexcept { return capacity_; }
    const std::byte* data() const noexcept { return buffer_.get(); }

private:
    std::unique_ptr<std::byte[]> buffer_;
    std::size_t capacity_;
    std::size_t offset_ = 0;
};

/* Alokátor kompatibilní s STL: std::vector<int, ArenaAllocator<int>> pak bere
 * paměť z arény místo z haldy. Stačí value_type, allocate, deallocate,
 * převodní konstruktor a porovnání. Zbytek si doplní std::allocator_traits.
 *
 * Alokátor sám nic nevlastní, je to jen "odkaz" na arénu. Aréna proto musí
 * žít déle než kontejner, který ji používá. */
template <class T>
class ArenaAllocator {
public:
    using value_type = T;

    explicit ArenaAllocator(Arena& arena) noexcept : arena_(&arena) {}

    /* Kontejner si někdy potřebuje alokátor pro JINÝ typ (std::list pro své
     * uzly, Debug MSVC pro ladicí "proxy" objekt). Proto převod z ArenaAllocator<U>. */
    template <class U>
    ArenaAllocator(const ArenaAllocator<U>& other) noexcept : arena_(other.arena_) {}

    T* allocate(std::size_t count)
    {
        if (count > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
            throw std::bad_array_new_length{};   // count * sizeof(T) by přeteklo
        }
        void* memory = arena_->allocate(count * sizeof(T), alignof(T));
        if (memory == nullptr) {
            throw std::bad_alloc{};   // kontejnery čekají výjimku, ne nullptr
        }
        return static_cast<T*>(memory);
    }

    /* Aréna jednotlivé bloky neuvolňuje, všechno najednou při reset(). */
    void deallocate(T*, std::size_t) noexcept {}

    /* Dva alokátory jsou "stejné", když paměť z jednoho jde vrátit druhému.
     * Operátor != si C++20 odvodí sám z ==. */
    template <class U>
    bool operator==(const ArenaAllocator<U>& other) const noexcept
    {
        return arena_ == other.arena_;
    }

private:
    template <class U>
    friend class ArenaAllocator;   // ať převodní konstruktor vidí arena_ jiné specializace

    Arena* arena_;
};

/* Demo 4 (v arena.cpp) */
void demo_templates();
