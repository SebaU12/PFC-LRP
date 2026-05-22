#include "../src/parser.h"
#include <iostream>
#include <stdexcept>

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Uso: ./test_parser <instancia.dat>\n";
    return 1;
  }

  try {
    std::cout << "Leyendo instancia...\n";
    InstanciaLRP inst = leer_instancia(argv[1]);
    std::cout << "Leida OK\n";
    std::cout << "nc=" << inst.num_clientes << " nd=" << inst.num_depositos
              << " Q=" << inst.Q << " F=" << inst.F << "\n";
    std::cout << "Dep[0]: x=" << inst.depositos[0].x
              << " y=" << inst.depositos[0].y
              << " cap=" << inst.depositos[0].capacidad
              << " aper=" << inst.depositos[0].costo_apertura << "\n";
    std::cout << "Cli[0]: x=" << inst.clientes[0].x
              << " y=" << inst.clientes[0].y
              << " dem=" << inst.clientes[0].demanda << "\n";

    std::cout << "Llamando mostrar_resumen...\n";
    mostrar_resumen(inst);

    // Checks
    bool ok = true;
    if ((int)inst.clientes.size() != inst.num_clientes) {
      std::cerr << "[ERROR] Clientes leidos: " << inst.clientes.size()
                << " esperados: " << inst.num_clientes << "\n";
      ok = false;
    }
    if ((int)inst.depositos.size() != inst.num_depositos) {
      std::cerr << "[ERROR] Depositos leidos: " << inst.depositos.size()
                << " esperados: " << inst.num_depositos << "\n";
      ok = false;
    }
    for (int i = 0; i < inst.num_clientes; ++i)
      if (inst.clientes[i].id != i + 1) {
        std::cerr << "[ERROR] ID cliente[" << i << "]=" << inst.clientes[i].id
                  << " esperado " << i + 1 << "\n";
        ok = false;
      }
    for (int i = 0; i < inst.num_depositos; ++i) {
      int esp = inst.num_clientes + i + 1;
      if (inst.depositos[i].id != esp) {
        std::cerr << "[ERROR] ID deposito[" << i << "]=" << inst.depositos[i].id
                  << " esperado " << esp << "\n";
        ok = false;
      }
    }
    if (inst.Q <= 0) {
      std::cerr << "[ERROR] Q<=0\n";
      ok = false;
    }
    if (inst.F <= 0) {
      std::cerr << "[ERROR] F<=0\n";
      ok = false;
    }

    if (ok)
      std::cout << "[OK] Parser correcto.\n";

  } catch (const std::exception &e) {
    std::cerr << "[EXCEPCION] " << e.what() << "\n";
    return 1;
  }
  return 0;
}
