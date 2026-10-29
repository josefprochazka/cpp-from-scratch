/*
 * noisy.hpp – "upovídaná" třída, která hlásí každý konstruktor, kopii,
 * přesun i destruktor. Díky ní je vidět, co C++ dělá za tebe.
 *
 * Třída, která sama spravuje nějaký zdroj, má pět speciálních metod
 * ("rule of five"): copy/move konstruktor, copy/move přiřazení a destruktor.
 * Tady je píšeme ručně jen kvůli výpisům. Normálně je za tebe vygeneruje
 * překladač ("rule of zero"), když jsou všechny členy samy rozumné typy
 * (std::string, std::vector, std::unique_ptr ...).
 */
#pragma once

#include <string>
#include <string_view>
#include <utility>

#include "util.hpp"

struct NoisyStats {
    int constructed = 0;
    int copied = 0;
    int moved = 0;
    int destroyed = 0;
};

class Noisy {
public:
    /* explicit = zakáže skrytý převod  Noisy n = std::string{"x"}; */
    explicit Noisy(std::string name) : name_(std::move(name))
    {
        ++stats.constructed;
        log("konstruktor");
    }

    /* Kopírovací konstruktor: nový objekt jako KOPIE jiného.
     * std::string se zkopíruje, tedy nová alokace + memcpy. */
    Noisy(const Noisy& other) : name_(other.name_)
    {
        ++stats.copied;
        log("KOPIE (copy constructor)");
    }

    /* Přesouvací konstruktor: "ukradne" vnitřnosti objektu, který už nikdo
     * nepotřebuje (dočasný objekt nebo std::move). U std::string se jen
     * přehodí ukazatel na buffer, nic se nekopíruje.
     * noexcept je důležité, viz demo 3 s std::vector. */
    Noisy(Noisy&& other) noexcept : name_(std::move(other.name_))
    {
        other.name_ = "(přesunutý)";   // krátké, vejde se do SSO -> žádná alokace
        ++stats.moved;
        log("přesun (move constructor)");
    }

    Noisy& operator=(const Noisy& other)
    {
        name_ = other.name_;   // přiřazení a = a zvládne std::string sám
        ++stats.copied;
        log("KOPIE (copy assignment)");
        return *this;
    }

    Noisy& operator=(Noisy&& other) noexcept
    {
        if (this != &other) {
            name_ = std::move(other.name_);
            other.name_ = "(přesunutý)";
        }
        ++stats.moved;
        log("přesun (move assignment)");
        return *this;
    }

    /* Destruktor se volá AUTOMATICKY, když objekt zanikne. Na tom stojí RAII. */
    ~Noisy()
    {
        ++stats.destroyed;
        log("destruktor");
    }

    const std::string& name() const noexcept { return name_; }

    /* inline static (C++17) = statická proměnná třídy definovaná rovnou tady,
     * bez nutnosti psát definici do .cpp souboru. */
    static inline NoisyStats stats{};
    static inline bool verbose = true;

private:
    void log(std::string_view what) const
    {
        if (verbose) {
            print("    [{}] {}\n", name_, what);
        }
    }

    std::string name_;
};
