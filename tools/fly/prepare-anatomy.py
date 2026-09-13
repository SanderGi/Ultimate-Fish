#!/usr/bin/env python3
"""Convert pinned Flybody OBJ/MJCF anatomy, preserving every body and joint.

Usage: python3 tools/fly/prepare-anatomy.py /path/to/flybody ui/public/fly/data
Requires NumPy. No MuJoCo runtime or mesh decimation is needed in the browser.
"""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET

import numpy as np

REVISION = 'd015e9bfe441bd90ae431bac24c55cb74bdbce26'


def rotation(quat):
    w, x, y, z = np.array(quat) / np.linalg.norm(quat)
    return np.array([[1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w)],
                     [2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w)],
                     [2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y)]])


def numbers(text):
    return [float(x) for x in text.split()]


def convert(source, destination):
    revision = subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip()
    if revision != REVISION:
        raise ValueError(f'Check out Flybody revision {REVISION} before converting')
    assets = source / 'flybody/fruitfly/assets'
    xml = assets / 'fruitfly.xml'
    root = ET.parse(xml).getroot()
    defaults = {}

    def read_defaults(element, inherited):
        attrs = {**inherited, **(element.find('joint').attrib if element.find('joint') is not None else {})}
        defaults[element.get('class', 'main')] = attrs
        for child in element.findall('default'):
            read_defaults(child, attrs)
    read_defaults(root.find('default'), {})
    meshes = {mesh.get('name'): mesh for mesh in root.findall('asset/mesh')}
    nodes, parts, chunks = [], [], []
    offset = 0
    hashes = {'fruitfly.xml': hashlib.sha256(xml.read_bytes()).hexdigest()}

    def visit(body, parent, inherited_class):
        nonlocal offset
        name = body.get('name')
        childclass = body.get('childclass', inherited_class)
        joints = []
        for joint in body.findall('joint'):
            attrs = {**defaults[joint.get('class', childclass)], **joint.attrib}
            if numbers(attrs.get('pos', '0 0 0')) != [0, 0, 0]:
                raise ValueError('Non-origin joint requires a separate pivot')
            joints.append({'name': attrs['name'], 'axis': numbers(attrs.get('axis', '0 0 1')),
                           'range': numbers(attrs['range']), 'rest': float(attrs.get('springref', '0'))})
        q = numbers(body.get('quat', '1 0 0 0'))
        nodes.append({'name': name, 'parent': parent, 'position': numbers(body.get('pos', '0 0 0')),
                      'quaternion': [q[1], q[2], q[3], q[0]], 'joints': joints})
        for geom in body.findall('geom'):
            if 'mesh' not in geom.attrib:
                continue  # Physics collision approximations are not surface anatomy.
            mesh = meshes[geom.get('mesh')]
            path = assets / mesh.get('file')
            hashes[path.name] = hashlib.sha256(path.read_bytes()).hexdigest()
            vertices, faces = [], []
            for line in path.read_text().splitlines():
                fields = line.split()
                if fields and fields[0] == 'v':
                    vertices.append([float(x) for x in fields[1:4]])
                elif fields and fields[0] == 'f':
                    face = [int(x.split('/')[0])-1 for x in fields[1:]]
                    for i in range(1, len(face)-1):
                        faces.append([face[0], face[i], face[i+1]])
            vertices = np.array(vertices) * numbers(mesh.get('scale', '0.1 0.1 0.1'))
            vertices = vertices @ rotation(numbers(geom.get('quat', '1 0 0 0'))).T
            vertices += numbers(geom.get('pos', '0 0 0'))
            # Weld duplicate OBJ vertices without dropping any anatomical faces.
            vertices, inverse = np.unique(vertices.astype('<f4'), axis=0, return_inverse=True)
            indices = inverse[np.array(faces)].astype('<u4')
            vertex_bytes, index_bytes = vertices.tobytes(), indices.tobytes()
            parts.append({'group': name, 'mesh': mesh.get('name'), 'material': geom.get('material', 'body'),
                          'positionByteOffset': offset, 'positionCount': len(vertices),
                          'indexByteOffset': offset+len(vertex_bytes), 'indexCount': indices.size})
            chunks.extend([vertex_bytes, index_bytes])
            offset += len(vertex_bytes)+len(index_bytes)
        for child in body.findall('body'):
            visit(child, name, childclass)
    visit(root.find('worldbody/body'), None, 'main')
    binary = b''.join(chunks)
    metadata = {'version': 2, 'source': 'https://github.com/TuragaLab/flybody', 'revision': REVISION,
                'license': 'Apache-2.0', 'model': 'Flybody articulated Drosophila melanogaster',
                'binary': 'model.bin', 'binarySha256': hashlib.sha256(binary).hexdigest(),
                'sourceFiles': hashes, 'outputTriangles': sum(p['indexCount']//3 for p in parts),
                'coordinateSystem': 'Flybody: X forward, Y left, Z up', 'nodes': nodes, 'parts': parts}
    destination.mkdir(parents=True, exist_ok=True)
    (destination / 'model.bin').write_bytes(binary)
    (destination / 'model.json').write_text(json.dumps(metadata, indent=2)+'\n')
    print(f'{len(nodes)} articulated bodies, {len(parts)} meshes, {metadata["outputTriangles"]} triangles; {len(binary):,} bytes')


if __name__ == '__main__':
    convert(Path(sys.argv[1]), Path(sys.argv[2]))
