// =============================================================================
// cromosoma.h — Representación genética de una solución LRP en el MA|PM
// =============================================================================
//
// El MA|PM necesita una forma compacta de codificar una solución LRP que sea
// fácil de cruzar (crossover) entre dos padres. Para eso usa el cromosoma
// de Prins et al. (2006), que tiene dos partes:
//
//   DS (Depot Status) — vector de m enteros, uno por depósito candidato.
//       DS[i] = 0    → depósito i está CERRADO
//       DS[i] = k>0  → depósito i está ABIERTO y su sublista de clientes
//                       empieza en la posición k (1-based) del vector CS.
//
//   CS (Client Sequence) — permutación de los n clientes (IDs 1-indexed).
//       Es la CONCATENACIÓN de las sublistas de clientes de cada depósito
//       abierto, en el orden en que aparecen en DS.
//
// Ejemplo con 4 clientes y 2 depósitos:
//   DS = [3, 1]  → depósito 0 abierto (clientes desde posición 3), depósito 1 abierto (desde posición 1)
//   CS = [2, 4, 1, 3]
//   → depósito 1 tiene clientes [2, 4] (posiciones 1..2)
//   → depósito 0 tiene clientes [1, 3] (posiciones 3..4)
//
// El cromosoma NO especifica las rutas. Las rutas se obtienen después
// aplicando el algoritmo Split, que las calcula óptimamente.
//
// =============================================================================
#pragma once
#include "../estado.h"
#include <limits>
#include <vector>

struct CromosomaLRP {
  std::vector<int> DS; // longitud m: estado de cada depósito candidato
  std::vector<int> CS; // longitud n: permutación de todos los clientes (1-indexed)
  double fitness;      // costo total de la solución (lo pone evaluar_cromosoma)

  CromosomaLRP() : fitness(std::numeric_limits<double>::infinity()) {}

  // ¿Está abierto el depósito dep_idx?
  bool abierto(int dep_idx) const { return DS[dep_idx] != 0; }

  // ¿Cuántos depósitos están abiertos?
  int n_abiertos() const {
    int cnt = 0;
    for (int v : DS)
      cnt += (v != 0);
    return cnt;
  }

  // Devuelve la sublista de clientes asignados al depósito dep_idx,
  // extraída de CS según la posición indicada por DS[dep_idx].
  std::vector<int> clientes_de(int dep_idx, const InstanciaLRP &inst) const;

  // Devuelve el rango [inicio, fin) 0-based en CS que corresponde al
  // depósito dep_idx. Útil para modificar esa región directamente.
  std::pair<int, int> rango_cs(int dep_idx, const InstanciaLRP &inst) const;
};

// -----------------------------------------------------------------------------
// Corrige un cromosoma que quedó infactible después de un crossover.
// Casos que repara:
//   1. Ningún depósito abierto → abre el más barato.
//   2. Algún depósito supera su capacidad W_i → mueve clientes a otros depósitos
//      (o abre un nuevo depósito si no hay espacio en ninguno).
// -----------------------------------------------------------------------------
void repair(CromosomaLRP &crom, const Matriz &mat, const InstanciaLRP &inst);

// -----------------------------------------------------------------------------
// Convierte un EstadoLRP (con rutas explícitas) en un CromosomaLRP.
// Se usa para convertir la solución inicial greedy en un cromosoma que
// el MA|PM pueda incluir en la población.
// -----------------------------------------------------------------------------
CromosomaLRP estado_a_cromosoma(const EstadoLRP &estado);

// -----------------------------------------------------------------------------
// Convierte un CromosomaLRP de vuelta a un EstadoLRP.
// Usa rutas_cache (resultado del último Split) si está disponible;
// si no, crea rutas degeneradas (un cliente por ruta) como fallback.
// -----------------------------------------------------------------------------
EstadoLRP cromosoma_a_estado(const CromosomaLRP &crom, const Rutas &rutas_cache,
                             const Matriz &mat, const InstanciaLRP &inst);
