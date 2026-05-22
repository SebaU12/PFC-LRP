#pragma once
#include "../distancia.h"
#include "../parser.h"
#include "cromosoma.h"
#include <vector>

// ─────────────────────────────────────────
// SplitResultado — resultado por depósito
// ─────────────────────────────────────────
struct SplitResultado {
  std::vector<Ruta> rutas; // partición óptima en rutas para este depósito
  double costo;            // F*|rutas| + costo total de arcos
  bool factible;           // false si algún cliente tiene demanda > Q
};

// ─────────────────────────────────────────
// split_deposito
//
// Fiel a Prins et al. 2006, Sección 3.1:
// "For a list of p customers, it builds an auxiliary graph (X,A,Z)
//  where X contains p+1 nodes indexed 0 to p, A contains arc (i,j)
//  i<j if a trip servicing customers S[i+1]..S[j] is feasible in
//  terms of capacity.  The weight z[i][j] is equal to the trip cost.
//  The optimal splitting corresponds to a min-cost path from 0 to p."
//
// Parámetros:
//   clientes — IDs 1-indexed en el orden definido por CS para este depósito
//   dep_idx  — índice 0-based del depósito en la matriz de distancias
//              (= inst.num_clientes + posición en inst.depositos)
//   mat      — matriz de distancias
//   inst     — instancia (Q, F, demandas)
//
// Complejidad: O(p²)
// ─────────────────────────────────────────
SplitResultado split_deposito(const std::vector<int> &clientes, int dep_idx,
                              const Matriz &mat, const InstanciaLRP &inst);

// ─────────────────────────────────────────
// evaluar_cromosoma
//
// Evalúa el cromosoma completo:
//   Para cada depósito i abierto (DS[i] != 0):
//     1. Extrae clientes_de(i) en orden de CS
//     2. Aplica split_deposito
//     3. Acumula costo apertura + costo routing
//     4. Penaliza exceso de capacidad del depósito:
//        alpha * max(0, demanda_total_dep - cap_dep)
//
// Modifica crom.fitness.
// Retorna vector<SplitResultado> indexado por dep_idx (0-based),
// con entradas vacías para depósitos cerrados.
//
// alpha = 1000.0 según el paper (Sección 3.1.5)
// ─────────────────────────────────────────
std::vector<SplitResultado> evaluar_cromosoma(CromosomaLRP &crom,
                                              const Matriz &mat,
                                              const InstanciaLRP &inst,
                                              double alpha = 1000.0);
