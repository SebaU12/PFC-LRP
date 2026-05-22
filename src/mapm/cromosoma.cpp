#include "cromosoma.h"
#include <algorithm>
#include <limits>
#include <stdexcept>

// ─────────────────────────────────────────
// rango_cs
//
// DS[i] es la posición 1-based en CS donde empieza la sublista
// del depósito i.  El fin de la sublista es la posición 1-based
// donde empieza el siguiente depósito abierto, o n+1 si es el
// último abierto.
//
// Para encontrar el fin: buscamos el menor DS[j] > DS[i]
// entre todos los depósitos abiertos j != i.
// ─────────────────────────────────────────
std::pair<int, int> CromosomaLRP::rango_cs(int dep_idx,
                                           const InstanciaLRP &inst) const {
  if (DS[dep_idx] == 0)
    return {0, 0}; // cerrado → rango vacío

  int inicio = DS[dep_idx] - 1; // convertir a 0-based
  int n = inst.num_clientes;

  // Buscar el inicio 0-based más cercano que sea ESTRICTAMENTE mayor
  // que el nuestro (garantiza rangos no solapados)
  int fin = n;
  for (int j = 0; j < (int)DS.size(); ++j) {
    if (j == dep_idx || DS[j] == 0)
      continue;
    int ini_j = DS[j] - 1;
    if (ini_j > inicio && ini_j < fin)
      fin = ini_j;
  }

  // Caso degenerado: dos depósitos con el mismo puntero
  // (no debe ocurrir tras estado_a_cromosoma corregido, pero
  // lo blindamos devolviendo rango vacío para el conflicto)
  if (fin == inicio)
    return {inicio, inicio};

  return {inicio, fin};
}

std::vector<int> CromosomaLRP::clientes_de(int dep_idx,
                                           const InstanciaLRP &inst) const {
  auto [ini, fin] = rango_cs(dep_idx, inst);
  return std::vector<int>(CS.begin() + ini, CS.begin() + fin);
}

// ─────────────────────────────────────────
// repair
// ─────────────────────────────────────────
void repair(CromosomaLRP &crom, const Matriz &mat, const InstanciaLRP &inst) {
  int n = inst.num_clientes;
  int m = inst.num_depositos;

  // ── Paso 1: verificar que al menos un depósito está abierto
  // "verifying that index 1 of CS is in DS"
  // Es decir, algún DS[i] == 1 (el cliente en posición 1 de CS
  // debe pertenecer a algún depósito abierto).
  bool alguno_abierto = false;
  for (int v : crom.DS)
    if (v != 0) {
      alguno_abierto = true;
      break;
    }

  if (!alguno_abierto) {
    // Abrir el depósito de menor costo de apertura
    int mejor = 0;
    for (int i = 1; i < m; ++i)
      if (inst.depositos[i].costo_apertura <
          inst.depositos[mejor].costo_apertura)
        mejor = i;
    crom.DS[mejor] = 1; // el único abierto toma todos los clientes desde pos 1
  }

  // ── Paso 2: corregir violaciones de capacidad de depósito
  // Iterar hasta que no haya violaciones (el repair puede
  // introducir nuevas al reasignar clientes)
  bool hubo_cambio = true;
  while (hubo_cambio) {
    hubo_cambio = false;

    for (int i = 0; i < m; ++i) {
      if (crom.DS[i] == 0)
        continue;

      // Calcular demanda total del depósito i
      auto cli = crom.clientes_de(i, inst);
      double demanda = 0.0;
      for (int c : cli)
        demanda += inst.clientes[c - 1].demanda;

      if (demanda <= inst.depositos[i].capacidad)
        continue;

      // Violación: escanear hacia atrás y remover clientes
      // hasta que la capacidad se respete
      auto [ini, fin] = crom.rango_cs(i, inst);

      // Trabajamos con una copia de la sublista para no
      // invalidar los rangos mientras modificamos CS
      std::vector<int> sublista(crom.CS.begin() + ini, crom.CS.begin() + fin);

      std::vector<int> removidos;
      double dem_actual = demanda;

      // Escanear hacia atrás
      while (dem_actual > inst.depositos[i].capacidad && !sublista.empty()) {
        int c = sublista.back();
        sublista.pop_back();
        dem_actual -= inst.clientes[c - 1].demanda;
        removidos.push_back(c);
      }

      if (removidos.empty())
        continue;
      hubo_cambio = true;

      // Actualizar CS: reemplazar rango del depósito i
      // con la sublista reducida
      crom.CS.erase(crom.CS.begin() + ini, crom.CS.begin() + fin);
      crom.CS.insert(crom.CS.begin() + ini, sublista.begin(), sublista.end());

      // Ajustar todos los punteros DS que apunten después
      // de la posición ini (se desplazaron por la reducción)
      int delta = (int)removidos.size();
      for (int j = 0; j < m; ++j) {
        if (crom.DS[j] == 0)
          continue;
        // posición 1-based: si apunta después del bloque
        // reducido, retrocede delta posiciones
        if (crom.DS[j] - 1 > ini)
          crom.DS[j] -= delta;
      }

      // Reasignar clientes removidos
      for (int c : removidos) {
        int idx_c = c - 1; // 0-based para la matriz

        // Buscar primer depósito abierto con capacidad
        int destino = -1;
        for (int j = 0; j < m; ++j) {
          if (crom.DS[j] == 0)
            continue;
          auto cli_j = crom.clientes_de(j, inst);
          double dem_j = 0.0;
          for (int cc : cli_j)
            dem_j += inst.clientes[cc - 1].demanda;
          if (dem_j + inst.clientes[idx_c].demanda <=
              inst.depositos[j].capacidad) {
            destino = j;
            break;
          }
        }

        if (destino == -1) {
          // Ningún depósito abierto tiene capacidad:
          // abrir el depósito con menor distancia al cliente
          // entre los cerrados
          double mejor_dist = std::numeric_limits<double>::infinity();
          for (int j = 0; j < m; ++j) {
            if (crom.DS[j] != 0)
              continue;
            int dep_mat_idx = n + j; // 0-based en matriz
            double d = mat[dep_mat_idx][idx_c];
            if (d < mejor_dist) {
              mejor_dist = d;
              destino = j;
            }
          }
          // Abrir ese depósito al final de CS
          crom.DS[destino] = (int)crom.CS.size() + 1;
        }

        // Insertar c al final de la sublista del depósito destino
        auto [ini_d, fin_d] = crom.rango_cs(destino, inst);
        crom.CS.insert(crom.CS.begin() + fin_d, c);

        // Ajustar punteros DS que apuntan después de fin_d
        for (int j = 0; j < m; ++j) {
          if (crom.DS[j] == 0)
            continue;
          if (crom.DS[j] - 1 >= fin_d)
            crom.DS[j] += 1;
        }
      }
    }
  }

  // Verificación final: CS debe seguir siendo permutación de n clientes
  if ((int)crom.CS.size() != n)
    throw std::runtime_error(
        "repair: CS tiene tamaño incorrecto tras repair: " +
        std::to_string(crom.CS.size()) + " esperado " + std::to_string(n));
}

