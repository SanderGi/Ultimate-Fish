/*
  Ultimate Fish - exact monotone information-set fixed point
  Copyright (C) 2026 Ultimate Fish contributors

  This file is part of Ultimate Fish and is licensed under GPLv3 or later.
*/

#include "information_solver.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <stdexcept>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>

namespace Stockfish::Ultimate {
namespace {

enum class StoredKind : std::uint8_t {
  Undefined,
  Or,
  And,
  OrOfPairs,
};

struct EquationRecord {
  std::uint64_t tokenOffset = 0;
  std::uint32_t count = 0;
  StoredKind kind = StoredKind::Undefined;
  std::uint8_t padding[3]{};
};

struct ReverseEdge {
  std::uint32_t parent = 0;
  std::uint32_t witness = InformationNoWitness;
  // OR/AND leave auxiliary false.  OR-of-pairs stores the other token.
  InformationToken auxiliary = InformationFalse;
};

static_assert(sizeof(EquationRecord) == 16);
static_assert(sizeof(InformationPair) == 8);
static_assert(sizeof(ReverseEdge) == 12);

[[nodiscard]] std::size_t checked_bytes(std::uint64_t count,
                                        std::size_t itemSize) {
  if (itemSize && count > std::numeric_limits<std::size_t>::max() / itemSize)
    throw std::runtime_error(
        "information fixed-point scratch array is too large");
  return static_cast<std::size_t>(count) * itemSize;
}

[[nodiscard]] std::uint32_t checked_rank_successor(std::uint32_t rank) {
  if (rank == std::numeric_limits<std::uint32_t>::max())
    throw std::runtime_error(
        "information fixed-point activation rank overflow");
  return rank + 1;
}

class ScratchFile {
public:
  explicit ScratchFile(const std::string &directory) {
    std::string path = directory;
    if (path.empty())
      throw std::runtime_error(
          "information fixed-point scratch directory is empty");
    if (path.back() != '/')
      path.push_back('/');
    path += "ultimatefish-information-XXXXXX";
    std::vector<char> writable(path.begin(), path.end());
    writable.push_back('\0');
    fd_ = ::mkstemp(writable.data());
    if (fd_ == -1)
      throw std::runtime_error(
          "cannot create information fixed-point scratch file: " +
          std::string(std::strerror(errno)));
    // The descriptor and any live mappings retain the inode.  Removing the
    // name immediately guarantees cleanup even if generation is killed.
    ::unlink(writable.data());
  }

  ScratchFile(const ScratchFile &) = delete;
  ScratchFile &operator=(const ScratchFile &) = delete;

  ~ScratchFile() {
    unmap();
    if (fd_ != -1)
      ::close(fd_);
  }

  void allocate(std::size_t bytes) {
    unmap();
    if (bytes > static_cast<std::uint64_t>(std::numeric_limits<off_t>::max()))
      throw std::runtime_error("information scratch file exceeds off_t");
    if (::ftruncate(fd_, static_cast<off_t>(bytes)) != 0)
      throw std::runtime_error(
          "cannot size information fixed-point scratch file: " +
          std::string(std::strerror(errno)));
    size_ = bytes;
    if (!bytes)
      return;
    mapping_ =
        ::mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (mapping_ == MAP_FAILED) {
      mapping_ = nullptr;
      throw std::runtime_error(
          "cannot map information fixed-point scratch file: " +
          std::string(std::strerror(errno)));
    }
  }

  void map_read_only(std::size_t bytes) {
    unmap();
    size_ = bytes;
    if (!bytes)
      return;
    mapping_ = ::mmap(nullptr, bytes, PROT_READ, MAP_SHARED, fd_, 0);
    if (mapping_ == MAP_FAILED) {
      mapping_ = nullptr;
      throw std::runtime_error("cannot map information token stream: " +
                               std::string(std::strerror(errno)));
    }
  }

  void append(const void *data, std::size_t bytes) {
    const auto *cursor = static_cast<const std::uint8_t *>(data);
    while (bytes) {
      const ssize_t written = ::write(fd_, cursor, bytes);
      if (written < 0) {
        if (errno == EINTR)
          continue;
        throw std::runtime_error("cannot append information token stream: " +
                                 std::string(std::strerror(errno)));
      }
      if (!written)
        throw std::runtime_error("short write to information token stream");
      cursor += written;
      bytes -= static_cast<std::size_t>(written);
      size_ += static_cast<std::size_t>(written);
    }
  }

