// =============================================================================
// alns.h — Motor del ALNS: configuración, tipos y punto de entrada
// =============================================================================
//
// El ALNS (Adaptive Large Neighborhood Search) es la metaheurística principal.
// En cada iteración:
//   1. Elige un destructor (con una ruleta probabilística ponderada)
//   2. Elige un reparador (misma ruleta)
//   3. Aplica destructor → reparador sobre la solución actual → s_nueva
//   4. Decide si aceptar s_nueva (Simulated Annealing)
//   5. Actualiza los pesos de la ruleta según cómo se desempeñó la combinación
//
// Los pesos de la ruleta se actualizan cada `segmento` iteraciones, usando
// los puntajes acumulados (σ1..σ4). Así, los operadores que funcionan bien
// en el problema actual se usan más, y los que no funcionan se usan menos.
//
// =============================================================================
#pragma once
#include "../algoritmo.h"
#include "operadores.h"
#include <functional>
#include <random>
#include <vector>

// -----------------------------------------------------------------------------
// Todos los parámetros que controlan el comportamiento del ALNS.
// Se pasan como un struct para no tener funciones con 10 parámetros.
// -----------------------------------------------------------------------------
struct ConfigALNS {
  // ── Simulated Annealing ────────────────────────────────────────────────────
  double temperatura_inicial; // T0 — temperatura de partida (acepta más soluciones peores)
  double temperatura_final;   // T_end — temperatura mínima (solo acepta mejoras)
  double factor_enfriamiento; // α ∈ (0,1) — cuánto se enfría por iteración: T *= α

  // ── Ruleta adaptativa ──────────────────────────────────────────────────────
  double factor_reaccion;     // ρ ∈ [0,1] — qué tanto aprende de cada segmento
                              //   ρ=0 → ignora resultados (pesos fijos)
                              //   ρ=1 → descarta todo el historial anterior
  int segmento;               // cada cuántas iteraciones se actualizan los pesos

  // ── Puntajes de recompensa para la ruleta ─────────────────────────────────
  // Cada vez que una combinación (destructor, reparador) produce un resultado,
  // recibe uno de estos puntajes para actualizar su peso en la ruleta.
  double sigma1;              // la nueva solución es el mejor global encontrado
  double sigma2;              // mejora la solución actual (pero no el global)
  double sigma3;              // aceptada por SA aunque no mejore (exploración)
  double sigma4;              // rechazada completamente

  int max_iteraciones;
  unsigned int semilla;
};

// -----------------------------------------------------------------------------
// Tipo de todos los operadores (destructores y reparadores).
// Reciben una copia del estado y el RNG, devuelven el estado modificado.
// Usar std::function permite registrarlos en vectores y elegirlos dinámicamente.
// -----------------------------------------------------------------------------
using OperadorFn = std::function<EstadoLRP(EstadoLRP, std::mt19937 &)>;

// -----------------------------------------------------------------------------
// Punto de entrada del motor ALNS.
// Recibe la solución inicial y los catálogos de operadores, ejecuta el loop
// completo y devuelve el ResultadoAlgoritmo con la mejor solución encontrada.
// -----------------------------------------------------------------------------
ResultadoAlgoritmo ejecutar_alns(const EstadoLRP &sol_inicial,
                                 const std::vector<OperadorFn> &destroyers,
                                 const std::vector<OperadorFn> &repairers,
                                 const ConfigALNS &cfg);
