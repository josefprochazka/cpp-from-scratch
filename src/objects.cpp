/*
 * objects.cpp – reference, padding a co je vlastně uvnitř objektu.
 *
 * Třída v C++ je pořád jen kus paměti. Metody v ní nezabírají místo,
 * virtuální funkce přidají jeden skrytý ukazatel a dědičnost jen poskládá
 * členy za sebe. Pojďme se na ty bajty podívat.
 */
#include "objects.hpp"
#include "memview.hpp"
#include "util.hpp"

#include <cstddef>   // offsetof
#include <cstdint>
#include <cstdlib>   // std::abs
#include <cstring>   // std::memset, std::memcpy
#include <memory>
#include <numbers>   // std::numbers::pi (C++20)
#include <string_view>
#include <type_traits>
#include <vector>

namespace {

/* Prohození přes REFERENCE.
 * C verze: swap_ints(int *a, int *b) a volání swap_ints(&x, &y).
 * Reference je "jiné jméno" pro existující proměnnou: nemůže být NULL
 * a nejde přesměrovat jinam. Uvnitř je to většinou obyčejný ukazatel,
 * jen se nemusí psát & při volání a * při použití. */
void swap_ints(int& a, int& b) noexcept
{
    const int tmp = a;
    a = b;
    b = tmp;
}

/* Stejné struktury jako v C verzi: stejné členy, jiné pořadí. */
struct Padded {
    char a;   // 1 B + 3 B výplně
    int  b;   // 4 B
    char c;   // 1 B + 3 B výplně na konci
};

struct Reordered {
    int  b;
    char a;
    char c;   // + jen 2 B výplně
};

/* offsetof je zaručený jen pro "standard layout" typy (struktury jako v C). */
static_assert(std::is_standard_layout_v<Padded>);

/* Prázdná třída. Má velikost 1 B, protože každý objekt musí mít vlastní adresu. */
struct Empty {};

/* Obyčejná třída s metodami. Metody NEZABÍRAJÍ místo v objektu. Jsou to
 * normální funkce, které dostanou skrytý parametr 'this' (ukazatel na objekt).
 * V C bys psal  int point_manhattan(const Point *self). */
class Point {
public:
    Point(int x, int y) : x_(x), y_(y) {}
    int manhattan() const { return std::abs(x_) + std::abs(y_); }

private:
    int x_;
    int y_;
};

/* Abstraktní třída s virtuálními funkcemi. Každý objekt potomka dostane skrytý
 * ukazatel "vptr" na tabulku virtuálních funkcí (vtable) své třídy.
 *
 * C vs. C++: v C se polymorfismus dělá ručně, stejně jako to dělá překladač:
 *     typedef struct { double (*area)(const void *self); } ShapeVTable;
 *     typedef struct { const ShapeVTable *vtable; int w, h; } Rectangle;
 *     double a = rect.vtable->area(&rect);
 * V C++ to za tebe vygeneruje překladač a nejde se splést v přetypování. */
class Shape {
public:
    /* Virtuální destruktor je nutný! Bez něj by  delete shape  (nebo
     * unique_ptr<Shape>) zavolal jen ~Shape a destruktor potomka by se přeskočil. */
    virtual ~Shape() = default;
    virtual double area() const = 0;   // = 0 -> čistě virtuální, musí ji dodat potomek
    virtual std::string_view name() const = 0;
};

/* final = od této třídy už nikdo nedědí (překladač pak může volání zrychlit). */
class Rectangle final : public Shape {
public:
    Rectangle(int w, int h) : w_(w), h_(h) {}
    double area() const override { return static_cast<double>(w_) * h_; }
    std::string_view name() const override { return "obdélník"; }

private:
    int w_;
    int h_;
};

class Circle final : public Shape {
public:
    explicit Circle(int r) : r_(r) {}
    double area() const override { return std::numbers::pi * r_ * r_; }
    std::string_view name() const override { return "kruh"; }

private:
    int r_;
};

/* Jednoduchá dědičnost: členy předka jsou na začátku, potomek přidá své za ně. */
struct Base {
    std::uint32_t base_value = 0x11111111;
};

struct Derived : Base {
    std::uint32_t derived_value = 0x22222222;
};

/* Vícenásobná dědičnost: dva předci za sebou. */
struct First {
    std::uint32_t first = 0xAAAAAAAA;
};

struct Second {
    std::uint32_t second = 0xBBBBBBBB;
};

struct Both : First, Second {
    std::uint32_t both = 0xCCCCCCCC;
};

/* Vzdálenost adresy member od začátku objektu v bajtech. */
std::ptrdiff_t offset_in(const void* object, const void* member)
{
    return static_cast<const std::byte*>(member) - static_cast<const std::byte*>(object);
}

/* Přečte prvních 8 (na 64bit) bajtů objektu, tedy vptr.
 * Kde přesně vptr leží, standard neříká. MSVC, GCC i Clang ho ale dávají
 * na začátek objektu. Je to čistě pozorování, v reálném kódu tohle nedělej. */
std::uintptr_t vptr_of(const Shape& shape)
{
    std::uintptr_t vptr = 0;
    std::memcpy(&vptr, &shape, sizeof vptr);
    return vptr;
}

}  // namespace

