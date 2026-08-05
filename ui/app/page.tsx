import type { Metadata } from "next";
import { UltimateWorkbench } from "./UltimateWorkbench";

export const metadata: Metadata = {
  title: "Ultimate Fish",
  description:
    "Build a team, edit Chess Ultimate positions, and analyze with Ultimate Fish.",
  other: {
    "codex-preview": "development",
  },
};

export default function Home() {
  return <UltimateWorkbench />;
}
