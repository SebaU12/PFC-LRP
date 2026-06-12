// =============================================================================
// cromosoma.cpp — Operaciones sobre CromosomaLRP
// =============================================================================
//
// Aquí viven tres grupos de operaciones:
//   1. Navegación del cromosoma (rango_cs, clientes_de)
//   2. Reparación post-crossover (repair)
//   3. Conversiones entre cromosoma y EstadoLRP
//
// =============================================================================
#include "cromosoma.h"
#include <algorithm>
#include <limits>
#include <stdexcept>

// -----------------------------------------------------------------------------
// Calcula el rango [inicio, fin) 0-based en CS que corresponde al depósito dep_idx.
//
// DS[dep_idx] es la posición 1-based donde empieza su sublista en CS.
// El fin es la posición de inicio del siguiente depósito abierto con DS > DS[dep_idx],
// o n (total de clientes) si dep_idx es el último depósito abierto.
//
// Ejemplo: DS = [3, 1, 0], n = 4
//   rango_cs(0) → inicio=2, fin=4  (posiciones 2..3 en CS)
//   rango_cs(1) → inicio=0, fin=2  (posiciones 0..1 en CS)
// -----------------------------------------------------------------------------
std::pair<int, int> CromosomaLRP::rango_cs(int dep_idx,
                                           const InstanciaLRP &inst) const {
  if (DS[dep_idx] == 0)
    return {0, 0}; // depósito cerrado — no tiene rango

  int inicio = DS[dep_idx] - 1; // convertir 1-based a 0-based
  int n = inst.num_clientes;
  int fin = n; // por defecto, va hasta el final de CS

  // Buscar el depósito abierto cuya sublista empiece justo después de inicio
  for (int j = 0; j < (int)DS.size(); ++j) {
    if (j == dep_idx || DS[j] == 0) continue;
    int ini_j = DS[j] - 1;
    if (ini_j > inicio && ini_j < fin)
      fin = ini_j;
  }

  if (fin == inicio) return {inicio, inicio}; // sublista vacía
  return {inicio, fin};
}

// Extraer la sublista de clientes de un depósito directamente desde CS.
std::vector<int> CromosomaLRP::clientes_de(int dep_idx,
                                           const InstanciaLRP &inst) const {
  auto [ini, fin] = rango_cs(dep_idx, inst);
  return std::vector<int>(CS.begin() + ini, CS.begin() + fin);
}

