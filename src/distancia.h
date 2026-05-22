#pragma once
#include "parser.h"
#include <vector>

// Matriz de distancias cuadrada (N x N).
// N = num_clientes + num_depositos.
// Indexación 0-based internamente:
//   [0 .. num_clientes-1]             → clientes
//   [num_clientes .. N-1]             → depósitos
//
// Si inst.es_entero == true  → dist * 100, truncado a int (almacenado como
// double). Si inst.es_entero == false → distancia euclidiana exacta.
using Matriz = std::vector<std::vector<double>>;

Matriz construir_matriz_distancias(const InstanciaLRP &inst);
