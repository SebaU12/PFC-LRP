#include "src/algoritmo.h"
#include "src/alns/operadores.h"
#include "src/alns/solver.h"
#include "src/distancia.h"
#include "src/estado.h"
#include "src/mapm/mapm.h"
#include "src/parser.h"
#include "src/solucion_inicial.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ─────────────────────────────────────────
// Leer BKS desde CSV
// ─────────────────────────────────────────
static std::map<std::string, double> leer_bks(const std::string &ruta_csv) {
  std::map<std::string, double> bks;
  std::ifstream f(ruta_csv);
  if (!f.is_open()) {
    std::cerr << "[WARN] No se pudo abrir BKS: " << ruta_csv << "\n";
    return bks;
  }
  std::string linea;
  std::getline(f, linea); // saltar header
  while (std::getline(f, linea)) {
    if (linea.empty() || linea[0] == '#')
      continue;
    if (!linea.empty() && linea.back() == '\r')
      linea.pop_back();
    std::istringstream ss(linea);
    std::string archivo;
    double valor;
    if (std::getline(ss, archivo, ',') && ss >> valor)
      bks[archivo] = valor;
  }
  return bks;
}

// ─────────────────────────────────────────
// Descomponer costo en Cd y Cr
// ─────────────────────────────────────────
static void descomponer_costo(const EstadoLRP &e, double &cd, double &cr) {
  cd = 0.0;
  cr = 0.0;
  const InstanciaLRP &inst = *e.datos;
  const Matriz &mat = *e.matriz;

  for (int dep_id : e.depositos_abiertos) {
    const auto &dep = inst.depositos[dep_id - inst.num_clientes - 1];
    cd += dep.costo_apertura;
  }
  for (const auto &[dep_id, lista] : e.rutas) {
    int idx_dep = dep_id - 1;
    for (const auto &r : lista) {
      if (r.empty())
        continue;
      cr += inst.F;
      cr += mat[idx_dep][r.front() - 1];
      for (int i = 0; i + 1 < (int)r.size(); ++i)
        cr += mat[r[i] - 1][r[i + 1] - 1];
      cr += mat[r.back() - 1][idx_dep];
    }
  }
}

// ─────────────────────────────────────────
// Resultado por instancia (un algoritmo)
// ─────────────────────────────────────────
struct ResultadoInstancia {
  std::string nombre;
  int num_clientes;
  int num_depositos;
  double costo_inicial;
  double costo_final;
  double cd;
  double cr;
  double bks;
  double gap_bks;
  double tiempo_seg;
  int nb_dep;
  int nb_veh;
  bool factible;
};

// ─────────────────────────────────────────
// Ejecutar un algoritmo sobre una instancia
// ─────────────────────────────────────────
static ResultadoInstancia
procesar_instancia(const std::string &ruta_dat,
                   const std::map<std::string, double> &bks_map,
                   IAlgoritmo &algo) {
  ResultadoInstancia res;
  res.bks = -1.0;
  res.gap_bks = -1.0;

  auto inst = leer_instancia(ruta_dat);
  res.nombre = inst.nombre;
  res.num_clientes = inst.num_clientes;
  res.num_depositos = inst.num_depositos;

  auto it = bks_map.find(inst.nombre);
  if (it != bks_map.end())
    res.bks = it->second;

  auto mat = construir_matriz_distancias(inst);
  auto sol_inicial = generar_solucion_inicial(inst, mat);
  res.costo_inicial = sol_inicial.objective();

  auto t0 = std::chrono::steady_clock::now();
  auto resultado = algo.ejecutar(sol_inicial);
  auto t1 = std::chrono::steady_clock::now();

  res.costo_final = resultado.costo_final;
  res.tiempo_seg = std::chrono::duration<double>(t1 - t0).count();

  const auto &mejor = resultado.mejor_estado;
  descomponer_costo(mejor, res.cd, res.cr);

  res.nb_dep = (int)mejor.depositos_abiertos.size();
  res.nb_veh = 0;
  for (const auto &[d, l] : mejor.rutas)
    res.nb_veh += (int)l.size();
  res.factible = mejor.no_asignados.empty();

  if (res.bks > 0)
    res.gap_bks = (res.costo_final - res.bks) / res.bks * 100.0;

  return res;
}

