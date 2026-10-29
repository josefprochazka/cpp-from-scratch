/*
 * tasks.hpp – úkolníček: std::vector úkolů + ukládání do binárního souboru.
 *
 * C vs. C++: C verze měla spojový seznam, kde každý uzel i každý titulek byl
 * samostatný malloc a mazání vyžadovalo 2x free ve správném pořadí.
 * Tady je std::vector<Task> a uvnitř std::string. Nikde žádné free/delete.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

/* Příznaky jako bity, stejně jako v C verzi. inline constexpr (C++17)
 * = konstanta v hlavičce bez porušení "one definition rule". */
namespace TaskFlags {
inline constexpr std::uint8_t Done   = 1u << 0;
inline constexpr std::uint8_t Urgent = 1u << 1;
}  // namespace TaskFlags

struct Task {
    int          id = 0;
    std::uint8_t flags = 0;
    std::string  title;     // C verze: char *title + vlastní malloc/free

    bool has(std::uint8_t flag) const noexcept { return (flags & flag) != 0; }
    void toggle(std::uint8_t flag) noexcept { flags = static_cast<std::uint8_t>(flags ^ flag); }
};

/* Vlastní typ výjimky pro poškozený soubor. Dědí ze std::runtime_error,
 * takže ji chytí i obecné catch (const std::exception&). */
class FileFormatError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;   // převezmi konstruktory předka
};

class TaskList {
public:
    static constexpr std::size_t MaxTitleLength = 200;

    /* Vyhodí std::length_error, když je název moc dlouhý. */
    Task& add(std::string title, std::uint8_t flags = 0);

    /* Vrací ukazatel, nebo nullptr. Proč ne std::optional<Task&>? Optional
     * na referenci v C++20 neexistuje (přijde až v C++26), a obyčejný
     * ukazatel je přesně "odkaz, nebo nic".
     * POZOR: po add() může vector realokovat a ukazatel přestane platit! */
    Task* find(int id) noexcept;

    /* Smazaný úkol vrátí (přesunutý ven z vectoru), nebo std::nullopt. */
    std::optional<Task> remove(int id);

    void show() const;
    std::size_t size() const noexcept { return tasks_.size(); }

    /* Při chybě vyhodí výjimku (std::runtime_error, FileFormatError, std::ios_base::failure). */
    void save(const std::filesystem::path& path) const;
    static TaskList load(const std::filesystem::path& path);

private:
    Task& add_with_id(int id, std::string title, std::uint8_t flags);

    std::vector<Task> tasks_;
    int next_id_ = 1;
};

/* Demo 6 – interaktivní úkolníček */
void demo_tasks();
