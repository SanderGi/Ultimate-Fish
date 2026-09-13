import * as T from 'three';

const palette = {
  body: 0x87602f, black: 0x211811, red: 0xb5371e, ocelli: 0xe3ab4a,
  'bristle-brown': 0x392513, lower: 0xb68a55, brown: 0x503322, membrane: 0xd1d8be,
};

export function createAnatomy(metadata, data) {
  if (metadata.version !== 2) throw Error('The articulated fly model needs reloading.');
  const root = new T.Group(), groups = {}, joints = [], wings = [];
  // Convert the source's X-forward, Z-up coordinates to Three's Y-up view.
  root.quaternion.set(-.5, -.5, -.5, .5);
  for (const node of metadata.nodes) {
    const group = new T.Group();
    group.name = node.name;
    group.position.fromArray(node.position);
    group.quaternion.fromArray(node.quaternion).normalize();
    (node.parent ? groups[node.parent] : root).add(group);
    groups[node.name] = group;
    joints.push({group, rest: group.quaternion.clone(), joints: node.joints.map(joint => ({
      ...joint, axis: new T.Vector3(...joint.axis).normalize(),
    }))});
  }
  for (const part of metadata.parts) {
    const geometry = new T.BufferGeometry();
    geometry.setAttribute('position', new T.BufferAttribute(new Float32Array(data, part.positionByteOffset, part.positionCount * 3), 3));
    geometry.setIndex(new T.BufferAttribute(new Uint32Array(data, part.indexByteOffset, part.indexCount), 1));
    geometry.computeVertexNormals();
    const membrane = part.material === 'membrane';
    const material = new T.MeshStandardMaterial({
      color: palette[part.material] ?? palette.body,
      roughness: part.material === 'red' ? .33 : .62, metalness: .04,
      side: T.DoubleSide, transparent: membrane, opacity: membrane ? .32 : 1,
      depthWrite: !membrane,
    });
    const mesh = new T.Mesh(geometry, material);
    mesh.name = part.mesh;
    groups[part.group].add(mesh);
  }
  for (const side of ['left', 'right']) {
    const entry = joints.find(entry => entry.group.name === `wing_${side}`);
    // Sample a whole stroke into translucent meshes. A single 185 Hz pose at
    // display refresh rates aliases into backwards or apparently frozen wings.
    const samples = [entry.group];
    for (let i = 1; i < 9; i++) {
      const sample = entry.group.clone();
      sample.children.forEach(mesh => { mesh.material = mesh.material.clone(); });
      entry.group.parent.add(sample);
      samples.push(sample);
    }
    wings.push({...entry, samples});
  }
  const turn = new T.Quaternion();
  function pose(entry, angles, group = entry.group) {
    group.quaternion.copy(entry.rest);
    entry.joints.forEach((joint, index) => {
      const angle = T.MathUtils.clamp(angles[index], ...joint.range);
      group.quaternion.multiply(turn.setFromAxisAngle(joint.axis, angle));
    });
  }
  function update(elapsed, paused, effort = 0) {
    for (const entry of joints) {
      if (entry.group.name.startsWith('wing_')) continue;
      const leg = entry.group.name.match(/_T([123])_(left|right)$/);
      const phase = leg ? Number(leg[1]) * 1.7 + (leg[2] === 'left' ? 0 : 2.3) : 0;
      pose(entry, entry.joints.map(joint => {
        // The source's spring references define its retracted flight posture.
        // Small, offset corrections move all six complete chains, not just feet.
        if (leg) {
          const amplitude = joint.name.startsWith('tarsus') ? .045 : .025;
          return joint.rest + amplitude * (1 + effort * .3) * Math.sin(elapsed * 2.1 + phase)
            + amplitude * .35 * Math.sin(elapsed * 3.7 + phase * 1.9);
        }
        if (entry.group.name === 'head') return joint.rest + .018 * Math.sin(elapsed * 1.3 + 1);
        if (entry.group.name.startsWith('antenna')) return joint.rest + .018 * Math.sin(elapsed * 3 + phase);
        if (entry.group.name.startsWith('abdomen')) return joint.rest + .006 * Math.sin(elapsed * 1.6);
        return joint.rest;
      }));
    }
    const phase = elapsed * Math.PI * 2 * 185;
    for (const wing of wings) wing.samples.forEach((sample, index) => {
      sample.visible = !paused || index === 0;
      const p = paused ? Math.PI * .55 : phase + index * Math.PI * 2 / wing.samples.length;
      // Authored periodic stroke using the source model's yaw/roll/pitch axes.
      pose(wing, [1.1 * Math.sin(p - Math.PI / 2) + .3,
        .25 * Math.sin(2 * p) - .1, 1.35 * Math.sin(p) + .8], sample);
      sample.children.forEach(mesh => {
        const membrane = mesh.name.includes('membrane');
        mesh.material.transparent = true;
        mesh.material.depthWrite = false;
        mesh.material.opacity = paused ? (membrane ? .32 : 1) : (membrane ? .045 : .12);
      });
    });
  }
  update(0, true);
  return {root, groups, update};
}
