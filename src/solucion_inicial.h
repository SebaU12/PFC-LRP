// =============================================================================
// solucion_inicial.h — Construcción de la solución de arranque para los algoritmos
// =============================================================================
//
// Antes de que cualquier metaheurística pueda mejorar una solución, necesita
// una solución de partida. Este archivo provee esa solución inicial.
//
// Estrategia usada: "All-Open + Greedy Insertion"
//
//   Paso 1 — All-Open: se abren TODOS los depósitos candidatos.
//             Así no hay que decidir cuáles abrir al principio — el algoritmo
//             se encarga de cerrar los innecesarios durante la optimización.
//
//   Paso 2 — Greedy Insertion: se asigna cada cliente al lugar que genere
//             el menor costo marginal de inserción. Para cada cliente sin
//             asignar se evalúa insertar en cada posición de cada ruta de cada
//             depósito, y se elige la combinación más barata.
//             Si ninguna ruta existente puede aceptar al cliente (por capacidad Q),
//             se abre una ruta nueva desde el depósito más conveniente.
//
// Esta solución no es óptima, pero es factible y sirve como punto de partida
// razonable para ALNS o MA|PM.
//
// =============================================================================
#pragma once
#include "estado.h"

// Función auxiliar: calcula la demanda total de una ruta.
// Está expuesta aquí porque los operadores del ALNS también la necesitan.
double demanda_ruta_externa(const Ruta &r, const InstanciaLRP &datos);

// Genera la solución inicial con la estrategia All-Open + Greedy Insertion.
EstadoLRP generar_solucion_inicial(const InstanciaLRP &inst, const Matriz &m);