  template <typename T> [[nodiscard]] T *data() {
    return static_cast<T *>(mapping_);
  }

  template <typename T> [[nodiscard]] const T *data() const {
    return static_cast<const T *>(mapping_);
  }

private:
  void unmap() {
    if (mapping_) {
      ::munmap(mapping_, size_);
      mapping_ = nullptr;
    }
  }

  int fd_ = -1;
  void *mapping_ = nullptr;
  std::size_t size_ = 0;
};

[[nodiscard]] StoredKind stored_kind(InformationEquationKind kind) {
  switch (kind) {
  case InformationEquationKind::Or:
    return StoredKind::Or;
  case InformationEquationKind::And:
    return StoredKind::And;
  case InformationEquationKind::OrOfPairs:
    return StoredKind::OrOfPairs;
  }
  throw std::runtime_error("unknown information equation kind");
}

} // namespace

struct InformationFixedPoint::Impl {
  Impl(std::uint32_t count, std::string directory)
      : variableCount(count), scratchDirectory(std::move(directory)),
        equationsFile(scratchDirectory), tokensFile(scratchDirectory) {
    if (!variableCount)
      throw std::runtime_error(
          "information fixed point requires at least one variable");
    if (variableCount >= InformationTrue)
      throw std::runtime_error(
          "information variable count collides with constant tokens");
    equationsFile.allocate(
        checked_bytes(variableCount, sizeof(EquationRecord)));
    equations = equationsFile.data<EquationRecord>();
    tokenBuffer.reserve(TokenBufferCapacity);
  }

  void validate_variable(std::uint32_t variable) const {
    if (variable >= variableCount)
      throw std::runtime_error("information variable is out of range");
  }

  void validate_token(InformationToken token) const {
    if (token != InformationFalse && token != InformationTrue &&
        token >= variableCount)
      throw std::runtime_error("information equation token is out of range");
  }

  [[nodiscard]] bool is_variable(InformationToken token) const {
    return token < variableCount;
  }

  void define(std::uint32_t parent, InformationEquationKind publicKind,
              const InformationToken *input, std::size_t count,
              std::size_t tokensPerItem) {
    if (solved || finalized)
      throw std::runtime_error("cannot define an equation after finalization");
    validate_variable(parent);
    if (equations[parent].kind != StoredKind::Undefined)
      throw std::runtime_error("information variable has multiple equations");
    if (count > std::numeric_limits<std::uint32_t>::max())
      throw std::runtime_error("information equation has too many children");
    if (count && !input)
      throw std::runtime_error("information equation has a null child array");
    const std::uint64_t tokenItems =
        static_cast<std::uint64_t>(count) * tokensPerItem;
    if (tokensPerItem && tokenItems / tokensPerItem != count)
      throw std::runtime_error("information token count overflow");
    if (tokenCount > std::numeric_limits<std::uint64_t>::max() - tokenItems)
      throw std::runtime_error("information token stream overflow");
    for (std::uint64_t index = 0; index < tokenItems; ++index)
      validate_token(input[index]);

    EquationRecord &equation = equations[parent];
    equation.tokenOffset = tokenCount;
    equation.count = static_cast<std::uint32_t>(count);
    equation.kind = stored_kind(publicKind);
    if (tokenItems)
      append_tokens(input, tokenItems);
    tokenCount += tokenItems;
  }

  void append_tokens(const InformationToken *input, std::uint64_t count) {
    while (count) {
      const std::size_t available = TokenBufferCapacity - tokenBuffer.size();
      const std::size_t take =
          static_cast<std::size_t>(std::min<std::uint64_t>(available, count));
      tokenBuffer.insert(tokenBuffer.end(), input, input + take);
      input += take;
      count -= take;
      if (tokenBuffer.size() == TokenBufferCapacity)
        flush_tokens();
    }
  }

  void flush_tokens() {
    if (tokenBuffer.empty())
      return;
    tokensFile.append(
        tokenBuffer.data(),
        checked_bytes(tokenBuffer.size(), sizeof(InformationToken)));
    tokenBuffer.clear();
  }