void demo_objects()
{
    print_header("2) Reference, ukazatele a objekty zevnitř");

    /* --- ukazatel vs. reference --- */
    int value = 5;
    int* ptr = &value;    // ukazatel: proměnná s adresou, může být nullptr
    int& ref = value;     // reference: jiné jméno pro value

    print("value = {}, *ptr = {}, ref = {}\n", value, *ptr, ref);
    print("&value = {}\n", static_cast<const void*>(&value));
    print("&ref   = {}   (reference nemá vlastní adresu, &ref je adresa value)\n",
          static_cast<const void*>(&ref));
    print("&ptr   = {}   (ukazatel je samostatná proměnná s vlastní adresou)\n",
          static_cast<const void*>(&ptr));
    ref = 10;
    print("Po  ref = 10;  je value = {}\n", value);

    int x = 1;
    int y = 2;
    print("\nPřed swap_ints(x, y): x = {}, y = {}\n", x, y);
    swap_ints(x, y);   // žádné &x, &y jako v C
    print("Po  swap_ints(x, y): x = {}, y = {}   (v praxi prostě std::swap(x, y))\n", x, y);

    /* --- padding, úplně stejně jako v C --- */
    print("\nZarovnání (padding) struktur funguje stejně jako v C:\n");
    print("  Padded    {{ char a; int b; char c; }}  sizeof = {:>2} B   offsetof a = {}, b = {}, c = {}\n",
          sizeof(Padded), offsetof(Padded, a), offsetof(Padded, b), offsetof(Padded, c));
    print("  Reordered {{ int b; char a; char c; }}  sizeof = {:>2} B   offsetof b = {}, a = {}, c = {}\n",
          sizeof(Reordered), offsetof(Reordered, b), offsetof(Reordered, a), offsetof(Reordered, c));

    Padded padded;
    std::memset(&padded, 0xCC, sizeof padded);   // ať je vidět výplň
    padded.a = 'A';
    padded.b = 0x11223344;
    padded.c = 'C';
    print("\n  Padded v paměti (CC = výplň, kterou vložil překladač):\n");
    hexdump_of(padded);

    /* --- co je uvnitř objektu --- */
    const Point point{3, -4};
    print("\nCo je uvnitř objektu:\n");
    print("  sizeof(Empty)     = {:>2} B   (prázdná třída, ale každý objekt potřebuje adresu)\n",
          sizeof(Empty));
    print("  sizeof(Point)     = {:>2} B   (2x int, metody místo nezabírají; manhattan() = {})\n",
          sizeof(Point), point.manhattan());
    print("  sizeof(Rectangle) = {:>2} B   (2x int + skrytý vptr + výplň)\n", sizeof(Rectangle));
    print("  sizeof(Circle)    = {:>2} B   (1x int + skrytý vptr + výplň)\n", sizeof(Circle));

    const Rectangle r1{3, 4};
    const Rectangle r2{5, 6};
    const Circle c1{2};
    print("\n  Rectangle r1(3, 4) v paměti: prvních 8 B je vptr, pak w = 3 a h = 4:\n");
    hexdump_of(r1);
    print("  vptr r1 = {:#x}\n", vptr_of(r1));
    print("  vptr r2 = {:#x}   (stejná třída = stejná vtable)\n", vptr_of(r2));
    print("  vptr c1 = {:#x}   (jiná třída = jiná vtable)\n", vptr_of(c1));

    /* Polymorfismus: jeden vector, různé třídy, volání přes vtable.
     * unique_ptr<Shape> objekty sám smaže a díky virtuálnímu destruktoru správně. */
    std::vector<std::unique_ptr<Shape>> shapes;
    shapes.push_back(std::make_unique<Rectangle>(3, 4));
    shapes.push_back(std::make_unique<Circle>(2));
    print("\n  Volání virtuální funkce přes ukazatel na předka (shape->area()):\n");
    for (const auto& shape : shapes) {
        print("    {:<9} plocha {:>6.2f}\n", shape->name(), shape->area());
    }

    /* --- layout dědičnosti --- */
    const Derived derived;
    print("\nDědičnost: Derived : Base, sizeof(Derived) = {} B\n", sizeof(Derived));
    print("  base_value na offsetu {}, derived_value na offsetu {} (předek je první)\n",
          offset_in(&derived, &derived.base_value), offset_in(&derived, &derived.derived_value));
    hexdump_of(derived);

    /* Vícenásobná dědičnost: ukazatel na druhého předka NENÍ stejná adresa
     * jako objekt. Překladač při převodu Both* -> Second* přičte offset. */
    const Both both;
    const First* as_first = &both;
    const Second* as_second = &both;
    print("\nVícenásobná dědičnost: Both : First, Second, sizeof(Both) = {} B\n", sizeof(Both));
    print("  &both               = {}\n", static_cast<const void*>(&both));
    print("  (First*)  &both     = {}   (stejná adresa)\n", static_cast<const void*>(as_first));
    print("  (Second*) &both     = {}   (posunuto o {} B!)\n",
          static_cast<const void*>(as_second), offset_in(&both, as_second));
    hexdump_of(both);
    print("  Proto v C++ nepřetypovávej ukazatele na objekty přes void* tam a zpátky.\n"
          "  static_cast posun dopočítá, reinterpret_cast a (void*) ne.\n");
}
