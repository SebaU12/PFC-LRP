#include "../src/alns/operadores.h"
#include "../src/distancia.h"
#include "../src/estado.h"
#include "../src/parser.h"
#include "../src/solucion_inicial.h"
#include <cmath>
#include <iostream>
#include <set>

static void check(bool cond, const std::string &msg) {
  if (!cond) {
    std::cerr << "[FAIL] " << msg << "\n";
    std::exit(1);
  }
  std::cout << "[OK]   " << msg << "\n";
}

// Verifica invariantes básicos de cualquier estado post-operador
static void check_integridad(const EstadoLRP &e, const std::string &nombre) {
  int N = e.datos->num_clientes;

  // Contar clientes únicos entre rutas + no_asignados
  std::set<int> vistos;
  bool ok = true;

  for (const auto &[dep, lista] : e.rutas)
    for (const auto &r : lista)
      for (int c : r.clientes) {
        if (vistos.count(c)) {
          std::cerr << "[FAIL] " << nombre << ": cliente " << c
                    << " duplicado en rutas\n";
          ok = false;
        }
        vistos.insert(c);
      }

  for (int c : e.no_asignados) {
    if (vistos.count(c)) {
      std::cerr << "[FAIL] " << nombre << ": cliente " << c
                << " está en rutas Y en no_asignados\n";
      ok = false;
    }
    vistos.insert(c);
  }

  if (!ok)
    std::exit(1);

  check((int)vistos.size() == N, nombre + ": total clientes conservado (" +
                                     std::to_string(vistos.size()) + "/" +
                                     std::to_string(N) + ")");

  // Rutas solo de depósitos abiertos
  std::set<int> abiertos(e.depositos_abiertos.begin(),
                         e.depositos_abiertos.end());
  bool deps_ok = true;
  for (const auto &[dep, lista] : e.rutas)
    if (!lista.empty() && !abiertos.count(dep))
      deps_ok = false;
  check(deps_ok, nombre + ": rutas solo en depósitos abiertos");
}

// Verifica que post-reparación no queden no_asignados
static void check_reparacion(const EstadoLRP &e, const std::string &nombre) {
  check(e.no_asignados.empty(),
        nombre + ": sin clientes huérfanos post-reparación");
  check_integridad(e, nombre);
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Uso: ./test_operadores <instancia.dat>\n";
    return 1;
  }

  auto inst = leer_instancia(argv[1]);
  auto m = construir_matriz_distancias(inst);
  auto sol = generar_solucion_inicial(inst, m);

  std::mt19937 rng(42);

  std::cout << "=== Test Fase 4: Operadores ===\n\n";

  // ── Destrucción ──────────────────────────────────────────
  std::cout << "-- Operadores de Destrucción --\n";

  auto test_destroy = [&](const std::string &nombre, auto fn) {
    auto destruido = fn(sol.copy(), rng);
    int liberados = (int)destruido.no_asignados.size();
    check(liberados > 0, nombre + ": libera al menos 1 cliente (" +
                             std::to_string(liberados) + " liberados)");
    check_integridad(destruido, nombre);
    return destruido;
  };

  auto d_random = test_destroy("random_removal", random_removal);
  auto d_worst = test_destroy("worst_removal", worst_removal);
  auto d_shaw = test_destroy("shaw_removal", shaw_removal);
  auto d_route = test_destroy("route_removal", route_removal);

  // depot_closing requiere >= 2 depósitos abiertos
  {
    auto destruido = depot_closing(sol.copy(), rng);
    int deps_antes = (int)sol.depositos_abiertos.size();
    int deps_despues = (int)destruido.depositos_abiertos.size();
    check(deps_despues == deps_antes - 1,
          "depot_closing: cierra exactamente 1 depósito");
    check_integridad(destruido, "depot_closing");
  }

  // ── Reparación ───────────────────────────────────────────
  std::cout << "\n-- Operadores de Reparación --\n";

  check_reparacion(greedy_repair(d_random.copy(), rng),
                   "greedy_repair(random)");
  check_reparacion(greedy_repair(d_worst.copy(), rng), "greedy_repair(worst)");
  check_reparacion(greedy_repair(d_shaw.copy(), rng), "greedy_repair(shaw)");
  check_reparacion(greedy_repair(d_route.copy(), rng), "greedy_repair(route)");

  check_reparacion(regret2_repair(d_random.copy(), rng),
                   "regret2_repair(random)");
  check_reparacion(regret2_repair(d_worst.copy(), rng),
                   "regret2_repair(worst)");
  check_reparacion(regret2_repair(d_shaw.copy(), rng), "regret2_repair(shaw)");
  check_reparacion(regret2_repair(d_route.copy(), rng),
                   "regret2_repair(route)");

  // ── Costo post destroy+repair ≤ costo inicial (no siempre,
  //    pero al menos debe ser finito y positivo) ─────────────
  std::cout << "\n-- Costos destroy+repair vs inicial --\n";
  double costo_base = sol.objective();
  auto pares = std::vector<std::pair<std::string, double>>{
      {"random+greedy", greedy_repair(d_random.copy(), rng).objective()},
      {"worst+greedy", greedy_repair(d_worst.copy(), rng).objective()},
      {"shaw+regret2", regret2_repair(d_shaw.copy(), rng).objective()},
      {"route+regret2", regret2_repair(d_route.copy(), rng).objective()},
  };
  for (auto &[nombre, costo] : pares) {
    check(costo > 0 && costo < 1e13,
          nombre + ": costo finito (" + std::to_string(costo) + ")");
    std::cout << "       costo base=" << costo_base << " | post=" << costo
              << " | delta=" << (costo - costo_base) << "\n";
  }

  std::cout << "\n[FASE 4 OK] Todos los tests pasaron.\n";
  return 0;
}
