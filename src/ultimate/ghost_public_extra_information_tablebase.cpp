/* Ultimate Fish reciprocal Bishop/Ghost exact-information CLI. GPLv3+. */

#include "ghost_public_extra_information_solver.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace Model = Stockfish::Ultimate::GhostPublicExtraExact;

int main(int argc, char** argv) {
    try {
        enum class Command { None, SelfTest, Compile, Merge, Verify, Measure,
                             Solve } command = Command::None;
        Model::TransitionOptions transition;
        Model::SolveOptions solve;
        std::vector<std::string> shards;
        std::uint32_t expectedGeometries = 0;
        const auto choose = [&](Command next) {
            if (command != Command::None)
                throw std::invalid_argument("choose one reciprocal command");
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
            else if (option == "--compile-transitions")
                choose(Command::Compile);
            else if (option == "--merge-transitions") choose(Command::Merge);
            else if (option == "--verify-transitions") choose(Command::Verify);
            else if (option == "--solve") choose(Command::Solve);
            else if (option == "--measure") {
                choose(Command::Measure);
                solve.measureIterations = std::stoul(value());
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
            else if (option == "--scratch") solve.scratchPrefix = value();
            else if (option == "--output") solve.outputOverlay = value();
            else if (option == "--output-arbitrary")
                solve.outputArbitrary = value();
            else if (option == "--source-sha256")
                solve.sourceSha256 = value();
            else if (option == "--model-sha256")
                solve.modelSha256 = value();
            else if (option == "--observation-sha256")
                solve.observationSha256 = value();
            else if (option == "--lower-source-sha256")
                solve.lowerGhostSourceSha256 = value();
            else if (option == "--lower-model-sha256")
                solve.lowerGhostModelSha256 = value();
            else if (option == "--lower-observation-sha256")
                solve.lowerGhostObservationSha256 = value();
            else if (option == "--lower-sidecar-sha256")
                solve.lowerGhostSidecarSha256 = value();
            else if (option == "--max-nodes")
                solve.maxNodes = std::stoul(value());
            else if (option == "--unique-slots")
                solve.uniqueSlots = std::stoull(value());
            else if (option == "--compact-every")
                solve.compactEvery = std::stoul(value());
            else
                throw std::invalid_argument("unknown option " + option);
        }
        if (command == Command::None)
            throw std::invalid_argument("missing reciprocal command");
        if (command == Command::SelfTest) {
            Model::exact_self_test(solve.scratchPrefix.empty()
              ? "/tmp/ultimate-reciprocal-bishop-ghost" : solve.scratchPrefix);
            const Model::ResourceEstimate estimate = Model::resource_estimate();
            std::cout << "reciprocal_bishop_ghost_resource geometries "
                      << estimate.geometries << " owner_roots "
                      << estimate.ownerRoots << " transition_bytes "
                      << estimate.estimatedTransitionBytes << " root_bytes "
                      << estimate.rootBytes << " bdd_bytes "
                      << estimate.bddBytes << " peak_scratch_bytes "
                      << estimate.peakScratchBytes << " peak_resident_bytes "
                      << estimate.peakResidentBytes << " belief_cap none\n";
        }
        else if (command == Command::Compile)
            Model::compile_transitions(transition);
        else if (command == Command::Merge)
            Model::merge_transitions(transition.prefix, shards,
                                     expectedGeometries);
        else if (command == Command::Verify)
            Model::verify_transitions(transition.prefix);
        else {
            const Model::SolveCertificate certificate = Model::solve_exact(solve);
            std::cout << "reciprocal_bishop_ghost_certificate dual_force_residual "
                      << certificate.arbitraryDualForceResidual
                      << " structural_residual "
                      << certificate.arbitraryStructuralResidual
                      << " singleton_residual "
                      << certificate.arbitrarySingletonResidual
                      << " transition_payload_sha256 "
                      << certificate.transitionPayloadSha256
                      << " arbitrary_sha256 " << certificate.arbitrarySha256
                      << '\n';
        }
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "reciprocal Bishop/Ghost error: " << error.what() << '\n';
        return 1;
    }
}