  [[nodiscard]] bool token_value(InformationToken token) const {
    if (token == InformationTrue)
      return true;
    if (token == InformationFalse)
      return false;
    validate_token(token);
    if (!solved)
      throw std::runtime_error("information fixed point has not been solved");
    return values[token] != 0;
  }

  [[nodiscard]] std::uint32_t token_rank(InformationToken token) const {
    if (token == InformationTrue)
      return 0;
    if (!is_variable(token) || !values[token])
      throw std::runtime_error("rank requested for a false information token");
    return ranks[token];
  }

  void add_degree(InformationToken token) {
    if (is_variable(token)) {
      if (offsets[token] == std::numeric_limits<std::uint64_t>::max())
        throw std::runtime_error("information reverse degree overflow");
      ++offsets[token];
    }
  }

  void append_reverse(InformationToken child, const ReverseEdge &edge) {
    if (!is_variable(child))
      return;
    const std::uint64_t slot = cursors[child]++;
    if (slot >= offsets[child + 1])
      throw std::runtime_error("information reverse CSR cursor overflow");
    reverseEdges[slot] = edge;
  }

  void build_reverse_csr() {
    for (std::uint32_t parent = 0; parent < variableCount; ++parent)
      if (equations[parent].kind == StoredKind::Undefined)
        throw std::runtime_error(
            "information variable is missing its equation");

    flush_tokens();
    tokensFile.map_read_only(
        checked_bytes(tokenCount, sizeof(InformationToken)));
    // Pointer arithmetic on a null mapping is undefined even at offset
    // zero.  An all-empty-AND system legitimately has no token stream.
    tokens = tokenCount ? tokensFile.data<InformationToken>() : &emptyToken;

    offsetsFile = std::make_unique<ScratchFile>(scratchDirectory);
    offsetsFile->allocate(
        checked_bytes(std::uint64_t(variableCount) + 1, sizeof(std::uint64_t)));
    offsets = offsetsFile->data<std::uint64_t>();

    for (std::uint32_t parent = 0; parent < variableCount; ++parent) {
      const EquationRecord &equation = equations[parent];
      const InformationToken *children = tokens + equation.tokenOffset;
      if (equation.kind == StoredKind::Or || equation.kind == StoredKind::And) {
        for (std::uint32_t child = 0; child < equation.count; ++child)
          add_degree(children[child]);
      } else {
        for (std::uint32_t action = 0; action < equation.count; ++action) {
          const InformationToken first = children[2 * action];
          const InformationToken second = children[2 * action + 1];
          if (first == InformationFalse || second == InformationFalse)
            continue;
          add_degree(first);
          add_degree(second);
        }
      }
    }

    std::uint64_t total = 0;
    for (std::uint32_t variable = 0; variable < variableCount; ++variable) {
      const std::uint64_t degree = offsets[variable];
      offsets[variable] = total;
      if (total > std::numeric_limits<std::uint64_t>::max() - degree)
        throw std::runtime_error("information reverse edge count overflow");
      total += degree;
    }
    offsets[variableCount] = total;
    reverseEdgeCount = total;

    cursorsFile = std::make_unique<ScratchFile>(scratchDirectory);
    cursorsFile->allocate(checked_bytes(variableCount, sizeof(std::uint64_t)));
    cursors = cursorsFile->data<std::uint64_t>();
    std::copy(offsets, offsets + variableCount, cursors);

    reverseFile = std::make_unique<ScratchFile>(scratchDirectory);
    reverseFile->allocate(checked_bytes(reverseEdgeCount, sizeof(ReverseEdge)));
    reverseEdges = reverseFile->data<ReverseEdge>();

    for (std::uint32_t parent = 0; parent < variableCount; ++parent) {
      const EquationRecord &equation = equations[parent];
      const InformationToken *children = tokens + equation.tokenOffset;
      if (equation.kind == StoredKind::Or || equation.kind == StoredKind::And) {
        for (std::uint32_t child = 0; child < equation.count; ++child)
          append_reverse(children[child], {parent, child, InformationFalse});
      } else {
        for (std::uint32_t action = 0; action < equation.count; ++action) {
          const InformationToken first = children[2 * action];
          const InformationToken second = children[2 * action + 1];
          if (first == InformationFalse || second == InformationFalse)
            continue;
          append_reverse(first, {parent, action, second});
          append_reverse(second, {parent, action, first});
        }
      }
    }
    for (std::uint32_t variable = 0; variable < variableCount; ++variable)
      if (cursors[variable] != offsets[variable + 1])
        throw std::runtime_error("information reverse CSR fill mismatch");

    // Cursor storage is no longer needed after construction.
    cursors = nullptr;
    cursorsFile.reset();
    finalized = true;
  }

