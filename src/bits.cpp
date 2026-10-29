/*
 * bits.cpp – bitové operace v praxi, tentokrát s nástroji C++20.
 */
#include "bits.hpp"
#include "util.hpp"

#include <array>
#include <bit>        // popcount, has_single_bit, bit_width, bit_ceil, rotl, bit_cast
#include <bitset>
#include <concepts>
#include <cstddef>    // std::byte
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

/* C vs. C++: v C to byla makra, třeba
 *     #define SET_BIT(reg, bit) ((reg) |= (1u << (bit)))
 * Makro nezná typy, argumenty může vyhodnotit víckrát a v debuggeru do něj
 * nevkročíš. constexpr šablona je stejně rychlá (překladač ji vloží na místo
 * volání), ale je to normální funkce s typy. */
template <std::unsigned_integral T>
constexpr T set_bit(T value, unsigned bit) noexcept
{
    return static_cast<T>(value | (T{1} << bit));
}

template <std::unsigned_integral T>
constexpr T clear_bit(T value, unsigned bit) noexcept
{
    return static_cast<T>(value & ~(T{1} << bit));
}

template <std::unsigned_integral T>
constexpr T toggle_bit(T value, unsigned bit) noexcept
{
    return static_cast<T>(value ^ (T{1} << bit));
}

template <std::unsigned_integral T>
constexpr bool test_bit(T value, unsigned bit) noexcept
{
    return ((value >> bit) & 1u) != 0;
}

/* Testy, které proběhnou při překladu: */
static_assert(set_bit(0b0000u, 2) == 0b0100u);
static_assert(clear_bit(0b1111u, 0) == 0b1110u);
static_assert(toggle_bit(toggle_bit(0b1010u, 3), 3) == 0b1010u);
static_assert(test_bit(0b1000u, 3) && !test_bit(0b1000u, 2));

/* Bity řídicího registru. enum class: jména nepřetečou do okolního scope
 * a nejde je omylem sečíst s obyčejným číslem (musí se převést explicitně). */
enum class RegBit : unsigned {
    Enable    = 0,   // bit 0: zapnout periferii
    Interrupt = 3,   // bit 3: povolit přerušení
    Led       = 7    // bit 7: rozsvítit LED
};

constexpr unsigned bit_index(RegBit bit) noexcept
{
    return static_cast<unsigned>(bit);   // C++23 na to má std::to_underlying
}

/* Barva jako struktura + constexpr převody do jednoho uint32_t a zpět. */
struct Rgb {
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
};

constexpr std::uint32_t pack(Rgb c) noexcept
{
    return (std::uint32_t{c.r} << 16) | (std::uint32_t{c.g} << 8) | std::uint32_t{c.b};
}

constexpr Rgb unpack(std::uint32_t v) noexcept
{
    return Rgb{static_cast<std::uint8_t>(v >> 16), static_cast<std::uint8_t>(v >> 8),
               static_cast<std::uint8_t>(v)};
}

static_assert(pack(Rgb{0xFF, 0x88, 0x22}) == 0xFF8822u);
static_assert(unpack(0xFF8822u).g == 0x88);

/* bitset.to_string() s podtržítkem po čtveřicích: 1000_0001 */
template <std::size_t N>
std::string grouped(const std::bitset<N>& bits)
{
    const std::string text = bits.to_string();
    std::string result;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (i != 0 && (text.size() - i) % 4 == 0) {
            result += '_';
        }
        result += text[i];
    }
    return result;
}

}  // namespace