// -----------------------------------------------------------------------------
// Repara un cromosoma que pudo quedar infactible después del crossover.
//
// El crossover mezcla DS de los dos padres, lo que puede generar dos problemas:
//   1. DS quedó todo en cero → nadie atiende a los clientes.
//   2. Algún depósito tiene más demanda asignada que su capacidad W_i.
//
// Este repair es iterativo: si al mover un cliente de un depósito sobrecargado
// a otro se genera una nueva sobrecarga, el bucle lo detecta y corrige.
// Termina cuando ningún depósito viola su capacidad.
//
// La invariante que siempre mantiene: CS tiene exactamente n clientes
// (todos los clientes, sin duplicados ni faltantes).
// -----------------------------------------------------------------------------
void repair(CromosomaLRP &crom, const Matriz &mat, const InstanciaLRP &inst) {
  int n = inst.num_clientes;
  int m = inst.num_depositos;

  // Paso 1: si todos los depósitos están cerrados, abrir el más barato
  bool alguno_abierto = false;
  for (int v : crom.DS)
    if (v != 0) { alguno_abierto = true; break; }

  if (!alguno_abierto) {
    int mejor = 0;
    for (int i = 1; i < m; ++i)
      if (inst.depositos[i].costo_apertura < inst.depositos[mejor].costo_apertura)
        mejor = i;
    crom.DS[mejor] = 1; // abre el depósito más barato y pone todos los clientes ahí
  }

  // Paso 2: corregir depósitos sobrecargados
  bool hubo_cambio = true;
  while (hubo_cambio) {
    hubo_cambio = false;

    for (int i = 0; i < m; ++i) {
      if (crom.DS[i] == 0) continue;

      // Calcular demanda total asignada a este depósito
      auto cli = crom.clientes_de(i, inst);
      double demanda = 0.0;
      for (int c : cli)
        demanda += inst.clientes[c - 1].demanda;

      if (demanda <= inst.depositos[i].capacidad) continue;

      // El depósito i está sobrecargado: sacar clientes del final de su sublista
      auto [ini, fin] = crom.rango_cs(i, inst);
      std::vector<int> sublista(crom.CS.begin() + ini, crom.CS.begin() + fin);
      std::vector<int> removidos;
      double dem_actual = demanda;

      while (dem_actual > inst.depositos[i].capacidad && !sublista.empty()) {
        int c = sublista.back();
        sublista.pop_back();
        dem_actual -= inst.clientes[c - 1].demanda;
        removidos.push_back(c);
      }

      if (removidos.empty()) continue;
      hubo_cambio = true;

      // Actualizar CS: reemplazar la sublista de i con la versión reducida
      crom.CS.erase(crom.CS.begin() + ini, crom.CS.begin() + fin);
      crom.CS.insert(crom.CS.begin() + ini, sublista.begin(), sublista.end());

      // Actualizar los punteros DS de todos los depósitos que quedaron desplazados
      int delta = (int)removidos.size();
      for (int j = 0; j < m; ++j) {
        if (crom.DS[j] == 0) continue;
        if (crom.DS[j] - 1 > ini)
          crom.DS[j] -= delta;
      }

      // Reubicar cada cliente removido en otro depósito que tenga espacio
      for (int c : removidos) {
        int idx_c = c - 1;
        int destino = -1;

        // Primero buscar un depósito YA ABIERTO con espacio
        for (int j = 0; j < m; ++j) {
          if (crom.DS[j] == 0) continue;
          auto cli_j = crom.clientes_de(j, inst);
          double dem_j = 0.0;
          for (int cc : cli_j)
            dem_j += inst.clientes[cc - 1].demanda;
          if (dem_j + inst.clientes[idx_c].demanda <= inst.depositos[j].capacidad) {
            destino = j;
            break;
          }
        }

        // Si ninguno tiene espacio, abrir el depósito cerrado más cercano
        if (destino == -1) {
          double mejor_dist = std::numeric_limits<double>::infinity();
          for (int j = 0; j < m; ++j) {
            if (crom.DS[j] != 0) continue;
            double d = mat[n + j][idx_c];
            if (d < mejor_dist) { mejor_dist = d; destino = j; }
          }
          crom.DS[destino] = (int)crom.CS.size() + 1; // lo abre al final de CS
        }

        // Insertar el cliente al final de la sublista del depósito destino
        auto [ini_d, fin_d] = crom.rango_cs(destino, inst);
        crom.CS.insert(crom.CS.begin() + fin_d, c);
        // Actualizar punteros DS desplazados por la inserción
        for (int j = 0; j < m; ++j) {
          if (crom.DS[j] == 0) continue;
          if (crom.DS[j] - 1 >= fin_d)
            crom.DS[j] += 1;
        }
      }
    }
  }

  if ((int)crom.CS.size() != n)
    throw std::runtime_error(
        "repair: CS tiene tamaño incorrecto: " +
        std::to_string(crom.CS.size()) + " esperado " + std::to_string(n));
}

