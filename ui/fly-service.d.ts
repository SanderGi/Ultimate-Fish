export function validCandidates(value: unknown): boolean;
export function isSameOrigin(request: Request): boolean;
export function inferFly(candidates: unknown): Promise<{status: number; body: Record<string, unknown>}>;
