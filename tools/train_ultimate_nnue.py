#!/usr/bin/env python3
"""Train and serialize the compact Ultimate Fish sparse neural evaluator."""

from __future__ import annotations

import argparse
import json
import os
import struct
import subprocess
from pathlib import Path

import numpy as np


PIECES = (
    "king", "jester", "knight", "pawn", "queen", "rook", "bishop",
    "berserker", "bomb", "ninja", "turtle", "ghost", "mage", "goop",
    "penguin", "parasite", "devil", "minion", "sludge", "sniper",
    "prince", "checker", "checkerKing", "giant", "copycat",
    "copycatClone", "angel", "halo", "fisherman", "dragon",
)
PIECE_INDEX = {name: index for index, name in enumerate(PIECES)}
HIDDEN = 32
LOCATION_SQUARES = 81
STATE_BUCKETS = 8
LOCATION_FEATURES = 2 * len(PIECES) * LOCATION_SQUARES
STATE_GROUPS = 37
GLOBAL_FEATURES = 84
INPUTS = LOCATION_FEATURES + 2 * len(PIECES) * STATE_GROUPS + GLOBAL_FEATURES
ACTIVATION_SCALE = 4096
OUTPUT_WEIGHT_SCALE = 16
OUTPUT_SCALE = ACTIVATION_SCALE * OUTPUT_WEIGHT_SCALE
MAGIC = b"UFNNUE1\0"


def square_index(name: str) -> int:
    if len(name) < 2 or name[0] not in "abcdefgh":
        raise ValueError(f"bad square {name!r}")
    rank = int(name[1:])
    if not 1 <= rank <= 10:
        raise ValueError(f"bad square {name!r}")
    return (rank - 1) * 8 + ord(name[0]) - ord("a")


def parse_pieces(upn: str) -> list[tuple[int, str, int, list[int]]]:
    pieces = []
    for field in upn.split(";")[1:]:
        if "=" in field:
            continue
        values = field.split(",")
        if len(values) < 3 or values[0] not in PIECE_INDEX:
            raise ValueError(f"bad UPN piece field {field!r}")
        defaults = [0, 0, 0, 0, 0, 1, -1, 1, -1, 0]
        for index, value in enumerate(values[3:13]):
            defaults[index] = int(value)
        pieces.append((PIECE_INDEX[values[0]], values[1], square_index(values[2]), defaults))
    return pieces


def features(upn: str, perspective: str) -> list[int]:
    result: list[int] = []
    fields = upn.split(";")
    metadata = {field.split("=", 1)[0]: field.split("=", 1)[1]
                for field in fields[1:] if "=" in field}
    forced = int(metadata.get("forced", "-1"))
    for piece_id, (piece_type, color, square, state) in enumerate(parse_pieces(upn)):
        action, cooldown, freeze, power, moved, visible, link, on_board, host, _order = state
        relative = piece_type + (0 if color == perspective else len(PIECES))
        oriented = square if perspective == "w" else 79 - square
        location = oriented if on_board else 80
        result.append(relative * LOCATION_SQUARES + location)
        group = 0
        if moved:
            result.append(LOCATION_FEATURES + relative * STATE_GROUPS + group)
        group += 1
        if not visible:
            result.append(LOCATION_FEATURES + relative * STATE_GROUPS + group)
        group += 1
        for value in (cooldown, freeze, power):
            if value:
                result.append(LOCATION_FEATURES + relative * STATE_GROUPS + group +
                              min(value, STATE_BUCKETS) - 1)
            group += STATE_BUCKETS
        for bit in range(8):
            if action & (1 << bit):
                result.append(LOCATION_FEATURES + relative * STATE_GROUPS + group + bit)
        group += 8
        if link != -1:
            result.append(LOCATION_FEATURES + relative * STATE_GROUPS + group)
        group += 1
        if host != -1:
            result.append(LOCATION_FEATURES + relative * STATE_GROUPS + group)
        group += 1
        if piece_id == forced:
            result.append(LOCATION_FEATURES + relative * STATE_GROUPS + group)
    global_offset = LOCATION_FEATURES + 2 * len(PIECES) * STATE_GROUPS
    if fields[0] == perspective:
        result.append(global_offset)
    continuation = int(metadata.get("cont", "0"))
    if continuation:
        result.append(global_offset + continuation)
    en_passant = metadata.get("ep", "-")
    if en_passant != "-":
        square = square_index(en_passant)
        result.append(global_offset + 3 + (square if perspective == "w" else 79 - square))
    return result


