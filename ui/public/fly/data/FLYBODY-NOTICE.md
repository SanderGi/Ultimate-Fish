# Flybody model notice

The browser model in this directory is derived from the anatomically detailed
fruit-fly body model published by the Turaga Lab:

- Project: `flybody`, fruit fly body model for MuJoCo physics
- Source: https://github.com/TuragaLab/flybody
- Authors: Roman Vaxenburg, Igor Siwanowicz, Josh Merel, Alice A. Robie,
  Carmen Morrow, Guido Novati, Zinovia Stefanidi, Gert-Jan Both, Gwyneth M.
  Card, Michael B. Reiser, Matthew M. Botvinick, Kristin M. Branson,
  Yuval Tassa, and Srinivas C. Turaga
- Development collaboration: Google DeepMind and HHMI Janelia Research Campus
- License: Apache License 2.0, included as `LICENSE`
- Publication: "Whole-body physics simulation of fruit fly locomotion",
  Nature 643, 1312-1320 (2025), https://doi.org/10.1038/s41586-025-09029-4

Ultimate Fly converts the original MuJoCo OBJ meshes into a browser binary while
preserving all 67 body nodes, joint axes, and the complete six leg chains. All
272,550 source triangles are retained; duplicate OBJ vertices are welded.
The source revision is d015e9bfe441bd90ae431bac24c55cb74bdbce26. The converter is
`tools/fly/prepare-anatomy.py`; model.json records input and binary SHA-256 hashes.
The flight posture uses Flybody's retracted-leg spring references. The wingbeat,
leg adjustments and hovering are authored presentation, not locomotion research
output or a muscle simulation. Wings use multiple translucent stroke samples to
avoid display-rate aliasing. The Apache 2.0 license is in FLYBODY-LICENSE.txt.
