#pragma once
#include "../estado.h"
#include <limits>
#include <vector>

// ─────────────────────────────────────────
// Cromosoma MA|PM — fiel a Prins et al. 2006
//
// DS[i]  : estado del depósito i (0-indexed sobre inst.depositos)
//           0          → depósito cerrado
//           k > 0      → depósito abierto; k es el índice 1-based
//                        en CS donde comienza su lista de clientes.
//
// CS     : permutación de todos los clientes (IDs 1-indexed),
//           longitud n.  Es la concatenación de las sublistas de
//           clientes de cada depósito abierto, en el orden en que
//           DS los referencia.  No hay delimitadores explícitos:
//           los límites entre depósitos se deducen de DS.
//
// Ejemplo (paper Fig.1, n=10, m=5):
//   DS = [13, 0, 1, 18, 0]
//   CS = [8,20,15,16,18, 2,10,4,22,17]  (1-indexed positions)
//   → Dep 1 abierto, clientes desde pos 13 de CS
//   → Dep 2 cerrado
//   → Dep 3 abierto, clientes desde pos 1 de CS
//   → Dep 4 abierto, clientes desde pos 18 de CS
//   → Dep 5 cerrado
//
// fitness: costo total = apertura + Split(routing) + penalización.
//          infinity si no ha sido evaluado.
// ─────────────────────────────────────────

struct CromosomaLRP {
  std::vector<int> DS; // longitud m
  std::vector<int> CS; // longitud n, IDs de cliente 1-indexed
  double fitness;

  CromosomaLRP() : fitness(std::numeric_limits<double>::infinity()) {}

  // ── Consultas sobre la estructura ─────────────────────

  // ¿Está abierto el depósito de índice dep_idx (0-based)?
  bool abierto(int dep_idx) const { return DS[dep_idx] != 0; }

  // Número de depósitos abiertos
  int n_abiertos() const {
    int cnt = 0;
    for (int v : DS)
      cnt += (v != 0);
    return cnt;
  }

  // ── Extracción de sublistas ───────────────────────────
  // Devuelve los clientes asignados al depósito dep_idx
  // en el orden que aparecen en CS.
  // Complejidad O(n + m).
  std::vector<int> clientes_de(int dep_idx, const InstanciaLRP &inst) const;

  // Devuelve el rango [inicio, fin) en CS (0-based)
  // para el depósito dep_idx.  inicio == fin si está cerrado.
  std::pair<int, int> rango_cs(int dep_idx, const InstanciaLRP &inst) const;
};

// ─────────────────────────────────────────
// Repair del cromosoma post-crossover
//
// Corrige dos tipos de infactibilidad:
//   1. Ningún depósito abierto (índice 1 de CS no está en DS):
//      abre el depósito más barato y le asigna DS(i) = 1.
//   2. Violación de capacidad de depósito:
//      escanea hacia atrás la sublista del depósito violado,
//      remueve clientes uno a uno y los reasigna al primer
//      depósito abierto con capacidad suficiente; si no hay
//      ninguno, abre el depósito más cercano al cliente.
//
// Después del repair el cromosoma puede evaluarse con Split.
// ─────────────────────────────────────────
void repair(CromosomaLRP &crom, const Matriz &mat, const InstanciaLRP &inst);

// ─────────────────────────────────────────
// Conversiones con EstadoLRP
// ─────────────────────────────────────────

// Construye un CromosomaLRP desde un EstadoLRP ya evaluado.
// CS se forma concatenando las rutas de cada depósito abierto.
// DS(i) apunta a la posición 1-based en CS donde empieza
// la sublista del depósito i.
CromosomaLRP estado_a_cromosoma(const EstadoLRP &estado);

// Reconstruye un EstadoLRP desde un cromosoma.
// rutas_cache contiene el resultado del último Split por depósito.
EstadoLRP cromosoma_a_estado(const CromosomaLRP &crom, const Rutas &rutas_cache,
                             const Matriz &mat, const InstanciaLRP &inst);
