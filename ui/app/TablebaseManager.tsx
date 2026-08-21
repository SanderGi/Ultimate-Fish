"use client";

import { useCallback, useEffect, useMemo, useState } from "react";

type DownloadJob = {
  status: "downloading" | "complete" | "error";
  receivedBytes: number;
  totalBytes: number;
  error: string | null;
};

type TablebaseEntry = {
  filename: string;
  displayName: string;
  sizeBytes: number;
  tablebaseBytes: number;
  sidecarCount: number;
  downloadedBytes: number;
  installed: boolean;
  available: boolean;
  job: DownloadJob | null;
};

type TablebaseInventory = {
  directory: string;
  availableBytes: number;
  usedBytes: number;
  catalogError: string | null;
  entries: TablebaseEntry[];
};

function formatBytes(bytes: number | null | undefined) {
  if (bytes === null || bytes === undefined || !Number.isFinite(bytes))
    return "—";
  if (bytes < 1024) return `${bytes} B`;
  const units = ["KiB", "MiB", "GiB", "TiB"];
  let value = bytes;
  let unit = -1;
  do {
    value /= 1024;
    unit += 1;
  } while (value >= 1024 && unit < units.length - 1);
  return `${value.toFixed(value >= 100 ? 0 : value >= 10 ? 1 : 2)} ${units[unit]}`;
}

async function errorMessage(response: Response, fallback: string) {
  const body = (await response.json().catch(() => ({}))) as { error?: string };
  return body.error ?? fallback;
}

export function TablebaseManager() {
  const [inventory, setInventory] = useState<TablebaseInventory | null>(null);
  const [query, setQuery] = useState("");
  const [message, setMessage] = useState<string | null>(null);
  const [action, setAction] = useState<string | null>(null);

  const refresh = useCallback(async () => {
    try {
      const response = await fetch("/api/engine/tablebases", {
        cache: "no-store",
      });
      if (!response.ok)
        throw new Error(await errorMessage(response, "Could not load tablebases."));
      setInventory((await response.json()) as TablebaseInventory);
      setMessage(null);
    } catch (error) {
      setMessage(error instanceof Error ? error.message : "Could not load tablebases.");
    }
  }, []);

  const downloading = inventory?.entries.some(
    (entry) => entry.job?.status === "downloading",
  ) ?? false;

  useEffect(() => {
    const timer = window.setTimeout(() => void refresh(), 0);
    return () => window.clearTimeout(timer);
  }, [refresh]);

  useEffect(() => {
    const timer = window.setInterval(() => void refresh(), downloading ? 1000 : 15000);
    return () => window.clearInterval(timer);
  }, [downloading, refresh]);

  const visibleEntries = useMemo(() => {
    const needle = query.trim().toLocaleLowerCase();
    if (!needle) return inventory?.entries ?? [];
    return (inventory?.entries ?? []).filter((entry) =>
      `${entry.displayName} ${entry.filename}`.toLocaleLowerCase().includes(needle));
  }, [inventory, query]);

  const download = async (filename: string) => {
    setAction(filename);
    setMessage(null);
    try {
      const response = await fetch("/api/engine/tablebases/download", {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify({ filename }),
      });
      if (!response.ok)
        throw new Error(await errorMessage(response, "Download could not start."));
      await refresh();
    } catch (error) {
      setMessage(error instanceof Error ? error.message : "Download could not start.");
    } finally {
      setAction(null);
    }
  };

  const remove = async (entry: TablebaseEntry) => {
    if (!window.confirm(`Delete ${entry.displayName} and its sidecars?`)) return;
    setAction(entry.filename);
    setMessage(null);
    try {
      const response = await fetch(
        `/api/engine/tablebases/${encodeURIComponent(entry.filename)}`,
        { method: "DELETE" },
      );
      if (!response.ok)
        throw new Error(await errorMessage(response, "Tablebase could not be deleted."));
      await refresh();
    } catch (error) {
      setMessage(error instanceof Error ? error.message : "Tablebase could not be deleted.");
    } finally {
      setAction(null);
    }
  };

  return (
    <section className="panel tablebase-panel">
      <div className="panel-heading">
        <div>
          <p className="eyebrow">TABLEBASE MANAGER</p>
          <h2>Exact endgames</h2>
        </div>
        <div className="tablebase-heading-status" title={inventory?.directory}>
          <span className="tablebase-storage-inline">
            <small>{formatBytes(inventory?.availableBytes)} free</small>
            <small>{formatBytes(inventory?.usedBytes)} used</small>
          </span>
          <span className="tablebase-count">
            {inventory?.entries.filter((entry) => entry.installed).length ?? 0}
          </span>
        </div>
      </div>
      <label className="search-field tablebase-search">
        <span aria-hidden="true">⌕</span>
        <input
          type="search"
          value={query}
          onChange={(event) => setQuery(event.target.value)}
          placeholder="Search material combinations"
          aria-label="Search available tablebases"
        />
      </label>
      {(message || inventory?.catalogError) && (
        <p className="tablebase-message" role="status">
          {message ?? inventory?.catalogError}
        </p>
      )}
      <div className="tablebase-list" aria-label="Available tablebases">
        {!inventory && !message && (
          <p className="empty-state">Loading the tablebase catalog…</p>
        )}
        {inventory && visibleEntries.length === 0 && (
          <p className="empty-state">No material combinations match this search.</p>
        )}
        {visibleEntries.map((entry) => {
          const active = entry.job?.status === "downloading";
          const progress = active && entry.job
            ? Math.min(100, (entry.job.receivedBytes / Math.max(1, entry.job.totalBytes)) * 100)
            : 0;
          return (
            <article
              className={`tablebase-row ${entry.installed ? "installed" : ""}`}
              key={entry.filename}
            >
              <div className="tablebase-row-main">
                <div>
                  <strong>{entry.displayName}</strong>
                  <small>
                    {formatBytes(entry.sizeBytes)}
                    {entry.sidecarCount > 0
                      ? ` · ${entry.sidecarCount} sidecar${entry.sidecarCount === 1 ? "" : "s"}`
                      : ""}
                  </small>
                </div>
                {entry.installed ? (
                  <button
                    type="button"
                    className="tablebase-action delete"
                    disabled={action === entry.filename || active}
                    onClick={() => void remove(entry)}
                    aria-label={`Delete ${entry.displayName} tablebase`}
                  >
                    Delete
                  </button>
                ) : (
                  <button
                    type="button"
                    className="tablebase-action"
                    disabled={!entry.available || action === entry.filename || active}
                    onClick={() => void download(entry.filename)}
                    aria-label={`Download ${entry.displayName} tablebase`}
                  >
                    {active ? "Downloading" : "Download"}
                  </button>
                )}
              </div>
              {active && entry.job && (
                <div className="tablebase-progress">
                  <span style={{ width: `${progress}%` }} />
                  <small>
                    {formatBytes(entry.job.receivedBytes)} / {formatBytes(entry.job.totalBytes)}
                  </small>
                </div>
              )}
              {entry.job?.status === "error" && (
                <small className="tablebase-error">{entry.job.error}</small>
              )}
            </article>
          );
        })}
      </div>
    </section>
  );
}
