/*
 * memview.hpp – hexdump a demo 1 (paměť pod lupou).
 */
#pragma once

#include <cstddef>
#include <span>

/* Vypíše bajty jako hex + ASCII.
 *
 * C vs. C++: C verze měla hexdump(const void *data, size_t size), tedy dva
 * parametry, které k sobě patří, ale překladač o tom neví. std::span drží
 * ukazatel i délku v jednom objektu (16 B, nic nealokuje).
 * std::byte je "jen bajt": není to znak ani číslo. */
void hexdump(std::span<const std::byte> bytes);

/* Hexdump libovolného objektu: hexdump_of(x) místo hexdump(&x, sizeof x).
 * Šablona zná typ, takže si velikost zjistí sama a nejde se splést. */
template <class T>
void hexdump_of(const T& object)
{
    hexdump(std::as_bytes(std::span<const T>(&object, 1)));
}

/* Leží adresa ptr uvnitř bloku [begin, begin + size)?
 * Porovnáváme čísla (uintptr_t). Porovnávat ukazatele do RŮZNÝCH objektů
 * operátorem < není v C ani v C++ definované. */
bool is_inside(const void* ptr, const void* begin, std::size_t size) noexcept;

/* Demo 1 */
void demo_memory_layout();
