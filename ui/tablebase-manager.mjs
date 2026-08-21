import { createHash } from "node:crypto";
import { createWriteStream, readFileSync } from "node:fs";
import {
  lstat,
  mkdir,
  readdir,
  rename,
  rm,
  statfs,
} from "node:fs/promises";
import path from "node:path";
import os from "node:os";
import { pipeline } from "node:stream/promises";

const DEFAULT_DATASET = "SanderGi/Ultimate-Fish-Tablebases";
const DEFAULT_HF_ORIGIN = "https://huggingface.co";
const CATALOG_TTL_MS = 5 * 60 * 1000;
const SIDECAR_EXTENSIONS = new Set([
  ".ufcross", ".ufgb", ".ufgd", ".ufgf", ".ufgg", ".ufgi", ".ufgm",
  ".ufgp", ".ufgx", ".uficapture", ".ufiw", ".ufja", ".ufjg", ".ufmg",
  ".ufog",
]);
const MANAGED_EXTENSIONS = new Set([".uftb", ...SIDECAR_EXTENSIONS]);
const SAFE_FILENAME = /^[a-z0-9][a-z0-9-]*\.(?:uftb|ufcross|ufgb|ufgd|ufgf|ufgg|ufgi|ufgm|ufgp|ufgx|uficapture|ufiw|ufja|ufjg|ufmg|ufog)$/;
const PIECE_NAMES = new Map([
  ["fisherman", "Fisherman"],
  ["berserker", "Berserker"],
  ["copycat", "CopyCat"],
  ["parasite", "Parasite"],
  ["penguin", "Penguin"],
  ["checker", "Checker"],
  ["jester", "Jester"],
  ["knight", "Knight"],
  ["bishop", "Bishop"],
  ["dragon", "Dragon"],
  ["sniper", "Sniper"],
  ["prince", "Prince"],
  ["turtle", "Turtle"],
  ["queen", "Queen"],
  ["ghost", "Ghost"],
  ["giant", "Giant"],
  ["angel", "Angel"],
  ["ninja", "Ninja"],
  ["bomb", "Bomb"],
  ["mage", "Mage"],
  ["pawn", "Pawn"],
  ["rook", "Rook"],
  ["q", "Queen"],
  ["r", "Rook"],
]);
const PIECE_TOKENS = [...PIECE_NAMES.keys()].sort((left, right) =>
  right.length - left.length);

function splitPieces(text, memo = new Map()) {
  if (!text) return [];
  if (memo.has(text)) return memo.get(text);
  for (const token of PIECE_TOKENS) {
    if (!text.startsWith(token)) continue;
    const rest = splitPieces(text.slice(token.length), memo);
    if (rest) {
      const result = [PIECE_NAMES.get(token), ...rest];
      memo.set(text, result);
      return result;
    }
  }
  memo.set(text, null);
  return null;
}

export function materialName(filename) {
  const stem = path.basename(filename, path.extname(filename));
  if (!stem.startsWith("k")) return stem;
  for (let index = 1; index < stem.length; index += 1) {
    if (stem[index] !== "k") continue;
    const first = splitPieces(stem.slice(1, index));
    const second = splitPieces(stem.slice(index + 1));
    if (!first || !second) continue;
    const side = (pieces) => ["King", ...pieces].join(" + ");
    return `${side(first)} vs ${side(second)}`;
  }
  return stem;
}

function managedFilename(filename) {
  return typeof filename === "string" && SAFE_FILENAME.test(filename) &&
    path.basename(filename) === filename;
}

function fileStem(filename) {
  return filename.slice(0, -path.extname(filename).length);
}

function authorizationHeaders(token) {
  return token ? { authorization: `Bearer ${token}` } : {};
}

function nextLink(header) {
  if (!header) return null;
  for (const part of header.split(",")) {
    const match = part.match(/<([^>]+)>;\s*rel="?next"?/i);
    if (match) return match[1];
  }
  return null;
}

function lfsSha256(entry) {
  const oid = entry?.lfs?.oid ?? entry?.lfs?.sha256;
  const digest = typeof oid === "string" ? oid.replace(/^sha256:/, "") : "";
  return /^[0-9a-f]{64}$/i.test(digest) ? digest.toLowerCase() : null;
}

