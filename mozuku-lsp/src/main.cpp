#include "lsp.hpp"
#include <iostream>

int main() {
  LSPServer server(std::cin, std::cout);
  server.run();
  return 0;
}
