// =============================================================================
// solucion_inicial.cpp — All-Open + Greedy Insertion para el LRP
// =============================================================================
//
// Construimos la primera solución en dos pasos:
//
//   1. Poner todos los clientes en la lista de "pendientes por asignar"
//   2. Repetir hasta que no queden pendientes:
//        - Tomar el primer cliente de la lista
//        - Probar cada posición posible en cada ruta de cada depósito abierto
//        - Elegir la inserción que sume menos al costo total
//        - Si no cabe en ninguna ruta existente → abrir una ruta nueva
//
// El "costo marginal de inserción" en una posición es:
//   dist(prev, cliente) + dist(cliente, next) - dist(prev, next)
// Es decir, cuánto EXTRA cuesta meter al cliente entre dos nodos ya conectados.
// Para una ruta nueva: F + 2 * dist(depósito, cliente) (ida y vuelta solo)
//
// =============================================================================
#include "solucion_inicial.h"
#include <limits>
#include <tuple>

// -----------------------------------------------------------------------------
// Calcula la demanda total que lleva una ruta.
// Esta función está expuesta en el .h porque los operadores ALNS también la usan.
// -----------------------------------------------------------------------------
double demanda_ruta_externa(const Ruta &r, const InstanciaLRP &datos) {
    double d = 0.0;
    for (int c : r.clientes)
        d += datos.clientes[c - 1].demanda;
    return d;
}

// -----------------------------------------------------------------------------
// Greedy insertion: asigna todos los clientes de no_asignados a alguna ruta,
// eligiendo siempre la inserción de menor costo marginal.
//
// Toma el estado actual (con clientes en no_asignados) y devuelve el estado
// con todos los clientes dentro de alguna ruta.
// -----------------------------------------------------------------------------
static EstadoLRP greedy_insertion(EstadoLRP estado) {
    const Matriz &mat = *estado.matriz;
    const double Q    = estado.datos->Q; // capacidad máxima del vehículo
    const double F    = estado.datos->F; // costo fijo por abrir una ruta nueva

    // Procesar clientes uno a uno hasta vaciar la lista de pendientes
    while (!estado.no_asignados.empty()) {
        double mejor_costo = std::numeric_limits<double>::infinity();
        std::tuple<int, int, int, int> mejor_mov = {-1, -1, -1, -1};
        // mejor_mov = (cliente, dep_id, índice_ruta, posición_en_ruta)
        // índice_ruta == -1 significa "abrir ruta nueva en ese depósito"

        int cliente = estado.no_asignados[0]; // cliente que intentamos insertar ahora
        int idx_c   = cliente - 1;            // índice 0-based en la matriz
        double dem_c = estado.datos->clientes[idx_c].demanda;

        // ── Probar cada depósito abierto ──────────────────────────────────────
        for (int dep_id : estado.depositos_abiertos) {
            int idx_dep     = dep_id - 1;
            auto &lista     = estado.rutas[dep_id];

            // Opción A: insertar en una ruta ya existente de este depósito
            for (int ri = 0; ri < (int)lista.size(); ++ri) {
                auto &ruta = lista[ri];

                // Verificar que la ruta tenga capacidad disponible
                if (demanda_ruta_externa(ruta, *estado.datos) + dem_c > Q)
                    continue;

                // Probar cada posición de inserción dentro de la ruta
                // (antes del cliente 0, entre 0 y 1, ..., después del último)
                for (int pos = 0; pos <= (int)ruta.clientes.size(); ++pos) {
                    // Nodo anterior a la posición de inserción
                    int prev = (pos == 0) ? idx_dep : ruta.clientes[pos - 1] - 1;
                    // Nodo siguiente a la posición de inserción
                    int next = (pos == (int)ruta.clientes.size()) ? idx_dep : ruta.clientes[pos] - 1;

                    // Costo marginal: cuánto extra cuesta meter al cliente aquí
                    double costo = mat[prev][idx_c] + mat[idx_c][next] - mat[prev][next];
                    if (costo < mejor_costo) {
                        mejor_costo = costo;
                        mejor_mov   = {cliente, dep_id, ri, pos};
                    }
                }
            }

            // Opción B: abrir una ruta nueva solo para este cliente
            // Costo = F (abrir el camión) + ida al cliente + vuelta al depósito
            double costo_nueva = mat[idx_dep][idx_c] * 2.0 + F;
            if (costo_nueva < mejor_costo) {
                mejor_costo = costo_nueva;
                mejor_mov   = {cliente, dep_id, -1, 0}; // -1 = ruta nueva
            }
        }

        // ── Aplicar la mejor inserción encontrada ─────────────────────────────
        auto [c_ins, m_dep, m_ri, m_pos] = mejor_mov;

        if (m_ri == -1) {
            // Abrir una ruta nueva en el depósito elegido con solo este cliente
            estado.rutas[m_dep].push_back(Ruta(m_dep, {c_ins}));
        } else {
            // Insertar en la posición elegida dentro de una ruta existente
            auto &ruta = estado.rutas[m_dep][m_ri];
            ruta.clientes.insert(ruta.clientes.begin() + m_pos, c_ins);
        }

        // Quitar al cliente de la lista de pendientes
        estado.no_asignados.erase(estado.no_asignados.begin());
    }

    return estado;
}

// -----------------------------------------------------------------------------
// Punto de entrada: crea el estado inicial con todos los depósitos abiertos
// y luego llama a greedy_insertion para asignar todos los clientes.
// -----------------------------------------------------------------------------
EstadoLRP generar_solucion_inicial(const InstanciaLRP &inst, const Matriz &m) {
    // Abrir todos los depósitos y registrar sus IDs
    std::vector<int> dep_ids;
    Rutas rutas_vacias;
    for (const auto &d : inst.depositos) {
        dep_ids.push_back(d.id);
        rutas_vacias[d.id] = {}; // cada depósito empieza sin rutas
    }

    // Todos los clientes empiezan en la lista de "pendientes"
    std::vector<int> todos;
    for (const auto &c : inst.clientes)
        todos.push_back(c.id);

    // Crear el estado vacío y llenarlo con greedy insertion
    EstadoLRP estado(rutas_vacias, dep_ids, todos, m, inst);
    return greedy_insertion(std::move(estado));
}
