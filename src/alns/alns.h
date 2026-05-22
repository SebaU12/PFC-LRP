#pragma once
#include "../algoritmo.h"
#include "operadores.h"
#include <functional>
#include <random>
#include <vector>

// ─────────────────────────────────────────
// Parámetros configurables del ALNS
// ─────────────────────────────────────────
struct ConfigALNS {
  // ── Simulated Annealing ───────────────
  double temperatura_inicial; // T0
  double temperatura_final;   // T_end
  double factor_enfriamiento; // α  ∈ (0,1)

  // ── Ruleta Adaptativa ─────────────────
  double factor_reaccion; // ρ  ∈ [0,1]
  int segmento;           // T_seg: iters entre actualizaciones de pesos

  // ── Puntajes de recompensa ────────────
  double sigma1; // nuevo óptimo global
  double sigma2; // mejora la solución actual
  double sigma3; // aceptada por SA (no mejora)
  double sigma4; // rechazada

  // ── Criterio de parada ────────────────
  int max_iteraciones;

  // ── Semilla RNG ───────────────────────
  unsigned int semilla;
};

// ─────────────────────────────────────────
// Tipo de operador: recibe estado + rng,
// devuelve estado modificado
// ─────────────────────────────────────────
using OperadorFn = std::function<EstadoLRP(EstadoLRP, std::mt19937 &)>;

// ─────────────────────────────────────────
// Función interna del loop ALNS
// Usada por AlgoritmALNS::ejecutar()
// ─────────────────────────────────────────
ResultadoAlgoritmo ejecutar_alns(const EstadoLRP &sol_inicial,
                                 const std::vector<OperadorFn> &destroyers,
                                 const std::vector<OperadorFn> &repairers,
                                 const ConfigALNS &cfg);
