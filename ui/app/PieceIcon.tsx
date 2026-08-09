type PieceIconProps = {
  id: string;
  color?: "white" | "black" | "neutral";
  className?: string;
};

function Glyph({ id }: { id: string }) {
  switch (id) {
    case "king":
      return (
        <>
          <path
            className="piece-fill"
            d="M29 5h6v7h7v7h-7v6h14c4 0 6 4 4 8L43 47l6 7c2 3 0 5-3 5H18c-3 0-5-2-3-5l6-7-10-14c-2-4 0-8 4-8h14v-6h-7v-7h7z"
          />
          <path className="piece-cut" d="M21 47h22M18 54h28" />
        </>
      );
    case "jester":
      return (
        <>
          <path
            className="piece-ink-stroke classic-outline"
            d="M29 5h6v7h7v7h-7v6h14c4 0 6 4 4 8L43 47l6 7c2 3 0 5-3 5H18c-3 0-5-2-3-5l6-7-10-14c-2-4 0-8 4-8h14v-6h-7v-7h7z"
          />
          <path className="piece-accent-stroke" d="M21 47h22M18 54h28" />
        </>
      );
    case "queen":
      return (
        <>
          <circle className="piece-accent-fill" cx="16" cy="17" r="3.5" />
          <circle className="piece-accent-fill" cx="26" cy="12" r="3.5" />
          <circle className="piece-accent-fill" cx="38" cy="12" r="3.5" />
          <circle className="piece-accent-fill" cx="48" cy="17" r="3.5" />
          <path
            className="piece-fill"
            d="M15 20l8 9 3-13 6 13 6-13 3 13 8-9-5 24H20zM18 47h28v6H18z"
          />
          <path className="piece-cut" d="M21 41h22" />
        </>
      );
    case "rook":
      return (
        <>
          <path
            className="piece-fill"
            d="M16 13h8v7h5v-7h6v7h5v-7h8v15l-5 5 4 19H17l4-19-5-5z"
          />
          <path className="piece-cut" d="M20 29h24M19 46h26" />
        </>
      );
    case "bishop":
      return (
        <>
          <path
            className="piece-fill"
            d="M32 10c8 7 12 14 10 21-1 5-5 8-9 10h9l5 11H17l5-11h9c-6-3-9-7-9-12 0-7 5-13 10-19z"
          />
          <path className="piece-cut" d="M35 19L26 34M20 46h24" />
        </>
      );
    case "knight":
      return (
        <>
          <path
            className="piece-fill"
            d="M29 5c12 0 22 10 22 22v14c0 3-1 5-3 7l-4 4 5 4c2 2 1 4-2 4H17c-3 0-4-3-2-5l6-6v-4c0-3 1-5 3-7l10-9h-6l-2 2c-3 3-7 3-10 1-2-2-3-4-3-7v-2c0-2 1-4 2-6l8-9V5z"
          />
          <path
            className="piece-accent-fill"
            d="M29 5c8 0 15 4 19 11-6-4-12-6-19-6z"
          />
          <circle className="piece-cut-dot" cx="29" cy="17" r="2.4" />
          <path className="piece-cut" d="M17 25h8M20 53h25" />
        </>
      );
    case "pawn":
      return (
        <>
          <circle className="piece-fill" cx="32" cy="20" r="9" />
          <path
            className="piece-fill"
            d="M23 28h18c0 8 2 13 7 19v5H16v-5c5-6 7-11 7-19z"
          />
          <path className="piece-cut" d="M20 45h24" />
        </>
      );
    case "checker":
      return (
        <>
          <circle className="piece-fill" cx="32" cy="32" r="21" />
          <circle className="piece-cut thick" cx="32" cy="32" r="15" />
          <path
            className="piece-accent-stroke"
            d="M21 28c7 4 15 4 22 0M21 36c7 4 15 4 22 0"
          />
        </>
      );
    case "checkerKing":
      return (
        <>
          <circle className="piece-fill" cx="32" cy="34" r="20" />
          <circle className="piece-cut thick" cx="32" cy="34" r="14" />
          <path className="piece-accent-fill" d="M21 30l4-10 7 7 7-7 4 10z" />
        </>
      );
    case "berserker":
      return (
        <text className="piece-symbol-text swords-symbol" x="32" y="34">
          ⚔
        </text>
      );
    case "bomb":
      return (
        <>
          <circle className="piece-fill" cx="30" cy="35" r="17" />
          <path
            className="piece-accent-stroke thick"
            d="M36 19c0-6 5-8 10-8 2 0 4-2 4-5"
          />
          <path
            className="piece-accent-fill"
            d="M47 7l3 3 4-1-2 4 3 3-5-1-2 4-1-5-5-1 4-2z"
          />
          <path className="piece-cut" d="M19 30c3-6 8-9 14-10" />
        </>
      );
    case "ninja":
      return (
        <>
          <path
            className="piece-fill"
            d="M14 38c0-15 7-25 18-25s18 10 18 25v14H14z"
          />
          <path
            className="piece-accent-fill"
            d="M15 30c11-5 23-5 34 0l-5 10H20z"
          />
          <path className="piece-cut thick" d="M22 34h20" />
          <circle className="piece-fill inverse" cx="26" cy="34" r="2" />
          <circle className="piece-fill inverse" cx="38" cy="34" r="2" />
        </>
      );
    case "turtle":
      return (
        <>
          <ellipse className="piece-fill" cx="31" cy="34" rx="18" ry="14" />
          <circle className="piece-fill" cx="50" cy="33" r="6" />
          <path
            className="piece-fill"
            d="M18 22l-7-4 2 9M18 46l-7 4 2-9M40 22l6-5 1 9M40 46l6 5 1-9"
          />
          <path
            className="piece-accent-stroke"
            d="M20 34l11-10 11 10-11 10zM31 24v20M20 34h22"
          />
          <circle className="piece-cut-dot" cx="52" cy="31" r="1.5" />
        </>
      );
    case "parasite":
      return (
        <>
          <path
            className="piece-accent-stroke thick"
            d="M32 9v8M32 47v8M9 32h8M47 32h8M15 15l6 6M43 43l6 6M49 15l-6 6M21 43l-6 6"
          />
          <path
            className="piece-fill"
            d="M32 16c12 0 18 7 18 16s-6 16-18 16-18-7-18-16 6-16 18-16z"
          />
          <circle className="piece-accent-fill" cx="27" cy="28" r="4" />
          <circle className="piece-accent-fill" cx="38" cy="34" r="5" />
          <circle className="piece-cut-dot" cx="27" cy="28" r="1.6" />
          <circle className="piece-cut-dot" cx="38" cy="34" r="2" />
        </>
      );
    case "giant":
      return (
        <>
          <path className="piece-accent-fill" d="M11 12h42v12H11z" />
          <rect
            className="piece-fill"
            x="15"
            y="29"
            width="12"
            height="8"
            rx="2"
          />
          <rect
            className="piece-fill"
            x="37"
            y="29"
            width="12"
            height="8"
            rx="2"
          />
          <path className="piece-fill" d="M17 46h30v6H17z" />
        </>
      );
    case "dragon":
      return (
        <g transform="translate(2 13) scale(.407)">
          <path
            className="piece-fill"
            fillRule="evenodd"
            clipRule="evenodd"
            d="M52.4,20.33C60.04,15.89,63.76,8.53,68.19,0c2.26,2.44,3.48,8.44,2.98,16.83
              c21.29-1.39,34.78,10.58,51.71-7.42C115.6,37.52,89.32,21.55,78.34,24
              c16.95,1.92,21.76,11.28,41.28,7.96c-6.72,8.09-21.59,6.45-34.11,0.75
              c2.21,6.08,24.83,10.01,20.22,28.16c-4.36-7.49-8.45-12.09-12.29-14.08
              c7.91,13.68,7.29,26.15-0.09,39.78c-1.63,3-3.62,5.73-6.05,8.09
              c3.38-7.47,5.02-14.43,3.58-20.48c-1.37,5.2-26.98,55.76-13.31,14.34
              C69,94,58,90,49,82C52,75,60,70,66.84,66.94
              c3.26-3.54,4.44-8.33,3.04-11.78c-4.08-10.04-18.37-7.09-23.55-0.96
              c-3.19,4.91-9.37,10.37-14.44,4.29c-0.83-0.99-1.3-2.26-1.43-3.78
              c2.56,1.09,5.12,1.51,7.68,0.26l-2.05-2.3c2.73-0.77,5.78,0.09,8.19-2.42
              l8.96-5.89c1.71-1.26-2.13-2.07-11.26-2.7c-0.77,1.99-0.42,3.81,1.79,5.38
              c-2.62,0.35-4.89-0.35-6.66-2.56c-1.04,0.81-0.89,1.38,1.9,5.81
              c-2.51,0.11-5.02-0.89-7.53-2.48c-0.58,2.02,0.27,4.04,0.41,6.06
              c-1.83-0.53-3.51-1.4-5.02-2.62c-1.52-1.23-2.89-2.8-4.09-4.72
              c-0.83-1.88-0.82-3.29-0.18-4.34c0.39-0.65,1.02-1.16,1.85-1.57
              c3.1-1.53,4.66-1.35,8.29-3.77c3.43-2.28,6.93-5.01,9.91-8.69
              C47.47,22.2,47.34,23.27,52.4,20.33L52.4,20.33z

              M51.69,29.57c-1.19,0.49-1.28,1.07-2.92,1.52c-0.49,0.13-1.35,0.87-1.62,1.3
              c-0.84,1.37,2.22,2.1,3.14,0.46c0.23-0.4,1.26-2.3,1.34-2.53
              c0.08-0.24,0.29-0.43,0.13-0.7C51.74,29.62,51.73,29.56,51.69,29.57
              L51.69,29.57z"
          />
        </g>
      );
    case "ghost":
      return (
        <>
          <path
            className="piece-fill"
            d="M14 51c3-6 2-11 2-19 0-12 7-21 16-21s16 9 16 21c0 8-1 13 2 19l-9-5-5 6-5-6-7 6-5-6z"
          />
          <ellipse className="piece-cut-dot" cx="26" cy="29" rx="3" ry="5" />
          <ellipse className="piece-cut-dot" cx="38" cy="29" rx="3" ry="5" />
          <path className="piece-accent-stroke" d="M24 39c5 3 11 3 16 0" />
        </>
      );
    case "mage":
      return (
        <>
          <path className="piece-accent-stroke wand-shaft" d="M17 49L42 24" />
          <path
            className="piece-accent-fill"
            d="M46 8l3 8 8 3-8 3-3 8-3-8-8-3 8-3z"
          />
          <path
            className="piece-ink-stroke"
            d="M34 12l5 4M53 28l-5-4M52 10l-4 5M38 29l4-5"
          />
        </>
      );
    case "penguin":
      return (
        <>
          <ellipse className="piece-fill" cx="32" cy="34" rx="15" ry="22" />
          <ellipse
            className="piece-accent-fill"
            cx="32"
            cy="39"
            rx="9"
            ry="13"
          />
          <path className="piece-accent-fill" d="M30 25h-9l9 6 9-6z" />
          <path
            className="piece-fill"
            d="M20 34L9 42l12 2M44 34l11 8-12 2M24 53l-7 4h12M40 53l7 4H35"
          />
          <circle className="piece-cut-dot" cx="27" cy="21" r="2" />
          <circle className="piece-cut-dot" cx="37" cy="21" r="2" />
        </>
      );
    case "devil":
      return (
        <>
          <path
            className="piece-fill"
            d="M17 15l11 8h8l11-8-4 14c5 5 7 11 5 20H16c-2-9 0-15 5-20z"
          />
          <path
            className="piece-accent-stroke thick"
            d="M49 17v32M43 23l6-7 6 7"
          />
          <path className="piece-cut" d="M24 35l5 3M40 35l-5 3M28 46h8" />
        </>
      );
    case "minion":
      return (
        <>
          <path
            className="piece-fill"
            d="M19 20l8 5h10l8-5-3 10c5 5 6 13 4 22H18c-2-9-1-17 4-22z"
          />
          <path className="piece-accent-fill" d="M25 35h14l-3 7h-8z" />
          <circle className="piece-cut-dot" cx="27" cy="32" r="2" />
          <circle className="piece-cut-dot" cx="37" cy="32" r="2" />
        </>
      );
    case "sludge":
      return (
        <>
          <path
            className="piece-fill"
            d="M10 48c7-4 6-12 11-15 4-2 4-14 11-14 8 0 7 11 13 13 6 2 4 12 9 16-11 6-33 6-44 0z"
          />
          <path
            className="piece-accent-fill"
            d="M18 44c4-4 6-9 8-15 2 6 2 12-1 17zM37 46c4-4 5-9 5-14 4 6 5 11 2 15z"
          />
          <circle className="piece-cut-dot" cx="31" cy="35" r="2.5" />
          <circle className="piece-cut-dot" cx="40" cy="38" r="2" />
        </>
      );
    case "goop":
      return (
        <>
          <path
            className="piece-fill"
            d="M8 42c5-6 12-5 16-10 3-4 2-12 8-12 7 0 6 9 10 12 5 4 11 2 14 10-7 11-41 11-48 0z"
          />
          <circle className="piece-accent-fill" cx="18" cy="42" r="4" />
          <circle className="piece-accent-fill" cx="45" cy="41" r="3" />
          <path className="piece-cut" d="M17 47c10-3 20-3 30 0" />
        </>
      );
    case "prince":
      return (
        <>
          <path
            className="piece-accent-fill"
            d="M22 18l5-8 5 8 5-8 5 8v8H22z"
          />
          <circle className="piece-fill" cx="32" cy="29" r="9" />
          <path className="piece-fill" d="M19 52c0-10 5-17 13-17s13 7 13 17z" />
          <path className="piece-accent-stroke" d="M32 37v12M25 44h14" />
          <path className="piece-cut" d="M22 52h20" />
        </>
      );
    case "sniper":
      return (
        <>
          <circle className="piece-fill target-ring" cx="32" cy="32" r="16" />
          <circle
            className="piece-accent-stroke thick"
            cx="32"
            cy="32"
            r="10"
          />
          <circle className="piece-accent-fill" cx="32" cy="32" r="3.5" />
          <path
            className="piece-fill target-lines"
            d="M29 7h6v15h-6zM29 42h6v15h-6zM7 29h15v6H7zM42 29h15v6H42z"
          />
        </>
      );
    case "fisherman":
      return (
        <>
          <path
            className="piece-accent-stroke thick"
            d="M20 13c16 0 24 8 24 19v12c0 8-12 10-16 2-2-4 1-8 5-8"
          />
          <path
            className="piece-fill"
            d="M10 22c7-8 15-8 23 0-8 7-16 7-23 0z"
          />
          <circle className="piece-cut-dot" cx="15" cy="21" r="1.8" />
          <path className="piece-fill" d="M33 22l8-6v12z" />
        </>
      );
    case "copycat":
    case "copycatClone": {
      const clone = id === "copycatClone";
      return (
        <>
          <path
            className="piece-fill"
            d="M17 24l5-11 8 8h4l8-8 5 11v23c-8 8-22 8-30 0z"
          />
          <path className="piece-cut" d="M24 34l4 2M40 34l-4 2M28 43l4 3 4-3" />
          <path
            className="piece-accent-stroke thick"
            d={
              clone ? "M47 48H15m0 0 6-6m-6 6 6 6" : "M17 48h32m0 0-6-6m6 6-6 6"
            }
          />
        </>
      );
    }
    case "angel":
      return (
        <>
          <ellipse
            className="piece-accent-stroke thick"
            cx="32"
            cy="14"
            rx="11"
            ry="5"
          />
          <path
            className="piece-fill"
            d="M29 24c-7-8-14-7-20-3 3 12 10 18 20 20v12h6V41c10-2 17-8 20-20-6-4-13-5-20 3z"
          />
          <path className="piece-cut" d="M12 24l17 14M52 24L35 38M32 25v25" />
        </>
      );
    case "halo":
      return (
        <>
          <ellipse
            className="piece-accent-stroke thick"
            cx="32"
            cy="24"
            rx="20"
            ry="9"
          />
          <ellipse className="piece-fill" cx="32" cy="24" rx="12" ry="4" />
          <path
            className="piece-accent-stroke"
            d="M16 40l-5 5M24 43l-2 7M40 43l2 7M48 40l5 5"
          />
          <path className="piece-fill" d="M21 49h22l5 5H16z" />
        </>
      );
    default:
      return <circle className="piece-fill" cx="32" cy="32" r="16" />;
  }
}

export default function PieceIcon({
  id,
  color = "neutral",
  className = "",
}: PieceIconProps) {
  const giant = id === "giant";
  const compactClassic = id === "king" || id === "jester" || id === "knight";
  return (
    <svg
      className={`piece-icon piece-icon-${id} ${color} ${className}`}
      viewBox="0 0 64 64"
      aria-hidden="true"
      focusable="false"
    >
      {giant ? (
        <rect
          className="piece-badge"
          x="3"
          y="3"
          width="58"
          height="58"
          rx="8"
        />
      ) : (
        <circle className="piece-badge" cx="32" cy="32" r="29" />
      )}
      <g
        transform={
          compactClassic ? "translate(5.76 5.76) scale(.82)" : undefined
        }
      >
        <Glyph id={id} />
      </g>
    </svg>
  );
}
