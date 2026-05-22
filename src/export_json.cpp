#include "export_json.h"

#include <fstream>
#include <iomanip>
#include <set>
#include <stdexcept>

// ─────────────────────────────────────────
// Helpers de serialización JSON manual
// (sin dependencias externas)
// ─────────────────────────────────────────

// Escapa caracteres especiales en strings JSON
static std::string json_str(const std::string &s) {
  std::string out = "\"";
  for (char c : s) {
    if (c == '"')
      out += "\\\"";
    else if (c == '\\')
      out += "\\\\";
    else if (c == '\n')
      out += "\\n";
    else
      out += c;
  }
  out += "\"";
  return out;
}

// double con precisión fija (4 decimales)
static std::string json_dbl(double v) {
  std::ostringstream ss;
  ss << std::fixed << std::setprecision(4) << v;
  return ss.str();
}

// ─────────────────────────────────────────
// exportar_solucion_json
// ─────────────────────────────────────────
void exportar_solucion_json(const EstadoLRP &e, const std::string &ruta) {
  std::ofstream f(ruta);
  if (!f.is_open())
    throw std::runtime_error("export_json: no se pudo abrir: " + ruta);

  const InstanciaLRP &inst = *e.datos;
  std::set<int> abiertos(e.depositos_abiertos.begin(),
                         e.depositos_abiertos.end());
  std::set<int> huerfanos(e.no_asignados.begin(), e.no_asignados.end());

  // Asignar un color index a cada depósito abierto (para el visualizador)
  std::map<int, int> color_dep; // dep_id → índice de color
  int ci = 0;
  for (int d : e.depositos_abiertos)
    color_dep[d] = ci++;

  f << "{\n";

  // ── Meta ─────────────────────────────────────────────────
  f << "  \"instancia\": " << json_str(inst.nombre) << ",\n";
  f << "  \"costo\": " << json_dbl(e.objective()) << ",\n";
  f << "  \"num_clientes\": " << inst.num_clientes << ",\n";
  f << "  \"num_depositos\": " << inst.num_depositos << ",\n";
  f << "  \"Q\": " << json_dbl(inst.Q) << ",\n";
  f << "  \"F\": " << json_dbl(inst.F) << ",\n";

  // ── Clientes ─────────────────────────────────────────────
  f << "  \"clientes\": [\n";
  for (int i = 0; i < inst.num_clientes; ++i) {
    const auto &c = inst.clientes[i];
    bool ultimo = (i == inst.num_clientes - 1);
    f << "    {"
      << "\"id\": " << c.id << ", "
      << "\"x\": " << json_dbl(c.x) << ", "
      << "\"y\": " << json_dbl(c.y) << ", "
      << "\"demanda\": " << json_dbl(c.demanda) << ", "
      << "\"huerfano\": " << (huerfanos.count(c.id) ? "true" : "false") << "}"
      << (ultimo ? "" : ",") << "\n";
  }
  f << "  ],\n";

  // ── Depósitos ─────────────────────────────────────────────
  f << "  \"depositos\": [\n";
  for (int i = 0; i < inst.num_depositos; ++i) {
    const auto &d = inst.depositos[i];
    bool ultimo = (i == inst.num_depositos - 1);
    bool open = abiertos.count(d.id);
    f << "    {"
      << "\"id\": " << d.id << ", "
      << "\"x\": " << json_dbl(d.x) << ", "
      << "\"y\": " << json_dbl(d.y) << ", "
      << "\"capacidad\": " << json_dbl(d.capacidad) << ", "
      << "\"costo_apertura\": " << json_dbl(d.costo_apertura) << ", "
      << "\"abierto\": " << (open ? "true" : "false") << ", "
      << "\"color_idx\": " << (open ? color_dep[d.id] : -1) << "}"
      << (ultimo ? "" : ",") << "\n";
  }
  f << "  ],\n";

  // ── Rutas ────────────────────────────────────────────────
  // Cada ruta incluye: depósito origen, índice de color,
  // demanda total y secuencia completa de nodos (dep→c1→…→cn→dep)
  f << "  \"rutas\": [\n";
  bool primera_ruta = true;
  for (int dep_id : e.depositos_abiertos) {
    auto it = e.rutas.find(dep_id);
    if (it == e.rutas.end())
      continue;

    for (const auto &r : it->second) {
      if (r.empty())
        continue;

      if (!primera_ruta)
        f << ",\n";
      primera_ruta = false;

      // Calcular demanda de esta ruta
      double dem = 0.0;
      for (int c : r)
        dem += inst.clientes[c - 1].demanda;

      f << "    {\n";
      f << "      \"deposito_id\": " << dep_id << ",\n";
      f << "      \"color_idx\": " << color_dep[dep_id] << ",\n";
      f << "      \"demanda\": " << json_dbl(dem) << ",\n";
      f << "      \"clientes\": [";
      for (int j = 0; j < (int)r.size(); ++j) {
        f << r[j];
        if (j + 1 < (int)r.size())
          f << ", ";
      }
      f << "]\n";
      f << "    }";
    }
  }
  if (!primera_ruta)
    f << "\n";
  f << "  ],\n";

  // ── No asignados ─────────────────────────────────────────
  f << "  \"no_asignados\": [";
  bool primero = true;
  for (int c : e.no_asignados) {
    if (!primero)
      f << ", ";
    f << c;
    primero = false;
  }
  f << "]\n";

  f << "}\n";
  f.close();
}