// ─────────────────────────────────────────
// estado_a_cromosoma
// ─────────────────────────────────────────
CromosomaLRP estado_a_cromosoma(const EstadoLRP &e) {
  const InstanciaLRP &inst = *e.datos;
  int n = inst.num_clientes;
  int m = inst.num_depositos;

  CromosomaLRP crom;
  crom.DS.assign(m, 0);
  crom.CS.reserve(n);

  int pos = 1; // posición 1-based en CS (avanza solo cuando hay clientes)
  for (int dep_id : e.depositos_abiertos) {
    int dep_idx = dep_id - n - 1; // 0-based en inst.depositos
    if (dep_idx < 0 || dep_idx >= m)
      throw std::runtime_error("estado_a_cromosoma: dep_id fuera de rango");

    // Recopilar todos los clientes de este depósito
    std::vector<int> cli_dep;
    auto it = e.rutas.find(dep_id);
    if (it != e.rutas.end())
      for (const auto &ruta : it->second)
        for (int c : ruta)
          cli_dep.push_back(c);

    // Solo marcar como abierto en DS si tiene al menos 1 cliente.
    // Un depósito abierto sin clientes no tiene sublista en CS
    // y su DS quedaría solapado con otro depósito → lo omitimos.
    if (cli_dep.empty())
      continue;

    crom.DS[dep_idx] = pos; // puntero 1-based al inicio de sublista
    for (int c : cli_dep) {
      crom.CS.push_back(c);
      ++pos;
    }
  }

  // Clientes no asignados: los agregamos al final asignados al primer
  // depósito abierto (solución de emergencia; no debería ocurrir)
  if (!e.no_asignados.empty()) {
    // Buscar primer depósito ya abierto en DS
    int dep_destino = -1;
    for (int i = 0; i < m; ++i)
      if (crom.DS[i] != 0) {
        dep_destino = i;
        break;
      }

    // Si no hay ninguno abierto (caso extremo), abrir el primero
    if (dep_destino == -1) {
      crom.DS[0] = pos;
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

// ─────────────────────────────────────────
// cromosoma_a_estado
// ─────────────────────────────────────────
EstadoLRP cromosoma_a_estado(const CromosomaLRP &crom, const Rutas &rutas_cache,
                             const Matriz &mat, const InstanciaLRP &inst) {
  int n = inst.num_clientes;
  int m = inst.num_depositos;

  std::vector<int> dep_abiertos;
  for (int i = 0; i < m; ++i)
    if (crom.DS[i] != 0)
      dep_abiertos.push_back(n + i + 1); // ID 1-indexed

  Rutas rutas;
  std::vector<int> no_asig;

  if (!rutas_cache.empty()) {
    rutas = rutas_cache;
    for (int dep_id : dep_abiertos)
      if (rutas.find(dep_id) == rutas.end())
        rutas[dep_id] = {};
  } else {
    // Sin cache: una ruta por cliente (solo para diagnóstico)
    for (int i = 0; i < m; ++i) {
      if (crom.DS[i] == 0)
        continue;
      int dep_id = n + i + 1;
      rutas[dep_id] = {};
      auto cli = crom.clientes_de(i, inst);
      for (int c : cli)
        rutas[dep_id].push_back({c});
    }
  }

  return EstadoLRP(rutas, dep_abiertos, no_asig, mat, inst);
}
