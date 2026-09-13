# Ultimate Fly — data, model and software notices

Ultimate Fly is a Chess Ultimate experiment by the Ultimate Fish contributors.
The game rules, circuit runtime, training and application code are GPLv3 or later.
Ultimate Fish preserves Fairy-Stockfish and Stockfish attribution and GPLv3.
Corresponding native referee/circuit sources and build scripts: [/fly/source.tar.gz](/fly/source.tar.gz).
License: [GPLv3](GPL-3.0.txt).

## Full circuit

Source: [MaleCNS v1.0](https://male-cns.janelia.org/download/).
Creators: FlyEM / HHMI Janelia, University of Cambridge, MRC Laboratory of Molecular
Biology, and Google Research. [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).
No endorsement is implied.

All Traced neurons with an internal Traced-to-Traced connection are included:
164,587 cells and 25,563,197 directed neuron-pair connections, comprising
124,025,046 synaptic contacts. Every measured internal edge is retained, including
one-contact edges. The graph is not sampled or reduced to a small circuit.
535 isolated Traced cells and non-Traced fragments are excluded. The manifest
pins SHA-256 hashes for all three official Feather source tables and our binary.

The 139,741 displayed positions are measured soma locations; missing coordinates
are not invented. Display coordinates are centered, uniformly scaled, and flipped
on Y/Z. A fixed index-modulo-eight subset receives computed activity telemetry;
Every measured soma has a persistent dim anatomical baseline. During computation,
recorded native full-circuit responses to 21 legal actions provide the waiting
animation, not live neural telemetry. The recording uses the same runtime and
quantization as current decisions; its metadata pins the graph, runtime and binary
hashes, source positions, moves and feature vectors. Playback slows the eight
iterations per candidate to 140 ms per frame and interpolates between frames.
Once the result arrives, its own computed activity replaces the recording; unsampled somata remain dim anatomical context. Every circuit cell is simulated,
regardless of whether its location is available or its activity is visualized.

Model assumptions: eight public action features, mapped round-robin by ascending
body ID into 13,956 visual projection / central sensory neurons. 2,129 descending,
central motor and VNC motor cells feed a trained linear readout. Presynaptic signs
are +1 acetylcholine, -1 GABA/glutamate, and 0 for other/unknown transmitters.
Weights are contact counts divided by total incoming absolute signed contact
count. Eight synchronous iterations use h' = .3h + .7z/(1+abs(z)), where
z = sensory current + 1.4Wh. Each candidate begins at zero. These are dimensionless
rate units, not biological spikes, voltages, calibrated physiological dynamics,
or a demonstration that real flies understand chess. "Active" means abs(h) > .02.
The topology matches the reference Fly Chess counts, but our sensory mapping,
rate model and trained readout are independent; we do not claim to reproduce its
private neural implementation or its reported chess performance.

Only the artificial motor readout is trained. Source data, graph, sensory mapping
and native rules remain fixed. See [training report](training-report.json).

## Anatomical fly

The anatomical mesh is derived from [Flybody](https://github.com/TuragaLab/flybody),
by Roman Vaxenburg, Igor Siwanowicz, Josh Merel, Alice A. Robie, Carmen Morrow,
Guido Novati, Zinovia Stefanidi, Gert-Jan Both, Gwyneth M. Card, Michael B. Reiser,
Matthew M. Botvinick, Kristin M. Branson, Yuval Tassa, and Srinivas C. Turaga.
Google DeepMind and HHMI Janelia collaboration. Apache 2.0; see
[license](FLYBODY-LICENSE.txt) and [original notice](FLYBODY-NOTICE.md).
Publication: [Whole-body physics simulation of fruit fly locomotion](https://doi.org/10.1038/s41586-025-09029-4).

The articulated mesh conversion is built directly from Flybody revision
`d015e9bfe441bd90ae431bac24c55cb74bdbce26` by `tools/fly/prepare-anatomy.py`.
It preserves every anatomical triangle and the original body/joint hierarchy,
including all intermediate tarsal segments. The original prototype used the
conversion from Mert Cobanov's [fly-connectome-template](https://github.com/cobanov/fly-connectome-template)
via Fly Dino; its attribution terms remain in [TEMPLATE-LICENSE.txt](TEMPLATE-LICENSE.txt).
Our Three.js flight animation is authored presentation, not a simulated muscle
system or a prediction of fly behavior. Retracted leg references come from the
source model; wing sweeps, small joint adjustments and hover/bank motion are
procedural. Multiple translucent wing poses approximate exposure during flight.

Three.js is MIT licensed: [license](THREE-LICENSE.txt). No remote font or analytics
services are loaded. The public page requests only this server's files and fly
inference and native referee endpoints; it does not request engine analysis.

## Inspiration

[Chess vs. a Fruit Fly](https://flychess-hq.vercel.app/) by ErnestoSOFTWARE inspired
the playable opponent, embodied presentation and neural visualization.
[Awesome Fly](https://github.com/cobanov/awesome-fly) supplied research pointers.
No unlicensed reference-app implementation code or trained weights were copied.