def feature_matrix(records: list[dict[str, object]], perspective: str):
    from scipy import sparse
    rows: list[int] = []
    columns: list[int] = []
    for row, record in enumerate(records):
        active = features(str(record["upn"]), perspective)
        rows.extend([row] * len(active))
        columns.extend(active)
    values = np.ones(len(rows), dtype=np.float32)
    return sparse.csr_matrix((values, (rows, columns)), shape=(len(records), INPUTS))


def load_records(paths: list[Path]) -> list[dict[str, object]]:
    records: list[dict[str, object]] = []
    for path in paths:
        with path.open(encoding="utf-8") as stream:
            for line in stream:
                if line.strip():
                    records.append(json.loads(line))
    if not records:
        raise ValueError("training dataset is empty")
    return records


def target(record: dict[str, object], outcome_weight: float) -> float:
    white_score = float(record["score_stm"]) * (1 if str(record["upn"])[0] == "w" else -1)
    white_score = float(np.clip(white_score, -2500, 2500))
    outcome = 1200.0 * int(record.get("result_white", 0))
    # Search remains the primary teacher; game outcome corrects horizon bias.
    teacher = (1.0 - outcome_weight) * white_score + outcome_weight * outcome
    static_white = float(record.get("static_stm", 0)) * (
        1 if str(record["upn"])[0] == "w" else -1)
    return float(np.clip(teacher - static_white, -3000, 3000)) / 1000.0


def quantize(embedding: np.ndarray, hidden_bias: np.ndarray,
             output_weights: np.ndarray, output_bias: float) -> tuple[np.ndarray, ...]:
    q_embedding = np.clip(np.rint(embedding * ACTIVATION_SCALE), -32768, 32767).astype("<i2")
    q_hidden_bias = np.rint(hidden_bias * ACTIVATION_SCALE).astype("<i4")
    q_output_weights = np.clip(
        np.rint(output_weights * OUTPUT_WEIGHT_SCALE), -32768, 32767).astype("<i2")
    q_output_bias = np.asarray(round(output_bias * OUTPUT_SCALE), dtype="<i4")
    return q_embedding, q_hidden_bias, q_output_weights, q_output_bias


