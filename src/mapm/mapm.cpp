// =============================================================================
// mapm.cpp — Implementación del algoritmo MA|PM (Prins et al. 2006)
// =============================================================================
//
// Este archivo implementa el loop principal del MA|PM tal como se describe
// en el Algorithm 1 del paper. La estructura es:
//
//   INICIALIZACIÓN:
//     - Convertir la solución inicial greedy en cromosoma y evaluarla.
//     - Generar la población inicial con NN y CW (con búsqueda local LS1).
//
//   LOOP EXTERIOR (outer): termina cuando NbAcc > MaxNbAcc
//     Al inicio de cada ciclo exterior: resetear Δ y NbNoAdd.
//
//     LOOP INTERIOR (inner): termina cuando NbNoAdd > MaxNbNoAdd O NbAcc > MaxNbAcc
//       1. Seleccionar padres A y B (torneo).
//       2. Crossover → hijo C.
//       3. Aplicar LS1 (prob p1) o LS2 (prob p2*(1-p1)) al hijo.
//       4. Si C mejora BestCost → actualizar BestSoln.
//       5. Medir distancia de C a la población:
//            - Si dist < Δ: rechazar (C demasiado similar) → NbNoAdd++, NbRej++
//            - Si dist ≥ Δ: aceptar → add_in_pop, NbAcc++, NbRej=0
//       6. Si NbRej > MaxNbRej: bajar Δ en 1 (relajar criterio de diversidad).
//
//     Al salir del loop interior: reintroducir BestSoln en la población,
//     luego refrescar_poblacion (GenPop2: conservar el mejor, regenerar el resto).
//
//   AL TERMINAR: convertir BestSoln de cromosoma a EstadoLRP para devolver.
//
// =============================================================================
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

// Calcular parámetros por defecto en función del tamaño del problema.
// Las fórmulas vienen de la Tabla 1 del paper (Prins et al. 2006).
ConfigMAPM config_por_defecto(int n, int m, unsigned int semilla) {
  ConfigMAPM c;
  c.NbIndiv    = (n + m) / 5 + 1;
  c.MaxNbAcc   = (n + m) * 10;
  c.MaxNbNoAdd = static_cast<int>((n + m) * 2.5);
  c.MaxNbRej   = c.NbIndiv;
  c.Delta_max  = (n + m) / 10 + 10;
  c.beta       = std::max(1, c.NbIndiv / 3);
  c.p1         = 0.5;
  c.p2         = 0.7;
  c.alpha_pen  = 1000.0;
  c.semilla    = semilla;
  return c;
}

AlgoritmMAPM::AlgoritmMAPM(ConfigMAPM cfg) : cfg_(cfg) {}

