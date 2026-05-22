#include "mapm.h"
#include "cromosoma.h"
#include "distancia_sol.h"
#include "local_search.h"
#include "operadores_ga.h"
#include "poblacion.h"
#include "split.h"
#include <algorithm>
#include <limits>
#include <random>

// ═════════════════════════════════════════
// config_por_defecto — Tabla 1 del paper
// ═════════════════════════════════════════
ConfigMAPM config_por_defecto(int n, int m, unsigned int semilla) {
  ConfigMAPM c;
  c.NbIndiv = (n + m) / 5 + 1;
  c.MaxNbAcc = (n + m) * 10;
  c.MaxNbNoAdd = static_cast<int>((n + m) * 2.5);
  c.MaxNbRej = c.NbIndiv;
  c.Delta_max = (n + m) / 10 + 10;
  c.beta = std::max(1, c.NbIndiv / 3);
  c.p1 = 0.5;
  c.p2 = 0.7;
  c.alpha_pen = 1000.0;
  c.semilla = semilla;
  return c;
}

// ═════════════════════════════════════════
// AlgoritmMAPM::AlgoritmMAPM
// ═════════════════════════════════════════
AlgoritmMAPM::AlgoritmMAPM(ConfigMAPM cfg) : cfg_(cfg) {}

// ═════════════════════════════════════════
// AlgoritmMAPM::ejecutar
//
// Implementa exactamente el Algorithm 1 de Prins et al. 2006:
//
//   NbAcc := 0; NbRej := 0; Delta := Delta_max
//   GenPop(P)
//   repeat                              ← outer loop
//     repeat                            ← inner loop
//       Selection(A,B)
//       Crossover(A,B,C)
//       if random < p1 then LS1(C)
//       else if random < p2 then LS2(C)
//       if cost(C) < BestCost then ...
//       if dP(C) < Delta then NbRej++; NbNoAdd++
//       else NbRej:=0; NbAcc++; AddInPop(C)
//       if NbRej > MaxNbRej then Delta = max(1,Delta-1)
//     until (NbNoAdd > MaxNbNoAdd AND NbAcc > MaxNbAcc)
//     Delta := Delta_max; NbNoAdd := 0
//     GenPop2(P, BestSoln)             ← refrescar_poblacion
//   until (NbAcc > MaxNbAcc)
//   return BestSoln
// ═════════════════════════════════════════
ResultadoAlgoritmo AlgoritmMAPM::ejecutar(const EstadoLRP &sol_inicial) {
  const InstanciaLRP &inst = *sol_inicial.datos;
  const Matriz &mat = *sol_inicial.matriz;
  int n = inst.num_clientes;
  int m = inst.num_depositos;

  // Recalcular config con n,m reales si NbIndiv==0 (config vacía)
  ConfigMAPM cfg =
      (cfg_.NbIndiv == 0) ? config_por_defecto(n, m, cfg_.semilla) : cfg_;

  std::mt19937 rng(cfg.semilla);
  std::uniform_real_distribution<double> u01(0.0, 1.0);

  // ── FnLocalSearch wrappers ────────────────────────────────
  // LS1 y LS2 requieren split_res; lo construimos internamente.
  FnLocalSearch fn_ls1 = [&](CromosomaLRP &c, const Matriz &mat_,
                             const InstanciaLRP &inst_) {
    auto sr = evaluar_cromosoma(c, mat_, inst_);
    LS1(c, sr, mat_, inst_);
  };

  // ── Contadores del Algorithm 1 ────────────────────────────
  int NbAcc = 0;
  int NbRej = 0;
  int NbNoAdd = 0;
  int Delta = cfg.Delta_max;
  int iter = 0; // total offspring generados

  double BestCost = std::numeric_limits<double>::infinity();
  CromosomaLRP BestSoln;

  // ResultadoAlgoritmo no tiene constructor por defecto porque EstadoLRP
  // tampoco lo tiene. Lo inicializamos con sol_inicial como mejor_estado
  // provisional; se sobreescribe al final con la solución real.
  ResultadoAlgoritmo res{
      sol_inicial.copy(),      // mejor_estado provisional
      sol_inicial.objective(), // costo_inicial
      sol_inicial.objective(), // costo_final provisional
      0,                       // iteraciones
      {}                       // historial_mejoras
  };

  // ── Incorporar sol_inicial como candidato al best ─────────
  {
    CromosomaLRP ini = estado_a_cromosoma(sol_inicial);
    evaluar_cromosoma(ini, mat, inst);
    if (ini.fitness < BestCost) {
      BestCost = ini.fitness;
      BestSoln = ini;
    }
  }

  // ── GenPop — Sección 4.1 ─────────────────────────────────
  // "first half NN + LS1, second half ECWA + LS1, all distinct (Δ=1)"
  std::vector<CromosomaLRP> pop =
      generar_poblacion(cfg.NbIndiv, inst, mat, rng, fn_ls1);

  // Actualizar best con la población inicial y registrar en historial.
  // iter=0 representa el estado post-GenPop (antes del primer offspring).
  for (const auto &c : pop) {
    if (c.fitness < BestCost) {
      BestCost = c.fitness;
      BestSoln = c;
    }
  }
  // Registrar siempre el best inicial como punto de partida del historial
  res.historial_mejoras.push_back({0, BestCost});

  // ── Outer loop ────────────────────────────────────────────
  // "repeat ... until (NbAcc > MaxNbAcc)"
  do {
    Delta = cfg.Delta_max; // línea 36 del Algorithm 1
    NbNoAdd = 0;           // línea 37

    // ── Inner loop ────────────────────────────────────────
    // "repeat ... until (NbNoAdd > MaxNbNoAdd AND NbAcc > MaxNbAcc)"
    do {
      ++iter;

      // Selection(A, B) — Sección 3.2
      int ia = seleccionar_padre_a(pop, cfg.beta, rng);
      int ib = seleccionar_padre_b(pop, ia, rng);

      // Crossover(A, B, C) — Sección 3.2
      CromosomaLRP C = crossover(pop[ia], pop[ib], mat, inst, rng);

      // Búsqueda local — Sección 3.3
      // "if random < p1 → LS1(C)
      //  else if random < p2 → LS2(C)"
      // P(LS1) = p1 = 0.5
      // P(LS2 | no LS1) = p2 = 0.7  → P(LS2) = (1-p1)*p2 = 0.35
      {
        auto sr = evaluar_cromosoma(C, mat, inst);
        double r = u01(rng);
        if (r < cfg.p1) {
          LS1(C, sr, mat, inst);
        } else if (r < cfg.p1 + (1.0 - cfg.p1) * cfg.p2) {
          LS2(C, sr, mat, inst);
        }
        // else: sin LS
      }

      // ¿Nuevo best? — líneas 19-23 del Algorithm 1
      if (C.fitness < BestCost) {
        BestCost = C.fitness;
        BestSoln = C;
        Delta = cfg.Delta_max;
        res.historial_mejoras.push_back({iter, BestCost});
      }

      // Population management — Sección 3.4
      {
        auto sr_c = evaluar_cromosoma(C, mat, inst);
        auto re_c = construir_rutas_explicitas(C, sr_c, mat, inst);

        std::vector<RutasExplicitas> pop_rutas;
        pop_rutas.reserve(pop.size());
        for (auto &p : pop) {
          auto sr_p = evaluar_cromosoma(p, mat, inst);
          pop_rutas.push_back(construir_rutas_explicitas(p, sr_p, mat, inst));
        }

        int d_pop = distancia_poblacion(re_c, pop_rutas, inst);

        if (d_pop < Delta) {
          // Rechazar — líneas 25-26
          ++NbRej;
          ++NbNoAdd;
        } else {
          // Aceptar — líneas 28-30
          NbRej = 0;
          ++NbAcc;
          add_in_pop(pop, C);
        }
      }

      // Ajuste dinámico de Delta — líneas 32-34
      if (NbRej > cfg.MaxNbRej)
        Delta = std::max(1, Delta - 1);

    } while (!(NbNoAdd > cfg.MaxNbNoAdd && NbAcc > cfg.MaxNbAcc));

    // GenPop2(P, BestSoln) — línea 38
    // "Every individual except the best is replaced"
    // Forzamos que BestSoln esté en la población antes de refrescar
    add_in_pop(pop, BestSoln);
    refrescar_poblacion(pop, inst, mat, rng, fn_ls1);

    // Actualizar best con la nueva población
    for (const auto &c : pop) {
      if (c.fitness < BestCost) {
        BestCost = c.fitness;
        BestSoln = c;
        res.historial_mejoras.push_back({iter, BestCost});
      }
    }

  } while (NbAcc <= cfg.MaxNbAcc);

  // ── Construir EstadoLRP de retorno ────────────────────────
  auto sr_best = evaluar_cromosoma(BestSoln, mat, inst);
  Rutas rutas_cache;
  for (int i = 0; i < m; ++i) {
    if (BestSoln.DS[i] == 0)
      continue;
    int dep_id = n + i + 1;
    rutas_cache[dep_id] = sr_best[i].rutas;
  }

  res.mejor_estado = cromosoma_a_estado(BestSoln, rutas_cache, mat, inst);
  res.costo_final = BestCost;
  res.iteraciones = iter;

  return res;
}
