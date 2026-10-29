/*
 * queue.hpp – fronta (FIFO) napsaná od nuly jako kruhový buffer.
 *
 * FIFO = First In, First Out: kdo dřív přijde, ten dřív mele.
 * Přidává se na KONEC (push), bere se z ČELA (pop). Jako fronta u pokladny.
 *
 * Jak to funguje uvnitř:
 * Prvky leží v jednom souvislém poli "slotů". Index head_ ukazuje na čelo
 * fronty a další volný slot (tail) se dopočítá jako head_ + size_. Když
 * index dojde na konec pole, pokračuje od nuly (proto "kruhový"). Díky tomu
 * pop() nemusí nic posouvat, jen pohne head_ o jedna dopředu. Je to O(1).
 *
 *     kapacita 4, ve frontě je Bára, Cyril, David (Bára je první):
 *
 *       [0] David   [1] ·       [2] Bára    [3] Cyril
 *                   ^ tail      ^ head
 *
 * Když je pole plné, push() alokuje dvakrát větší a prvky do něj přesune
 * (std::move) srovnané od indexu 0. Stejně to dělá std::vector.
 *
 * C vs. C++: v C by fronta byla struct s void* polem a element_size, nebo
 * spojový seznam, kde každý uzel je jeden malloc. Tady je to šablona:
 * Queue<std::string> i Queue<int> si vygeneruje překladač a hlídá typy.
 * Paměť uvolní destruktor std::vector, takže žádné free() ani delete.
 *
 * Ve standardní knihovně je hotová std::queue (z <queue>). V praxi použij tu,
 * tady ji píšeme sami, abychom viděli, co dělá uvnitř.
 */
#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

template <class T>
class Queue {
public:
    /* explicit = zakáže nechtěný převod typu  Queue<int> q = 8; */
    explicit Queue(std::size_t initial_capacity = 4)
        : slots_(initial_capacity == 0 ? 1 : initial_capacity)
    {
    }

    /* Přidá prvek na konec fronty (anglicky enqueue). Amortizovaně O(1). */
    void push(T value)
    {
        if (size_ == slots_.size()) {
            grow();
        }
        slots_[tail()].emplace(std::move(value));   // prázdný slot se "zaplní"
        ++size_;
    }

    /* Vezme prvek z čela fronty (anglicky dequeue). O(1).
     * Prázdná fronta vrátí std::nullopt. std::queue::pop() by tady bylo
     * nedefinované chování, my to hlídáme. */
    std::optional<T> pop()
    {
        if (size_ == 0) {
            return std::nullopt;
        }
        std::optional<T> value = std::move(slots_[head_]);
        slots_[head_].reset();                       // slot je zase prázdný
        head_ = (head_ + 1) % slots_.size();         // čelo se posune, nic se nekopíruje
        --size_;
        return value;
    }

    /* Kdo je na řadě, bez odebrání. Na prázdné frontě vyhodí výjimku. */
    const T& front() const
    {
        if (size_ == 0) {
            throw std::out_of_range{"fronta je prázdná"};
        }
        return *slots_[head_];
    }

    /* i-tý prvek v pořadí fronty (0 = čelo), ne i-tý slot v poli. */
    const T& at(std::size_t i) const
    {
        if (i >= size_) {
            throw std::out_of_range{"index mimo frontu"};
        }
        return *slots_[index_of(i)];
    }

    /* Vyprázdní frontu. Kapacita (alokované pole) zůstává. */
    void clear()
    {
        for (auto& slot : slots_) {
            slot.reset();
        }
        head_ = 0;
        size_ = 0;
    }

    std::size_t size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }
    std::size_t capacity() const noexcept { return slots_.size(); }

    /* Pohled "pod kapotu", aby šlo vypsat, jak fronta leží v paměti. */
    std::size_t head() const noexcept { return head_; }
    std::size_t tail() const noexcept { return index_of(size_); }
    std::span<const std::optional<T>> slots() const noexcept { return slots_; }

private:
    /* Převod pořadí ve frontě na index v poli. Modulo dělá to "dokola". */
    std::size_t index_of(std::size_t i) const noexcept
    {
        return (head_ + i) % slots_.size();
    }

    /* Dvakrát větší pole. Prvky se přesunou v pořadí fronty na začátek,
     * takže v novém poli už nejsou "přetočené" přes konec. */
    void grow()
    {
        std::vector<std::optional<T>> bigger(slots_.size() * 2);
        for (std::size_t i = 0; i < size_; ++i) {
            bigger[i] = std::move(slots_[index_of(i)]);
        }
        slots_ = std::move(bigger);   // staré pole se tady uvolní samo
        head_ = 0;
    }

    /* std::optional<T> = "prvek, nebo nic". Prázdné sloty tak opravdu nic
     * neobsahují a odebraný std::string se uvolní hned při pop(). */
    std::vector<std::optional<T>> slots_;
    std::size_t head_ = 0;   // index čela
    std::size_t size_ = 0;   // počet prvků ve frontě
};
