// Implementación del validador de restricciones LRP.
// Referencia formal: Löffler 2023, Cavagnini 2025 (modelo de dos índices).
#include "validador.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <set>
#include <unordered_map>

// ── Helpers de impresión ──────────────────────────────────────────────────────

static void sep(char c = '-', int w = 56) {
    std::cout << std::string(w, c) << "\n";
}

static void print_ok(const std::string &label) {
    std::cout << "  [OK]   " << label << "\n";
}

static void print_fail(const std::string &label) {
    std::cout << "  [FAIL] " << label << "\n";
}

// ── Implementación principal ──────────────────────────────────────────────────

ResultadoValidacion validar_solucion(const EstadoLRP &e, bool imprimir) {
    const InstanciaLRP &inst = *e.datos;
    const Matriz       &mat  = *e.matriz;
    int nc = inst.num_clientes;
    int nd = inst.num_depositos;

    ResultadoValidacion res{};
    res.factible = true;

    // ── Colecciones auxiliares ────────────────────────────────────────────────
    std::set<int> deps_abiertos(e.depositos_abiertos.begin(),
                                 e.depositos_abiertos.end());

    // Frecuencia de aparición de cada cliente en las rutas
    std::unordered_map<int, int> freq;
    freq.reserve(nc);
    for (int c = 1; c <= nc; ++c) freq[c] = 0;

    // Contador de rutas activas y acumulación de costos de viaje
    int num_rutas_activas = 0;
    res.costo_apertura   = 0.0;
    res.costo_fijo_rutas = 0.0;
    res.costo_viaje      = 0.0;

    // ─────────────────────────────────────────────────────────────────────────
    // R6: coherencia estructural — id_deposito en Ruta == clave del mapa
    // (verificación previa, necesaria para que el resto tenga sentido)
    // ─────────────────────────────────────────────────────────────────────────
    res.r6_ok = true;
    res.rutas_con_id_inconsistente = 0;
    for (const auto &[dep_id, lista] : e.rutas) {
        for (const auto &r : lista) {
            if (!r.clientes.empty() && r.id_deposito != dep_id) {
                res.r6_ok = false;
                ++res.rutas_con_id_inconsistente;
            }
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Recorrer todas las rutas: acumular frecuencias de clientes y costos
    // ─────────────────────────────────────────────────────────────────────────
    int num_ruta_global = 0;

    for (const auto &[dep_id, lista] : e.rutas) {
        int idx_dep = dep_id - 1;   // índice 0-based en la matriz

        // R3: verificar que este depósito esté abierto
        if (!deps_abiertos.count(dep_id)) {
            res.rutas_deposito_cerrado.push_back(dep_id);
        }

        for (const auto &r : lista) {
            if (r.clientes.empty()) continue;
            ++num_ruta_global;
            ++num_rutas_activas;

            // Registrar aparición de cada cliente
            for (int c : r.clientes) {
                if (freq.count(c)) freq[c]++;
                // Clientes fuera de rango se ignorarán — su ausencia en freq los marca
            }

            // R5: capacidad de vehículo (Ec. 11)
            double demanda_ruta = 0.0;
            for (int c : r.clientes)
                demanda_ruta += inst.clientes[c - 1].demanda;

            if (demanda_ruta > inst.Q + 1e-9) {
                res.violaciones_vehiculo.push_back(
                    {num_ruta_global, dep_id, demanda_ruta, inst.Q});
            }

            // Acumular costo de viaje: dep → c1 → … → cp → dep
            double viaje = mat[idx_dep][r.clientes.front() - 1];
            for (int i = 0; i + 1 < (int)r.clientes.size(); ++i)
                viaje += mat[r.clientes[i] - 1][r.clientes[i + 1] - 1];
            viaje += mat[r.clientes.back() - 1][idx_dep];
            res.costo_viaje += viaje;
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    // R1 (Ecs. 2 & 3): cada cliente visitado exactamente una vez
    // ─────────────────────────────────────────────────────────────────────────
    for (int c : e.no_asignados)
        res.clientes_huerfanos.push_back(c);

    for (int c = 1; c <= nc; ++c) {
        if (freq[c] == 0 && !std::count(e.no_asignados.begin(),
                                        e.no_asignados.end(), c))
            res.clientes_huerfanos.push_back(c);   // no está en ningún lado
        if (freq[c] > 1)
            res.clientes_duplicados.push_back(c);
    }

    res.r1_ok = res.clientes_huerfanos.empty() && res.clientes_duplicados.empty();

    // ─────────────────────────────────────────────────────────────────────────
    // R2 (Ec. 4): cada cliente asignado a exactamente un depósito
    // Dado que R1 garantiza que cada cliente aparece una vez, y cada ruta
    // pertenece a un único depósito, R2 se satisface si y sólo si R1 se satisface.
    // ─────────────────────────────────────────────────────────────────────────
    res.r2_ok = res.r1_ok;

    // ─────────────────────────────────────────────────────────────────────────
    // R3 (Ec. 5): rutas sólo en depósitos abiertos (w_ij ≤ y_i)
    // ─────────────────────────────────────────────────────────────────────────
    res.r3_ok = res.rutas_deposito_cerrado.empty();

    // ─────────────────────────────────────────────────────────────────────────
    // R4 (Ec. 6): capacidad de depósito  sum(d_j * w_ij) ≤ W_i * y_i
    // ─────────────────────────────────────────────────────────────────────────
    res.r4_ok = true;
    for (int dep_id : e.depositos_abiertos) {
        int dep_idx = dep_id - nc - 1;   // índice en inst.depositos (0-based)
        const auto &dep = inst.depositos[dep_idx];

        double demanda_dep = 0.0;
        auto it = e.rutas.find(dep_id);
        if (it != e.rutas.end())
            for (const auto &r : it->second)
                for (int c : r.clientes)
                    demanda_dep += inst.clientes[c - 1].demanda;

        if (demanda_dep > dep.capacidad + 1e-9) {
            res.violaciones_deposito.push_back({dep_id, demanda_dep, dep.capacidad});
            res.r4_ok = false;
        }

        // Acumular costo de apertura
        res.costo_apertura += dep.costo_apertura;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // R5: resultado final
    // ─────────────────────────────────────────────────────────────────────────
    res.r5_ok = res.violaciones_vehiculo.empty();

    // ─────────────────────────────────────────────────────────────────────────
    // Costo fijo de rutas y costo total
    // ─────────────────────────────────────────────────────────────────────────
    res.costo_fijo_rutas = inst.F * num_rutas_activas;
    res.costo_total      = e.objective();   // usa la función oficial (con penalizaciones)

    // ─────────────────────────────────────────────────────────────────────────
    // Factibilidad global
    // ─────────────────────────────────────────────────────────────────────────
    res.factible = res.r1_ok && res.r2_ok && res.r3_ok &&
                   res.r4_ok && res.r5_ok && res.r6_ok;

    // ─────────────────────────────────────────────────────────────────────────
    // Impresión del reporte
    // ─────────────────────────────────────────────────────────────────────────
    if (!imprimir) return res;

    sep('=');
    std::cout << "      VALIDACIÓN DE RESTRICCIONES LRP\n";
    sep('=');
    std::cout << "Instancia: " << inst.nombre
              << "  |  n=" << nc << "  m=" << nd
              << "  |  Q=" << inst.Q << "  F=" << inst.F << "\n";
    std::cout << "Depósitos abiertos: ";
    for (int d : e.depositos_abiertos) std::cout << d << " ";
    std::cout << "\nRutas activas: " << num_rutas_activas << "\n";
    sep();

    // ── R1: visita exacta (Ecs. 2 & 3) ──────────────────────────────────────
    std::cout << "R1 (Ecs. 2 & 3) — Cada cliente visitado exactamente una vez:\n";
    if (res.r1_ok) {
        print_ok("Todos los " + std::to_string(nc) +
                 " clientes aparecen exactamente una vez");
    } else {
        if (!res.clientes_huerfanos.empty()) {
            print_fail("Clientes sin ruta asignada (huérfanos): ");
            std::cout << "         IDs: ";
            for (int c : res.clientes_huerfanos) std::cout << c << " ";
            std::cout << "\n";
        }
        if (!res.clientes_duplicados.empty()) {
            print_fail("Clientes duplicados en rutas: ");
            std::cout << "         IDs: ";
            for (int c : res.clientes_duplicados) std::cout << c << " ";
            std::cout << "\n";
        }
    }
    sep();

    // ── R2: asignación única (Ec. 4) ─────────────────────────────────────────
    std::cout << "R2 (Ec. 4)  — Cada cliente asignado a exactamente un depósito:\n";
    if (res.r2_ok)
        print_ok("Asignación uno-a-uno garantizada (implícita de R1 + estructura)");
    else
        print_fail("Violación heredada de R1 (clientes sin asignación única)");
    sep();

    // ── R3: asignación a depósitos abiertos (Ec. 5) ──────────────────────────
    std::cout << "R3 (Ec. 5)  — Asignación sólo a depósitos abiertos (w_ij ≤ y_i):\n";
    if (res.r3_ok) {
        print_ok("Todas las rutas pertenecen a depósitos abiertos");
    } else {
        for (int dep_id : res.rutas_deposito_cerrado)
            print_fail("Ruta en depósito NO abierto: Dep " + std::to_string(dep_id));
    }
    sep();

    // ── R4: capacidad de depósito (Ec. 6) ────────────────────────────────────
    std::cout << "R4 (Ec. 6)  — Capacidad de depósito (sum d_j ≤ W_i):\n";
    if (res.r4_ok) {
        for (int dep_id : e.depositos_abiertos) {
            int dep_idx = dep_id - nc - 1;
            const auto &dep = inst.depositos[dep_idx];
            double dem = 0.0;
            auto it = e.rutas.find(dep_id);
            if (it != e.rutas.end())
                for (const auto &r : it->second)
                    for (int c : r.clientes)
                        dem += inst.clientes[c - 1].demanda;
            std::cout << "  [OK]   Dep " << dep_id
                      << ": demanda=" << std::fixed << std::setprecision(2) << dem
                      << " ≤ W=" << dep.capacidad << "\n";
        }
    } else {
        for (int dep_id : e.depositos_abiertos) {
            int dep_idx = dep_id - nc - 1;
            const auto &dep = inst.depositos[dep_idx];
            double dem = 0.0;
            auto it = e.rutas.find(dep_id);
            if (it != e.rutas.end())
                for (const auto &r : it->second)
                    for (int c : r.clientes)
                        dem += inst.clientes[c - 1].demanda;
            bool viol = (dem > dep.capacidad + 1e-9);
            if (viol)
                std::cout << "  [FAIL] Dep " << dep_id
                          << ": demanda=" << std::fixed << std::setprecision(2) << dem
                          << " > W=" << dep.capacidad << " *** VIOLACIÓN ***\n";
            else
                std::cout << "  [OK]   Dep " << dep_id
                          << ": demanda=" << std::fixed << std::setprecision(2) << dem
                          << " ≤ W=" << dep.capacidad << "\n";
        }
    }
    sep();

    // ── R5: capacidad de vehículo (Ec. 11) ───────────────────────────────────
    std::cout << "R5 (Ec. 11) — Capacidad de vehículo por ruta (sum d_j ≤ Q="
              << inst.Q << "):\n";
    {
        int nr = 0;
        for (const auto &[dep_id, lista] : e.rutas) {
            for (const auto &r : lista) {
                if (r.clientes.empty()) continue;
                ++nr;
                double dem = 0.0;
                for (int c : r.clientes)
                    dem += inst.clientes[c - 1].demanda;
                bool viol = (dem > inst.Q + 1e-9);
                std::cout << (viol ? "  [FAIL] " : "  [OK]   ")
                          << "Ruta " << nr << " (Dep " << dep_id << "): carga="
                          << std::fixed << std::setprecision(2) << dem
                          << " ≤ Q=" << inst.Q
                          << (viol ? "  *** VIOLACIÓN ***" : "") << "\n";
            }
        }
    }
    sep();

    // ── R6: coherencia estructural ────────────────────────────────────────────
    std::cout << "R6 (coherencia) — id_deposito en Ruta == clave del mapa:\n";
    if (res.r6_ok)
        print_ok("Estructura interna consistente");
    else
        print_fail(std::to_string(res.rutas_con_id_inconsistente) +
                   " ruta(s) con id_deposito inconsistente");
    sep();

    // ── Función objetivo — desglose (Ec. 1) ──────────────────────────────────
    std::cout << "FUNCIÓN OBJETIVO Z (Ec. 1) — min sum O_i*y_i + F*rutas + sum c_ij*x_ij:\n";
    std::cout << "  Apertura depósitos (sum O_i * y_i): "
              << std::fixed << std::setprecision(2) << res.costo_apertura << "\n";
    std::cout << "  Costo fijo rutas   (F * " << num_rutas_activas << " rutas):   "
              << res.costo_fijo_rutas << "\n";
    std::cout << "  Costo de viaje     (sum c_ij * x_ij):  "
              << res.costo_viaje << "\n";
    double z_sin_pen = res.costo_apertura + res.costo_fijo_rutas + res.costo_viaje;
    std::cout << "  Z (sin penalizaciones):                 "
              << z_sin_pen << "\n";
    if (!res.factible) {
        double pen = res.costo_total - z_sin_pen;
        std::cout << "  Penalizaciones por violaciones:         "
                  << pen << "\n";
        std::cout << "  Z (con penalizaciones):                 "
                  << res.costo_total << "\n";
    }
    sep('=');

    // ── Veredicto final ───────────────────────────────────────────────────────
    if (res.factible) {
        std::cout << "STATUS: SOLUCIÓN FACTIBLE  — Z = "
                  << std::fixed << std::setprecision(2) << res.costo_total << "\n";
    } else {
        std::cout << "STATUS: SOLUCIÓN INFACTIBLE  — restricciones violadas:\n";
        if (!res.r1_ok) std::cout << "  ✗ R1 (Ecs. 2 & 3): clientes no visitados/duplicados\n";
        if (!res.r3_ok) std::cout << "  ✗ R3 (Ec. 5):      rutas en depósitos cerrados\n";
        if (!res.r4_ok) std::cout << "  ✗ R4 (Ec. 6):      capacidad de depósito excedida\n";
        if (!res.r5_ok) std::cout << "  ✗ R5 (Ec. 11):     capacidad de vehículo excedida\n";
        if (!res.r6_ok) std::cout << "  ✗ R6 (coherencia): id_deposito inconsistente\n";
    }
    sep('=');

    return res;
}
