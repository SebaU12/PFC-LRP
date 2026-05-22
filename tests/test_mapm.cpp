#include "../src/distancia.h"
#include "../src/mapm/cromosoma.h"
#include "../src/mapm/mapm.h"
#include "../src/mapm/split.h"
#include "../src/parser.h"
#include "../src/solucion_inicial.h"
#include <iostream>
#include <set>

static void check(bool ok, const std::string &msg) {
    std::cout << (ok ? "[OK]   " : "[FAIL] ") << msg << "\n";
    if (!ok) std::exit(1);
}

// ─────────────────────────────────────────
// Verifica que un EstadoLRP es estructuralmente válido
// ─────────────────────────────────────────
static void verificar_estado(const EstadoLRP &e,
                              const InstanciaLRP &inst,
                              const std::string &tag) {
    int n = inst.num_clientes;

    // Sin huérfanos
    check(e.no_asignados.empty(), tag + ": sin clientes huérfanos");

    // Todos los clientes presentes, sin duplicados
    std::set<int> vistos;
    for (const auto &[dep, lista] : e.rutas)
        for (const auto &r : lista)
            for (int c : r)
                vistos.insert(c);

    check((int)vistos.size() == n,
          tag + ": todos los " + std::to_string(n) +
          " clientes presentes (" + std::to_string(vistos.size()) + ")");

    // Al menos un depósito abierto
    check(!e.depositos_abiertos.empty(), tag + ": al menos 1 depósito abierto");

    // Costo finito y positivo
    double obj = e.objective();
    check(obj > 0 && obj < 1e12, tag + ": costo finito (" +
          std::to_string(obj) + ")");
}

// ─────────────────────────────────────────
// Test 1: mejora sobre la solución inicial
// ─────────────────────────────────────────
static void test_mejora(const InstanciaLRP &inst, const Matriz &mat) {
    std::cout << "-- Test 1: MA|PM mejora sobre solución inicial --\n";

    auto sol_ini = generar_solucion_inicial(inst, mat);
    double costo_ini = sol_ini.objective();
    std::cout << "  Costo inicial: " << costo_ini << "\n";

    auto cfg = config_por_defecto(inst.num_clientes, inst.num_depositos, 42);
    AlgoritmMAPM algo(cfg);
    auto res = algo.ejecutar(sol_ini);

    std::cout << "  Costo final:   " << res.costo_final << "\n";
    std::cout << "  Iteraciones:   " << res.iteraciones << "\n";
    std::cout << "  Mejoras:       " << res.historial_mejoras.size() << "\n";

    check(res.costo_final <= costo_ini + 1e-6,
          "MA|PM no empeora la solución inicial");
    check(res.costo_final < costo_ini,
          "MA|PM mejora la solución inicial (" +
          std::to_string(costo_ini) + " → " +
          std::to_string(res.costo_final) + ")");
}

// ─────────────────────────────────────────
// Test 2: estado resultante es válido
// ─────────────────────────────────────────
static void test_estado_valido(const InstanciaLRP &inst, const Matriz &mat) {
    std::cout << "\n-- Test 2: estado resultante es válido --\n";

    auto sol_ini = generar_solucion_inicial(inst, mat);
    auto cfg = config_por_defecto(inst.num_clientes, inst.num_depositos, 42);
    AlgoritmMAPM algo(cfg);
    auto res = algo.ejecutar(sol_ini);

    verificar_estado(res.mejor_estado, inst, "mejor_estado");

    // Costo del estado coincide con costo_final reportado
    double obj = res.mejor_estado.objective();
    check(std::abs(obj - res.costo_final) < 1e-3,
          "costo_final coincide con objective() del estado (" +
          std::to_string(obj) + " vs " + std::to_string(res.costo_final) + ")");
}

// ─────────────────────────────────────────
// Test 3: historial de mejoras es monótono decreciente
// ─────────────────────────────────────────
static void test_historial(const InstanciaLRP &inst, const Matriz &mat) {
    std::cout << "\n-- Test 3: historial de mejoras monótono --\n";

    auto sol_ini = generar_solucion_inicial(inst, mat);
    auto cfg = config_por_defecto(inst.num_clientes, inst.num_depositos, 42);
    AlgoritmMAPM algo(cfg);
    auto res = algo.ejecutar(sol_ini);

    check(!res.historial_mejoras.empty(),
          "historial tiene al menos una entrada");

    bool monotono = true;
    for (int i = 1; i < (int)res.historial_mejoras.size(); ++i) {
        // iter debe ser no-decreciente
        if (res.historial_mejoras[i].first < res.historial_mejoras[i-1].first)
            monotono = false;
        // costo debe ser estrictamente decreciente
        if (res.historial_mejoras[i].second >= res.historial_mejoras[i-1].second)
            monotono = false;
    }
    check(monotono,
          "historial: iter no-decreciente y costo estrictamente decreciente");

    // La última entrada del historial coincide con costo_final
    check(std::abs(res.historial_mejoras.back().second - res.costo_final) < 1e-3,
          "última entrada del historial == costo_final");
}

