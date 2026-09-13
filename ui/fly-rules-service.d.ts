export function validGame(value: unknown): boolean;
export function refereeGame(game: unknown, signal?: AbortSignal): Promise<{status: number; body: Record<string, unknown>}>;