async function existingStatfsTarget(directory) {
  let candidate = directory;
  while (true) {
    try {
      await lstat(candidate);
      return candidate;
    } catch (error) {
      const parent = path.dirname(candidate);
      if (parent === candidate) throw error;
      candidate = parent;
    }
  }
}

export function configuredTablebaseDirectory(uiDirectory, configured) {
  if (!configured)
    return path.resolve(uiDirectory, "../tablebases");
  const entries = configured.split(path.delimiter).filter(Boolean);
  const directory = entries.find((entry) => path.extname(entry) !== ".uftb");
  if (!directory)
    throw new Error(
      "ULTIMATE_TABLEBASE_PATH must include a directory to manage downloads.",
    );
  return path.resolve(directory);
}

export function resolveHuggingFaceToken({
  env = process.env,
  homeDirectory = os.homedir(),
} = {}) {
  for (const name of ["HF_TOKEN", "HUGGING_FACE_HUB_TOKEN", "HUGGINGFACE_TOKEN"]) {
    const value = env[name]?.trim();
    if (value) return value;
  }
  const cacheRoot = env.HF_HOME
    ? path.resolve(env.HF_HOME)
    : env.XDG_CACHE_HOME
      ? path.resolve(env.XDG_CACHE_HOME, "huggingface")
      : path.resolve(homeDirectory, ".cache/huggingface");
  const candidates = [
    env.HF_TOKEN_PATH && path.resolve(env.HF_TOKEN_PATH),
    path.join(cacheRoot, "token"),
    path.resolve(homeDirectory, ".huggingface/token"),
  ].filter(Boolean);
  for (const candidate of candidates) {
    try {
      const value = readFileSync(candidate, "utf8").trim();
      if (value) return value;
    } catch (error) {
      if (error?.code !== "ENOENT" && error?.code !== "EACCES") throw error;
    }
  }
  return null;
}

