/*
 * memview.cpp – pohled pod kapotu: adresy, bajty, endianita, std::string zevnitř.
 *
 * C++ je pořád "blízko u hardwaru": adresy, sizeof i jednotlivé bajty vidíš
 * úplně stejně jako v C. Navíc dostaneš nástroje, se kterými se na bajty díváš
 * bezpečněji: std::span, std::byte, std::bit_cast a std::endian.
 */
#include "memview.hpp"
#include "util.hpp"

#include <algorithm>
#include <bit>        // std::bit_cast, std::endian
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/* C vs. C++: místo 'static' na globální úrovni se v C++ používá anonymní
 * namespace. Všechno uvnitř je viditelné jen v tomto souboru. */
namespace {

int g_initialized_global = 42;   // sekce .data
int g_zero_global;               // sekce .bss (nulová)

/* Konstantní data jdou do sekce jen pro čtení (.rdata / .rodata). */
const char g_message[] = "Ahoj z read-only dat";

/* C verze měla makro PRINT_SIZE(type) se stringifikací #type.
 * Šablona název typu z kódu nevyrobí (C++ zatím nemá reflexi), takže
 * jméno předáme jako text. Zato typ hlídá překladač. */
template <class T>
void print_size(std::string_view name)
{
    print("  {:<24} {:>3} B\n", name, sizeof(T));
}

}  // namespace

void hexdump(std::span<const std::byte> bytes)
{
    for (std::size_t offset = 0; offset < bytes.size(); offset += 16) {
        /* subspan = "okno" do části dat, nic se nekopíruje */
        const auto line = bytes.subspan(offset, std::min<std::size_t>(16, bytes.size() - offset));

        print("  {}  ", static_cast<const void*>(line.data()));

        for (std::size_t i = 0; i < 16; ++i) {
            if (i < line.size()) {
                /* std::byte není číslo, na číslo ho převede až std::to_integer */
                print("{:02X} ", std::to_integer<unsigned>(line[i]));
            } else {
                print("   ");
            }
        }

        print(" |");
        for (const std::byte b : line) {
            const auto c = std::to_integer<unsigned char>(b);
            std::cout << ((c >= 32 && c < 127) ? static_cast<char>(c) : '.');   // netisknutelné jako '.'
        }
        print("|\n");
    }
}

bool is_inside(const void* ptr, const void* begin, std::size_t size) noexcept
{
    const auto p = reinterpret_cast<std::uintptr_t>(ptr);
    const auto b = reinterpret_cast<std::uintptr_t>(begin);
    return p >= b && p < b + size;
}