def integer_predict(x_white, x_black,
                    quantized: tuple[np.ndarray, ...]) -> np.ndarray:
    embedding, hidden_bias, output_weights, output_bias = quantized
    white = np.clip(x_white @ embedding.astype(np.int32) + hidden_bias, 0, ACTIVATION_SCALE)
    black = np.clip(x_black @ embedding.astype(np.int32) + hidden_bias, 0, ACTIVATION_SCALE)
    total = (white.astype(np.int64) @ output_weights[0].astype(np.int64) +
             black.astype(np.int64) @ output_weights[1].astype(np.int64) +
             int(output_bias))
    return np.trunc(total / OUTPUT_SCALE).astype(np.int32)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("data", nargs="+", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--epochs", type=int, default=12)
    parser.add_argument("--batch-size", type=int, default=512)
    parser.add_argument("--learning-rate", type=float, default=0.003)
    parser.add_argument("--seed", type=int, default=20260806)
    parser.add_argument("--outcome-weight", type=float, default=0.2)
    parser.add_argument("--verify-engine",
                        help="fresh Ultimate Fish binary used for C++/Python parity")
    args = parser.parse_args()
    records = load_records(args.data)
    rng = np.random.default_rng(args.seed)
    games = sorted({int(record["game"]) for record in records})
    # Hold out complete games, not adjacent positions from the same trajectory.
    # Every fifth game gives a stable 20% split while cycling across fixtures.
    validation_games = set(games[::5])
    train_indices = np.asarray(
        [i for i, record in enumerate(records) if int(record["game"]) not in validation_games])
    valid_indices = np.asarray(
        [i for i, record in enumerate(records) if int(record["game"]) in validation_games])
    if not len(train_indices) or not len(valid_indices):
        order = rng.permutation(len(records))
        split = max(1, int(len(records) * 0.8))
        train_indices, valid_indices = order[:split], order[split:]

    print(f"building sparse features for {len(records)} records ({INPUTS} inputs)")
    x_white = feature_matrix(records, "w")
    x_black = feature_matrix(records, "b")
    if not 0 <= args.outcome_weight <= 1:
        parser.error("--outcome-weight must be between zero and one")
    targets = np.asarray(
        [target(record, args.outcome_weight) for record in records], dtype=np.float32)
    baseline_mae = float(np.mean(np.abs(targets[valid_indices]))) * 1000.0
    print(f"zero-residual validation MAE {baseline_mae:.2f} cp")

    embedding = rng.normal(0, 0.003, (INPUTS, HIDDEN)).astype(np.float32)
    hidden_bias = np.full(HIDDEN, 0.10, dtype=np.float32)
    output_weights = rng.normal(0, 0.02, (2, HIDDEN)).astype(np.float32)
    output_bias = np.float32(0)
    velocity_embedding = np.zeros_like(embedding)
    velocity_hidden = np.zeros_like(hidden_bias)
    velocity_output = np.zeros_like(output_weights)
    velocity_bias = np.float32(0)
    momentum = 0.9
    best_mae = float("inf")
    best_parameters: tuple[np.ndarray, np.ndarray, np.ndarray, float] | None = None

    for epoch in range(args.epochs):
        shuffled = rng.permutation(train_indices)
        for start in range(0, len(shuffled), args.batch_size):
            indices = shuffled[start:start + args.batch_size]
            white_pre = x_white[indices] @ embedding + hidden_bias
            black_pre = x_black[indices] @ embedding + hidden_bias
            white = np.clip(white_pre, 0, 1)
            black = np.clip(black_pre, 0, 1)
            prediction = white @ output_weights[0] + black @ output_weights[1] + output_bias
            residual = prediction - targets[indices]
            # Huber gradients prevent mate scores and tactical outliers from dominating.
            gradient = np.clip(residual, -0.4, 0.4) / len(indices)
            grad_output = np.stack((white.T @ gradient, black.T @ gradient))
            grad_bias = gradient.sum()
            grad_white = gradient[:, None] * output_weights[0]
            grad_black = gradient[:, None] * output_weights[1]
            grad_white *= (white_pre > 0) & (white_pre < 1)
            grad_black *= (black_pre > 0) & (black_pre < 1)
            grad_embedding = np.asarray(
                x_white[indices].T @ grad_white + x_black[indices].T @ grad_black,
                dtype=np.float32,
            )
            grad_hidden = grad_white.sum(axis=0) + grad_black.sum(axis=0)

            velocity_embedding = momentum * velocity_embedding + grad_embedding
            velocity_hidden = momentum * velocity_hidden + grad_hidden
            velocity_output = momentum * velocity_output + grad_output
            velocity_bias = momentum * velocity_bias + grad_bias
            embedding -= args.learning_rate * velocity_embedding
            hidden_bias -= args.learning_rate * velocity_hidden
            output_weights -= args.learning_rate * velocity_output
            output_bias -= args.learning_rate * velocity_bias

        valid_white = np.clip(x_white[valid_indices] @ embedding + hidden_bias, 0, 1)
        valid_black = np.clip(x_black[valid_indices] @ embedding + hidden_bias, 0, 1)
        valid_prediction = (valid_white @ output_weights[0] +
                            valid_black @ output_weights[1] + output_bias)
        mae = float(np.mean(np.abs(valid_prediction - targets[valid_indices]))) * 1000.0
        if mae < best_mae:
            best_mae = mae
            best_parameters = (embedding.copy(), hidden_bias.copy(),
                               output_weights.copy(), float(output_bias))
        print(f"epoch {epoch + 1}/{args.epochs}: validation MAE {mae:.2f} cp"
              f" (best {best_mae:.2f})")

    assert best_parameters is not None
    embedding, hidden_bias, output_weights, output_bias_value = best_parameters

    quantized = quantize(embedding, hidden_bias, output_weights * 1000.0,
                         output_bias_value * 1000.0)
    integer = integer_predict(x_white[valid_indices], x_black[valid_indices], quantized)
    float_prediction = (np.clip(x_white[valid_indices] @ embedding + hidden_bias, 0, 1) @
                        output_weights[0] +
                        np.clip(x_black[valid_indices] @ embedding + hidden_bias, 0, 1) @
                        output_weights[1] + output_bias_value) * 1000.0
    parity = np.abs(integer - np.trunc(float_prediction).astype(np.int32))
    print(f"quantization parity: mean {parity.mean():.3f} cp, max {parity.max()} cp")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    q_embedding, q_hidden, q_output, q_bias = quantized
    with args.output.open("wb") as stream:
        stream.write(struct.pack("<8sIIIII", MAGIC, 1, INPUTS, HIDDEN,
                                 ACTIVATION_SCALE, OUTPUT_SCALE))
        stream.write(q_embedding.tobytes(order="C"))
        stream.write(q_hidden.tobytes(order="C"))
        stream.write(q_output.tobytes(order="C"))
        stream.write(q_bias.tobytes())
    metadata = {
        "version": 1,
        "records": len(records),
        "training_records": len(train_indices),
        "validation_records": len(valid_indices),
        "inputs": INPUTS,
        "hidden": HIDDEN,
        "outcome_weight": args.outcome_weight,
        "validation_mae_cp": best_mae,
        "zero_residual_validation_mae_cp": baseline_mae,
        "quantization_mean_error_cp": float(parity.mean()),
        "quantization_max_error_cp": int(parity.max()),
    }
    args.output.with_suffix(args.output.suffix + ".json").write_text(
        json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {args.output} ({args.output.stat().st_size} bytes)")
    if args.verify_engine:
        environment = dict(os.environ)
        environment["ULTIMATE_NNUE_FILE"] = str(args.output.resolve())
        process = subprocess.Popen(
            [args.verify_engine], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, text=True, bufsize=1, env=environment,
        )
        assert process.stdin is not None and process.stdout is not None
        checked = 0
        try:
            for offset, index in enumerate(valid_indices[:100]):
                upn = str(records[int(index)]["upn"])
                process.stdin.write("position upn " + upn + "\n")
                process.stdin.flush()
                if process.stdout.readline().strip() != "positionok":
                    raise RuntimeError("engine rejected validation UPN")
                process.stdin.write("eval\n")
                process.stdin.flush()
                response = process.stdout.readline().split()
                actual = int(response[2])
                expected = (int(integer[offset]) * (1 if upn[0] == "w" else -1) +
                            int(records[int(index)].get("static_stm", 0)))
                if actual != expected:
                    raise RuntimeError(
                        f"NNUE parity failed at validation record {index}: "
                        f"C++ {actual}, Python {expected}")
                checked += 1
        finally:
            if process.poll() is None:
                process.stdin.write("quit\n")
                process.stdin.flush()
                process.wait(timeout=5)
        print(f"C++ inference parity: {checked}/{checked} exact")


if __name__ == "__main__":
    main()
