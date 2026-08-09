/*
  Ultimate Fish - strict arbitrary-mask Ghost sidecar loader tests
  Copyright (C) 2026 Ultimate Fish contributors

  This file is part of Ultimate Fish and is licensed under GPLv3 or later.
*/

#include "../src/ultimate/ghost_information_probe.h"

#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void u16(std::ofstream& output, std::uint16_t value) {
    output.put(static_cast<char>(value));
    output.put(static_cast<char>(value >> 8));
}

void u32(std::ofstream& output, std::uint32_t value) {
    u16(output, static_cast<std::uint16_t>(value));
    u16(output, static_cast<std::uint16_t>(value >> 16));
}

void u64(std::ofstream& output, std::uint64_t value) {
    u32(output, static_cast<std::uint32_t>(value));
    u32(output, static_cast<std::uint32_t>(value >> 32));
}

void mask(std::ofstream& output, std::uint64_t low,
          std::uint16_t high = 0) {
    u64(output, low);
    u16(output, high);
}

void write_fixture(const std::string& path, const std::string& concrete,
                   const std::string& model,
                   const std::string& observation,
                   bool malformedReverseMap = false) {
    constexpr std::uint32_t HeaderBytes = 320;
    constexpr std::uint32_t NodeBytes = 9;
    constexpr std::uint32_t GeometryBytes = 844;
    constexpr std::uint32_t NodeCount = 2;
    constexpr std::uint32_t GeometryCount = 1;
    const std::uint64_t geometryOffset = HeaderBytes + NodeCount * NodeBytes;
    const std::uint64_t stratumOffset = geometryOffset + GeometryBytes;
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
        throw std::runtime_error("cannot create strict-loader fixture");
    output.write("UFGM1\0\0\0", 8);
    for (const std::uint32_t value : {
           1u, HeaderBytes, 11u, 0u, 8u, 10u, 80u, 1'971'840u, 2u,
           GeometryCount, malformedReverseMap ? 1u : 0u, NodeCount,
           NodeBytes, GeometryBytes, 18u, 0u})
        u32(output, value);
    u64(output, HeaderBytes);
    u64(output, geometryOffset);
    u64(output, stratumOffset);
    output.write(concrete.data(), concrete.size());
    output.write(model.data(), model.size());
    output.write(observation.data(), observation.size());
    std::array<char, 32> semantics{};
    const std::string label = "history-mask-public-view-v2";
    std::copy(label.begin(), label.end(), semantics.begin());
    output.write(semantics.data(), semantics.size());

    // Canonical false and true terminals.
    output.put(80);
    u32(output, 0);
    u32(output, 0);
    output.put(80);
    u32(output, 1);
    u32(output, 1);

    // Hidden geometry: Kings a1/b1; Ghost c1 is an owner terminal and d1 an
    // observer terminal.  These are deliberately two distinct public outcome
    // observations, so a mask containing both must be rejected.
    output.put(0);
    output.put(0);
    output.put(1);
    output.put(0);
    mask(output, malformedReverseMap ? 1ULL << 4 : 0);  // live
    mask(output, (1ULL << 2) | (1ULL << 3));
    mask(output, 1ULL << 2);            // owner terminal
    mask(output, 1ULL << 3);            // observer terminal
    for (unsigned square = 0; square < 80; ++square)
        u32(output, malformedReverseMap && square == 4 ? 0u : 0xffffffffu);
    for (unsigned square = 0; square < 80; ++square)
        u32(output, 0);
    std::array<char, 160> visible{};
    output.write(visible.data(), visible.size());
    if (malformedReverseMap) {
        u32(output, 0);                  // geometry
        mask(output, 1ULL << 5);         // not a subset of geometry.live
        u32(output, 0);                  // observer root
    }
    const std::uint64_t expectedSize =
      stratumOffset + (malformedReverseMap ? 18 : 0);
    if (!output || static_cast<std::uint64_t>(output.tellp()) != expectedSize)
        throw std::runtime_error("strict-loader fixture has wrong size");
}

}  // namespace

int main() {
    using namespace Stockfish::Ultimate;
    const std::string path = "/tmp/ultimate-ghost-probe-terminal.ufgm";
    const std::string concrete(64, 'a');
    const std::string model(64, 'b');
    const std::string observation(64, 'c');
    write_fixture(path, concrete, model, observation);
    const GhostInformationProbe probe(
      path, concrete, model, observation);
    const GhostInformationProbeResult owner = probe.probe(
      0, 0, 1, 2, false, {1ULL << 2, 0});
    const GhostInformationProbeResult observer = probe.probe(
      0, 0, 1, 3, false, {1ULL << 3, 0});
    if (!owner.terminal || !owner.ownerForce || owner.observerForce ||
        !observer.terminal || observer.ownerForce || !observer.observerForce)
        throw std::runtime_error("terminal singleton force result is wrong");
    bool rejectedMixed = false;
    try {
        (void)probe.probe(0, 0, 1, 2, false,
                         {(1ULL << 2) | (1ULL << 3), 0});
    }
    catch (const std::runtime_error&) {
        rejectedMixed = true;
    }
    bool rejectedHash = false;
    try {
        (void)GhostInformationProbe(path, std::string(64, 'd'), model,
                                    observation);
    }
    catch (const std::runtime_error&) {
        rejectedHash = true;
    }
    const std::string malformed =
      "/tmp/ultimate-ghost-probe-malformed-reverse.ufgm";
    write_fixture(malformed, concrete, model, observation, true);
    bool rejectedReverseMap = false;
    try {
        (void)GhostInformationProbe(
          malformed, concrete, model, observation);
    }
    catch (const std::runtime_error&) {
        rejectedReverseMap = true;
    }
    if (!rejectedMixed || !rejectedHash || !rejectedReverseMap)
        throw std::runtime_error("strict Ghost sidecar rejection failed");
    std::cout << "ghost_information_probe terminal_singletons 2"
              << " mixed_terminal_rejected 1 hash_mismatch_rejected 1"
              << " malformed_reverse_map_rejected 1\n";
}