void demo_memory_layout()
{
    print_header("1) Paměť pod lupou");

    int local = 7;                 // zásobník (stack), zanikne s koncem funkce
    static int static_local = 13;  // jako globální, jen viditelná uvnitř funkce

    /* C vs. C++: v C tu byl malloc + kontrola NULL + free na konci.
     * std::make_unique alokuje na haldě a vrátí "chytrý ukazatel", který paměť
     * uvolní SÁM, až zanikne (konec funkce, return i výjimka).
     * Když dojde paměť, vyletí výjimka std::bad_alloc, takže kontrola na NULL odpadá. */
    auto heap_value = std::make_unique<int>(99);

    print("Kde co bydlí v paměti (adresy se mezi spuštěními mění kvůli ASLR):\n\n");
    print("  kód     funkce demo_memory_layout  {:#x}\n",
          reinterpret_cast<std::uintptr_t>(&demo_memory_layout));
    print("  rodata  g_message                  {}\n", static_cast<const void*>(g_message));
    print("  data    g_initialized_global       {}\n", static_cast<const void*>(&g_initialized_global));
    print("  bss     g_zero_global              {}\n", static_cast<const void*>(&g_zero_global));
    print("  static  static_local               {}\n", static_cast<const void*>(&static_local));
    print("  heap    *heap_value                {}\n", static_cast<const void*>(heap_value.get()));
    print("  stack   local                      {}\n", static_cast<const void*>(&local));
    print("\n  Všimni si: globální a statická data leží u sebe, halda a zásobník jinde.\n");

    print("\nVelikosti typů (sizeof) na této platformě:\n");
    print_size<char>("char");
    print_size<bool>("bool");
    print_size<int>("int");
    print_size<long>("long");        // Windows: 4 B, Linux 64bit: 8 B, nepřenositelné!
    print_size<long long>("long long");
    print_size<double>("double");
    print_size<void*>("void *");
    print_size<std::byte>("std::byte");

    /* Typy ze standardní knihovny jsou taky jen struktury s pár ukazateli. */
    print("\n...a typy ze standardní knihovny:\n");
    print_size<std::unique_ptr<int>>("std::unique_ptr<int>");
    print_size<std::shared_ptr<int>>("std::shared_ptr<int>");
    print_size<std::string_view>("std::string_view");
    print_size<std::span<int>>("std::span<int>");
    print_size<std::optional<int>>("std::optional<int>");
    print_size<std::vector<int>>("std::vector<int>");
    print_size<std::string>("std::string");
    print_size<std::function<void()>>("std::function<void()>");
    print("\n  unique_ptr = 1 ukazatel, tedy žádná režie oproti holému ukazateli z C.\n"
          "  shared_ptr = 2 ukazatele (objekt + řídicí blok s počítadlem odkazů).\n"
          "  string_view a span = ukazatel + délka. vector = 3 ukazatele\n"
          "  (začátek dat, konec dat, konec kapacity). optional = hodnota + bool.\n"
          "  V Debug buildu MSVC jsou kontejnery větší o ladicí ukazatel.\n");

    /* --- Endianita --- */
    const std::uint32_t number = 0x11223344;
    print("\nČíslo 0x11223344 v paměti:\n");
    hexdump_of(number);

    /* C verze zjišťovala endianitu za běhu: *(unsigned char *)&number == 0x44.
     * C++20 to ví už při překladu (std::endian::native) a 'if constexpr'
     * nepotřebnou větev rovnou zahodí. */
    if constexpr (std::endian::native == std::endian::little) {
        print("  -> LITTLE endian (x86/x64/ARM): nejnižší bajt je první.\n");
    } else {
        print("  -> BIG endian: nejvyšší bajt je první.\n");
    }

    /* --- double podle IEEE 754, rozebraný přes std::bit_cast --- */
    const double pi = 3.14159265358979;
    print("\ndouble {:.5f} v paměti (formát IEEE 754, 8 bajtů):\n", pi);
    hexdump_of(pi);

    /* C vs. C++: v C by ses na bity double podíval přes union nebo memcpy.
     * V C++ je čtení "jiného" členu unionu nedefinované chování
     * a *(uint64_t *)&pi porušuje strict aliasing.
     * std::bit_cast (C++20) je ta správná cesta a funguje i v constexpr. */
    const auto bits = std::bit_cast<std::uint64_t>(pi);
    const auto sign = bits >> 63;
    const auto exponent = (bits >> 52) & 0x7FF;
    const auto mantissa = bits & 0xF'FFFF'FFFF'FFFF;   // ' = oddělovač číslic (C++14)
    print("  std::bit_cast<uint64_t>(pi) = {:#018x}\n", bits);
    print("  znaménko {}, exponent {} (tj. 2^{}), mantisa {:#015x}\n",
          sign, exponent, static_cast<int>(exponent) - 1023, mantissa);

    /* --- std::string zevnitř: Small String Optimization (SSO) --- */
    const std::string short_text = "Ahoj, C++!";
    const std::string long_text = "Tenhle řetězec je moc dlouhý, aby se vešel dovnitř objektu.";

    print("\nstd::string a SSO (small string optimization), sizeof(std::string) = {} B:\n",
          sizeof(std::string));
    print("  krátký \"{}\": data na {} -> {}\n", short_text,
          static_cast<const void*>(short_text.data()),
          is_inside(short_text.data(), &short_text, sizeof short_text)
              ? "UVNITŘ objektu (žádná alokace!)"
              : "na haldě");
    print("  dlouhý ({} B):   data na {} -> {}\n", long_text.size(),
          static_cast<const void*>(long_text.data()),
          is_inside(long_text.data(), &long_text, sizeof long_text)
              ? "uvnitř objektu"
              : "na haldě (string si drží jen ukazatel)");
    print("\n  Bajty objektu std::string s krátkým textem (text je přímo v nich):\n");
    hexdump_of(short_text);
    print("  C vs. C++: v C je řetězec char[] nebo char* + strlen a o paměť se staráš sám.\n"
          "  std::string zná svou délku, sám se zvětšuje a sám se uvolní.\n");

    /* --- neinicializovaná vs. vynulovaná paměť --- */
    /* C vs. C++: malloc(16) vrací neinicializovanou paměť, calloc vynulovanou.
     * V C++20 tomu odpovídá make_unique_for_overwrite<T[]>(n) a make_unique<T[]>(n). */
    constexpr std::size_t raw_size = 16;
    const auto raw = std::make_unique_for_overwrite<unsigned char[]>(raw_size);
    print("\nmake_unique_for_overwrite<unsigned char[]>(16), neinicializované (jako malloc):\n");
    hexdump(std::as_bytes(std::span{raw.get(), raw_size}));
    print("  V Debug buildu MSVC je nová halda vyplněná 0xCD, v Release tam je smetí.\n");

    const auto zeroed = std::make_unique<unsigned char[]>(raw_size);
    print("make_unique<unsigned char[]>(16), vynulované (jako calloc):\n");
    hexdump(std::as_bytes(std::span{zeroed.get(), raw_size}));

    /* Žádné free() ani delete: heap_value, raw i zeroed se uvolní samy tady,
     * na konci funkce, v opačném pořadí, než vznikly. */
}
