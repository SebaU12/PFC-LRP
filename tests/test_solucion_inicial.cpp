#include "../src/distancia.h"
#include "../src/estado.h"
#include "../src/export_json.h"
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

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Uso: ./test_solucion_inicial <instancia.dat>\n";
    return 1;
  }

  auto inst = leer_instancia(argv[1]);
  auto m = construir_matriz_distancias(inst);
  auto sol = generar_solucion_inicial(inst, m);

  std::cout << "=== Test Fase 3: Solución Inicial ===\n\n";

  // ── 1. Sin clientes huérfanos ─────────────────────────────
  check(sol.no_asignados.empty(), "No hay clientes huérfanos");

  // ── 2. Todos los clientes están en alguna ruta ────────────
  std::set<int> en_rutas;
  for (const auto &[dep, lista] : sol.rutas)
    for (const auto &ruta : lista)
      for (int c : ruta.clientes)
        en_rutas.insert(c);

  check((int)en_rutas.size() == inst.num_clientes,
        "Todos los clientes están ruteados (" +
            std::to_string(en_rutas.size()) + "/" +
            std::to_string(inst.num_clientes) + ")");

  // ── 3. Ningún cliente duplicado ───────────────────────────
  int total_en_rutas = 0;
  for (const auto &[dep, lista] : sol.rutas)
    for (const auto &r : lista)
      total_en_rutas += (int)r.clientes.size();

  check(total_en_rutas == inst.num_clientes,
        "Sin clientes duplicados en rutas");

  // ── 4. Capacidad de vehículo respetada (salvo clientes con
  //       demanda individual > Q, que se penalizan en objective)
  int rutas_ok = 0, rutas_violadas = 0;
  for (const auto &[dep, lista] : sol.rutas) {
    for (const auto &r : lista) {
      double dem = demanda_ruta_externa(r, inst);
      if (dem > inst.Q) {
        // Solo es aceptable si la ruta tiene UN cliente
        // cuya demanda ya supera Q individualmente
        bool inevitable =
            (r.clientes.size() == 1 && inst.clientes[r.clientes[0] - 1].demanda > inst.Q);
        if (!inevitable) {
          std::cerr << "  [DIAG] Violación evitable — Dep " << dep
                    << " | demanda=" << dem << " > Q=" << inst.Q << " | "
                    << r.clientes.size() << " clientes\n";
        }
        rutas_violadas++;
      } else {
        rutas_ok++;
      }
    }
  }
  check(
      rutas_violadas == 0 ||
          [&]() {
            // Todas las violaciones deben ser de cliente único con demanda > Q
            for (const auto &[dep, lista] : sol.rutas)
              for (const auto &r : lista)
                if (demanda_ruta_externa(r, inst) > inst.Q)
                  if (!(r.clientes.size() == 1 &&
                        inst.clientes[r.clientes[0] - 1].demanda > inst.Q))
                    return false;
            return true;
          }(),
      "Violaciones de Q solo por clientes con demanda individual > Q");

  int clientes_super_Q = 0;
  for (const auto &c : inst.clientes)
    if (c.demanda > inst.Q)
      clientes_super_Q++;
  std::cout << "       (rutas válidas: " << rutas_ok
            << " | rutas con cliente > Q: " << rutas_violadas
            << " | clientes individuales > Q en instancia: " << clientes_super_Q
            << ")\n";

  // ── 5. Solo depósitos válidos tienen rutas ────────────────
  std::set<int> deps_validos(sol.depositos_abiertos.begin(),
                             sol.depositos_abiertos.end());
  bool deps_ok = true;
  for (const auto &[dep, lista] : sol.rutas)
    if (deps_validos.find(dep) == deps_validos.end())
      deps_ok = false;
  check(deps_ok, "Rutas solo asignadas a depósitos abiertos");

  // ── 6. Costo inicial es finito y positivo ─────────────────
  double costo = sol.objective();
  check(costo > 0 && costo < 1e12, "Costo inicial finito y positivo");

  // ── Resumen ───────────────────────────────────────────────
  int n_rutas = 0;
  for (const auto &[dep, lista] : sol.rutas)
    n_rutas += (int)lista.size();

  double dem_total = 0;
  for (const auto &c : inst.clientes)
    dem_total += c.demanda;
  int min_teorico = (int)std::ceil(dem_total / inst.Q);

  std::cout << "\n-- Resumen --\n";
  std::cout << "  Costo inicial:       " << costo << "\n";
  std::cout << "  Vehículos usados:    " << n_rutas << "\n";
  std::cout << "  Mínimo teórico:      " << min_teorico << "\n";
  std::cout << "  Depósitos abiertos:  " << sol.depositos_abiertos.size()
            << "\n";

  // ── Exportar JSON para visualización ─────────────────────
  std::string json_out = "solucion_inicial.json";
  exportar_solucion_json(sol, json_out);
  std::cout << "\nJSON exportado → " << json_out << "\n";
  std::cout << "\n[FASE 3 OK] Todos los tests pasaron.\n";
  return 0;
}
