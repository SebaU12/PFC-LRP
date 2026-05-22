#pragma once
#include <string>
#include <vector>

// ─────────────────────────────────────────
// Estructuras de datos base
// ─────────────────────────────────────────

struct Cliente {
  int id; // 1-indexed (igual que Python)
  double x, y;
  double demanda;
};

struct Deposito {
  int id; // 1-indexed, arranca en num_clientes+1
  double x, y;
  double capacidad;      // W_i
  double costo_apertura; // O_i
};

struct InstanciaLRP {
  std::string nombre;
  int num_clientes;
  int num_depositos;

  std::vector<Cliente> clientes;   // tamaño = num_clientes
  std::vector<Deposito> depositos; // tamaño = num_depositos

  double Q; // capacidad del vehículo
  double F; // costo fijo por vehículo/ruta
  int flota_maxima;
  bool es_entero; // true → distancias × 100, truncar a int
};

// ─────────────────────────────────────────
// Funciones públicas
// ─────────────────────────────────────────

// Lee un archivo .dat de Prodhon (adaptado a LRP monoperiodo).
// Lanza std::runtime_error si el archivo no se puede abrir o está malformado.
InstanciaLRP leer_instancia(const std::string &ruta);

// Imprime un resumen formateado en consola (equivalente a
// mostrar_resumen_instancia del Python).
void mostrar_resumen(const InstanciaLRP &inst);
