/*
 * util.cpp – implementace pomocných funkcí z util.hpp
 */
#include "util.hpp"

#include <charconv>       // std::from_chars
#include <system_error>   // std::errc

void print_header(std::string_view title)
{
    print("\n==========================================================\n");
    print("  {}\n", title);
    print("==========================================================\n\n");
}

std::optional<std::string> read_line(std::string_view prompt)
{
    if (!prompt.empty()) {
        std::cout << prompt << std::flush;
    }

    /* std::getline čte do std::string – ten si sám zvětšuje paměť, takže
     * nemůže přetéct jako buffer u gets(). Délku řádku nemusíme řešit. */
    std::string line;
    if (!std::getline(std::cin, line)) {
        return std::nullopt;   // EOF nebo chyba
    }
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();       // pro jistotu, kdyby vstup přišel s Windows konci řádků
    }
    return line;
}

std::optional<int> read_int(std::string_view prompt)
{
    const auto line = read_line(prompt);
    if (!line) {
        return std::nullopt;
    }

    /* string_view = "okno" do existujícího řetězce (ukazatel + délka).
     * Oříznutí mezer tak nic nekopíruje, jen posune začátek a konec okna. */
    std::string_view text = *line;
    while (!text.empty() && text.front() == ' ') {
        text.remove_prefix(1);
    }
    while (!text.empty() && text.back() == ' ') {
        text.remove_suffix(1);
    }
    if (text.empty()) {
        return std::nullopt;
    }

    /* std::from_chars (C++17) je obdoba strtol, ale nealokuje, nehází výjimky,
     * nezávisí na locale a chybu vrací v návratové hodnotě, ne v globálním errno. */
    int value = 0;
    const char* const first = text.data();
    const char* const last = first + text.size();
    const auto [end, error] = std::from_chars(first, last, value);

    if (error != std::errc{} || end != last) {
        return std::nullopt;   // není číslo, přetečení, nebo smetí za číslem
    }
    return value;
}

void wait_for_enter()
{
    read_line("\n[Enter] pro návrat do menu...");
}
