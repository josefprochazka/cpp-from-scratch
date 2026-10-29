/*
 * arena.cpp – implementace arény a demo 4: šablony, arena a alokátory.
 */
#include "arena.hpp"
#include "fixed_stack.hpp"
#include "memview.hpp"
#include "util.hpp"

#include <algorithm>
#include <array>
#include <concepts>
#include <memory_resource>   // std::pmr (C++17)
#include <string>
#include <string_view>
#include <vector>

Arena::Arena(std::size_t capacity)
    /* make_unique_for_overwrite = paměť se NEnuluje, stejně jako u malloc. */
    : buffer_(std::make_unique_for_overwrite<std::byte[]>(capacity)), capacity_(capacity)
{
}

void* Arena::allocate(std::size_t size, std::size_t align) noexcept
{
    /* C verze: (align & (align - 1)) != 0. V C++20 na to je funkce v <bit>. */
    if (!std::has_single_bit(align)) {
        return nullptr;
    }

    const auto current = reinterpret_cast<std::uintptr_t>(buffer_.get() + offset_);
    const auto aligned = align_up(current, static_cast<std::uintptr_t>(align));
    const std::size_t padding = aligned - current;
    const std::size_t remaining = capacity_ - offset_;

    /* Dvě podmínky místo  padding + size > remaining, protože ten součet
     * by u obřího size mohl přetéct. */
    if (padding > remaining || size > remaining - padding) {
        return nullptr;   // došlo místo
    }

    std::byte* result = buffer_.get() + offset_ + padding;
    offset_ += padding + size;
    return result;
}

namespace {

/* Concept (C++20) = pojmenovaná podmínka na typ. sum(FixedStack<std::string, 3>)
 * se nepřeloží s čitelnou hláškou "constraints not satisfied" místo
 * stránky chyb z útrob šablony. */
template <class T>
concept Number = std::integral<T> || std::floating_point<T>;

template <Number T, std::size_t N>
constexpr T sum(const FixedStack<T, N>& stack)
{
    T total{};
    for (const T& item : stack) {
        total += item;
    }
    return total;
}

/* Celý zásobník proběhne UŽ PŘI PŘEKLADU. Ve výsledném .exe bude jen číslo 6. */
constexpr int sum_of_popped()
{
    FixedStack<int, 4> stack;
    stack.push(1);
    stack.push(2);
    stack.push(3);

    int total = 0;
    while (const auto value = stack.pop()) {
        total += *value;
    }
    return total;
}

constexpr int compile_time_sum = sum_of_popped();
static_assert(compile_time_sum == 6, "FixedStack nefunguje ani při překladu");

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

/* Pomocník pro demo: alokuje a vypíše, co se stalo (jako alloc_verbose v C). */
void* alloc_verbose(Arena& arena, std::size_t size, std::size_t align, std::string_view what)
{
    const std::size_t before = arena.used();
    void* ptr = arena.allocate(size, align);

    if (ptr == nullptr) {
        print("  {:<24} {:>4} B  align {:>2}  -> nullptr (v aréně není místo)\n", what, size, align);
    } else {
        const auto start = static_cast<std::size_t>(static_cast<const std::byte*>(ptr) - arena.data());
        print("  {:<24} {:>4} B  align {:>2}  -> offset {:>4}  (výplň {} B)  {}\n",
              what, size, align, start, start - before, ptr);
    }
    return ptr;
}

}  // namespace

