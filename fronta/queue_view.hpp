/*
 * queue_view.hpp – výpis fronty do konzole.
 *
 * Výpis je schválně oddělený od samotné fronty: Queue jen ukládá data
 * a nic netiskne, o vzhled se starají tyhle funkce.
 */
#pragma once

#include <string>

#include "queue.hpp"

/* Fronta v pořadí, jak se bude obsluhovat:  čelo -> Anna -> Bára -> konec */
void print_fifo(const Queue<std::string>& queue);

/* Vnitřek kruhového bufferu: všechny sloty, jejich adresy a kde je head a tail. */
void print_ring(const Queue<std::string>& queue);
