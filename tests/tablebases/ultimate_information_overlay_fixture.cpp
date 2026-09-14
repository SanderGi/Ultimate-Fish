// Exercise the same header serializer used by the Ghost/ordinary solver.
#include "information_overlay_format.h"
#include <fstream>
using namespace Stockfish::Ultimate;
int main(int argc, char** argv) {
    if (argc != 9) return 2;
    std::ofstream out(argv[1], std::ios::binary);
    write_information_overlay_header(out, static_cast<PieceType>(std::stoul(argv[2])),
      static_cast<PieceType>(std::stoul(argv[3])), static_cast<Color>(std::stoul(argv[4])),
      std::stoul(argv[5]), std::stoul(argv[6]), argv[7], argv[8]);
}