void demo_templates()
{
    print_header("4) Šablony, arena a alokátory");

    /* --- 1. generický kontejner --- */
    print("Šablona FixedStack<T, N>: jeden kód, libovolný typ, data přímo v objektu.\n\n");

    FixedStack<int, 8> int_stack;
    for (int i = 1; i <= 5; ++i) {
        int_stack.push(i * i);
    }
    print("  FixedStack<int, 8>:         sizeof = {:>3} B, size = {}, sum() = {}\n",
          sizeof int_stack, int_stack.size(), sum(int_stack));

    FixedStack<std::string, 3> word_stack;
    word_stack.push("šablony");
    word_stack.push("místo");
    word_stack.push("void*");
    const bool pushed = word_stack.push("navíc");
    print("  FixedStack<std::string, 3>: sizeof = {:>3} B, plný, push(\"navíc\") vrátil {}\n",
          sizeof word_stack, pushed);

    print("  pop() vrací std::optional:");
    while (const auto word = word_stack.pop()) {
        print(" \"{}\"", *word);
    }
    print("\n  pop() na prázdném zásobníku vrátí {}\n",
          word_stack.pop().has_value() ? "hodnotu" : "std::nullopt");
    print("  sum_of_popped() = {}. Spočítal to už překladač (constexpr + static_assert).\n",
          compile_time_sum);

    /* --- 2. arena jako třída --- */
    Arena arena{1024};
    print("\nAréna: jeden blok {} B na {}\n\n", arena.capacity(), static_cast<const void*>(arena.data()));

    constexpr std::string_view label = "C++20";
    auto* name = static_cast<char*>(alloc_verbose(arena, label.size() + 1, alignof(char), "char[6] \"C++20\""));
    if (name != nullptr) {
        std::ranges::copy(label, name);
        name[label.size()] = '\0';
    }

    /* Typované varianty: místo  (double *)arena_alloc(&a, sizeof(double), _Alignof(double))
     * stačí create<double>(...). Velikost i zarovnání si šablona zjistí sama. */
    const std::size_t before_double = arena.used();
    double* d = arena.create<double>(2.71828);
    print("  {:<24} create<double>(2.71828), využito {} -> {} B\n", "double", before_double, arena.used());

    const std::size_t before_points = arena.used();
    std::span<Vec3> points = arena.create_array<Vec3>(10);
    print("  {:<24} create_array<Vec3>(10), využito {} -> {} B\n", "Vec3[10]", before_points, arena.used());
    for (std::size_t i = 0; i < points.size(); ++i) {
        points[i].x = static_cast<float>(i);
        points[i].y = static_cast<float>(i * i);
    }

    (void)alloc_verbose(arena, 1, 1, "char");
    (void)alloc_verbose(arena, 64, 16, "buffer 64 B (align 16)");
    (void)alloc_verbose(arena, 2000, 8, "obří blok 2000 B");

    print("\nVyužito {} z {} B.\n", arena.used(), arena.capacity());
    if (name != nullptr && d != nullptr && points.size() == 10) {
        print("Data jsou normálně použitelná: name = \"{}\", d = {:.5f}, points[3] = ({}, {}, {})\n",
              name, *d, points[3].x, points[3].y, points[3].z);
    }

    print("\nPrvních 48 bajtů arény (\"C++20\", výplň, double a začátek Vec3):\n");
    hexdump(std::span{arena.data(), 48});

    arena.reset();
    print("\narena.reset(): využito {} B. Všechno \"uvolněno\" jedním přiřazením.\n", arena.used());

    /* --- 3. std::vector s vlastním alokátorem --- */
    print("\nstd::vector<int, ArenaAllocator<int>>: vector bere paměť z arény:\n");
    {
        std::vector<int, ArenaAllocator<int>> numbers{ArenaAllocator<int>{arena}};
        for (int i = 0; i < 10; ++i) {
            const auto old_capacity = numbers.capacity();
            numbers.push_back(i);
            if (numbers.capacity() != old_capacity) {
                print("  push_back({}) -> capacity {:>2}, aréna využita {:>3} B\n",
                      i, numbers.capacity(), arena.used());
            }
        }
        print("  Při každé realokaci zůstal starý blok v aréně ležet, protože aréna\n"
              "  jednotlivě nic neuvolňuje. S numbers.reserve(10) by stačila jedna alokace.\n");
    }   // vector zanikne, zavolá deallocate (u nás nic nedělá)
    arena.reset();

    /* --- 4. std::pmr: aréna přímo ze standardní knihovny --- */
    print("\nstd::pmr::monotonic_buffer_resource je hotová \"aréna\" ve standardu (C++17):\n");

    std::array<std::byte, 2048> buffer{};   // 2 KB paměti na ZÁSOBNÍKU
    std::pmr::monotonic_buffer_resource resource{buffer.data(), buffer.size(),
                                                 std::pmr::null_memory_resource()};

    /* pmr::vector a pmr::string berou paměť z resource místo z haldy.
     * Vector navíc předá svůj alokátor i řetězcům uvnitř. */
    std::pmr::vector<std::pmr::string> words(&resource);
    words.reserve(4);
    words.emplace_back("Tenhle řetězec je dost dlouhý na alokaci,");
    words.emplace_back("a přesto se nevolá new ani malloc:");
    words.emplace_back("všechno leží v poli bajtů na zásobníku.");

    print("  pole words.data() na {} -> {}\n", static_cast<const void*>(words.data()),
          is_inside(words.data(), buffer.data(), buffer.size()) ? "v bufferu" : "na haldě");
    for (const auto& word : words) {
        print("  \"{}\"\n      data na {} -> {}\n", word, static_cast<const void*>(word.data()),
              is_inside(word.data(), buffer.data(), buffer.size()) ? "v bufferu na zásobníku" : "na haldě");
    }

    try {
        std::pmr::vector<std::byte> too_big(&resource);
        too_big.resize(4096);
    } catch (const std::bad_alloc&) {
        print("  Požadavek na 4096 B: buffer došel a záložní zdroj je null_memory_resource\n"
              "  -> výjimka std::bad_alloc. (S výchozím zdrojem by si došel na haldu.)\n");
    }
}
