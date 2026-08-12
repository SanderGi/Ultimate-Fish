export const MAX_ENGINE_SEARCH_DEPTH = 50;

export function normalizedSearchDepth(
  requestedDepth,
  fallbackDepth = 4,
  maximumDepth = MAX_ENGINE_SEARCH_DEPTH,
) {
  return Math.max(
    1,
    Math.min(maximumDepth, Number(requestedDepth) || fallbackDepth),
  );
}
