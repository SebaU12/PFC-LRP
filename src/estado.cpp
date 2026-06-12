// =============================================================================
// estado.cpp — Evaluación y visualización de una solución LRP
// =============================================================================
//
// Aquí viven las tres funciones que "miden" una solución:
//   demanda_ruta()     → cuánto carga lleva un camión en una ruta
//   demanda_deposito() → cuánta carga total sale de un depósito
//   objective()        → el costo total Z de la solución (la función a minimizar)
//   imprimir_solucion()→ muestra la solución con todos los detalles en consola
//
// =============================================================================
#include "estado.h"
#include <iomanip>
#include <iostream>

// -----------------------------------------------------------------------------
// Suma las demandas de todos los clientes que visita esta ruta.
// Se usa para verificar que la ruta no supere la capacidad Q del vehículo.
// -----------------------------------------------------------------------------
double EstadoLRP::demanda_ruta(const Ruta &r) const {
    double d = 0.0;
    for (int c : r.clientes)
        d += datos->clientes[c - 1].demanda; // c es 1-based → c-1 para indexar
    return d;
}

// -----------------------------------------------------------------------------
// Suma las demandas de todas las rutas que salen de un depósito.
// Se usa para verificar que el depósito no supere su capacidad W_i.
// -----------------------------------------------------------------------------
double EstadoLRP::demanda_deposito(int dep_id) const {
    double total = 0.0;
    auto it = rutas.find(dep_id);
    if (it != rutas.end())
        for (const auto &r : it->second)
            total += demanda_ruta(r);
    return total;
}

// -----------------------------------------------------------------------------
// Calcula el costo total Z de la solución (Ec. 1 del paper).
//
// Z = (costos de abrir depósitos)
//   + (costos fijos de cada vehículo/ruta)
//   + (costos de viaje por los arcos recorridos)
//   + (penalizaciones por infactibilidad)
//
// La penalización de 100000 por unidad de violación hace que una solución
// infactible siempre sea "peor" que cualquier factible, guiando al algoritmo
// a preferir soluciones que respeten las restricciones.
// -----------------------------------------------------------------------------
double EstadoLRP::objective() const {
    double z = 0.0;

    // ── Costo de apertura de depósitos: sum(O_i * y_i) ───────────────────────
    for (int dep_id : depositos_abiertos) {
        const auto &dep = datos->depositos[dep_id - datos->num_clientes - 1];
        z += dep.costo_apertura;
    }

    // ── Costo de cada ruta: F (fijo) + distancias de viaje ───────────────────
    // Cada ruta es un circuito:  depósito → c1 → c2 → ... → cp → depósito
    // El costo de viaje se calcula sumando los arcos uno a uno.
    for (const auto &[dep_id, lista_rutas] : rutas) {
        int idx_dep = dep_id - 1; // convertir ID 1-based a índice 0-based de la matriz
        for (const auto &ruta : lista_rutas) {
            if (ruta.clientes.empty()) continue; // saltear rutas vacías

            z += datos->F; // costo fijo por usar este camión

            // Arco de salida: depósito → primer cliente
            z += (*matriz)[idx_dep][ruta.clientes.front() - 1];

            // Arcos intermedios: cliente → siguiente cliente
            for (int i = 0; i + 1 < (int)ruta.clientes.size(); ++i)
                z += (*matriz)[ruta.clientes[i] - 1][ruta.clientes[i + 1] - 1];

            // Arco de regreso: último cliente → depósito
            z += (*matriz)[ruta.clientes.back() - 1][idx_dep];
        }
    }

    // ── Penalización por exceso de capacidad en depósitos ────────────────────
    // Si la demanda total asignada a un depósito supera W_i, se penaliza
    // proporcionalmente al exceso (no se rechaza la solución, pero se encarece mucho).
    for (int dep_id : depositos_abiertos) {
        const auto &dep = datos->depositos[dep_id - datos->num_clientes - 1];
        double dem = demanda_deposito(dep_id);
        if (dem > dep.capacidad)
            z += 100000.0 * (dem - dep.capacidad);
    }

    // ── Penalización por clientes sin asignar ─────────────────────────────────
    // Cada cliente en no_asignados suma 100000 al costo — señal fuerte de que
    // la solución está incompleta.
    z += 100000.0 * static_cast<int>(no_asignados.size());

    return z;
}

// -----------------------------------------------------------------------------
// Imprime la solución de forma legible: ruta por ruta, con carga y costos.
// También actualiza ruta.costo y ruta.demanda_acumulada como efecto secundario.
// -----------------------------------------------------------------------------
void EstadoLRP::imprimir_solucion() {
    std::cout << "\n====================================================\n";
    std::cout << "          MEJOR SOLUCIÓN ENCONTRADA                 \n";
    std::cout << "====================================================\n";

    int num_ruta = 1;

    for (int dep_id : depositos_abiertos) {
        const auto &dep = datos->depositos[dep_id - datos->num_clientes - 1];
        std::cout << "\nDepósito " << dep_id
                  << " (apertura: " << dep.costo_apertura
                  << ", cap: " << dep.capacidad << "):\n";

        auto it = rutas.find(dep_id);
        if (it == rutas.end()) continue;

        double carga_dep = 0.0;
        for (auto &ruta : it->second) {
            if (ruta.clientes.empty()) continue;

            int idx_dep = dep_id - 1;

            // Recalcular costo de viaje de esta ruta
            double costo_viaje = (*matriz)[idx_dep][ruta.clientes.front() - 1];
            for (int i = 0; i + 1 < (int)ruta.clientes.size(); ++i)
                costo_viaje += (*matriz)[ruta.clientes[i] - 1][ruta.clientes[i + 1] - 1];
            costo_viaje += (*matriz)[ruta.clientes.back() - 1][idx_dep];

            // Recalcular demanda de esta ruta
            ruta.demanda_acumulada = 0.0;
            for (int c : ruta.clientes)
                ruta.demanda_acumulada += datos->clientes[c - 1].demanda;

            // Guardar el costo total (F + viaje) en el campo del struct
            ruta.costo = datos->F + costo_viaje;

            // Mostrar: Dep → c1 → c2 → ... → Dep | carga | costos
            std::cout << "  Ruta " << num_ruta++ << ": [Dep " << dep_id << "]";
            for (int c : ruta.clientes)
                std::cout << " -> " << c;
            std::cout << " -> [Dep " << dep_id << "]";
            std::cout << " | carga: " << ruta.demanda_acumulada << "/" << datos->Q;
            std::cout << " | viaje: " << std::fixed << std::setprecision(2) << costo_viaje;
            std::cout << " | F: " << datos->F;
            std::cout << " | total_ruta: " << ruta.costo << "\n";

            carga_dep += ruta.demanda_acumulada;
        }
        std::cout << "  => Carga depósito: " << carga_dep << "/" << dep.capacidad << "\n";
    }

    if (!no_asignados.empty()) {
        std::cout << "\n*** Clientes no asignados: ";
        for (int c : no_asignados) std::cout << c << " ";
        std::cout << "\n";
    }

    std::cout << "----------------------------------------------------\n";
    std::cout << "COSTO TOTAL: " << std::fixed << std::setprecision(2)
              << objective() << "\n";
    std::cout << "====================================================\n\n";
}
