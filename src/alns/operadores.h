// =============================================================================
// operadores.h — Catálogo de operadores de destrucción y reparación del ALNS
// =============================================================================
//
// El ALNS trabaja en dos fases por iteración:
//
//   DESTRUCCIÓN: toma la solución actual y le quita algunos clientes.
//                Los clientes removidos van a la lista no_asignados.
//                El objetivo es "romper" una región de la solución para
//                poder reconstruirla de una forma diferente (y mejor).
//
//   REPARACIÓN:  toma la solución dañada (con no_asignados) y reinserta
//                todos los clientes pendientes de la manera más conveniente.
//
// Convención de todos los operadores:
//   - Reciben el estado POR VALOR (trabajan sobre una copia, no el original)
//   - Reciben el RNG por referencia (para que la aleatoriedad sea reproducible)
//   - Devuelven el estado modificado
//
// Esto permite encadenarlos limpiamente:
//   EstadoLRP s_nuevo = reparador(destructor(s.copy(), rng), rng);
//
// =============================================================================
#pragma once
#include "../estado.h"
#include <random>

// ── OPERADORES DE DESTRUCCIÓN ─────────────────────────────────────────────────
// Cada uno quita ~20% de los clientes pero con diferente criterio de selección.

// Quita clientes al azar. Introduce ruido puro — bueno para escapar de
// óptimos locales cuando los demás operadores se quedan estancados.
EstadoLRP random_removal(EstadoLRP estado, std::mt19937 &rng);

// Quita los clientes que más "cuestan" en su posición actual (mayor ahorro
// si se remueven). Ataca las ineficiencias más evidentes de la solución.
EstadoLRP worst_removal(EstadoLRP estado, std::mt19937 &rng);

// Quita un cluster de clientes geográficamente cercanos entre sí (Shaw 1998).
// La idea: clientes vecinos pueden reorganizarse juntos para mejorar las rutas.
EstadoLRP shaw_removal(EstadoLRP estado, std::mt19937 &rng);

// Elimina rutas completas (no clientes individuales). Presiona al algoritmo
// a reducir el número total de vehículos usados, ahorrando costos fijos F.
EstadoLRP route_removal(EstadoLRP estado, std::mt19937 &rng);

// Cierra un depósito entero y saca a todos sus clientes. Obliga a explorar
// configuraciones con menos depósitos abiertos, ahorrando costos O_i.
EstadoLRP depot_closing(EstadoLRP estado, std::mt19937 &rng);

// ── OPERADORES DE REPARACIÓN ──────────────────────────────────────────────────
// Reinsertan todos los clientes de no_asignados en alguna ruta.

// Greedy: para cada cliente pendiente, encuentra la inserción de menor costo
// marginal y la aplica inmediatamente. Rápido pero puede tomar decisiones
// localmente buenas que resultan malas globalmente.
EstadoLRP greedy_repair(EstadoLRP estado, std::mt19937 &rng);

// Regret-2: antes de insertar, mira cuánto "se arrepentiría" si no lo pusiera
// en su mejor posición (diferencia entre la 1ra y 2da mejor opción).
// Prioriza a los clientes más "difíciles" de colocar — tiende a dar mejores
// soluciones que greedy a cambio de ser más lento.
EstadoLRP regret2_repair(EstadoLRP estado, std::mt19937 &rng);
