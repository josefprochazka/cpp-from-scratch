/*
 * tasks.cpp – seznam úkolů a jeho uložení do binárního souboru.
 */
#include "tasks.hpp"
#include "util.hpp"

#include <algorithm>
#include <array>
#include <concepts>
#include <format>
#include <fstream>
#include <ios>
#include <string_view>
#include <utility>

namespace {

constexpr std::string_view TasksFile = "tasks.dat";
constexpr std::string_view FileMagic = "TASK";   // "magické" bajty na začátku souboru
constexpr std::uint32_t FileVersion = 1;

/* ---------------------------------------------------------------------------
 * Binární soubor, formát je STEJNÝ jako v c-from-scratch (vše little endian):
 *   "TASK"        4 B   magic
 *   version       4 B
 *   count         4 B
 *   pro každý úkol:
 *     id          4 B
 *     flags       1 B
 *     title_len   2 B
 *     title       title_len B (bez '\0')
 *
 * Soubor tasks.dat z C verze tedy jde načíst tady a naopak.
 *
 * Proč nezapsat rovnou out.write((char*)&task, sizeof(Task))? Ze stejného
 * důvodu jako v C: padding, ukazatele uvnitř (std::string drží ukazatel na
 * haldu!) a pořadí bajtů. Proto zapisujeme pole po bajtech, přesně podle formátu.
 * ------------------------------------------------------------------------- */

/* C verze měla write_u16 a write_u32. Šablona pokryje všechny velikosti. */
template <std::unsigned_integral T>
void write_le(std::ostream& out, T value)
{
    std::array<char, sizeof(T)> bytes{};
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        bytes[i] = static_cast<char>((value >> (8 * i)) & 0xFFu);
    }
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

template <std::unsigned_integral T>
T read_le(std::istream& in)
{
    std::array<char, sizeof(T)> bytes{};
    if (!in.read(bytes.data(), static_cast<std::streamsize>(bytes.size()))) {
        throw FileFormatError{"soubor je zkrácený"};
    }
    T value = 0;
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        const auto part = static_cast<T>(static_cast<unsigned char>(bytes[i]));
        value = static_cast<T>(value | (part << (8 * i)));
    }
    return value;
}

}  // namespace

Task& TaskList::add(std::string title, std::uint8_t flags)
{
    return add_with_id(next_id_, std::move(title), flags);
}

Task& TaskList::add_with_id(int id, std::string title, std::uint8_t flags)
{
    if (title.size() > MaxTitleLength) {
        throw std::length_error{std::format("název má {} B, maximum je {} B", title.size(), MaxTitleLength)};
    }

    /* C verze: malloc uzlu, malloc titulku, a když druhý selže, uklidit první.
     * Tady: když dojde paměť, vyletí std::bad_alloc a vector zůstane, jak byl.
     * Titulek se do vectoru PŘESUNE, takže se znaky nekopírují. */
    Task& task = tasks_.emplace_back(Task{id, flags, std::move(title)});

    if (id >= next_id_) {
        next_id_ = id + 1;
    }
    return task;
}

Task* TaskList::find(int id) noexcept
{
    /* std::ranges::find s "projekcí": hledá prvek, jehož člen id == id.
     * V C to byla ruční smyčka přes t->next. */
    const auto it = std::ranges::find(tasks_, id, &Task::id);
    return it != tasks_.end() ? &*it : nullptr;
}

std::optional<Task> TaskList::remove(int id)
{
    const auto it = std::ranges::find(tasks_, id, &Task::id);
    if (it == tasks_.end()) {
        return std::nullopt;
    }
    Task removed = std::move(*it);
    tasks_.erase(it);   // prvky za ním se posunou (přesunou) o jedno dopředu
    return removed;
}

void TaskList::show() const
{
    if (tasks_.empty()) {
        print("  (žádné úkoly)\n");
        return;
    }

    print("  ID  hotovo  urgent  název                      adresa v paměti\n");
    print("  --  ------  ------  -------------------------  ------------------\n");
    for (const Task& t : tasks_) {
        print("  {:>2}   [{}]      {}    {:<25}  {}\n",
              t.id,
              t.has(TaskFlags::Done) ? 'x' : ' ',
              t.has(TaskFlags::Urgent) ? '!' : ' ',
              t.title,
              static_cast<const void*>(&t));
    }
    print("\n  Úkoly leží ve vectoru TĚSNĚ ZA SEBOU (adresy rostou o sizeof(Task) = {} B),\n"
          "  na rozdíl od uzlů spojového seznamu v C verzi. Procesorová cache to má ráda.\n",
          sizeof(Task));
}

void TaskList::save(const std::filesystem::path& path) const
{
    /* std::ios::binary = žádné převody \n na \r\n (v C to bylo "wb"). */
    std::ofstream out{path, std::ios::binary};
    if (!out) {
        throw std::runtime_error{std::format("soubor {} nejde otevřít pro zápis", path.string())};
    }

    /* Od teď každá chyba zápisu vyhodí výjimku. Nemusíme kontrolovat
     * návratovou hodnotu každého zápisu jako u fwrite. */
    out.exceptions(std::ios::failbit | std::ios::badbit);

    out.write(FileMagic.data(), static_cast<std::streamsize>(FileMagic.size()));
    write_le(out, FileVersion);
    write_le(out, static_cast<std::uint32_t>(tasks_.size()));

    for (const Task& t : tasks_) {
        write_le(out, static_cast<std::uint32_t>(t.id));
        write_le(out, t.flags);
        write_le(out, static_cast<std::uint16_t>(t.title.size()));   // add() hlídá max. 200 B
        out.write(t.title.data(), static_cast<std::streamsize>(t.title.size()));
    }

    /* close() zapíše zbytek bufferu na disk. Kdyby to selhalo (plný disk),
     * díky exceptions() vyletí výjimka. Jinak by soubor zavřel destruktor
     * (RAII), ale chybu by tiše spolkl. */
    out.close();
}