void demo_bits()
{
    print_header("5) Bitové operace");

    /* Na mikrokontroléru bys psal pořád stejně jako v C:
     *     auto* ctrl = reinterpret_cast<volatile std::uint32_t*>(0x40021000);
     *     *ctrl |= 1u << 0;
     * Na PC to nejde (OS by nás zastavil), tak registr simulujeme. */

    /* --- registr jako std::bitset --- */
    std::bitset<8> control;
    const auto show = [&control](std::string_view label) {
        print("  {:<24} {}  (0x{:02X})\n", label, grouped(control), control.to_ulong());
    };

    print("Simulovaný 8bitový řídicí registr jako std::bitset<8>:\n");
    show("start");
    control.set(bit_index(RegBit::Enable));
    show("set(Enable)");
    control.set(bit_index(RegBit::Interrupt));
    show("set(Interrupt)");
    control.flip(bit_index(RegBit::Led));
    show("flip(Led)");
    control.flip(bit_index(RegBit::Led));
    show("flip(Led) znovu");
    control.reset(bit_index(RegBit::Enable));
    show("reset(Enable)");
    print("  test(Interrupt) = {}, count() = {}, any() = {}\n",
          control.test(bit_index(RegBit::Interrupt)), control.count(), control.any());

    /* bitset::test hlídá rozsah. V C by TEST_BIT(reg, 40) na 32bit čísle byl
     * posun o víc, než je šířka typu, tedy nedefinované chování. */
    try {
        const bool bit8 = control.test(8);
        print("  test(8) = {}\n", bit8);
    } catch (const std::out_of_range&) {
        print("  test(8) -> výjimka std::out_of_range (bit 8 v 8bitovém registru není)\n");
    }

    /* --- constexpr funkce místo maker --- */
    std::uint8_t reg = 0;
    reg = set_bit(reg, bit_index(RegBit::Led));
    reg = toggle_bit(reg, bit_index(RegBit::Enable));
    reg = set_bit(reg, bit_index(RegBit::Interrupt));
    reg = clear_bit(reg, bit_index(RegBit::Interrupt));
    print("\nTotéž s constexpr funkcemi na uint8_t (set Led, toggle Enable, set+clear Interrupt):\n");
    print("  {}  (0x{:02X}), test_bit(Led) = {}\n",
          grouped(std::bitset<8>{reg}), reg, test_bit(reg, bit_index(RegBit::Led)));

    /* --- <bit> --- */
    /* C verze počítala jedničky Kernighanovým trikem ve smyčce.
     * std::popcount a spol. překladač přeloží na jedinou instrukci procesoru
     * (POPCNT, LZCNT, TZCNT), pokud ji cílový procesor má. */
    print("\n<bit> (C++20):\n");
    print("  {:>5}  {:<14}  {:>8}  {:>14}  {:>9}  {:>8}\n",
          "číslo", "binárně", "popcount", "has_single_bit", "bit_width", "bit_ceil");
    constexpr std::array<std::uint32_t, 6> samples{1, 6, 64, 255, 1000, 1024};
    for (const std::uint32_t n : samples) {
        print("  {:>5}  {:<14}  {:>8}  {:>14}  {:>9}  {:>8}\n",
              n, grouped(std::bitset<12>{n}), std::popcount(n),
              std::has_single_bit(n) ? "ano" : "ne", std::bit_width(n), std::bit_ceil(n));
    }
    print("  countr_zero(40) = {} (nul zprava; 40 = 0b101000)\n", std::countr_zero(40u));
    print("  rotl(0b1000'0001 jako uint8_t, 1) = {:#010b} (rotace: bit, který vypadne vlevo, přijde zprava)\n",
          std::rotl(std::uint8_t{0b1000'0001}, 1));

    /* --- std::byte --- */
    /* std::byte je "jen bajt", ne číslo ani znak. Jdou na něj jen bitové
     * operace. V C je unsigned char zároveň bajt, číslo i znak a překladač
     * nepozná, co z toho jsi myslel. */
    std::byte flags{0b0011'0000};
    flags |= std::byte{0b0000'0001};
    flags <<= 1;
    // flags = flags + 1;   // CHYBA překladu: std::byte není číslo
    print("\nstd::byte po |= 0b1 a <<= 1: {:#010b} (sčítání by byla chyba překladu)\n",
          std::to_integer<unsigned>(flags));

    /* --- barva zabalená do čísla --- */
    constexpr Rgb orange{0xFF, 0x88, 0x22};
    constexpr std::uint32_t color = pack(orange);   // spočítá překladač
    const Rgb back = unpack(color);
    print("\nBarva RGB({}, {}, {}) zabalená do 32 bitů: 0x{:06X} (constexpr, spočítáno při překladu)\n",
          orange.r, orange.g, orange.b, color);
    print("  rozbaleno zpět: R = {}, G = {}, B = {}\n", back.r, back.g, back.b);

    /* Jak to leží v paměti? std::bit_cast na pole bajtů, bez ukazatelů a bez UB. */
    const auto bytes = std::bit_cast<std::array<std::uint8_t, 4>>(color);
    print("  bajty v paměti: {:02X} {:02X} {:02X} {:02X} (little endian: B, G, R, 00)\n",
          bytes[0], bytes[1], bytes[2], bytes[3]);

    /* --- XOR swap z C verze --- */
    unsigned a = 12;
    unsigned c = 34;
    std::swap(a, c);
    print("\nXOR swap z C verze? V C++ prostě std::swap(a, c): a = {}, c = {}.\n"
          "  Čitelné a překladač ho optimalizuje nejlíp, jak umí.\n", a, c);
}