// ─────────────────────────────────────────
// Imprimir tabla — un solo algoritmo
// ─────────────────────────────────────────
static void imprimir_tabla(const std::vector<ResultadoInstancia> &resultados) {
  const int wn = 22, wc = 5, wd = 5, wv = 5, wcd = 10, wcr = 10, wcost = 12,
            wbks = 10, wgap = 8, wt = 7;
  int total = wn + wc + wd + wv + wcd + wcr + wcost + wbks + wgap + wt + 22;

  auto line = [&]() { std::cout << std::string(total, '-') << "\n"; };

  line();
  std::cout << std::left << std::setw(wn) << "Instancia" << std::right
            << std::setw(wc) << "n" << std::setw(wd) << "dep" << std::setw(wv)
            << "veh" << std::setw(wcd) << "Cd" << std::setw(wcr) << "Cr"
            << std::setw(wcost) << "Costo" << std::setw(wbks) << "BKS"
            << std::setw(wgap) << "Gap%" << std::setw(wt) << "CPU(s)" << "\n";
  line();

  double gap_sum = 0.0;
  int gap_cnt = 0;
  for (const auto &r : resultados) {
    std::cout << std::left << std::setw(wn) << r.nombre << std::right
              << std::setw(wc) << r.num_clientes << std::setw(wd) << r.nb_dep
              << std::setw(wv) << r.nb_veh << std::setw(wcd) << (long long)r.cd
              << std::setw(wcr) << (long long)r.cr << std::setw(wcost)
              << (long long)r.costo_final;
    if (r.bks > 0) {
      std::cout << std::setw(wbks) << (long long)r.bks << std::fixed
                << std::setprecision(2) << std::setw(wgap) << r.gap_bks;
      gap_sum += r.gap_bks;
      ++gap_cnt;
    } else {
      std::cout << std::setw(wbks) << "N/A" << std::setw(wgap) << "N/A";
    }
    std::cout << std::fixed << std::setprecision(1) << std::setw(wt)
              << r.tiempo_seg << (r.factible ? "" : " [INF]") << "\n";
  }
  line();
  if (gap_cnt > 0)
    std::cout << "Gap/BKS promedio: " << std::fixed << std::setprecision(2)
              << gap_sum / gap_cnt << "% (" << gap_cnt << " instancias)\n";
}

// ─────────────────────────────────────────
// Imprimir tabla comparativa — ambos algoritmos
// ─────────────────────────────────────────
static void
imprimir_tabla_comparativa(const std::vector<ResultadoInstancia> &alns,
                           const std::vector<ResultadoInstancia> &mapm) {

  const int wn = 22, wc = 5, wf = 12, wg = 8, wt = 7;
  // Columnas: instancia | n | ALNS_costo | ALNS_gap | ALNS_cpu |
  //                        | MAPM_costo | MAPM_gap | MAPM_cpu | ganador
  int total = wn + wc + (wf + wg + wt) * 2 + 10;

  auto line = [&]() { std::cout << std::string(total, '-') << "\n"; };

  line();
  std::cout << std::left << std::setw(wn) << "Instancia" << std::right
            << std::setw(wc) << "n" << std::setw(wf) << "ALNS" << std::setw(wg)
            << "Gap%" << std::setw(wt) << "CPU(s)" << std::setw(wf) << "MA|PM"
            << std::setw(wg) << "Gap%" << std::setw(wt) << "CPU(s)"
            << "  Mejor\n";
  line();

  double gap_alns = 0, gap_mapm = 0;
  int cnt = 0;
  int wins_alns = 0, wins_mapm = 0, ties = 0;

  for (int i = 0; i < (int)alns.size() && i < (int)mapm.size(); ++i) {
    const auto &a = alns[i];
    const auto &m = mapm[i];

    std::cout << std::left << std::setw(wn) << a.nombre << std::right
              << std::setw(wc) << a.num_clientes;

    // ALNS
    std::cout << std::setw(wf) << (long long)a.costo_final;
    if (a.bks > 0)
      std::cout << std::fixed << std::setprecision(2) << std::setw(wg)
                << a.gap_bks;
    else
      std::cout << std::setw(wg) << "N/A";
    std::cout << std::fixed << std::setprecision(1) << std::setw(wt)
              << a.tiempo_seg;

    // MA|PM
    std::cout << std::setw(wf) << (long long)m.costo_final;
    if (m.bks > 0)
      std::cout << std::fixed << std::setprecision(2) << std::setw(wg)
                << m.gap_bks;
    else
      std::cout << std::setw(wg) << "N/A";
    std::cout << std::fixed << std::setprecision(1) << std::setw(wt)
              << m.tiempo_seg;

    // Ganador
    double diff = a.costo_final - m.costo_final;
    if (diff > 1e-3) {
      std::cout << "  MA|PM";
      ++wins_mapm;
    } else if (diff < -1e-3) {
      std::cout << "  ALNS";
      ++wins_alns;
    } else {
      std::cout << "  =";
      ++ties;
    }
    std::cout << (a.factible && m.factible ? "" : " [INF]") << "\n";

    if (a.bks > 0 && m.bks > 0) {
      gap_alns += a.gap_bks;
      gap_mapm += m.gap_bks;
      ++cnt;
    }
  }

  line();
  std::cout << "Victorias — ALNS: " << wins_alns << "  MA|PM: " << wins_mapm
            << "  Empates: " << ties << "\n";
  if (cnt > 0)
    std::cout << "Gap/BKS promedio — ALNS: " << std::fixed
              << std::setprecision(2) << gap_alns / cnt
              << "%   MA|PM: " << gap_mapm / cnt << "%\n";
}