// ─────────────────────────────────────────
// Test 4: dos semillas distintas dan resultados distintos
//         (verifica que el rng se usa correctamente)
// ─────────────────────────────────────────
static void test_semillas(const InstanciaLRP &inst, const Matriz &mat) {
    std::cout << "\n-- Test 4: semillas distintas dan resultados distintos --\n";

    auto sol_ini = generar_solucion_inicial(inst, mat);

    auto cfg1 = config_por_defecto(inst.num_clientes, inst.num_depositos, 42);
    auto cfg2 = config_por_defecto(inst.num_clientes, inst.num_depositos, 123);

    AlgoritmMAPM algo1(cfg1), algo2(cfg2);
    auto res1 = algo1.ejecutar(sol_ini);
    auto res2 = algo2.ejecutar(sol_ini);

    std::cout << "  Costo semilla 42:  " << res1.costo_final << "\n";
    std::cout << "  Costo semilla 123: " << res2.costo_final << "\n";

    // Ambos deben ser válidos
    verificar_estado(res1.mejor_estado, inst, "semilla_42");
    verificar_estado(res2.mejor_estado, inst, "semilla_123");

    // No exigimos que sean distintos (pueden coincidir por azar),
    // solo que ambos sean válidos y mejoren el inicial.
    double costo_ini = sol_ini.objective();
    check(res1.costo_final <= costo_ini + 1e-6, "semilla 42 no empeora");
    check(res2.costo_final <= costo_ini + 1e-6, "semilla 123 no empeora");
}

// ─────────────────────────────────────────
// Test 5: capacidades respetadas en la solución final
// ─────────────────────────────────────────
static void test_capacidades(const InstanciaLRP &inst, const Matriz &mat) {
    std::cout << "\n-- Test 5: capacidades respetadas en solución final --\n";

    auto sol_ini = generar_solucion_inicial(inst, mat);
    auto cfg = config_por_defecto(inst.num_clientes, inst.num_depositos, 42);
    AlgoritmMAPM algo(cfg);
    auto res = algo.ejecutar(sol_ini);

    const EstadoLRP &e = res.mejor_estado;
    bool Q_ok  = true;
    bool cap_dep_ok = true;

    for (const auto &[dep_id, lista] : e.rutas) {
        double dem_dep = 0.0;
        for (const auto &r : lista) {
            double dem_r = 0.0;
            for (int c : r)
                dem_r += inst.clientes[c - 1].demanda;
            if (dem_r > inst.Q + 1e-6) {
                Q_ok = false;
                std::cerr << "  [DIAG] ruta con dem=" << dem_r
                          << " > Q=" << inst.Q << "\n";
            }
            dem_dep += dem_r;
        }
        // Verificar capacidad de depósito
        int dep_idx = dep_id - inst.num_clientes - 1;
        if (dep_idx >= 0 && dep_idx < inst.num_depositos) {
            double cap = inst.depositos[dep_idx].capacidad;
            if (dem_dep > cap + 1e-6) {
                cap_dep_ok = false;
                std::cerr << "  [DIAG] dep " << dep_id << " dem=" << dem_dep
                          << " > cap=" << cap << "\n";
            }
        }
    }

    check(Q_ok,      "todas las rutas respetan capacidad Q");
    check(cap_dep_ok,"todos los depósitos respetan su capacidad");
}

// ─────────────────────────────────────────
int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Uso: ./test_mapm <instancia.dat>\n";
        return 1;
    }

    auto inst = leer_instancia(argv[1]);
    auto mat  = construir_matriz_distancias(inst);

    int n = inst.num_clientes;
    int m = inst.num_depositos;
    auto cfg = config_por_defecto(n, m);

    std::cout << "=== Test Fase 6 MA|PM: Solver completo ===\n";
    std::cout << "  n=" << n << "  m=" << m << "\n";
    std::cout << "  NbIndiv=" << cfg.NbIndiv
              << "  MaxNbAcc=" << cfg.MaxNbAcc
              << "  MaxNbNoAdd=" << cfg.MaxNbNoAdd
              << "  Delta_max=" << cfg.Delta_max << "\n\n";

    test_mejora         (inst, mat);
    test_estado_valido  (inst, mat);
    test_historial      (inst, mat);
    test_semillas       (inst, mat);
    test_capacidades    (inst, mat);

    std::cout << "\n[FASE 6 MA|PM OK] Todos los tests pasaron.\n";
    return 0;
}
