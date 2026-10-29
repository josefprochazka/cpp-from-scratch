/*
 * util.hpp – drobné pomocné funkce pro práci s konzolí.
 *
 * C vs. C++: místo include guardu (#ifndef UTIL_H / #define UTIL_H / #endif)
 * stačí #pragma once. Není to ve standardu, ale umí to všechny běžné překladače.
 */
#pragma once

#include <format>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

/* Typově bezpečná náhrada printf:  print("{} + {} = {}\n", 1, 2, 3);
 *
 * Formátovací řetězec se kontroluje UŽ PŘI PŘEKLADU. Když nesedí počet nebo typ
 * argumentů, je to chyba překladu. V C je printf("%s", 42) jen varování
 * a za běhu pád. C++23 má hotové std::print, my jsme v C++20, tak si ho
 * napíšeme sami. (Až přejdeš na C++23, tuhle funkci smaž a použij <print>.) */
template <class... Args>
void print(std::format_string<Args...> fmt, Args&&... args)
{
    std::cout << std::format(fmt, std::forward<Args>(args)...);
}

/* Vypíše nadpis sekce. */
void print_header(std::string_view title);

/* Načte řádek ze stdin (bez '\n'). std::nullopt = konec vstupu (Ctrl+Z).
 * C vs. C++: žádný buffer pevné délky, std::string si naroste sám. */
std::optional<std::string> read_line(std::string_view prompt);

/* Načte celé číslo. std::nullopt = neplatný vstup nebo konec vstupu.
 * C verze vracela bool a výsledek zapisovala přes ukazatel (int *out).
 * std::optional nese "hodnotu, nebo nic" v jednom návratovém typu. */
std::optional<int> read_int(std::string_view prompt);

/* Počká, až uživatel zmáčkne Enter. */
void wait_for_enter();
