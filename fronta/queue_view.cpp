/*
 * queue_view.cpp – implementace výpisů z queue_view.hpp
 */
#include "queue_view.hpp"
#include "util.hpp"

#include <optional>
#include <string_view>

void print_fifo(const Queue<std::string>& queue)
{
    if (queue.empty()) {
        print("  Fronta je prázdná.\n");
        return;
    }

    print("  čelo -> ");
    for (std::size_t i = 0; i < queue.size(); ++i) {
        print("{}{}", i == 0 ? "" : " -> ", queue.at(i));
    }
    print(" <- konec   (počet: {})\n", queue.size());
}

void print_ring(const Queue<std::string>& queue)
{
    print("\n  Kruhový buffer: kapacita {}, obsazeno {}, sizeof(slot) = {} B\n",
          queue.capacity(), queue.size(), sizeof(std::optional<std::string>));

    const auto slots = queue.slots();
    for (std::size_t i = 0; i < slots.size(); ++i) {
        /* Prázdný slot je std::nullopt, vypíšeme místo něj tečku. */
        const std::string_view text = slots[i] ? std::string_view{*slots[i]} : "·";

        std::string_view mark;
        if (!queue.empty() && i == queue.head()) {
            mark = "<- head (odtud se bere)";
        } else if (queue.size() < queue.capacity() && i == queue.tail()) {
            mark = "<- tail (sem přijde další)";
        }

        /* Adresy rostou o sizeof(slot): sloty leží v paměti těsně za sebou. */
        print("    [{}]  {}  {:<12} {}\n", i, static_cast<const void*>(&slots[i]), text, mark);
    }

    if (queue.size() == queue.capacity()) {
        print("  Buffer je plný. Další push() alokuje dvakrát větší pole.\n");
    }
}
