// Validación de ALNS y MA|PM contra fuerza bruta en instancias pequeñas.
// Uso: ./validate <instancia.dat> [<instancia2.dat> ...]
//      ./validate benchmark/coord10-2-1.dat benchmark/coord10-2-2.dat benchmark/coord12-2-1.dat
#include "src/alns/operadores.h"
#include "src/alns/solver.h"
#include "src/brute_force/solver.h"
#include "src/distancia.h"
#include "src/mapm/mapm.h"
#include "src/parser.h"
#include "src/solucion_inicial.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

static std::unique_ptr<IAlgoritmo> crear_alns() {
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
  return std::make_unique<AlgoritmALNS>(
      cfg,
      std::vector<OperadorFn>{random_removal, worst_removal, shaw_removal,
                              route_removal, depot_closing},
      std::vector<OperadorFn>{greedy_repair, regret2_repair});
}

static double run(IAlgoritmo &algo, const EstadoLRP &sol_ini, double &cpu_s) {
  auto t0 = std::chrono::steady_clock::now();
  auto res = algo.ejecutar(sol_ini);
  cpu_s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  return res.costo_final;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Uso: ./validate <inst1.dat> [inst2.dat ...]\n";
    return 1;
  }

  std::ofstream csv("resultados_validacion.csv");
  csv << "instancia,n,m,bf_costo,bf_cpu_seg,"
         "alns_costo,alns_cpu_seg,alns_gap_pct,"
         "mapm_costo,mapm_cpu_seg,mapm_gap_pct\n";

  const int wn = 18, wc = 12, wg = 9, wt = 8;
  int total_w = wn + 3 * (wc + wg + wt) + 5;
  auto line = [&]() { std::cout << std::string(total_w, '-') << "\n"; };

  line();
  std::cout << std::left << std::setw(wn) << "Instancia" << std::right
            << std::setw(wc) << "BruteForce" << std::setw(wg) << "CPU(s)"
            << std::setw(wt) << "gap%"
            << std::setw(wc) << "ALNS" << std::setw(wg) << "CPU(s)"
            << std::setw(wt) << "gap%"
            << std::setw(wc) << "MA|PM" << std::setw(wg) << "CPU(s)"
            << std::setw(wt) << "gap%"
            << "\n";
  line();

  for (int i = 1; i < argc; ++i) {
    std::string ruta = argv[i];
    try {
      auto inst = leer_instancia(ruta);
      auto mat = construir_matriz_distancias(inst);
      auto sol_ini = generar_solucion_inicial(inst, mat);

      AlgoritmFuerzaBruta fb;
      auto algo_alns = crear_alns();
      ConfigMAPM cfg_mapm{};
      cfg_mapm.semilla = 42;
      AlgoritmMAPM algo_mapm(cfg_mapm);

      double cpu_fb, cpu_alns, cpu_mapm;
      double cost_fb   = run(fb,         sol_ini, cpu_fb);
      double cost_alns = run(*algo_alns, sol_ini, cpu_alns);
      double cost_mapm = run(algo_mapm,  sol_ini, cpu_mapm);

      auto gap = [&](double c) {
        return (cost_fb > 1e-6) ? (c - cost_fb) / cost_fb * 100.0 : 0.0;
      };

      std::string nombre = ruta;
      auto pos = ruta.find_last_of("/\\");
      if (pos != std::string::npos) nombre = ruta.substr(pos + 1);

      std::cout << std::left << std::setw(wn) << nombre << std::right
                << std::fixed << std::setprecision(0)
                << std::setw(wc) << cost_fb
                << std::setprecision(2)
                << std::setw(wg) << cpu_fb
                << std::setw(wt) << 0.0
                << std::setprecision(0)
                << std::setw(wc) << cost_alns
                << std::setprecision(2)
                << std::setw(wg) << cpu_alns
                << std::setprecision(2)
                << std::setw(wt) << gap(cost_alns)
                << std::setprecision(0)
                << std::setw(wc) << cost_mapm
                << std::setprecision(2)
                << std::setw(wg) << cpu_mapm
                << std::setprecision(2)
                << std::setw(wt) << gap(cost_mapm)
                << "\n";

      csv << std::fixed << std::setprecision(4)
          << nombre << "," << inst.num_clientes << "," << inst.num_depositos << ","
          << cost_fb  << "," << cpu_fb  << ","
          << cost_alns << "," << cpu_alns << "," << gap(cost_alns) << ","
          << cost_mapm << "," << cpu_mapm << "," << gap(cost_mapm) << "\n";
    } catch (const std::exception &e) {
      std::cerr << "ERROR en " << ruta << ": " << e.what() << "\n";
    }
  }

  line();
  std::cout << "gap% = (algoritmo - fuerza_bruta) / fuerza_bruta * 100\n";
  std::cout << "CSV exportado -> resultados_validacion.csv\n";
  return 0;
}