// ─────────────────────────────────────────
// Exportar CSV — un algoritmo
// ─────────────────────────────────────────
static void exportar_csv(const std::vector<ResultadoInstancia> &resultados,
                         const std::string &ruta) {
  std::ofstream f(ruta);
  f << "instancia,n,m,costo_inicial,cd,cr,costo_final,"
       "bks,gap_bks_pct,cpu_seg,nb_dep,nb_veh,factible\n";
  for (const auto &r : resultados) {
    f << r.nombre << "," << r.num_clientes << "," << r.num_depositos << ","
      << std::fixed << std::setprecision(2) << r.costo_inicial << "," << r.cd
      << "," << r.cr << "," << r.costo_final << "," << r.bks << "," << r.gap_bks
      << "," << r.tiempo_seg << "," << r.nb_dep << "," << r.nb_veh << ","
      << (r.factible ? "1" : "0") << "\n";
  }
  std::cout << "CSV exportado → " << ruta << "\n";
}

// ─────────────────────────────────────────
// Exportar CSV comparativo — ambos algoritmos
// ─────────────────────────────────────────
static void
exportar_csv_comparativo(const std::vector<ResultadoInstancia> &alns,
                         const std::vector<ResultadoInstancia> &mapm,
                         const std::string &ruta) {
  std::ofstream f(ruta);
  f << "instancia,n,m,bks,"
       "alns_costo,alns_gap_pct,alns_cpu_seg,alns_nb_dep,alns_nb_veh,"
       "mapm_costo,mapm_gap_pct,mapm_cpu_seg,mapm_nb_dep,mapm_nb_veh,"
       "mejor\n";
  for (int i = 0; i < (int)alns.size() && i < (int)mapm.size(); ++i) {
    const auto &a = alns[i];
    const auto &m = mapm[i];
    std::string mejor = (a.costo_final < m.costo_final - 1e-3)   ? "ALNS"
                        : (m.costo_final < a.costo_final - 1e-3) ? "MAPM"
                                                                 : "empate";
    f << a.nombre << "," << a.num_clientes << "," << a.num_depositos << ","
      << std::fixed << std::setprecision(2) << a.bks << "," << a.costo_final
      << "," << a.gap_bks << "," << a.tiempo_seg << "," << a.nb_dep << ","
      << a.nb_veh << "," << m.costo_final << "," << m.gap_bks << ","
      << m.tiempo_seg << "," << m.nb_dep << "," << m.nb_veh << "," << mejor
      << "\n";
  }
  std::cout << "CSV comparativo exportado → " << ruta << "\n";
}

// ─────────────────────────────────────────
// Constructores de algoritmos
// ─────────────────────────────────────────
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

  std::vector<OperadorFn> destroyers = {
      random_removal, worst_removal, shaw_removal, route_removal, depot_closing,
  };
  std::vector<OperadorFn> repairers = {greedy_repair, regret2_repair};

  return std::make_unique<AlgoritmALNS>(cfg, destroyers, repairers);
}

static std::unique_ptr<IAlgoritmo> crear_mapm(int n, int m) {
  auto cfg = config_por_defecto(n, m, 42);
  return std::make_unique<AlgoritmMAPM>(cfg);
}

