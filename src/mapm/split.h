// =============================================================================
// split.h — Algoritmo Split: de permutación de clientes a rutas óptimas
// =============================================================================
//
// El cromosoma del MA|PM solo dice "este depósito atiende a estos clientes,
// en este orden". Pero NO dice cómo agruparlos en rutas de vehículo.
// El algoritmo Split resuelve esa pregunta de forma óptima.
//
// IDEA CENTRAL (Prins et al. 2006):
//   Dado un depósito y una secuencia de p clientes [c1, c2, ..., cp],
//   encontrar la forma de cortarla en sub-secuencias (rutas) tal que:
//     - Cada ruta no supere la capacidad Q del vehículo.
//     - La suma de (F + distancias de viaje) de todas las rutas sea mínima.
//
//   Esto es equivalente a encontrar el camino más corto en un grafo auxiliar:
//     - Nodos: 0, 1, 2, ..., p  (el nodo i representa "hemos ruteado los primeros i clientes")
//     - Arco (i→j): costo de hacer una ruta con los clientes [ci+1, ..., cj]
//                   = F + dist(depósito, ci+1) + dist(ci+1, ci+2) + ... + dist(cj, depósito)
//                   (solo existe si la demanda total del segmento ≤ Q)
//
//   El camino más corto de 0 a p = la partición óptima en rutas.
//   Se resuelve con Bellman-Ford (o equivalentemente, programación dinámica), O(p²).
//
// =============================================================================
#pragma once
#include "../distancia.h"
#include "../parser.h"
#include "cromosoma.h"
#include <vector>

// Secuencia plana de IDs de clientes (solo para uso interno del módulo MAPM).
// Se llama RutaVec para distinguirla del struct Ruta de estado.h.
using RutaVec = std::vector<int>;

// Lo que devuelve el Split de un depósito: las rutas formadas y su costo total.
struct SplitResultado {
  std::vector<RutaVec> rutas; // cada RutaVec es una ruta: [c1, c2, ..., cp]
  double costo;               // F*num_rutas + sum(distancias de viaje)
  bool factible;              // false solo si algún cliente tiene demanda > Q (caso degenerado)
};

// -----------------------------------------------------------------------------
// Aplica el algoritmo Split a una lista de clientes para un depósito específico.
// Devuelve las rutas óptimas y su costo.
//
// Parámetros:
//   clientes  — sublista de IDs de clientes asignados a este depósito (1-indexed)
//   dep_idx   — índice 0-based del depósito en la matriz (= nc + i)
// -----------------------------------------------------------------------------
SplitResultado split_deposito(const std::vector<int> &clientes, int dep_idx,
                              const Matriz &mat, const InstanciaLRP &inst);

// -----------------------------------------------------------------------------
// Evalúa el cromosoma completo: aplica Split a cada depósito abierto,
// suma costos de apertura y routing, y penaliza exceso de capacidad de depósito.
//
// Modifica crom.fitness con el costo total calculado.
// Devuelve un vector de SplitResultado indexado por dep_idx (0-based), útil
// para la búsqueda local que necesita saber las rutas de cada depósito.
//
// alpha: factor de penalización por unidad de demanda excedida en un depósito.
// -----------------------------------------------------------------------------
std::vector<SplitResultado> evaluar_cromosoma(CromosomaLRP &crom,
                                              const Matriz &mat,
                                              const InstanciaLRP &inst,
                                              double alpha = 1000.0);
