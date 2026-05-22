#pragma once
#include "../estado.h"
#include <random>

// Todos los operadores reciben el estado por valor (ya copiado
// por el caller) y un RNG por referencia.
// Retornan el estado modificado.

// ── Destrucción ───────────────────────────────────────────────
EstadoLRP random_removal(EstadoLRP estado, std::mt19937 &rng);
EstadoLRP worst_removal(EstadoLRP estado, std::mt19937 &rng);
EstadoLRP shaw_removal(EstadoLRP estado, std::mt19937 &rng);
EstadoLRP route_removal(EstadoLRP estado, std::mt19937 &rng);
EstadoLRP depot_closing(EstadoLRP estado, std::mt19937 &rng);

// ── Reparación ────────────────────────────────────────────────
EstadoLRP greedy_repair(EstadoLRP estado, std::mt19937 &rng);
EstadoLRP regret2_repair(EstadoLRP estado, std::mt19937 &rng);