// ─────────────────────────────────────────
// main
// ─────────────────────────────────────────
int main(int argc, char *argv[]) {
  if (argc < 3) {
    std::cerr
        << "Uso: ./benchmark <dir_instancias> <bks.csv> [alns|mapm|ambos]\n";
    std::cerr << "Ej:  ./benchmark ../benchmark ../bks.csv ambos\n";
    return 1;
  }

  std::string dir_instancias = argv[1];
  std::string ruta_bks = argv[2];
  std::string modo = (argc >= 4) ? argv[3] : "alns";

  if (modo != "alns" && modo != "mapm" && modo != "ambos") {
    std::cerr << "Modo inválido: '" << modo << "'. Usar alns | mapm | ambos\n";
    return 1;
  }

  // ── Cargar BKS ──────────────────────────────────────────
  auto bks_map = leer_bks(ruta_bks);
  std::cout << "BKS cargadas: " << bks_map.size() << " instancias\n";

  // ── Listar instancias ────────────────────────────────────
  std::vector<std::string> archivos;
  for (const auto &e : fs::directory_iterator(dir_instancias))
    if (e.path().extension() == ".dat")
      archivos.push_back(e.path().string());
  std::sort(archivos.begin(), archivos.end());
  std::cout << "Instancias:  " << archivos.size() << "\n";
  std::cout << "Modo:        " << modo << "\n\n";

  // ── Modo: un solo algoritmo ──────────────────────────────
  if (modo != "ambos") {
    // Para MA|PM necesitamos n,m de la primera instancia para construir cfg.
    // config_por_defecto se recalcula internamente en ejecutar() si NbIndiv==0,
    // pero aquí construimos el algo una vez y lo reutilizamos: pasamos n=0,m=0
    // para que ejecutar() lo recalcule por instancia.
    std::unique_ptr<IAlgoritmo> algo;
    if (modo == "alns") {
      algo = crear_alns();
    } else {
      // NbIndiv=0 señala a ejecutar() que debe recalcular config
      ConfigMAPM cfg{};
      cfg.semilla = 42;
      algo = std::make_unique<AlgoritmMAPM>(cfg);
    }

    std::cout << "Algoritmo: " << algo->nombre() << "\n\n";

    std::vector<ResultadoInstancia> resultados;
    int idx = 1;
    for (const auto &ruta : archivos) {
      std::string nombre = fs::path(ruta).filename().string();
      std::cout << "[" << std::setw(2) << idx++ << "/" << archivos.size()
                << "] " << std::left << std::setw(24) << nombre << std::flush;
      try {
        auto res = procesar_instancia(ruta, bks_map, *algo);
        std::cout << "cost=" << std::setw(10) << (long long)res.costo_final
                  << " Cd=" << std::setw(8) << (long long)res.cd
                  << " Cr=" << std::setw(8) << (long long)res.cr;
        if (res.bks > 0)
          std::cout << " gap=" << std::fixed << std::setprecision(2)
                    << res.gap_bks << "%";
        std::cout << " (" << std::fixed << std::setprecision(1)
                  << res.tiempo_seg << "s)\n";
        resultados.push_back(res);
      } catch (const std::exception &e) {
        std::cerr << "ERROR: " << e.what() << "\n";
      }
    }

    std::cout << "\n";
    imprimir_tabla(resultados);
    exportar_csv(resultados, "resultados_" + modo + ".csv");
    return 0;
  }

  // ── Modo: ambos ──────────────────────────────────────────
  std::vector<ResultadoInstancia> res_alns, res_mapm;
  int idx = 1;

  for (const auto &ruta : archivos) {
    std::string nombre = fs::path(ruta).filename().string();
    std::cout << "[" << std::setw(2) << idx++ << "/" << archivos.size() << "] "
              << std::left << std::setw(24) << nombre << std::flush;

    try {
      auto inst = leer_instancia(ruta);
      int n = inst.num_clientes, m = inst.num_depositos;

      auto algo_alns = crear_alns();
      auto algo_mapm = crear_mapm(n, m);

      auto ra = procesar_instancia(ruta, bks_map, *algo_alns);
      auto rm = procesar_instancia(ruta, bks_map, *algo_mapm);

      std::cout << "ALNS=" << std::setw(8) << (long long)ra.costo_final << " ("
                << std::fixed << std::setprecision(1) << ra.tiempo_seg << "s)"
                << "  MAPM=" << std::setw(8) << (long long)rm.costo_final
                << " (" << std::fixed << std::setprecision(1) << rm.tiempo_seg
                << "s)";
      double diff = ra.costo_final - rm.costo_final;
      if (diff > 1e-3)
        std::cout << "  → MA|PM gana";
      else if (diff < -1e-3)
        std::cout << "  → ALNS gana";
      else
        std::cout << "  → empate";
      std::cout << "\n";

      res_alns.push_back(ra);
      res_mapm.push_back(rm);
    } catch (const std::exception &e) {
      std::cerr << "ERROR: " << e.what() << "\n";
    }
  }

  std::cout << "\n";
  imprimir_tabla_comparativa(res_alns, res_mapm);
  exportar_csv_comparativo(res_alns, res_mapm, "resultados_comparativo.csv");
  return 0;
}
