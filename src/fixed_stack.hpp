/*
 * fixed_stack.hpp – generický zásobník s pevnou kapacitou (šablona).
 *
 * Všechna data jsou PŘÍMO v objektu, žádná halda. FixedStack<int, 8>
 * jako lokální proměnná = 8 intů na zásobníku.
 *
 * C vs. C++: v C by generický kontejner vypadal nějak takto:
 *     typedef struct { void *data; size_t element_size, size, capacity; } Stack;
 *     stack_push(&s, &value);            // void*: překladač nehlídá typ
 *     int x = *(int *)stack_pop(&s);     // přetypování, a když se spleteš, je to UB
 * Šablona vygeneruje kód pro každý typ zvlášť. FixedStack<int, 8>
 * a FixedStack<std::string, 3> jsou dvě různé třídy. Typy hlídá překladač,
 * velikost prvku zná při překladu (žádné element_size za běhu) a prvky
 * se správně konstruují i ruší (std::string by přes memcpy kopírovat nešel).
 */
#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <utility>

template <class T, std::size_t Capacity>
class FixedStack {
public:
    /* constexpr = jde použít i při překladu (viz static_assert v arena.cpp). */
    constexpr bool push(T value)
    {
        if (size_ == Capacity) {
            return false;   // plno; výjimka by tu taky šla, ale zásobník je "levný"
        }
        items_[size_++] = std::move(value);
        return true;
    }

    /* std::optional místo "vrať -1, když je prázdný" nebo bool + výstupní parametr. */
    constexpr std::optional<T> pop()
    {
        if (size_ == 0) {
            return std::nullopt;
        }
        return std::move(items_[--size_]);
    }

    constexpr std::size_t size() const noexcept { return size_; }
    constexpr bool empty() const noexcept { return size_ == 0; }
    static constexpr std::size_t capacity() noexcept { return Capacity; }

    /* begin/end -> jde použít v range-for i v algoritmech */
    constexpr const T* begin() const noexcept { return items_.data(); }
    constexpr const T* end() const noexcept { return items_.data() + size_; }

private:
    std::array<T, Capacity> items_{};
    std::size_t size_ = 0;
};
