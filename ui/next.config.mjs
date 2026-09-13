/** @type {import('next').NextConfig} */
const nextConfig = {
  output: "standalone",
  // The Dockerfile explicitly supplies fly data and native executables. Avoid
  // tracing runtime-selected filesystem paths into the standalone JS bundle.
  outputFileTracingExcludes: {
    "/api/fly/*": [
      "./*.mjs", "./*.ts", "./*.json", "./*.md", "./app/**", "./fly/**",
      "./tests/**", "./public/**",
    ],
  },
  async rewrites() { return [{ source: "/fly", destination: "/fly/index.html" }]; },
};

export default nextConfig;
