#include "parser.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

// ─────────────────────────────────────────
// Helpers internos
// ─────────────────────────────────────────

static std::vector<std::vector<std::string>>
tokenizar_archivo(const std::string &ruta) {
  std::ifstream f(ruta);
  if (!f.is_open())
    throw std::runtime_error("No se pudo abrir: " + ruta);

  std::vector<std::vector<std::string>> lineas;
  std::string linea;
  while (std::getline(f, linea)) {
    // Fix CRLF: eliminar \r de archivos con line endings de Windows
    if (!linea.empty() && linea.back() == '\r')
      linea.pop_back();

    std::istringstream ss(linea);
    std::vector<std::string> tokens;
    std::string tok;
    while (ss >> tok)
      tokens.push_back(tok);
    if (!tokens.empty())
      lineas.push_back(tokens);
  }
  return lineas;
}

// ─────────────────────────────────────────
// leer_instancia — formato Prins (LRP)
// ─────────────────────────────────────────
InstanciaLRP leer_instancia(const std::string &ruta) {
  auto lineas = tokenizar_archivo(ruta);

  std::string nombre = ruta;
  auto pos = ruta.find_last_of("/\\");
  if (pos != std::string::npos)
    nombre = ruta.substr(pos + 1);

  InstanciaLRP inst;
  inst.nombre = nombre;

  int idx = 0;

  // ── Línea 0: num_clientes ────────────────────────────
  inst.num_clientes = std::stoi(lineas[idx++][0]);

  // ── Línea 1: num_depositos ───────────────────────────
  inst.num_depositos = std::stoi(lineas[idx++][0]);

  // ── Coordenadas depósitos ────────────────────────────
  inst.depositos.resize(inst.num_depositos);
  for (int i = 0; i < inst.num_depositos; ++i, ++idx) {
    inst.depositos[i].id = inst.num_clientes + i + 1;
    inst.depositos[i].x = std::stod(lineas[idx][0]);
    inst.depositos[i].y = std::stod(lineas[idx][1]);
  }

  // ── Coordenadas clientes ─────────────────────────────
  inst.clientes.resize(inst.num_clientes);
  for (int i = 0; i < inst.num_clientes; ++i, ++idx) {
    inst.clientes[i].id = i + 1;
    inst.clientes[i].x = std::stod(lineas[idx][0]);
    inst.clientes[i].y = std::stod(lineas[idx][1]);
  }

  // ── Capacidad vehículo (Q) ───────────────────────────
  inst.Q = std::stod(lineas[idx++][0]);

  // ── Capacidades depósitos (W_i) ──────────────────────
  for (int i = 0; i < inst.num_depositos; ++i, ++idx)
    inst.depositos[i].capacidad = std::stod(lineas[idx][0]);

  // ── Demandas clientes (d_j) ──────────────────────────
  for (int i = 0; i < inst.num_clientes; ++i, ++idx)
    inst.clientes[i].demanda = std::stod(lineas[idx][0]);

  // ── Costos apertura depósitos (O_i) ──────────────────
  for (int i = 0; i < inst.num_depositos; ++i, ++idx)
    inst.depositos[i].costo_apertura = std::stod(lineas[idx][0]);

  // ── Costo fijo vehículo (F) ──────────────────────────
  inst.F = std::stod(lineas[idx++][0]);

  // ── Flag entero/real ──────────────────────────────────
  inst.es_entero = (std::stoi(lineas[idx][0]) == 0);

  return inst;
}

// ─────────────────────────────────────────
// mostrar_resumen
// ─────────────────────────────────────────
void mostrar_resumen(const InstanciaLRP &inst) {
  const std::string sep65(65, '=');
  const std::string sep35(35, '-');
  const std::string sep50(50, '-');

  std::cout << sep65 << "\n";
  std::cout << "RESUMEN DE LA INSTANCIA: " << inst.nombre << "\n";
  std::cout << sep65 << "\n";

  std::cout << "\n1. PARAMETROS GLOBALES\n" << sep35 << "\n";
  std::cout << std::left << std::setw(25)
            << "Total Clientes:" << inst.num_clientes << "\n"
            << std::setw(25) << "Total Depositos:" << inst.num_depositos << "\n"
            << std::setw(25) << "Capacidad Vehiculo (Q):" << inst.Q << "\n"
            << std::setw(25) << "Costo Vehiculo (F):" << inst.F << "\n"
            << std::setw(25)
            << "Distancias (*100 int):" << (inst.es_entero ? "Si" : "No")
            << "\n";

  std::cout << "\n2. INSTALACIONES POTENCIALES (DEPOSITOS)\n"
            << std::string(65, '-') << "\n";
  std::cout << std::left << std::setw(10) << "ID Nodo" << " | " << std::setw(10)
            << "Coord X" << " | " << std::setw(10) << "Coord Y" << " | "
            << std::setw(15) << "Capacidad (W)"
            << " | Costo Apertura (O)\n"
            << std::string(65, '-') << "\n";
  for (const auto &d : inst.depositos) {
    std::cout << std::setw(10) << d.id << " | " << std::setw(10) << d.x << " | "
              << std::setw(10) << d.y << " | " << std::setw(15) << d.capacidad
              << " | " << d.costo_apertura << "\n";
  }

  std::cout << "\n3. CLIENTES (Muestra de los primeros 5 y ultimos 2)\n"
            << sep50 << "\n";
  std::cout << std::left << std::setw(10) << "ID Nodo" << " | " << std::setw(10)
            << "Coord X" << " | " << std::setw(10) << "Coord Y"
            << " | Demanda (d)\n"
            << sep50 << "\n";

  int total = inst.num_clientes;
  auto print_cliente = [&](const Cliente &c) {
    std::cout << std::setw(10) << c.id << " | " << std::setw(10) << c.x << " | "
              << std::setw(10) << c.y << " | " << c.demanda << "\n";
  };
  for (int i = 0; i < std::min(5, total); ++i)
    print_cliente(inst.clientes[i]);
  if (total > 7)
    std::cout << std::setw(10) << "..." << " | " << std::setw(10) << "..."
              << " | " << std::setw(10) << "..." << " | ...\n";
  for (int i = std::max(5, total - 2); i < total; ++i)
    print_cliente(inst.clientes[i]);

  std::cout << sep65 << "\n\n";
}
