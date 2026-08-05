import type { Metadata } from "next";
import "./globals.css";

export const metadata: Metadata = {
  title: "Ultimate Fish",
  description: "A local Chess Ultimate play, draft, and analysis interface.",
  icons: {
    icon: "/ultimate-fish-logo.png",
    shortcut: "/ultimate-fish-logo.png",
    apple: "/ultimate-fish-logo.png",
  },
};

export default function RootLayout({
  children,
}: Readonly<{
  children: React.ReactNode;
}>) {
  return (
    <html lang="en">
      <body>{children}</body>
    </html>
  );
}