export function createTablebaseManager({
  directory,
  dataset = DEFAULT_DATASET,
  revision = "main",
  hfOrigin = DEFAULT_HF_ORIGIN,
  token,
  fetchImpl = globalThis.fetch,
  catalogTtlMs = CATALOG_TTL_MS,
} = {}) {
  if (!directory) throw new Error("A tablebase directory is required.");
  if (typeof fetchImpl !== "function") throw new Error("fetch is unavailable.");
  const targetDirectory = path.resolve(directory);
  const origin = hfOrigin.replace(/\/$/, "");
  const encodedDataset = dataset.split("/").map(encodeURIComponent).join("/");
  const encodedRevision = encodeURIComponent(revision);
  let catalogCache = null;
  let catalogFetchedAt = 0;
  const jobs = new Map();

  async function fetchCatalog({ force = false } = {}) {
    if (!force && catalogCache && Date.now() - catalogFetchedAt < catalogTtlMs)
      return catalogCache;
    let url = `${origin}/api/datasets/${encodedDataset}/tree/${encodedRevision}/tablebases?recursive=true&expand=false`;
    const files = [];
    while (url) {
      const response = await fetchImpl(url, {
        headers: authorizationHeaders(token),
      });
      if (!response.ok) {
        const accessHint = response.status === 401 || response.status === 403
          ? " Set HF_TOKEN or HUGGING_FACE_HUB_TOKEN to an approved dataset token."
          : "";
        throw new Error(
          `Hugging Face catalog request failed (${response.status}).${accessHint}`,
        );
      }
      const page = await response.json();
      if (!Array.isArray(page))
        throw new Error("Hugging Face returned an invalid tablebase catalog.");
      for (const entry of page) {
        if (entry?.type !== "file" || typeof entry.path !== "string") continue;
        const filename = path.posix.basename(entry.path);
        if (!entry.path.startsWith("tablebases/") || !managedFilename(filename))
          continue;
        const size = Number(entry.lfs?.size ?? entry.size);
        if (!Number.isSafeInteger(size) || size < 0) continue;
        files.push({ filename, size, sha256: lfsSha256(entry) });
      }
      const next = nextLink(response.headers.get("link"));
      url = next ? new URL(next, origin).toString() : null;
    }
    const groups = new Map();
    for (const file of files) {
      const stem = fileStem(file.filename);
      if (!groups.has(stem)) groups.set(stem, []);
      groups.get(stem).push(file);
    }
    catalogCache = [...groups.entries()]
      .filter(([, group]) => group.some((file) => file.filename.endsWith(".uftb")))
      .map(([stem, group]) => {
        const primary = group.find((file) => file.filename === `${stem}.uftb`);
        return {
          filename: primary.filename,
          displayName: materialName(primary.filename),
          sizeBytes: group.reduce((total, file) => total + file.size, 0),
          tablebaseBytes: primary.size,
          sidecarCount: group.length - 1,
          files: group.sort((left, right) => left.filename.localeCompare(right.filename)),
        };
      });
    catalogFetchedAt = Date.now();
    return catalogCache;
  }

  async function localFiles() {
    let entries = [];
    try {
      entries = await readdir(targetDirectory, { withFileTypes: true });
    } catch (error) {
      if (error?.code !== "ENOENT") throw error;
    }
    const files = [];
    for (const entry of entries) {
      if (!entry.isFile() || !managedFilename(entry.name)) continue;
      const info = await lstat(path.join(targetDirectory, entry.name));
      files.push({ filename: entry.name, size: info.size });
    }
    return files;
  }

  async function diskSpace() {
    const info = await statfs(await existingStatfsTarget(targetDirectory), {
      bigint: true,
    });
    return Number(info.bavail * info.bsize);
  }

  function jobSnapshot(filename) {
    const job = jobs.get(filename);
    if (!job) return null;
    return {
      status: job.status,
      receivedBytes: job.receivedBytes,
      totalBytes: job.totalBytes,
      error: job.error ?? null,
    };
  }

  async function inventory() {
    let catalog = catalogCache ?? [];
    let catalogError = null;
    try {
      catalog = await fetchCatalog();
    } catch (error) {
      catalogError = error instanceof Error ? error.message : "Catalog unavailable.";
    }
    const installedFiles = await localFiles();
    const installedByStem = new Map();
    for (const file of installedFiles) {
      const stem = fileStem(file.filename);
      if (!installedByStem.has(stem)) installedByStem.set(stem, []);
      installedByStem.get(stem).push(file);
    }
    const known = new Set();
    const entries = catalog.map((entry) => {
      const stem = fileStem(entry.filename);
      known.add(stem);
      const local = installedByStem.get(stem) ?? [];
      const localByName = new Map(local.map((file) => [file.filename, file]));
      const installed = entry.files.every((file) =>
        localByName.get(file.filename)?.size === file.size);
      return {
        filename: entry.filename,
        displayName: entry.displayName,
        sizeBytes: entry.sizeBytes,
        tablebaseBytes: entry.tablebaseBytes,
        sidecarCount: entry.sidecarCount,
        downloadedBytes: local.reduce((total, file) => total + file.size, 0),
        installed,
        available: true,
        job: jobSnapshot(entry.filename),
      };
    });
    for (const [stem, local] of installedByStem) {
      if (known.has(stem) || !local.some((file) => file.filename === `${stem}.uftb`))
        continue;
      const filename = `${stem}.uftb`;
      const size = local.reduce((total, file) => total + file.size, 0);
      entries.push({
        filename,
        displayName: materialName(filename),
        sizeBytes: size,
        tablebaseBytes: local.find((file) => file.filename === filename)?.size ?? 0,
        sidecarCount: Math.max(0, local.length - 1),
        downloadedBytes: size,
        installed: true,
        available: false,
        job: jobSnapshot(filename),
      });
    }
    entries.sort((left, right) =>
      Number(right.installed) - Number(left.installed) ||
      left.displayName.localeCompare(right.displayName) ||
      left.filename.localeCompare(right.filename));
    return {
      directory: targetDirectory,
      availableBytes: await diskSpace(),
      usedBytes: installedFiles.reduce((total, file) => total + file.size, 0),
      catalogError,
      entries,
    };
  }

  async function downloadFile(file, temporary) {
    const encodedFile = file.filename.split("/").map(encodeURIComponent).join("/");
    const url = `${origin}/datasets/${encodedDataset}/resolve/${encodedRevision}/tablebases/${encodedFile}?download=true`;
    const response = await fetchImpl(url, {
      headers: authorizationHeaders(token),
      redirect: "follow",
    });
    if (!response.ok || !response.body) {
      const accessHint = response.status === 401 || response.status === 403
        ? " Authenticate with `hf auth login` or provide an approved HF_TOKEN."
        : "";
      throw new Error(
        `Download failed for ${file.filename} (${response.status}).${accessHint}`,
      );
    }
    const hash = createHash("sha256");
    let received = 0;
    const job = jobs.get(`${fileStem(file.filename)}.uftb`);
    const progress = new TransformStream({
      transform(chunk, controller) {
        const bytes = Buffer.from(chunk);
        hash.update(bytes);
        received += bytes.length;
        if (job) job.receivedBytes += bytes.length;
        controller.enqueue(bytes);
      },
    });
    await pipeline(
      response.body.pipeThrough(progress),
      createWriteStream(temporary, { flags: "wx" }),
    );
    if (received !== file.size)
      throw new Error(
        `${file.filename} downloaded ${received} bytes; expected ${file.size}.`,
      );
    const digest = hash.digest("hex");
    if (file.sha256 && digest !== file.sha256)
      throw new Error(`${file.filename} failed SHA-256 verification.`);
  }

  async function performDownload(entry) {
    const job = jobs.get(entry.filename);
    const temporary = [];
    try {
      await mkdir(targetDirectory, { recursive: true });
      for (const file of entry.files) {
        const name = `.${file.filename}.${process.pid}.download`;
        const destination = path.join(targetDirectory, name);
        await rm(destination, { force: true });
        temporary.push({ file, destination });
        await downloadFile(file, destination);
      }
      for (const item of temporary)
        await rename(item.destination, path.join(targetDirectory, item.file.filename));
      job.status = "complete";
    } catch (error) {
      job.status = "error";
      job.error = error instanceof Error ? error.message : "Download failed.";
    } finally {
      await Promise.all(temporary.map((item) => rm(item.destination, { force: true })));
    }
  }

  async function startDownload(filename) {
    if (!managedFilename(filename) || path.extname(filename) !== ".uftb")
      throw new Error("Invalid tablebase filename.");
    const active = jobs.get(filename);
    if (active?.status === "downloading") return jobSnapshot(filename);
    const catalog = await fetchCatalog();
    const entry = catalog.find((candidate) => candidate.filename === filename);
    if (!entry) throw new Error("Tablebase is not available in the dataset.");
    const free = await diskSpace();
    const reserved = [...jobs.values()]
      .filter((job) => job.status === "downloading")
      .reduce((total, job) =>
        total + Math.max(0, job.totalBytes - job.receivedBytes), 0);
    if (entry.sizeBytes > free - reserved)
      throw new Error(
        `Not enough disk space: ${entry.sizeBytes} bytes required, ${Math.max(0, free - reserved)} available.`,
      );
    const current = jobs.get(filename);
    if (current?.status === "downloading") return jobSnapshot(filename);
    const job = {
      status: "downloading",
      receivedBytes: 0,
      totalBytes: entry.sizeBytes,
      error: null,
      promise: null,
    };
    jobs.set(filename, job);
    job.promise = performDownload(entry);
    return jobSnapshot(filename);
  }

  async function waitForDownload(filename) {
    await jobs.get(filename)?.promise;
    return jobSnapshot(filename);
  }

  async function deleteTablebase(filename) {
    if (!managedFilename(filename) || path.extname(filename) !== ".uftb")
      throw new Error("Invalid tablebase filename.");
    if (jobs.get(filename)?.status === "downloading")
      throw new Error("Wait for the active download before deleting this tablebase.");
    const stem = fileStem(filename);
    const files = await localFiles();
    const matching = files.filter((file) => fileStem(file.filename) === stem);
    await Promise.all(matching.map((file) =>
      rm(path.join(targetDirectory, file.filename), { force: true })));
    jobs.delete(filename);
    return { deleted: matching.map((file) => file.filename) };
  }

  return {
    directory: targetDirectory,
    deleteTablebase,
    fetchCatalog,
    inventory,
    startDownload,
    waitForDownload,
  };
}

export { MANAGED_EXTENSIONS, SIDECAR_EXTENSIONS };
