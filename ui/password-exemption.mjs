export const PASSWORD_EXEMPT_PATH = "/ultimate-tablebase-grid.svg";
// Explicit assets only: never exempt the engine API or arbitrary /fly children.
export const FLY_PUBLIC_PATHS = new Set([
  "/fly", "/fly/", "/fly/index.html", "/fly/style.css", "/fly/app.js",
  "/fly/pieces.svg", "/fly/source.tar.gz",
  "/fly/data/model.json", "/fly/data/model.bin", "/fly/data/neurons.json",
  "/fly/data/readout.json", "/fly/data/manifest.json", "/fly/data/training-report.json",
  "/fly/data/NOTICE.md", "/fly/data/FLYBODY-NOTICE.md", "/fly/data/FLYBODY-LICENSE.txt",
  "/fly/data/GPL-3.0.txt", "/fly/data/THREE-LICENSE.txt", "/fly/data/TEMPLATE-LICENSE.txt",
  "/fly/data/recorded-activity.json", "/fly/data/recorded-activity.bin",
  "/api/fly/choose", "/api/fly/state",
]);
export function isPasswordExemptPath(pathname) {
  return pathname === PASSWORD_EXEMPT_PATH || FLY_PUBLIC_PATHS.has(pathname);
}
