/**
 * Build concise inspector pills for public state that changes how a piece can
 * act.
 *
 * @param {{id: string, action: number, cooldown: number, freeze: number,
 *   power: number, moved: boolean, visible: boolean, link?: string}} piece
 * @returns {string[]}
 */
export function pieceStateLabels(piece) {
  const labels = [];

  switch (piece.id) {
    case "ghost":
      labels.push(piece.visible ? "revealed" : "hidden");
      break;
    case "berserker":
      labels.push(`power ${piece.power + 1}`);
      break;
    case "pawn":
      labels.push(piece.moved ? "moved" : "double-step ready");
      break;
    case "sniper":
      labels.push(piece.cooldown ? `reload ${piece.cooldown}` : "shot ready");
      break;
    case "devil":
      labels.push(piece.cooldown ? `cooldown ${piece.cooldown}` : "spawn ready");
      break;
    case "penguin":
      labels.push(piece.action ? "aura active" : "aura inactive");
      break;
    case "king":
    case "jester":
    case "rook":
      labels.push(piece.moved ? "moved" : "unmoved");
      break;
    case "copycat":
    case "copycatClone":
      labels.push(piece.link ? "paired" : "unpaired");
      break;
    case "angel":
      labels.push(piece.link ? "linked" : "unlinked");
      break;
  }

  if (piece.freeze > 0) labels.push(`frozen ×${piece.freeze}`);
  if (piece.cooldown > 0 && piece.id !== "sniper" && piece.id !== "devil")
    labels.push(`cooldown ${piece.cooldown}`);
  if (piece.power > 0 && piece.id !== "berserker")
    labels.push(`power ${piece.power + 1}`);

  return labels;
}
