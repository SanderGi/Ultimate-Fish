/* Ultimate Fish UFIW2 force-role contract. GPLv3 or later. */
#ifndef ULTIMATE_INFORMATION_OVERLAY_FORMAT_H
#define ULTIMATE_INFORMATION_OVERLAY_FORMAT_H

#include "position.h"
#include <algorithm>
#include <cstdint>
#include <ostream>
#include <stdexcept>
#include <string>

namespace Stockfish::Ultimate {

// UFIW2 byte 20 is the material's SECONDARY color. The primary is normalized
// White. It has never been a general owner-color field. Existing files need
// no rewrite: derive force ownership from both piece slots and this color.
// Jester bits 0/1 mean White/Black; single-Ghost bits mean owner/observer.
// Crossed and double-Ghost formats require their dedicated readers.
inline Color information_first_force_color(PieceType primary,
                                            PieceType secondary,
                                            Color secondaryColor) {
    if (secondaryColor != Color::White && secondaryColor != Color::Black)
        throw std::runtime_error("invalid UFIW2 secondary color");
    const bool jester = primary == PieceType::Jester || secondary == PieceType::Jester;
    const bool primaryGhost = primary == PieceType::Ghost;
    const bool secondaryGhost = secondary == PieceType::Ghost;
    if ((jester && (primaryGhost || secondaryGhost)) ||
        (primaryGhost && secondaryGhost))
        throw std::runtime_error("UFIW2 joint hidden material needs dedicated force semantics");
    if (jester || primaryGhost)
        return Color::White;
    if (secondaryGhost)
        return secondaryColor;
    throw std::runtime_error("UFIW2 material has no supported force semantics");
}

inline void write_information_overlay_header(
  std::ostream& output, PieceType primary, PieceType secondary,
  Color secondaryColor, std::uint32_t states, std::uint32_t substates,
  const std::string& sourceSha, const std::string& modelSha) {
    const auto validSha = [](const std::string& sha) {
        return sha.size() == 64 && std::all_of(sha.begin(), sha.end(), [](char c) {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        });
    };
    if (!validSha(sourceSha) || !validSha(modelSha))
        throw std::runtime_error("invalid UFIW2 source/model hash");
    (void)information_first_force_color(primary, secondary, secondaryColor);
    output.write("UFIW2\0\0\0", 8);
    for (std::uint32_t word : {2u, static_cast<std::uint32_t>(primary),
         static_cast<std::uint32_t>(secondary), static_cast<std::uint32_t>(secondaryColor),
         states, substates})
        output.write(reinterpret_cast<const char*>(&word), sizeof(word));
    output.write(sourceSha.data(), 64);
    output.write(modelSha.data(), 64);
    if (!output)
        throw std::runtime_error("failed writing UFIW2 material header");
}

} // namespace Stockfish::Ultimate
#endif
