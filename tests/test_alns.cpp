#include "../src/alns/operadores.h"
#include "../src/alns/solver.h"
#include "../src/distancia.h"
#include "../src/estado.h"
#include "../src/export_json.h"
#include "../src/parser.h"
#include "../src/solucion_inicial.h"
#include <iostream>
#include <set>

static void check(bool cond, const std::string &msg) {
  if (!cond) {
    std::cerr << "[FAIL] " << msg << "\n";
    std::exit(1);
  }
  std::cout << "[OK]   " << msg << "\n";
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Uso: ./test_alns <instancia.dat>\n";
    return 1;
  }

  auto inst = leer_instancia(argv[1]);
  auto mat = construir_matriz_distancias(inst);
  auto sol_inicial = generar_solucion_inicial(inst, mat);

  std::cout << "=== Test Fase 5: ALNS ===\n\n";

  // ── Configuración idéntica al main ───────────────────
  ConfigALNS cfg;
  cfg.temperatura_inicial = 50000.0;
  cfg.temperatura_final = 1.0;
  cfg.factor_enfriamiento = 0.998;
  cfg.factor_reaccion = 0.1;
  cfg.segmento = 100;
  cfg.sigma1 = 33.0;
  cfg.sigma2 = 9.0;
  cfg.sigma3 = 13.0;
  cfg.sigma4 = 1.0;
  cfg.max_iteraciones = 5000;
  cfg.semilla = 42;

  std::vector<OperadorFn> destroyers = {
      random_removal, worst_removal, shaw_removal, route_removal, depot_closing,
  };
  std::vector<OperadorFn> repairers = {
      greedy_repair,
      regret2_repair,
  };

  AlgoritmALNS algo(cfg, destroyers, repairers);

  double costo_inicial = sol_inicial.objective();
  auto resultado = algo.ejecutar(sol_inicial);
  const auto &mejor = resultado.mejor_estado;

  // ── 1. Mejora sobre la solución inicial ───────────────
  check(resultado.costo_final < costo_inicial,
        "ALNS mejora el costo inicial (" + std::to_string(costo_inicial) +
            " → " + std::to_string(resultado.costo_final) + ")");

  // ── 2. Sin clientes huérfanos ─────────────────────────
  check(mejor.no_asignados.empty(), "Solución final sin clientes huérfanos");

  // ── 3. Todos los clientes presentes exactamente una vez
  std::set<int> vistos;
  bool duplicado = false;
  for (const auto &[dep, lista] : mejor.rutas)
    for (const auto &r : lista)
      for (int c : r.clientes) {
        if (vistos.count(c)) {
          duplicado = true;
        }
        vistos.insert(c);
      }
  check(!duplicado, "Sin clientes duplicados en la solución final");
  check((int)vistos.size() == inst.num_clientes,
        "Todos los clientes están ruteados (" + std::to_string(vistos.size()) +
            "/" + std::to_string(inst.num_clientes) + ")");

  // ── 4. Depósitos válidos ──────────────────────────────
  std::set<int> abiertos(mejor.depositos_abiertos.begin(),
                         mejor.depositos_abiertos.end());
  bool deps_ok = true;
  for (const auto &[dep, lista] : mejor.rutas)
    if (!lista.empty() && !abiertos.count(dep))
      deps_ok = false;
  check(deps_ok, "Rutas solo en depósitos abiertos");

  // ── 5. Historial no vacío y monótono decreciente ──────
  check(!resultado.historial_mejoras.empty(), "Historial de mejoras no vacío");
  bool monotono = true;
  for (int i = 1; i < (int)resultado.historial_mejoras.size(); ++i)
    if (resultado.historial_mejoras[i].second >=
        resultado.historial_mejoras[i - 1].second)
      monotono = false;
  check(monotono, "Historial monótono decreciente");

  // ── Resumen ───────────────────────────────────────────
  int n_rutas = 0;
  for (const auto &[d, l] : mejor.rutas)
    n_rutas += (int)l.size();

  std::cout << "\n-- Resumen --\n";
  std::cout << "  Costo inicial:       " << costo_inicial << "\n";
  std::cout << "  Costo final:         " << resultado.costo_final << "\n";
  std::cout << "  Mejora total:        "
            << costo_inicial - resultado.costo_final << "\n";
  std::cout << "  Depósitos abiertos:  ";
  for (int d : mejor.depositos_abiertos)
    std::cout << d << " ";
  std::cout << "\n";
  std::cout << "  Vehículos usados:    " << n_rutas << "\n";
  std::cout << "  Mejoras encontradas: " << resultado.historial_mejoras.size()
            << "\n";

  std::cout << "\n-- Historial de mejoras --\n";
  for (auto &[it, costo] : resultado.historial_mejoras)
    std::cout << "  iter " << it << " → " << costo << "\n";

  // ── Exportar JSON ─────────────────────────────────────
  exportar_solucion_json(mejor, "solucion_alns_test.json");
  std::cout << "\nJSON exportado → solucion_alns_test.json\n";

  std::cout << "\n[FASE 5 OK] Todos los tests pasaron.\n";
  return 0;
}
