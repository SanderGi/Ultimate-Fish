// Exact memoization of the stateless, checkpoint-specific circuit policy. GPLv3+.
export function createFlyPolicy(readout, command, cacheSize = 4096) {
  const scoresByFeatures = new Map();
  return async function choose(candidates) {
    // No rounding: only identical serialized inputs share a result. Preserve the
    // original candidate order, including first-candidate tie breaking.
    const keys = candidates.map(features => features.join(' '));
    const scoresByKey = new Map();
    const missing = [...new Set(keys)].filter(key => {
      if (!scoresByFeatures.has(key)) return true;
      const score = scoresByFeatures.get(key);
      scoresByFeatures.delete(key);
      scoresByFeatures.set(key, score);
      scoresByKey.set(key, score);
      return false;
    });
    if (missing.length) {
      const rows = await command(`${missing.length} ${missing.join(' ')}`);
      for (let j = 0; j < missing.length; j++) {
        const row = rows[j];
        if (!Array.isArray(row) || row.length !== readout.weights.length ||
            !row.every(Number.isFinite)) throw Error('Invalid fly motor outputs');
        // Keep the trained readout's operation and summation order unchanged.
        const score = row.reduce((sum, v, i) =>
          sum + (v - readout.mean[i]) / readout.scale[i] * readout.weights[i], 0);
        scoresByKey.set(missing[j], score);
        scoresByFeatures.set(missing[j], score);
        if (scoresByFeatures.size > cacheSize)
          scoresByFeatures.delete(scoresByFeatures.keys().next().value);
      }
    }
    const scores = keys.map(key => scoresByKey.get(key));
    const index = scores.indexOf(Math.max(...scores));
    // Always record the selected action's actual full-circuit response, even
    // when its score was cached or it appeared elsewhere in the input list.
    await command(`1 ${keys[index]}`);
    const telemetry = await command('0');
    return {index, scores, ...telemetry};
  };
}