  void allocate_solution() {
    valuesFile = std::make_unique<ScratchFile>(scratchDirectory);
    valuesFile->allocate(checked_bytes(variableCount, sizeof(std::uint8_t)));
    values = valuesFile->data<std::uint8_t>();

    settledFile = std::make_unique<ScratchFile>(scratchDirectory);
    settledFile->allocate(checked_bytes(variableCount, sizeof(std::uint8_t)));
    settled = settledFile->data<std::uint8_t>();

    ranksFile = std::make_unique<ScratchFile>(scratchDirectory);
    ranksFile->allocate(checked_bytes(variableCount, sizeof(std::uint32_t)));
    ranks = ranksFile->data<std::uint32_t>();

    witnessesFile = std::make_unique<ScratchFile>(scratchDirectory);
    witnessesFile->allocate(
        checked_bytes(variableCount, sizeof(std::uint32_t)));
    witnesses = witnessesFile->data<std::uint32_t>();
    std::fill(witnesses, witnesses + variableCount, InformationNoWitness);

    remainingFile = std::make_unique<ScratchFile>(scratchDirectory);
    remainingFile->allocate(
        checked_bytes(variableCount, sizeof(std::uint64_t)));
    remaining = remainingFile->data<std::uint64_t>();

    queueFile = std::make_unique<ScratchFile>(scratchDirectory);
    queueFile->allocate(checked_bytes(variableCount, sizeof(std::uint32_t)));
    queue = queueFile->data<std::uint32_t>();
  }

  void activate(std::uint32_t variable, std::uint32_t rank,
                std::uint32_t witness) {
    if (values[variable])
      return;
    if (!rank)
      throw std::runtime_error("information variable activated at rank zero");
    if (queueTail >= variableCount)
      throw std::runtime_error("information activation queue overflow");
    values[variable] = 1;
    ranks[variable] = rank;
    witnesses[variable] = witness;
    queue[queueTail++] = variable;
  }

  void initialize_gates() {
    for (std::uint32_t parent = 0; parent < variableCount; ++parent) {
      const EquationRecord &equation = equations[parent];
      const InformationToken *children = tokens + equation.tokenOffset;
      if (equation.kind == StoredKind::Or) {
        for (std::uint32_t child = 0; child < equation.count; ++child)
          if (children[child] == InformationTrue) {
            activate(parent, 1, child);
            break;
          }
      } else if (equation.kind == StoredKind::And) {
        bool blocked = false;
        std::uint64_t variables = 0;
        std::uint32_t constantWitness = InformationNoWitness;
        for (std::uint32_t child = 0; child < equation.count; ++child) {
          if (children[child] == InformationFalse) {
            blocked = true;
            break;
          }
          if (children[child] == InformationTrue)
            constantWitness = child;
          else
            ++variables;
        }
        if (blocked)
          remaining[parent] = std::numeric_limits<std::uint64_t>::max();
        else {
          remaining[parent] = variables;
          if (!variables)
            activate(parent, 1, constantWitness);
        }
      } else {
        for (std::uint32_t action = 0; action < equation.count; ++action)
          if (children[2 * action] == InformationTrue &&
              children[2 * action + 1] == InformationTrue) {
            activate(parent, 1, action);
            break;
          }
      }
    }
  }

