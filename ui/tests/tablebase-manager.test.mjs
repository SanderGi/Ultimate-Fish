import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import { access, mkdir, mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import test from "node:test";

import {
  configuredTablebaseDirectory,
  createTablebaseManager,
  materialName,
  resolveHuggingFaceToken,
} from "../tablebase-manager.mjs";

function sha256(value) {
  return createHash("sha256").update(value).digest("hex");
}

function catalogFile(filename, contents) {
  return {
    type: "file",
    path: `tablebases/${filename}`,
    size: contents.length,
    lfs: { oid: sha256(contents), size: contents.length },
  };
}

test("material names describe both armies", () => {
  assert.equal(materialName("kqk.uftb"), "King + Queen vs King");
  assert.equal(
    materialName("kqueenberserkerk.uftb"),
    "King + Queen + Berserker vs King",
  );
  assert.equal(
    materialName("kcopycatkangel.uftb"),
    "King + CopyCat vs King + Angel",
  );
  assert.equal(materialName("kdevilk.ufds"), "King + Devil vs King");
});

test("stateful Devil partitions install and delete as one verified class", async () => {
  const directory = await mkdtemp(path.join(os.tmpdir(), "ultimatefish-devil-tablebase-"));
  const a1 = Buffer.from("certified stateful Devil a1 partition");
  const d3 = Buffer.from("certified stateful Devil d3 partition");
  const payloads = new Map([
    ["kdevilk-a1.ufds", a1],
    ["kdevilk-d3.ufds", d3],
  ]);
  let changed = 0;
  const fetchImpl = async (url) => {
    if (String(url).includes("/api/datasets/"))
      return Response.json([
        catalogFile("kdevilk-a1.ufds", a1),
        catalogFile("kdevilk-d3.ufds", d3),
      ]);
    const match = String(url).match(/\/tablebases\/([^?]+)/);
    const payload = payloads.get(decodeURIComponent(match?.[1] ?? ""));
    return payload ? new Response(payload) : new Response("missing", { status: 404 });
  };

  try {
    await writeFile(path.join(directory, "kdevilk-a1.ufds"), a1);
    const manager = createTablebaseManager({
      directory,
      fetchImpl,
      onFilesChanged: () => { changed += 1; },
    });
    let inventory = await manager.inventory();
    assert.equal(inventory.entries.length, 1);
    assert.deepEqual(
      {
        filename: inventory.entries[0].filename,
        displayName: inventory.entries[0].displayName,
        installed: inventory.entries[0].installed,
        partitionCount: inventory.entries[0].partitionCount,
        sidecarCount: inventory.entries[0].sidecarCount,
      },
      {
        filename: "kdevilk.ufds",
        displayName: "King + Devil vs King",
        installed: false,
        partitionCount: 2,
        sidecarCount: 0,
      },
    );

    await manager.startDownload("kdevilk.ufds");
    assert.equal((await manager.waitForDownload("kdevilk.ufds")).status, "complete");
    inventory = await manager.inventory();
    assert.equal(inventory.entries[0].installed, true);
    assert.deepEqual(await readFile(path.join(directory, "kdevilk-a1.ufds")), a1);
    assert.deepEqual(await readFile(path.join(directory, "kdevilk-d3.ufds")), d3);
    assert.equal(changed, 1);

    const deleted = await manager.deleteTablebase("kdevilk.ufds");
    assert.deepEqual(deleted.deleted.sort(), ["kdevilk-a1.ufds", "kdevilk-d3.ufds"]);
    assert.equal(changed, 2);
    await assert.rejects(manager.startDownload("kdevilk-a4.ufds"), /not available/);
    await assert.rejects(manager.startDownload("../kdevilk.ufds"), /Invalid/);
  } finally {
    await rm(directory, { recursive: true, force: true });
  }
});

test("oversized Devil shards reconstruct one canonical verified UFDS", async () => {
  const directory = await mkdtemp(path.join(os.tmpdir(), "ultimatefish-devil-shards-"));
  const a1 = Buffer.from("direct a1 partition");
  const first = Buffer.from("certified c2 first half ");
  const second = Buffer.from("and certified c2 second half");
  const c2 = Buffer.concat([first, second]);
  const manifest = Buffer.from(`${JSON.stringify({
    schema: "ultimate-fish-ufds-shard-manifest-v1",
    filename: "kdevilk-c2.ufds",
    bytes: c2.length,
    sha256: sha256(c2),
    square: 10,
    parts: [
      { filename: "kdevilk-c2-part000.ufdsp", bytes: first.length, sha256: sha256(first) },
      { filename: "kdevilk-c2-part001.ufdsp", bytes: second.length, sha256: sha256(second) },
    ],
  }, null, 2)}\n`);
  const payloads = new Map([
    ["kdevilk-a1.ufds", a1],
    ["kdevilk-c2.ufdsm", manifest],
    ["kdevilk-c2-part000.ufdsp", first],
    ["kdevilk-c2-part001.ufdsp", second],
  ]);
  const fetchImpl = async (url) => {
    if (String(url).includes("/api/datasets/"))
      return Response.json([...payloads].map(([filename, contents]) =>
        catalogFile(filename, contents)));
    const match = String(url).match(/\/tablebases\/([^?]+)/);
    const payload = payloads.get(decodeURIComponent(match?.[1] ?? ""));
    return payload ? new Response(payload) : new Response("missing", { status: 404 });
  };

  try {
    const manager = createTablebaseManager({ directory, fetchImpl });
    const before = await manager.inventory();
    assert.equal(before.entries[0].partitionCount, 2);
    assert.equal(before.entries[0].sizeBytes, a1.length + c2.length);
    await manager.startDownload("kdevilk.ufds");
    assert.equal((await manager.waitForDownload("kdevilk.ufds")).status, "complete");
    assert.deepEqual(await readFile(path.join(directory, "kdevilk-a1.ufds")), a1);
    assert.deepEqual(await readFile(path.join(directory, "kdevilk-c2.ufds")), c2);
    await assert.rejects(access(path.join(directory, "kdevilk-c2-part000.ufdsp")));
    await assert.rejects(access(path.join(directory, "kdevilk-c2.ufdsm")));
    assert.equal((await manager.inventory()).entries[0].installed, true);
  } finally {
    await rm(directory, { recursive: true, force: true });
  }
});

test("the first configured directory is managed", () => {
  assert.equal(
    configuredTablebaseDirectory(
      "/work/ui", "/one/file.uftb:/one/kdevilk-a1.ufds:/two/tables"),
    "/two/tables",
  );
  assert.throws(
    () => configuredTablebaseDirectory(
      "/work/ui", "/one/file.uftb:/one/kdevilk-a1.ufds"),
    /must include a directory/,
  );
});

test("catalog entries without an authenticated LFS digest fail closed", async () => {
  const directory = await mkdtemp(path.join(os.tmpdir(), "ultimatefish-no-lfs-hash-"));
  try {
    const manager = createTablebaseManager({
      directory,
      fetchImpl: async () => Response.json([{
        type: "file",
        path: "tablebases/kqk.uftb",
        size: 123,
      }]),
    });
    await assert.rejects(manager.fetchCatalog(), /lacks a valid LFS SHA-256/);
  } finally {
    await rm(directory, { recursive: true, force: true });
  }
});

test("environment tokens override Hugging Face CLI credentials", async () => {
  const home = await mkdtemp(path.join(os.tmpdir(), "ultimatefish-hf-token-"));
  try {
    const tokenDirectory = path.join(home, ".cache/huggingface");
    await mkdir(tokenDirectory, { recursive: true });
    await writeFile(path.join(tokenDirectory, "token"), "cli-token\n");
    assert.equal(resolveHuggingFaceToken({ env: {}, homeDirectory: home }), "cli-token");
    assert.equal(
      resolveHuggingFaceToken({ env: { HF_TOKEN: "server-token" }, homeDirectory: home }),
      "server-token",
    );
  } finally {
    await rm(home, { recursive: true, force: true });
  }
});

test("inventory, verified group download, and group deletion", async () => {
  const directory = await mkdtemp(path.join(os.tmpdir(), "ultimatefish-tablebases-"));
  const queen = Buffer.from("certified queen tablebase");
  const queenSidecar = Buffer.from("certified queen sidecar");
  const pawn = Buffer.from("certified pawn tablebase");
  const payloads = new Map([
    ["kqk.uftb", queen],
    ["kqk.ufiw", queenSidecar],
    ["kpawnk.uftb", pawn],
  ]);
  const requests = [];
  const fetchImpl = async (url, options = {}) => {
    requests.push({ url: String(url), options });
    if (String(url).includes("/api/datasets/")) {
      return Response.json([
        catalogFile("kqk.uftb", queen),
        catalogFile("kqk.ufiw", queenSidecar),
        catalogFile("kpawnk.uftb", pawn),
      ]);
    }
    const match = String(url).match(/\/tablebases\/([^?]+)/);
    const filename = decodeURIComponent(match?.[1] ?? "");
    const payload = payloads.get(filename);
    return payload
      ? new Response(payload)
      : new Response("missing", { status: 404 });
  };

  try {
    await writeFile(path.join(directory, "kpawnk.uftb"), pawn);
    // An incomplete group must remain downloadable rather than appearing installed.
    await writeFile(path.join(directory, "kqk.uftb"), queen);
    const manager = createTablebaseManager({
      directory,
      token: "dataset-token",
      fetchImpl,
    });

    let inventory = await manager.inventory();
    assert.match(requests[0].url, /recursive=true&expand=false$/);
    assert.deepEqual(
      inventory.entries.map(({ filename, installed }) => ({ filename, installed })),
      [
        { filename: "kpawnk.uftb", installed: true },
        { filename: "kqk.uftb", installed: false },
      ],
    );
    assert.equal(inventory.usedBytes, pawn.length + queen.length);
    assert.equal(inventory.entries[1].sizeBytes, queen.length + queenSidecar.length);
    assert.equal(inventory.entries[1].sidecarCount, 1);

    assert.equal((await manager.startDownload("kqk.uftb")).status, "downloading");
    assert.equal((await manager.waitForDownload("kqk.uftb")).status, "complete");
    assert.deepEqual(await readFile(path.join(directory, "kqk.uftb")), queen);
    assert.deepEqual(await readFile(path.join(directory, "kqk.ufiw")), queenSidecar);

    inventory = await manager.inventory();
    assert.equal(inventory.entries[0].filename, "kpawnk.uftb");
    assert.equal(inventory.entries[1].filename, "kqk.uftb");
    assert.ok(inventory.entries.every((entry) => entry.installed));
    assert.ok(requests.every((request) =>
      request.options.headers.authorization === "Bearer dataset-token"));

    const deleted = await manager.deleteTablebase("kqk.uftb");
    assert.deepEqual(deleted.deleted.sort(), ["kqk.ufiw", "kqk.uftb"]);
    await assert.rejects(access(path.join(directory, "kqk.uftb")));
    await assert.rejects(access(path.join(directory, "kqk.ufiw")));
    await assert.rejects(manager.startDownload("../kqk.uftb"), /Invalid/);
  } finally {
    await rm(directory, { recursive: true, force: true });
  }
});

test("a digest mismatch never replaces an installed file", async () => {
  const directory = await mkdtemp(path.join(os.tmpdir(), "ultimatefish-bad-tablebase-"));
  const oldContents = Buffer.from("old certified contents");
  const newContents = Buffer.from("corrupt transport contents");
  try {
    await writeFile(path.join(directory, "kqk.uftb"), oldContents);
    const manager = createTablebaseManager({
      directory,
      fetchImpl: async (url) => String(url).includes("/api/datasets/")
        ? Response.json([{
          ...catalogFile("kqk.uftb", newContents),
          lfs: { oid: sha256(Buffer.from("expected other contents")), size: newContents.length },
        }])
        : new Response(newContents),
    });
    await manager.startDownload("kqk.uftb");
    const job = await manager.waitForDownload("kqk.uftb");
    assert.equal(job.status, "error");
    assert.match(job.error, /SHA-256/);
    assert.deepEqual(await readFile(path.join(directory, "kqk.uftb")), oldContents);
  } finally {
    await rm(directory, { recursive: true, force: true });
  }
});
