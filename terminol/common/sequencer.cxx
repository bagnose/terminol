// vi:noai:sw=4

#include "terminol/support/debug.hxx"

int main() {
  for (int i = 0; i != 1024; ++i) {
    std::cout << (i % 10);
  }

  std::cout << std::endl;

    return 0;
}
