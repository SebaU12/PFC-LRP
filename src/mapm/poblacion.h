#pragma once
#include "../distancia.h"
#include "../parser.h"
#include "cromosoma.h"
#include "split.h"
#include <functional>
#include <random>
#include <vector>

// ─────────────────────────────────────────
// Tipo de función de mejora local
// Se usa para conectar LS1/LS2 cuando estén disponibles (Fase 4).
// Por defecto se pasa una función vacía (no-op).
// ─────────────────────────────────────────
using FnLocalSearch =
    std::function<void(CromosomaLRP &, const Matriz &, const InstanciaLRP &)>;

// ─────────────────────────────────────────
// Heurística 1 — Nearest Neighbor (NN)
//
// Fiel al paper: "randomized constructive heuristic based on the
// Nearest Neighbor Algorithm".
//
// Algoritmo:
//   1. Abrir todos los depósitos (igual que sol_inicial del ALNS).
//   2. Construir CS asignando cada cliente al depósito más cercano
//      con capacidad disponible (con perturbación aleatoria
//      controlada por alpha_nn para diversificar la población).
//   3. Dentro de cada depósito, ordenar clientes por vecino más
//      cercano partiendo de un cliente aleatorio.
//   4. Cerrar depósitos vacíos y compactar DS.
//   5. Evaluar con Split.
// ─────────────────────────────────────────
CromosomaLRP heuristica_nn(const InstanciaLRP &inst, const Matriz &mat,
                           std::mt19937 &rng, double alpha_nn = 0.3);

// ─────────────────────────────────────────
// Heurística 2 — Clarke & Wright extendido (ECWA)
//
// Fiel al paper: "greedy randomized heuristic based on an extended
// Clarke and Wright algorithm (ECWA) for the LRP".
//
// Algoritmo:
//   1. Calcular savings s(i,j) = dist(dep,i) + dist(dep,j) - dist(i,j)
//      para cada par de clientes (i,j) y cada depósito candidato.
//      El saving se extiende al LRP incluyendo el costo de apertura
//      del depósito: savings efectivo considera también si fusionar
//      rutas permite cerrar un depósito.
//   2. Ordenar savings de mayor a menor.
//   3. Con perturbación aleatoria (alpha_cw), fusionar pares en rutas
//      respetando Q y capacidad de depósito.
//   4. Construir DS y CS a partir de las rutas resultantes.
//   5. Evaluar con Split.
// ─────────────────────────────────────────
CromosomaLRP heuristica_cw(const InstanciaLRP &inst, const Matriz &mat,
                           std::mt19937 &rng, double alpha_cw = 0.3);

// ─────────────────────────────────────────
// generar_poblacion
//
// Fiel al paper, Sección 4.1:
//   "The algorithm begins by creating a population of NbIndiv good
//    solutions using two different randomized heuristics. The first
//    half using NN + LS1, the second half using ECWA + LS1.
//    All initial solutions are distinct (Δ=1)."
//
// ls: función de mejora local a aplicar tras cada construcción.
//     Se pasa una lambda no-op hasta que LS1 esté implementado.
//
// Garantiza que todos los cromosomas de la población son distintos
// usando la medida de distancia D(T,U) >= 1.
// ─────────────────────────────────────────
std::vector<CromosomaLRP> generar_poblacion(
    int NbIndiv, const InstanciaLRP &inst, const Matriz &mat, std::mt19937 &rng,
    const FnLocalSearch &ls = [](CromosomaLRP &, const Matriz &,
                                 const InstanciaLRP &) {});

// ─────────────────────────────────────────
// refrescar_poblacion  (GenPop2 del paper)
//
// "Every individual except the best (having the best fitness)
//  is replaced in the population."
//
// Se llama cuando NbNoAdd > MaxNbNoAdd iteraciones consecutivas
// sin que ningún offspring entre a la población.
// ─────────────────────────────────────────
void refrescar_poblacion(
    std::vector<CromosomaLRP> &pop, const InstanciaLRP &inst, const Matriz &mat,
    std::mt19937 &rng,
    const FnLocalSearch &ls = [](CromosomaLRP &, const Matriz &,
                                 const InstanciaLRP &) {});