TaskList TaskList::load(const std::filesystem::path& path)
{
    std::ifstream in{path, std::ios::binary};
    if (!in) {
        throw std::runtime_error{std::format("soubor {} neexistuje nebo nejde otevřít", path.string())};
    }

    std::array<char, 4> magic{};
    in.read(magic.data(), static_cast<std::streamsize>(magic.size()));
    if (!in || std::string_view{magic.data(), magic.size()} != FileMagic) {
        throw FileFormatError{"soubor nezačíná hlavičkou TASK"};
    }

    const auto version = read_le<std::uint32_t>(in);
    if (version != FileVersion) {
        throw FileFormatError{std::format("nepodporovaná verze souboru {}", version)};
    }
    const auto count = read_le<std::uint32_t>(in);

    /* Načítáme do NOVÉHO seznamu. Když cokoliv selže výjimkou, nový seznam
     * se zahodí (destruktor uvolní, co se stihlo načíst) a seznam volajícího
     * zůstane netknutý. C verze musela při chybě ručně uklízet. */
    TaskList list;
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto id = read_le<std::uint32_t>(in);
        const auto flags = read_le<std::uint8_t>(in);
        const auto length = read_le<std::uint16_t>(in);

        if (static_cast<std::size_t>(length) > MaxTitleLength) {   // nevěř datům ze souboru!
            throw FileFormatError{std::format("úkol {} má podezřele dlouhý název ({} B)", id, length)};
        }

        std::string title(length, '\0');
        if (!in.read(title.data(), length)) {
            throw FileFormatError{"soubor je zkrácený uprostřed názvu"};
        }
        list.add_with_id(static_cast<int>(id), std::move(title), flags);
    }
    return list;   // vrácení z funkce = přesun (nebo rovnou copy elision), žádná kopie
}

/* --------------------------------------------------------------------------- */

void demo_tasks()
{
    print_header("6) Úkolníček: std::vector, výjimky a binární soubor");

    TaskList list;
    try {
        list = TaskList::load(TasksFile);
        print("Načteno {} úkolů ze souboru {}.\n", list.size(), TasksFile);
    } catch (const std::exception& e) {
        print("Soubor {} se nepodařilo načíst ({}).\nZačínáme s ukázkovými úkoly.\n", TasksFile, e.what());
        list.add("Naučit se RAII", TaskFlags::Done);
        list.add("Přepsat arénu na šablonu", TaskFlags::Urgent);
        list.add("Hodit projekt na GitHub");
    }
    print("Každý úkol: sizeof(Task) = {} B (titulek do 15 znaků je přímo v něm díky SSO).\n\n",
          sizeof(Task));
    list.show();

    for (;;) {
        print("\n  1 přidat  2 vypsat  3 hotovo  4 urgentní  5 smazat  6 uložit  7 načíst  0 zpět\n");
        const auto choice = read_int("  > ");
        if (!choice) {
            if (std::cin.eof()) {
                break;
            }
            continue;
        }
        if (*choice == 0) {
            break;
        }

        /* Všechny chyby z jedné volby se ošetří na jednom místě. */
        try {
            switch (*choice) {
            case 1: {
                const auto title = read_line("  Název: ");
                if (title && !title->empty()) {
                    const Task& task = list.add(*title);
                    print("  Přidán úkol {} na adrese {}.\n", task.id, static_cast<const void*>(&task));
                }
                break;
            }
            case 2:
                list.show();
                break;
            case 3:
            case 4: {
                const auto id = read_int("  ID úkolu: ");
                Task* task = id ? list.find(*id) : nullptr;
                if (task == nullptr) {
                    print("  Úkol nenalezen.\n");
                    break;
                }
                task->toggle(*choice == 3 ? TaskFlags::Done : TaskFlags::Urgent);   // XOR přepne bit
                print("  Příznaky úkolu {}: {:#04x}\n", task->id, task->flags);
                break;
            }
            case 5: {
                const auto id = read_int("  ID úkolu: ");
                std::optional<Task> removed;
                if (id) {
                    removed = list.remove(*id);
                }
                if (removed) {
                    print("  Smazán úkol \"{}\". Žádné free(), std::string se uvolní sám.\n", removed->title);
                } else {
                    print("  Úkol nenalezen.\n");
                }
                break;
            }
            case 6:
                list.save(TasksFile);
                print("  Uloženo do {} ({} B).\n", TasksFile, std::filesystem::file_size(TasksFile));
                break;
            case 7:
                /* Když load selže, vyletí výjimka a přiřazení se vůbec neprovede,
                 * takže stávající seznam zůstane beze změny. */
                list = TaskList::load(TasksFile);
                print("  Načteno {} úkolů.\n", list.size());
                list.show();
                break;
            default:
                print("  Neznámá volba.\n");
                break;
            }
        } catch (const FileFormatError& e) {
            print("  Poškozený soubor: {}\n", e.what());
        } catch (const std::exception& e) {
            print("  Chyba: {}\n", e.what());
        }
    }

    print("  Úklid: nic. Destruktor vectoru uvolní všech {} úkolů sám.\n", list.size());
}