// -----------------------------------------------------------------------------
// Convierte un EstadoLRP (solución con rutas explícitas) en un CromosomaLRP.
//
// Lee las rutas de cada depósito abierto y concatena sus clientes en CS,
// anotando en DS la posición de inicio de cada sublista.
// Los clientes no_asignados (si los hay) se agregan al final, asignados al
// primer depósito abierto que encuentre.
// -----------------------------------------------------------------------------
CromosomaLRP estado_a_cromosoma(const EstadoLRP &e) {
  const InstanciaLRP &inst = *e.datos;
  int n = inst.num_clientes;
  int m = inst.num_depositos;

  CromosomaLRP crom;
  crom.DS.assign(m, 0);
  crom.CS.reserve(n);

  // Para cada depósito abierto, concatenar sus clientes en CS y anotar la posición
  int pos = 1; // posición 1-based en CS
  for (int dep_id : e.depositos_abiertos) {
    int dep_idx = dep_id - n - 1; // dep_id es nc+i+1, dep_idx es i (0-based)
    if (dep_idx < 0 || dep_idx >= m)
      throw std::runtime_error("estado_a_cromosoma: dep_id fuera de rango");

    // Recopilar todos los clientes de este depósito (en orden de visita)
    std::vector<int> cli_dep;
    auto it = e.rutas.find(dep_id);
    if (it != e.rutas.end())
      for (const auto &ruta : it->second)
        for (int c : ruta.clientes)
          cli_dep.push_back(c);

    if (cli_dep.empty()) continue; // depósito abierto pero sin clientes

    crom.DS[dep_idx] = pos;
    for (int c : cli_dep) {
      crom.CS.push_back(c);
      ++pos;
    }
  }

  // Si hay clientes sin asignar, los metemos al primer depósito abierto
  if (!e.no_asignados.empty()) {
    int dep_destino = -1;
    for (int i = 0; i < m; ++i)
      if (crom.DS[i] != 0) { dep_destino = i; break; }
    if (dep_destino == -1) {
      crom.DS[0] = pos; // forzar apertura del depósito 0
      dep_destino = 0;
    }
    for (int c : e.no_asignados) {
      crom.CS.push_back(c);
      ++pos;
    }
  }

  if ((int)crom.CS.size() != n)
    throw std::runtime_error("estado_a_cromosoma: CS incompleto, tiene " +
                             std::to_string(crom.CS.size()) + " esperado " +
                             std::to_string(n));

  crom.fitness = e.objective();
  return crom;
}

// -----------------------------------------------------------------------------
// Convierte un CromosomaLRP de vuelta a un EstadoLRP con rutas reales.
//
// Si rutas_cache NO está vacío, se usan esas rutas (vienen del Split más
// reciente y son las rutas óptimas para ese cromosoma).
//
// Si rutas_cache ESTÁ vacío (fallback), se crea una ruta por cliente —
// solución válida pero subóptima, útil solo como inicialización de emergencia.
// -----------------------------------------------------------------------------
EstadoLRP cromosoma_a_estado(const CromosomaLRP &crom, const Rutas &rutas_cache,
                             const Matriz &mat, const InstanciaLRP &inst) {
  int n = inst.num_clientes;
  int m = inst.num_depositos;

  // Lista de depósitos abiertos (IDs 1-indexed: nc + i + 1)
  std::vector<int> dep_abiertos;
  for (int i = 0; i < m; ++i)
    if (crom.DS[i] != 0)
      dep_abiertos.push_back(n + i + 1);

  Rutas rutas;
  std::vector<int> no_asig;

  if (!rutas_cache.empty()) {
    // Caso normal: usar las rutas del Split
    rutas = rutas_cache;
    for (int dep_id : dep_abiertos)
      if (rutas.find(dep_id) == rutas.end())
        rutas[dep_id] = {};
  } else {
    // Fallback: una ruta por cliente (no óptimo, pero al menos es factible)
    for (int i = 0; i < m; ++i) {
      if (crom.DS[i] == 0) continue;
      int dep_id = n + i + 1;
      rutas[dep_id] = {};
      for (int c : crom.clientes_de(i, inst))
        rutas[dep_id].push_back(Ruta(dep_id, {c}));
    }
  }

  return EstadoLRP(rutas, dep_abiertos, no_asig, mat, inst);
}
