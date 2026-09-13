// The browser keeps only a public action history; the server adjudicates it.
export function createReferee(fetchState = fetch) {
  let game = null, epoch = 0, controller = null;
  return async function referee(command, args = []) {
    let next;
    if (command === 'reset') {
      ++epoch;
      controller?.abort();
      game = null;
      next = {preset: args[0], reflection: args[1], moves: []};
    } else {
      if (!game) throw Error('Start a new game to reconnect to the referee.');
      next = {...game, moves: command === 'move' ? [...game.moves, args[0]] : game.moves.slice(0, args[0])};
    }
    const token = epoch;
    controller = new AbortController();
    const response = await fetchState('/api/fly/state', {
      method: 'POST', headers: {'content-type': 'application/json'},
      body: JSON.stringify(next), signal: controller.signal,
    });
    const state = await response.json();
    if (token !== epoch) throw Error('Game replaced.');
    if (!response.ok) throw Error(state.error || 'The referee could not update the board.');
    game = next;
    return state;
  };
}
