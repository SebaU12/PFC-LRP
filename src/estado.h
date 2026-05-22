#pragma once
#include "distancia.h"
#include "parser.h"
#include <map>
#include <vector>

// Ruta = secuencia ordenada de IDs de cliente (1-indexed)
using Ruta = std::vector<int>;
// Rutas por depósito: dep_id (1-indexed) → lista de rutas
using Rutas = std::map<int, std::vector<Ruta>>;

struct EstadoLRP {
  Rutas rutas;
  std::vector<int> depositos_abiertos; // IDs 1-indexed
  std::vector<int> no_asignados;       // IDs de cliente 1-indexed

  // Referencias compartidas (no se copian, son solo punteros ligeros)
  const Matriz *matriz;
  const InstanciaLRP *datos;

  // Constructor principal
  EstadoLRP(Rutas r, std::vector<int> dep_abiertos, std::vector<int> no_asig,
            const Matriz &mat, const InstanciaLRP &dat)
      : rutas(std::move(r)), depositos_abiertos(std::move(dep_abiertos)),
        no_asignados(std::move(no_asig)), matriz(&mat), datos(&dat) {}

  // Deep copy del estado (matriz y datos son compartidos, no se duplican)
  EstadoLRP copy() const {
    return EstadoLRP(rutas, depositos_abiertos, no_asignados, *matriz, *datos);
  }

  // Función objetivo — espejo exacto del Python
  double objective() const;

  // Helpers de consulta
  double demanda_ruta(const Ruta &r) const;
  double demanda_deposito(int dep_id) const;
};
