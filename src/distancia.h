// =============================================================================
// distancia.h — Matriz de distancias entre todos los nodos del problema
// =============================================================================
//
// Una vez leída la instancia, necesitamos saber cuánto cuesta ir de cualquier
// nodo a cualquier otro. Eso es exactamente lo que guarda la Matriz.
//
// La idea es calcularla UNA SOLA VEZ al inicio y luego consultarla en O(1)
// durante todo el algoritmo (en vez de recalcular la raíz cuadrada miles de veces).
//
// CONVENCIÓN DE ÍNDICES (importante para no confundirse):
//   La matriz usa índices 0-based, pero los IDs en la solución son 1-based.
//   Siempre hay que restar 1 al acceder: matriz[cliente_id - 1][otro_id - 1]
//
//   Clientes:  índices [0 .. nc-1]
//   Depósitos: índices [nc .. nc+nd-1]
//
//   Ejemplo con n=10 clientes y m=2 depósitos:
//     Cliente 1  → índice 0
//     Cliente 10 → índice 9
//     Depósito 11 → índice 10   (ID 11 - 1 = 10)
//     Depósito 12 → índice 11   (ID 12 - 1 = 11)
//
// =============================================================================
#pragma once
#include "parser.h"
#include <vector>

// La Matriz es simplemente una tabla de distancias de tamaño (n+m) × (n+m).
// Acceso: matriz[i][j] = costo de ir del nodo i al nodo j (ambos 0-based).
using Matriz = std::vector<std::vector<double>>;

// Construye y devuelve la matriz de distancias para la instancia dada.
// Calcula la distancia euclidiana entre cada par de nodos.
// Si inst.es_entero == true, multiplica por 100 y trunca a entero (estándar Prodhon).
Matriz construir_matriz_distancias(const InstanciaLRP &inst);
