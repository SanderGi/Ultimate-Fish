/* Ultimate Fish exact Bomb/Ghost information CLI. GPLv3+. */

#include "ghost_bomb_information_solver.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace Model = Stockfish::Ultimate::GhostBombExact;

int main(int argc, char** argv) {
    try {
        enum class Command { None, SelfTest, SourceTest, Compile, Merge,
                             Verify, Measure, Solve } command = Command::None;
        Model::TransitionOptions transition;
        Model::SolveOptions solve;
        std::vector<std::string> shards;
        std::uint32_t expectedGeometries = 0;
        const auto choose = [&](Command next) {
            if (command != Command::None)
                throw std::invalid_argument("choose one Bomb command");
            command = next;
        };
        for (int index = 1; index < argc; ++index) {
            const std::string option = argv[index];
            const auto value = [&]() -> std::string {
                if (++index >= argc)
                    throw std::invalid_argument(option + " needs a value");
                return argv[index];
            };
            if (option == "--self-test") choose(Command::SelfTest);
            else if (option == "--verify-source")
                choose(Command::SourceTest);
            else if (option == "--compile-transitions")
                choose(Command::Compile);
            else if (option == "--merge-transitions") choose(Command::Merge);
            else if (option == "--verify-transitions") choose(Command::Verify);
            else if (option == "--solve") choose(Command::Solve);
            else if (option == "--measure") {
                choose(Command::Measure);
                solve.measureIterations = std::stoul(value());
            }
            else if (option == "--orientation") {
                const std::string material = value();
                if (material == "same")
                    transition.orientation = solve.orientation =
                      Model::Orientation::Same;
                else if (material == "opposing")
                    transition.orientation = solve.orientation =
                      Model::Orientation::Opposing;
                else
                    throw std::invalid_argument(
                      "orientation must be same or opposing");
            }
            else if (option == "--transition-prefix")
                transition.prefix = solve.transitionPrefix = value();
            else if (option == "--geometry-begin")
                transition.geometryBegin = std::stoul(value());
            else if (option == "--geometry-count")
                transition.geometryCount = std::stoul(value());
            else if (option == "--shard") shards.push_back(value());
            else if (option == "--expected-geometries")
                expectedGeometries = std::stoul(value());
            else if (option == "--input") solve.sourceTable = value();
            else if (option == "--lower-ghost-sidecar")
                solve.lowerGhostSidecar = value();
            else if (option == "--lower-bomb-table")
                transition.lowerBombTable = solve.lowerBombTable = value();
            else if (option == "--scratch") solve.scratchPrefix = value();
            else if (option == "--output") solve.outputOverlay = value();
            else if (option == "--output-arbitrary")
                solve.outputArbitrary = value();
            else if (option == "--source-sha256")
                transition.sourceSha256 = solve.sourceSha256 = value();
            else if (option == "--model-sha256")
                transition.modelSha256 = solve.modelSha256 = value();
            else if (option == "--observation-sha256")
                transition.observationSha256 =
                  solve.observationSha256 = value();
            else if (option == "--lower-source-sha256")
                solve.lowerGhostSourceSha256 = value();
            else if (option == "--lower-model-sha256")
                solve.lowerGhostModelSha256 = value();
            else if (option == "--lower-observation-sha256")
                solve.lowerGhostObservationSha256 = value();
            else if (option == "--lower-sidecar-sha256")
                solve.lowerGhostSidecarSha256 = value();
            else if (option == "--lower-bomb-sha256")
                transition.lowerBombSha256 =
                  solve.lowerBombFullSha256 = value();
            else if (option == "--lower-bomb-source-sha256")
                transition.lowerBombSourceSha256 =
                  solve.lowerBombSourceSha256 = value();
            else if (option == "--lower-bomb-model-sha256")
                transition.lowerBombModelSha256 =
                  solve.lowerBombModelSha256 = value();
            else if (option == "--max-nodes")
                solve.maxNodes = std::stoul(value());
            else if (option == "--unique-slots")
                solve.uniqueSlots = std::stoull(value());
            else if (option == "--compact-every")
                solve.compactEvery = std::stoul(value());
            else if (option == "--workers")
                solve.workers = std::stoul(value());
            else if (option == "--resume-fixed-point")
                solve.resumeFixedPoint = true;
            else if (option == "--resume-converged")
                solve.resumeConverged = true;
            else if (option == "--resume-iteration")
                solve.resumeIteration = std::stoull(value());
            else if (option == "--resume-current-slot") {
                const std::string slot = value();
                if (slot != "current" && slot != "next")
                    throw std::invalid_argument(
                      "resume current slot must be current or next");
                solve.resumeCurrentInNextSlot = slot == "next";
            }
            else if (option == "--resume-bdd-slot") {
                const std::string slot = value();
                if (slot != "a" && slot != "b")
                    throw std::invalid_argument(
                      "resume BDD slot must be a or b");
                solve.resumeBddSlot = slot.front();
            }
            else
                throw std::invalid_argument("unknown option " + option);
        }
        if (command == Command::None)
            throw std::invalid_argument("missing Bomb command");
        if (command == Command::SelfTest) {
            const std::string scratch = solve.scratchPrefix.empty()
              ? "/tmp/ultimate-bomb-ghost" : solve.scratchPrefix;
            Model::exact_self_test(scratch);
            if (!solve.sourceTable.empty())
                std::cout << "bomb_ghost_normalized_source_sha256 "
                          << Model::verify_source_normalization(
                               solve.sourceTable, solve.orientation,
                               solve.sourceSha256, scratch) << '\n';
            const auto estimate = Model::resource_estimate();
            std::cout << "bomb_ghost_resource geometries "
                      << estimate.geometries << " concrete_worlds "
                      << estimate.concreteWorlds << " owner_roots "
                      << estimate.ownerRoots << " transition_bytes "
                      << estimate.estimatedTransitionBytes
                      << " peak_scratch_bytes " << estimate.peakScratchBytes
                      << " peak_resident_bytes "
                      << estimate.peakResidentBytes
                      << " belief_cap none\n";
        }
        else if (command == Command::SourceTest) {
            if (solve.sourceTable.empty())
                throw std::invalid_argument("source test requires --input");
            std::cout << "bomb_ghost_normalized_source_sha256 "
                      << Model::verify_source_normalization(
                           solve.sourceTable, solve.orientation,
                           solve.sourceSha256,
                           solve.scratchPrefix.empty()
                             ? "/tmp/ultimate-bomb-ghost-source"
                             : solve.scratchPrefix) << '\n';
        }
        else if (command == Command::Compile)
            Model::compile_transitions(transition);
        else if (command == Command::Merge)
            Model::merge_transitions(transition, shards, expectedGeometries);
        else if (command == Command::Verify)
            Model::verify_transitions(transition);
        else {
            const auto certificate = Model::solve_exact(solve);
            std::cout << "bomb_ghost_certificate dual_force_residual "
                      << certificate.dualForceResidual
                      << " structural_residual "
                      << certificate.structuralResidual
                      << " singleton_residual "
                      << certificate.singletonResidual
                      << " source_remap_residual "
                      << certificate.sourceRemapResidual
                      << " normalized_source_sha256 "
                      << certificate.normalizedSourceSha256
                      << " transition_payload_sha256 "
                      << certificate.transitionPayloadSha256
                      << " arbitrary_sha256 " << certificate.arbitrarySha256
                      << '\n';
        }
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "Bomb/Ghost error: " << error.what() << '\n';
        return 1;
    }
}