  void propagate() {
    while (queueHead < queueTail) {
      const std::uint32_t child = queue[queueHead++];
      if (settled[child])
        throw std::runtime_error("information variable settled more than once");
      settled[child] = 1;
      const std::uint32_t childRank = ranks[child];
      for (std::uint64_t slot = offsets[child]; slot < offsets[child + 1];
           ++slot) {
        const ReverseEdge &edge = reverseEdges[slot];
        if (values[edge.parent])
          continue;
        const StoredKind kind = equations[edge.parent].kind;
        if (kind == StoredKind::Or)
          activate(edge.parent, checked_rank_successor(childRank),
                   edge.witness);
        else if (kind == StoredKind::And) {
          if (remaining[edge.parent] ==
              std::numeric_limits<std::uint64_t>::max())
            continue;
          if (!remaining[edge.parent])
            throw std::runtime_error("information AND counter underflow");
          if (!--remaining[edge.parent])
            activate(edge.parent, checked_rank_successor(childRank),
                     edge.witness);
        } else if (kind == StoredKind::OrOfPairs) {
          const InformationToken other = edge.auxiliary;
          const bool otherSettled = other == InformationTrue ||
                                    (is_variable(other) && settled[other]);
          if (otherSettled) {
            const std::uint32_t otherRank =
                other == InformationTrue ? 0 : ranks[other];
            activate(edge.parent,
                     checked_rank_successor(std::max(childRank, otherRank)),
                     edge.witness);
          }
        } else
          throw std::runtime_error(
              "undefined information equation in propagation");
      }
    }
  }

  [[nodiscard]] InformationSolveSummary verify_solution() const {
    if (!solved)
      throw std::runtime_error(
          "cannot verify an unsolved information fixed point");
    InformationSolveSummary summary;
    summary.variables = variableCount;
    summary.reverseEdges = reverseEdgeCount;

    for (std::uint32_t parent = 0; parent < variableCount; ++parent) {
      const EquationRecord &equation = equations[parent];
      const InformationToken *children = tokens + equation.tokenOffset;
      bool expected = false;
      std::uint32_t expectedRank = 0;
      bool witnessValid = false;

      if (equation.kind == StoredKind::Or) {
        std::uint32_t best = std::numeric_limits<std::uint32_t>::max();
        for (std::uint32_t child = 0; child < equation.count; ++child)
          if (token_value(children[child])) {
            expected = true;
            best = std::min(
                best, checked_rank_successor(token_rank(children[child])));
          }
        if (expected) {
          expectedRank = best;
          const std::uint32_t witness = witnesses[parent];
          witnessValid =
              witness < equation.count && token_value(children[witness]) &&
              checked_rank_successor(token_rank(children[witness])) ==
                  expectedRank;
        }
      } else if (equation.kind == StoredKind::And) {
        expected = true;
        std::uint32_t maximum = 0;
        for (std::uint32_t child = 0; child < equation.count; ++child) {
          if (!token_value(children[child])) {
            expected = false;
            break;
          }
          maximum = std::max(maximum, token_rank(children[child]));
        }
        if (expected) {
          expectedRank = checked_rank_successor(maximum);
          if (!equation.count)
            witnessValid = witnesses[parent] == InformationNoWitness;
          else {
            const std::uint32_t witness = witnesses[parent];
            witnessValid = witness < equation.count &&
                           token_value(children[witness]) &&
                           token_rank(children[witness]) == maximum;
          }
        }
      } else if (equation.kind == StoredKind::OrOfPairs) {
        std::uint32_t best = std::numeric_limits<std::uint32_t>::max();
        for (std::uint32_t action = 0; action < equation.count; ++action) {
          const InformationToken first = children[2 * action];
          const InformationToken second = children[2 * action + 1];
          if (!token_value(first) || !token_value(second))
            continue;
          expected = true;
          best = std::min(best, checked_rank_successor(std::max(
                                    token_rank(first), token_rank(second))));
        }
        if (expected) {
          expectedRank = best;
          const std::uint32_t witness = witnesses[parent];
          if (witness < equation.count) {
            const InformationToken first = children[2 * witness];
            const InformationToken second = children[2 * witness + 1];
            witnessValid =
                token_value(first) && token_value(second) &&
                checked_rank_successor(std::max(
                    token_rank(first), token_rank(second))) == expectedRank;
          }
        }
      } else
        throw std::runtime_error(
            "undefined information equation in verification");

      const bool actual = values[parent] != 0;
      summary.activated += actual;
      if (actual != expected)
        ++summary.bellmanResidual;
      if (actual) {
        if (ranks[parent] != expectedRank || !witnessValid)
          ++summary.rankResidual;
      } else if (ranks[parent] || witnesses[parent] != InformationNoWitness)
        ++summary.rankResidual;
    }
    return summary;
  }

