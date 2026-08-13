/* Ultimate Fish authenticated opposed Ghost/Ghost lower oracles. GPLv3+. */

#ifndef ULTIMATE_OPPOSED_GHOST_PAIR_INFORMATION_LOWER_ORACLE_H_INCLUDED
#define ULTIMATE_OPPOSED_GHOST_PAIR_INFORMATION_LOWER_ORACLE_H_INCLUDED

#include "opposed_ghost_pair_information_solver.h"

#include <memory>
#include <string>

namespace Stockfish::Ultimate::OpposedGhostPairSolver {

struct LowerOracleOptions {
    std::string jesterTable;
    std::string jesterTableSha256;
    std::string jesterOverlay;
    std::string jesterOverlaySha256;
    std::string jesterModelSha256;

    std::string ghostSidecar;
    std::string ghostSidecarSha256;
    std::string ghostSourceSha256;
    std::string ghostModelSha256;
    std::string ghostObservationSha256;
};

struct LowerOracleCertificate {
    std::string jesterTableSha256;
    std::string jesterOverlaySha256;
    std::string ghostSidecarSha256;
    std::uint64_t jesterStates = 0;
    std::uint64_t ghostGeometries = 0;
    std::uint64_t ghostStrata = 0;
    std::uint64_t ghostNodes = 0;
};

// Used by the production CLI and deterministic artifact tests so dependency
// hashes are computed by the same audited implementation as the loader.
[[nodiscard]] std::string authenticated_file_sha256(const std::string& path);
[[nodiscard]] std::string authenticated_file_range_sha256(
  const std::string& path, std::uint64_t offset, std::uint64_t count);

// Loads only fully authenticated lower artifacts. Queries validate the entire
// inherited belief against the lower artifact's live/terminal and mover-private
// decision strata before evaluating its exact arbitrary-belief force function.
class AuthenticatedLowerForceOracle final : public LowerForceOracle {
   public:
    explicit AuthenticatedLowerForceOracle(const LowerOracleOptions& options);
    ~AuthenticatedLowerForceOracle() override;
    AuthenticatedLowerForceOracle(AuthenticatedLowerForceOracle&&) noexcept;
    AuthenticatedLowerForceOracle& operator=(
      AuthenticatedLowerForceOracle&&) noexcept;
    AuthenticatedLowerForceOracle(const AuthenticatedLowerForceOracle&) = delete;
    AuthenticatedLowerForceOracle& operator=(
      const AuthenticatedLowerForceOracle&) = delete;

    [[nodiscard]] bool force(
      const LowerJesterForceQuery& query) const override;
    [[nodiscard]] bool force(
      const LowerGhostForceQuery& query) const override;
    [[nodiscard]] const LowerOracleCertificate& certificate() const;

   private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace Stockfish::Ultimate::OpposedGhostPairSolver

#endif

