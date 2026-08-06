# Ultimate Fish tablebases

These bundled files are exact WDL/DTW retrograde solutions for the native
8x10 board. Each class contains 985,920 positions with both Kings and the named
Ivory attacker on distinct squares, either side to move, and all three
`moved=true`. The moved-state restriction makes the class closed by excluding
castling; the probe rejects every state with cooldown, freeze, power,
attachment, forced continuation, en passant, or additional live material.

| File | Class | In-class edges | Win / loss / draw states | SHA-256 |
| --- | --- | ---: | --- | --- |
| `krk.uftb` | King+Rook vs King | 11,970,912 | 534,768 / 414,300 / 36,852 | `5ebcd91e971d7bb571100264f1c5de28b293524ab9c2d37f67b0e22171c8c253` |
| `kqk.uftb` | King+Queen vs King | 15,744,492 | 534,768 / 413,304 / 37,848 | `6fb370ab71c6b45050afd8415bfcc6138c2239e80c32b8754f68b4852fa902dd` |
| `kninjak.uftb` | King+Ninja vs King | 12,610,592 | 534,768 / 413,376 / 37,776 | `9d7faea0cf0354c0036fa8348f92737e8b5f4e5a6e85a4b29c6e02b9210f1044` |
| `kdragonk.uftb` | King+Dragon vs King | 11,904,156 | 534,768 / 414,164 / 36,988 | `f889d6efbe8c8eca7423f273dc74202097d6435ecfc98e958fc63771cad8d2a4` |

Those hashes describe format version 2, which packs WDL and 14-bit DTW into
two bytes per state. Every file was validated after retrograde propagation by
regenerating all legal successors and checking the Bellman equations for every
state.

Regenerate them with `tools/generate_ultimate_tablebases.sh`. Checkpoints are
piece-tagged and resumable. Lone Bishop, Knight, and Turtle classes are not
bundled because the recovered native insufficient-material rule makes every
such position an immediate draw.
