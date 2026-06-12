// test_validador.cpp — verifica que la función validar_solucion() detecta
// correctamente soluciones factibles e infactibles del LRP.
// Uso: ./test_validador <instancia.dat>
#include "src/alns/operadores.h"
#include "src/alns/solver.h"
#include "src/distancia.h"
#include "src/mapm/mapm.h"
#include "src/parser.h"
#include "src/solucion_inicial.h"
#include "src/validador.h"

#include <algorithm>
#include <iostream>
#include <string>

static int n_ok = 0, n_fail = 0;

static void check(bool cond, const std::string &msg) {
    if (cond) { std::cout << "  [OK]   " << msg << "\n"; ++n_ok; }
    else       { std::cout << "  [FAIL] " << msg << "\n"; ++n_fail; }
}

// ─────────────────────────────────────────────────────────────────────────────
// Caso 1: solución inicial (all-open greedy) — debe ser factible
// ─────────────────────────────────────────────────────────────────────────────
static void test_sol_inicial(const InstanciaLRP &inst, const Matriz &mat) {
    std::cout << "\n=== Caso 1: Solución Inicial (Greedy All-Open) ===\n";
    auto sol = generar_solucion_inicial(inst, mat);
    auto r = validar_solucion(sol, /*imprimir=*/true);

    check(r.r1_ok,    "R1: todos los clientes ruteados, sin duplicados");
    check(r.r2_ok,    "R2: asignación única por cliente");
    check(r.r3_ok,    "R3: rutas sólo en depósitos abiertos");
    check(r.r4_ok,    "R4: capacidad de depósito respetada");
    check(r.r5_ok,    "R5: capacidad de vehículo respetada");
    check(r.r6_ok,    "R6: coherencia id_deposito");
    check(r.factible, "Solución inicial es factible");
}

// ─────────────────────────────────────────────────────────────────────────────
// Caso 2: solución ALNS — debe ser factible
// ─────────────────────────────────────────────────────────────────────────────
static void test_alns(const InstanciaLRP &inst, const Matriz &mat) {
    std::cout << "\n=== Caso 2: Solución ALNS ===\n";
    auto sol_ini = generar_solucion_inicial(inst, mat);

    ConfigALNS cfg;
    cfg.temperatura_inicial  = 50000.0;
    cfg.temperatura_final    = 1.0;
    cfg.factor_enfriamiento  = 0.998;
    cfg.factor_reaccion      = 0.1;
    cfg.segmento             = 100;
    cfg.sigma1 = 33.0; cfg.sigma2 = 9.0;
    cfg.sigma3 = 13.0; cfg.sigma4 = 1.0;
    cfg.max_iteraciones      = 3000;
    cfg.semilla              = 42;

    AlgoritmALNS algo(cfg,
        {random_removal, worst_removal, shaw_removal, route_removal, depot_closing},
        {greedy_repair, regret2_repair});

    auto res = algo.ejecutar(sol_ini);
    auto r = validar_solucion(res.mejor_estado, /*imprimir=*/true);

    check(r.r1_ok,    "R1: todos los clientes ruteados, sin duplicados");
    check(r.r2_ok,    "R2: asignación única por cliente");
    check(r.r3_ok,    "R3: rutas sólo en depósitos abiertos");
    check(r.r4_ok,    "R4: capacidad de depósito respetada");
    check(r.r5_ok,    "R5: capacidad de vehículo respetada");
    check(r.r6_ok,    "R6: coherencia id_deposito");
    check(r.factible, "Solución ALNS es factible");
}

// ─────────────────────────────────────────────────────────────────────────────
// Caso 3: solución MA|PM — debe ser factible
// ─────────────────────────────────────────────────────────────────────────────
static void test_mapm(const InstanciaLRP &inst, const Matriz &mat) {
    std::cout << "\n=== Caso 3: Solución MA|PM ===\n";
    auto sol_ini = generar_solucion_inicial(inst, mat);

    ConfigMAPM cfg = config_por_defecto(inst.num_clientes, inst.num_depositos, 42);
    AlgoritmMAPM algo(cfg);

    auto res = algo.ejecutar(sol_ini);
    auto r = validar_solucion(res.mejor_estado, /*imprimir=*/true);

    check(r.r1_ok,    "R1: todos los clientes ruteados, sin duplicados");
    check(r.r2_ok,    "R2: asignación única por cliente");
    check(r.r3_ok,    "R3: rutas sólo en depósitos abiertos");
    check(r.r4_ok,    "R4: capacidad de depósito respetada");
    check(r.r5_ok,    "R5: capacidad de vehículo respetada");
    check(r.r6_ok,    "R6: coherencia id_deposito");
    check(r.factible, "Solución MA|PM es factible");
}

// ─────────────────────────────────────────────────────────────────────────────
// Caso 4: solución artificialmente infactible — detector debe encontrar fallos
// ─────────────────────────────────────────────────────────────────────────────
static void test_infactible(const InstanciaLRP &inst, const Matriz &mat) {
    std::cout << "\n=== Caso 4: Solución Infactible Artificial ===\n";

    // Partimos de la solución inicial y la corrompemos deliberadamente
    auto sol = generar_solucion_inicial(inst, mat);

    // Infactibilidad 1: mover primer cliente de cualquier ruta a no_asignados
    // (viola R1 — cliente huérfano)
    for (auto &[dep_id, lista] : sol.rutas) {
        for (auto &r : lista) {
            if (!r.clientes.empty()) {
                int c = r.clientes.front();
                r.clientes.erase(r.clientes.begin());
                sol.no_asignados.push_back(c);
                goto fin_extraccion;
            }
        }
    }
    fin_extraccion:;

    // Infactibilidad 2: duplicar ese mismo cliente en otra ruta (viola R1 — duplicado)
    if (!sol.no_asignados.empty()) {
        int c_dup = sol.no_asignados.back();
        for (auto &[dep_id, lista] : sol.rutas) {
            if (!lista.empty() && !lista[0].clientes.empty()) {
                lista[0].clientes.push_back(c_dup);
                break;
            }
        }
        // El cliente sigue en no_asignados Y está en una ruta → duplicado/huérfano
    }

    auto r = validar_solucion(sol, /*imprimir=*/true);

    // El validador debe detectar al menos una violación
    check(!r.factible, "Detector: solución infactible correctamente detectada");
    check(!r.r1_ok,    "Detector R1: huérfanos o duplicados detectados");
    check(r.costo_total > 0.0, "Costo total calculado (con penalizaciones)");
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Uso: ./test_validador <instancia.dat>\n";
        return 1;
    }

    auto inst = leer_instancia(argv[1]);
    auto mat  = construir_matriz_distancias(inst);

    test_sol_inicial(inst, mat);
    test_alns(inst, mat);
    test_mapm(inst, mat);
    test_infactible(inst, mat);

    std::cout << "\n";
    std::string(56, '=');
    if (n_fail == 0)
        std::cout << "[TEST VALIDADOR OK] Todos los " << n_ok << " checks pasaron.\n";
    else
        std::cout << "[TEST VALIDADOR] " << n_ok << " OK  |  " << n_fail << " FAIL\n";

    return (n_fail > 0) ? 1 : 0;
}
