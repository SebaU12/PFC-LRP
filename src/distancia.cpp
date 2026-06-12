// =============================================================================
// distancia.cpp — Construcción de la matriz de distancias
// =============================================================================
//
// Estrategia: aplanamos todos los nodos (primero clientes, luego depósitos)
// en una lista temporal de coordenadas, y luego llenamos la tabla con la
// distancia euclidiana entre cada par.
//
// Por qué aplanar primero: así el doble for queda limpio y no hay que
// distinguir entre "i es cliente" o "i es depósito" dentro del loop.
//
// =============================================================================
#include "distancia.h"
#include <cmath>

Matriz construir_matriz_distancias(const InstanciaLRP &inst) {
    int N = inst.num_clientes + inst.num_depositos; // total de nodos en el mapa

    // Tabla NxN inicializada en 0 (la diagonal queda en 0: distancia de un nodo a sí mismo)
    Matriz m(N, std::vector<double>(N, 0.0));

    // ── Aplanar todos los nodos en un vector de coordenadas ──────────────────
    // Orden: primero los nc clientes (índices 0..nc-1),
    //        luego los nd depósitos (índices nc..nc+nd-1).
    // Esto define la convención de índices que usa todo el proyecto.
    struct Nodo { double x, y; };
    std::vector<Nodo> nodos;
    nodos.reserve(N);
    for (const auto &c : inst.clientes)
        nodos.push_back({c.x, c.y});
    for (const auto &d : inst.depositos)
        nodos.push_back({d.x, d.y});

    // ── Calcular distancias para cada par (i, j) ──────────────────────────────
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            if (i == j) continue; // la diagonal ya es 0, no hace falta calcular

            double dx   = nodos[i].x - nodos[j].x;
            double dy   = nodos[i].y - nodos[j].y;
            double dist = std::sqrt(dx * dx + dy * dy); // distancia euclidiana estándar

            // Algunos benchmarks requieren distancias enteras para reproducir
            // resultados exactos de la literatura: se multiplica por 100 y se trunca.
            // Si es_entero == false, se usa el valor real flotante directamente.
            m[i][j] = inst.es_entero ? static_cast<int>(dist * 100) : dist;
        }
    }

    return m;
}
