#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

int main(int argc, char *argv[]) {
  std::ifstream f(argv[1]);
  std::vector<std::vector<std::string>> lineas;
  std::string linea;
  while (std::getline(f, linea)) {
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
  std::cout << "Total: " << lineas.size() << "\n";
  int idx = 0;
  int nc = std::stoi(lineas[idx++][0]);
  int nd = std::stoi(lineas[idx++][0]);
  std::cout << "nc=" << nc << " nd=" << nd << "\n";
  for (int i = 0; i < nd; ++i, ++idx)
    std::cout << "dep[" << i << "] idx=" << idx << " '" << lineas[idx][0]
              << "' '" << lineas[idx][1] << "'\n";
  for (int i = 0; i < nc; ++i, ++idx)
    std::cout << "cli[" << i << "] idx=" << idx << " '" << lineas[idx][0]
              << "' '" << lineas[idx][1] << "'\n";
  std::cout << "Q idx=" << idx << " '" << lineas[idx][0] << "'\n";
  ++idx;
  for (int i = 0; i < nd; ++i, ++idx)
    std::cout << "cap[" << i << "] idx=" << idx << " '" << lineas[idx][0]
              << "'\n";
  for (int i = 0; i < nc; ++i, ++idx)
    std::cout << "dem[" << i << "] idx=" << idx << " '" << lineas[idx][0]
              << "'\n";
  for (int i = 0; i < nd; ++i, ++idx)
    std::cout << "aper[" << i << "] idx=" << idx << " '" << lineas[idx][0]
              << "'\n";
  std::cout << "F idx=" << idx << " '" << lineas[idx][0] << "'\n";
  ++idx;
  std::cout << "flag idx=" << idx << " '" << lineas[idx][0] << "'\n";
  std::cout << "[OK]\n";
  return 0;
}
