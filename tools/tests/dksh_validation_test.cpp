#include <lmnx/dksh.hpp>
#include <array>
#include <cstdio>
int main() {
    std::array<uint8_t,512> datos{};
    auto word = [&](size_t p, uint32_t valor) {
        for (unsigned b=0;b<4;++b) datos[p+b] = uint8_t(valor >> (8*b));
    };
    word(0,0x48534B44); word(4,24); word(8,256); word(12,256);
    word(16,24); word(20,1); word(24,0); word(28,128); word(32,4);
    if (!lmnx::validarDKSH(datos.data(),datos.size())) return 1;
    for (size_t n=0; n<datos.size(); ++n)
        if (lmnx::validarDKSH(datos.data(),n)) return 2;
    word(20,0xFFFFFFFF);
    if (lmnx::validarDKSH(datos.data(),datos.size())) return 3;
    word(20,1); word(28,256);
    if (lmnx::validarDKSH(datos.data(),datos.size())) return 4;
    std::puts("DKSH: valid header accepted; truncation, overflow and invalid entry point rejected.");
}