  std::uint32_t variableCount;
  static constexpr std::size_t TokenBufferCapacity = 1U << 20;
  std::string scratchDirectory;
  ScratchFile equationsFile;
  ScratchFile tokensFile;
  EquationRecord *equations = nullptr;
  const InformationToken *tokens = nullptr;
  InformationToken emptyToken = InformationFalse;
  std::uint64_t tokenCount = 0;
  std::vector<InformationToken> tokenBuffer;
  std::uint64_t reverseEdgeCount = 0;
  bool finalized = false;
  bool solved = false;

  std::unique_ptr<ScratchFile> offsetsFile;
  std::unique_ptr<ScratchFile> cursorsFile;
  std::unique_ptr<ScratchFile> reverseFile;
  std::unique_ptr<ScratchFile> valuesFile;
  std::unique_ptr<ScratchFile> settledFile;
  std::unique_ptr<ScratchFile> ranksFile;
  std::unique_ptr<ScratchFile> witnessesFile;
  std::unique_ptr<ScratchFile> remainingFile;
  std::unique_ptr<ScratchFile> queueFile;

  std::uint64_t *offsets = nullptr;
  std::uint64_t *cursors = nullptr;
  ReverseEdge *reverseEdges = nullptr;
  std::uint8_t *values = nullptr;
  std::uint8_t *settled = nullptr;
  std::uint32_t *ranks = nullptr;
  std::uint32_t *witnesses = nullptr;
  std::uint64_t *remaining = nullptr;
  std::uint32_t *queue = nullptr;
  std::uint64_t queueHead = 0;
  std::uint64_t queueTail = 0;
};

InformationFixedPoint::InformationFixedPoint(std::uint32_t variableCount,
                                             std::string scratchDirectory)
    : impl_(
          std::make_unique<Impl>(variableCount, std::move(scratchDirectory))) {}

InformationFixedPoint::~InformationFixedPoint() = default;
InformationFixedPoint::InformationFixedPoint(
    InformationFixedPoint &&) noexcept = default;
InformationFixedPoint &
InformationFixedPoint::operator=(InformationFixedPoint &&) noexcept = default;

void InformationFixedPoint::define_or(std::uint32_t parent,
                                      const InformationToken *children,
                                      std::size_t count) {
  impl_->define(parent, InformationEquationKind::Or, children, count, 1);
}

void InformationFixedPoint::define_and(std::uint32_t parent,
                                       const InformationToken *children,
                                       std::size_t count) {
  impl_->define(parent, InformationEquationKind::And, children, count, 1);
}

void InformationFixedPoint::define_or_of_pairs(std::uint32_t parent,
                                               const InformationPair *pairs,
                                               std::size_t count) {
  impl_->define(parent, InformationEquationKind::OrOfPairs,
                reinterpret_cast<const InformationToken *>(pairs), count, 2);
}

InformationSolveSummary InformationFixedPoint::solve() {
  if (impl_->solved)
    throw std::runtime_error("information fixed point has already been solved");
  impl_->build_reverse_csr();
  impl_->allocate_solution();
  impl_->initialize_gates();
  impl_->propagate();
  impl_->solved = true;
  const InformationSolveSummary summary = impl_->verify_solution();
  if (summary.bellmanResidual || summary.rankResidual)
    throw std::runtime_error(
        "information fixed-point verification has a residual");
  return summary;
}

InformationSolveSummary InformationFixedPoint::verify() const {
  return impl_->verify_solution();
}

bool InformationFixedPoint::value(InformationToken token) const {
  return impl_->token_value(token);
}

std::uint32_t
InformationFixedPoint::activation_rank(std::uint32_t variable) const {
  impl_->validate_variable(variable);
  if (!impl_->solved)
    throw std::runtime_error("information fixed point has not been solved");
  return impl_->ranks[variable];
}

std::uint32_t
InformationFixedPoint::witness_index(std::uint32_t variable) const {
  impl_->validate_variable(variable);
  if (!impl_->solved)
    throw std::runtime_error("information fixed point has not been solved");
  return impl_->witnesses[variable];
}

} // namespace Stockfish::Ultimate
