"use client";

import { useEffect, useRef, type RefObject } from "react";

const DESKTOP_TOP_INSET = 98;
const DESKTOP_BOTTOM_INSET = 22;

function clamp(value: number, minimum: number, maximum: number) {
  return Math.min(maximum, Math.max(minimum, value));
}

export function useSmartSticky<T extends HTMLElement>(
  disableAtWidth: number,
): RefObject<T | null> {
  const ref = useRef<T>(null);

  useEffect(() => {
    const element = ref.current;
    if (!element) return;

    let previousScrollY = window.scrollY;
    let stickyTop = DESKTOP_TOP_INSET;
    let frame: number | null = null;
    let remeasurePending = true;

    const update = () => {
      frame = null;
      const shouldRemeasure = remeasurePending;
      remeasurePending = false;
      const enabled = window.innerWidth > disableAtWidth;
      const scrollY = window.scrollY;
      if (!enabled) {
        element.style.removeProperty("--sidebar-sticky-top");
        previousScrollY = scrollY;
        return;
      }

      const height = Math.max(
        element.getBoundingClientRect().height,
        element.scrollHeight,
      );
      const availableHeight =
        window.innerHeight - DESKTOP_TOP_INSET - DESKTOP_BOTTOM_INSET;
      const minimumTop = Math.min(
        DESKTOP_TOP_INSET,
        window.innerHeight - DESKTOP_BOTTOM_INSET - height,
      );

      if (height <= availableHeight) {
        stickyTop = DESKTOP_TOP_INSET;
      } else if (shouldRemeasure) {
        stickyTop = clamp(
          element.getBoundingClientRect().top,
          minimumTop,
          DESKTOP_TOP_INSET,
        );
      } else {
        stickyTop = clamp(
          stickyTop - (scrollY - previousScrollY),
          minimumTop,
          DESKTOP_TOP_INSET,
        );
      }

      element.style.setProperty("--sidebar-sticky-top", `${stickyTop}px`);
      previousScrollY = scrollY;
    };

    const schedule = (remeasure = false) => {
      remeasurePending ||= remeasure;
      if (frame !== null) return;
      frame = window.requestAnimationFrame(update);
    };

    const handleScroll = () => schedule();
    const handleResize = () => schedule(true);
    const resizeObserver = new ResizeObserver(() => schedule(true));
    resizeObserver.observe(element);
    window.addEventListener("scroll", handleScroll, { passive: true });
    window.addEventListener("resize", handleResize);
    schedule(true);

    return () => {
      if (frame !== null) window.cancelAnimationFrame(frame);
      resizeObserver.disconnect();
      window.removeEventListener("scroll", handleScroll);
      window.removeEventListener("resize", handleResize);
      element.style.removeProperty("--sidebar-sticky-top");
    };
  }, [disableAtWidth]);

  return ref;
}
