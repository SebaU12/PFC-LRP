#include "src/alns/operadores.h"
#include "src/alns/solver.h"
#include "src/distancia.h"
#include "src/estado.h"
#include "src/export_json.h"
#include "src/parser.h"
#include "src/solucion_inicial.h"

#include <iostream>
#include <string>

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Uso: ./alns_lrp <instancia.dat>\n";
    return 1;
  }

  // ── 1. Lectura y construcción ─────────────────────────
  auto inst = leer_instancia(argv[1]);
  mostrar_resumen(inst);
  auto mat = construir_matriz_distancias(inst);
  auto sol_inicial = generar_solucion_inicial(inst, mat);

  std::cout << "Costo solución inicial: " << sol_inicial.objective() << "\n";
  std::cout << "Vehículos iniciales:    " << [&] {
    int n = 0;
    for (auto &[d, l] : sol_inicial.rutas)
      n += (int)l.size();
    return n;
  }() << "\n\n";

  // ── 2. Configuración del ALNS ─────────────────────────
  // Todos los parámetros están aquí — modifícalos libremente
  ConfigALNS cfg;

  // Simulated Annealing
  cfg.temperatura_inicial = 50000.0;
  cfg.temperatura_final = 1.0;
  cfg.factor_enfriamiento = 0.998;

  // Ruleta adaptativa
  cfg.factor_reaccion = 0.1;
  cfg.segmento = 100; // iters entre actualizaciones de pesos

  // Puntajes de recompensa (mismos que Python)
  cfg.sigma1 = 33.0; // nuevo óptimo global
  cfg.sigma2 = 9.0;  // mejora solución actual
  cfg.sigma3 = 13.0; // aceptada por SA
  cfg.sigma4 = 1.0;  // rechazada

  // Criterio de parada y semilla
  cfg.max_iteraciones = 5000;
  cfg.semilla = 42;

  // ── 3. Construcción del algoritmo ─────────────────────
  std::vector<OperadorFn> destroyers = {
      random_removal, worst_removal, shaw_removal, route_removal, depot_closing,
  };
  std::vector<OperadorFn> repairers = {
      greedy_repair,
      regret2_repair,
  };

  AlgoritmALNS algo(cfg, destroyers, repairers);

  // ── 4. Ejecutar ───────────────────────────────────────
  std::cout << "Ejecutando " << algo.nombre() << " (" << cfg.max_iteraciones
            << " iteraciones)...\n";
  auto resultado = algo.ejecutar(sol_inicial);

  // ── 5. Imprimir mejor solución con detalle de rutas ──
  resultado.mejor_estado.imprimir_solucion();

  // ── Reporte de resultados ─────────────────────────────
  int n_rutas = 0;
  for (const auto &[d, l] : resultado.mejor_estado.rutas)
    n_rutas += (int)l.size();

  std::cout << "\n── Resultado ──────────────────────────────\n";
  std::cout << "Costo inicial:       " << resultado.costo_inicial << "\n";
  std::cout << "Costo final:         " << resultado.costo_final << "\n";
  std::cout << "Mejora:              "
            << resultado.costo_inicial - resultado.costo_final << "\n";
  std::cout << "Depósitos abiertos:  ";
  for (int d : resultado.mejor_estado.depositos_abiertos)
    std::cout << d << " ";
  std::cout << "\nVehículos usados:    " << n_rutas << "\n";

  std::cout << "\n── Historial de mejoras ───────────────────\n";
  for (auto &[it, costo] : resultado.historial_mejoras)
    std::cout << "  iter " << it << " → " << costo << "\n";

  // ── 6. Diagnóstico de costo ───────────────────────────
  {
    const auto &e = resultado.mejor_estado;
    double z_apertura = 0, z_vehiculos = 0, z_distancia = 0;
    double z_pen_deposito = 0;

    for (int dep_id : e.depositos_abiertos) {
      const auto &dep = inst.depositos[dep_id - inst.num_clientes - 1];
      z_apertura += dep.costo_apertura;

      double dem_dep = 0;
      for (const auto &r : e.rutas.at(dep_id))
        for (int c : r.clientes)
          dem_dep += inst.clientes[c - 1].demanda;
      if (dem_dep > dep.capacidad)
        z_pen_deposito += 100000.0 * (dem_dep - dep.capacidad);
    }
    for (const auto &[dep_id, lista] : e.rutas) {
      int idx_dep = dep_id - 1;
      for (const auto &r : lista) {
        if (r.clientes.empty())
          continue;
        z_vehiculos += inst.F;
        z_distancia += mat[idx_dep][r.clientes.front() - 1];
        for (int i = 0; i + 1 < (int)r.clientes.size(); ++i)
          z_distancia += mat[r.clientes[i] - 1][r.clientes[i + 1] - 1];
        z_distancia += mat[r.clientes.back() - 1][idx_dep];
      }
    }
    double z_pen_huerfanos = 100000.0 * e.no_asignados.size();

    std::cout << "\n── Desglose del costo final ───────────────\n";
    std::cout << "  Apertura depósitos:  " << z_apertura << "\n";
    std::cout << "  Costo vehículos:     " << z_vehiculos << "\n";
    std::cout << "  Costo distancias:    " << z_distancia << "\n";
    std::cout << "  Pen. cap. depósito:  " << z_pen_deposito << "\n";
    std::cout << "  Pen. huérfanos:      " << z_pen_huerfanos << "\n";
    std::cout << "  TOTAL:               "
              << z_apertura + z_vehiculos + z_distancia + z_pen_deposito +
                     z_pen_huerfanos
              << "\n";

    // Verificar capacidad de depósitos
    std::cout << "\n── Capacidad depósitos ────────────────────\n";
    for (int dep_id : e.depositos_abiertos) {
      const auto &dep = inst.depositos[dep_id - inst.num_clientes - 1];
      double dem = 0;
      for (const auto &r : e.rutas.at(dep_id))
        for (int c : r.clientes)
          dem += inst.clientes[c - 1].demanda;
      std::cout << "  Dep " << dep_id << ": demanda=" << dem
                << " / cap=" << dep.capacidad
                << (dem > dep.capacidad ? "  *** VIOLACION ***" : "  OK")
                << "\n";
    }
  }

  // ── 7. Exportar JSON ──────────────────────────────────
  std::string json_out = "solucion_final.json";
  exportar_solucion_json(resultado.mejor_estado, json_out);
  std::cout << "\nJSON exportado → " << json_out << "\n";

  return 0;
}