ResultadoAlgoritmo AlgoritmMAPM::ejecutar(const EstadoLRP &sol_inicial) {
  const InstanciaLRP &inst = *sol_inicial.datos;
  const Matriz &mat = *sol_inicial.matriz;
  int n = inst.num_clientes;
  int m = inst.num_depositos;

  // Si NbIndiv==0, el usuario construyó la config vacía → usar defaults
  ConfigMAPM cfg = (cfg_.NbIndiv == 0)
                       ? config_por_defecto(n, m, cfg_.semilla)
                       : cfg_;

  std::mt19937 rng(cfg.semilla);
  std::uniform_real_distribution<double> u01(0.0, 1.0);

  // LS1 empaquetada como FnLocalSearch para pasarla a generar_poblacion/refrescar
  FnLocalSearch fn_ls1 = [&](CromosomaLRP &c, const Matriz &mat_,
                             const InstanciaLRP &inst_) {
    auto sr = evaluar_cromosoma(c, mat_, inst_);
    LS1(c, sr, mat_, inst_);
  };

  // Contadores del loop
  int NbAcc = 0;   // total de hijos aceptados en la población
  int NbRej = 0;   // rechazos consecutivos (para bajar Δ)
  int NbNoAdd = 0; // hijos consecutivos no añadidos (para refrescar)
  int Delta = cfg.Delta_max; // umbral de diversidad actual
  int iter = 0;    // contador de generaciones

  double BestCost = std::numeric_limits<double>::infinity();
  CromosomaLRP BestSoln;

  // Inicializar resultado con la solución de entrada
  ResultadoAlgoritmo res{
      sol_inicial.copy(),
      sol_inicial.objective(),
      sol_inicial.objective(),
      0,
      {}
  };

  // Convertir la solución inicial greedy en cromosoma y ver si ya es buen punto
  {
    CromosomaLRP ini = estado_a_cromosoma(sol_inicial);
    evaluar_cromosoma(ini, mat, inst);
    if (ini.fitness < BestCost) { BestCost = ini.fitness; BestSoln = ini; }
  }

  // Generar población inicial (con LS1 aplicado a cada individuo)
  std::vector<CromosomaLRP> pop =
      generar_poblacion(cfg.NbIndiv, inst, mat, rng, fn_ls1);

  for (const auto &c : pop)
    if (c.fitness < BestCost) { BestCost = c.fitness; BestSoln = c; }
  res.historial_mejoras.push_back({0, BestCost}); // punto de partida

  // ══ LOOP EXTERIOR ══════════════════════════════════════════════════════════
  // Cada iteración del loop exterior = un "ciclo de evolución" completo.
  // Termina cuando NbAcc supera MaxNbAcc (suficiente evolución acumulada).
  do {
    Delta = cfg.Delta_max; // reiniciar umbral de diversidad
    NbNoAdd = 0;

    // ── LOOP INTERIOR ───────────────────────────────────────────────────────
    // Genera hijos hasta que la población converja (NbNoAdd > MaxNbNoAdd)
    // o se haya acumulado suficiente evolución (NbAcc > MaxNbAcc).
    // Se usa OR para evitar loop infinito cuando todos los hijos son aceptados.
    do {
      ++iter;

      // Paso 1: seleccionar padres y cruzar
      int ia = seleccionar_padre_a(pop, cfg.beta, rng);
      int ib = seleccionar_padre_b(pop, ia, rng);
      CromosomaLRP C = crossover(pop[ia], pop[ib], mat, inst, rng);

      // Paso 2: aplicar búsqueda local al hijo (probabilísticamente)
      {
        auto sr = evaluar_cromosoma(C, mat, inst);
        double r = u01(rng);
        if (r < cfg.p1)
          LS1(C, sr, mat, inst);                          // búsqueda completa (50%)
        else if (r < cfg.p1 + (1.0 - cfg.p1) * cfg.p2)
          LS2(C, sr, mat, inst);                          // búsqueda rápida (35%)
        // sin búsqueda local (15%): el hijo entra sin mejorar
      }

      // Paso 3: actualizar mejor global si el hijo es mejor
      if (C.fitness < BestCost) {
        BestCost = C.fitness;
        BestSoln = C;
        Delta = cfg.Delta_max; // resetear Δ — hay diversidad nueva
        res.historial_mejoras.push_back({iter, BestCost});
      }

      // Paso 4: decidir si el hijo entra a la población
      // Medir cuán diferente es C de los individuos actuales.
      // Si es demasiado similar (dist < Δ), no añadirlo para preservar diversidad.
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
          ++NbRej;    // C demasiado similar a alguien de la población
          ++NbNoAdd;
        } else {
          NbRej = 0;
          ++NbAcc;
          add_in_pop(pop, C); // reemplaza al peor de la población
        }
      }

      // Paso 5: si muchos rechazos seguidos, relajar el umbral de diversidad
      if (NbRej > cfg.MaxNbRej)
        Delta = std::max(1, Delta - 1);

    } while (!(NbNoAdd > cfg.MaxNbNoAdd || NbAcc > cfg.MaxNbAcc));

    // Fin del loop interior: reinsertar el mejor conocido y refrescar la población
    add_in_pop(pop, BestSoln); // asegurar que BestSoln esté en la población
    refrescar_poblacion(pop, inst, mat, rng, fn_ls1); // GenPop2: conservar el mejor, regenerar el resto

    // El refresco puede haber generado individuos mejores que BestSoln
    for (const auto &c : pop) {
      if (c.fitness < BestCost) {
        BestCost = c.fitness;
        BestSoln = c;
        res.historial_mejoras.push_back({iter, BestCost});
      }
    }

  } while (NbAcc <= cfg.MaxNbAcc);

  // ══ CONVERTIR RESULTADO ════════════════════════════════════════════════════
  // BestSoln es un cromosoma. Necesitamos un EstadoLRP con rutas explícitas.
  // Re-ejecutar Split sobre BestSoln para obtener las rutas y convertir.
  auto sr_best = evaluar_cromosoma(BestSoln, mat, inst);
  Rutas rutas_cache;
  for (int i = 0; i < m; ++i) {
    if (BestSoln.DS[i] == 0) continue;
    int dep_id = n + i + 1; // convertir dep_idx (0-based) a dep_id (1-indexed)
    for (const auto &rv : sr_best[i].rutas)
      rutas_cache[dep_id].push_back(Ruta(dep_id, rv));
  }

  res.mejor_estado = cromosoma_a_estado(BestSoln, rutas_cache, mat, inst);
  res.costo_final  = BestCost;
  res.iteraciones  = iter;
  return res;
}
