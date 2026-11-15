#!/usr/bin/env python3
"""
BRG to glTF 2.0 / GLB Converter for Age of Mythology.
Based on Petar Tasev's AoMEngineLibrary (BrgGltfConverter.cs & brg.bt).

Converts proprietary Ensemble Studios BANG BRG 3D models into standard glTF 2.0
including:
  - Base mesh geometry (positions, normals, UVs, triangle indices)
  - Morph target keyframes (per-frame position/normal deltas)
  - glTF morph animation channels and samplers
  - Material definitions and texture mappings (.ddt / .png)
"""

import sys
import os
import struct
import json
import argparse

sys.path.insert(0, os.path.dirname(__file__))
from bar_tool import BarArchive


def aom_half_to_float(b_bytes):
    """AoM truncated float32: upper 16 bits of IEEE 754 float32."""
    return struct.unpack('<f', b'\x00\x00' + b_bytes)[0]


class BrgMesh:
    def __init__(self):
        self.version = 0
        self.flags = 0
        self.num_vertices = 0
        self.num_faces = 0
        self.is_secondary = False
        self.vertices = []
        self.normals = []
        self.uvs = []
        self.face_mats = []
        self.indices = []
        self.vert_mats = []
        self.hotspot = (0.0, 0.0, 0.0)
        self.center = (0.0, 0.0, 0.0)


class BrgMaterial:
    def __init__(self):
        self.id = 0
        self.flags = 0
        self.name = ""
        self.diffuse = (1.0, 1.0, 1.0)
        self.ambient = (0.2, 0.2, 0.2)
        self.specular = (0.0, 0.0, 0.0)


class BrgModel:
    def __init__(self):
        self.name = ""
        self.num_materials = 0
        self.num_meshes = 0
        self.anim_time = 0.0
        self.fps = 30.0
        self.num_frames = 1
        self.meshes = []
        self.materials = []


def parse_brg(data, name="model"):
    if len(data) < 28 or data[:4] != b'BANG':
        raise ValueError("Invalid BRG data: missing BANG header")

    model = BrgModel()
    model.name = name
    magic, u1, num_materials, u2, num_meshes, space, rev = struct.unpack('<4sIIIIIB3x', data[:28])
    model.num_materials = num_materials
    model.num_meshes = num_meshes

    pos = 28
    if num_meshes > 1 or data[pos:pos+4] == b'ASET':
        tag, num_frames, frame_step, anim_time, freq, spf, fps, space2 = struct.unpack('<4sIfffffI', data[pos:pos+32])
        model.num_frames = num_frames
        model.anim_time = anim_time
        model.fps = fps if fps > 0.0 else (1.0 / spf if spf > 0.0 else 30.0)
        pos += 32

    for mi in range(num_meshes):
        if pos + 4 > len(data):
            break
        tag = data[pos:pos+4]
        if tag != b'MESI':
            break

        mesh = BrgMesh()
        hdr = data[pos+4:pos+84]
        ver, fmt, nv, nf, interp, atype, udc = struct.unpack('<HHhhBBH', hdr[:12])
        center = struct.unpack('<3f', hdr[12:24])
        radius = struct.unpack('<f', hdr[24:28])[0]
        mass = struct.unpack('<3f', hdr[28:40])
        hotspot = struct.unpack('<3f', hdr[40:52])
        ext_sz, flags = struct.unpack('<HH', hdr[52:56])
        min_ext = struct.unpack('<3f', hdr[56:68])
        max_ext = struct.unpack('<3f', hdr[68:80])

        mesh.version = ver
        mesh.flags = flags
        mesh.num_vertices = nv
        mesh.num_faces = nf
        mesh.hotspot = hotspot
        mesh.center = center
        mesh.is_secondary = bool(flags & 0x0400)

        p = pos + 84

        # Read vertices
        is_half = (ver == 22)
        v_stride = 6 if is_half else 12
        for i in range(nv):
            if is_half:
                x = aom_half_to_float(data[p+i*6+0 : p+i*6+2])
                y = aom_half_to_float(data[p+i*6+2 : p+i*6+4])
                z = aom_half_to_float(data[p+i*6+4 : p+i*6+6])
            else:
                x, y, z = struct.unpack('<fff', data[p+i*12:p+i*12+12])
            mesh.vertices.append((x, y, z))
        p += nv * v_stride

        # Read normals
        for i in range(nv):
            if is_half:
                x = aom_half_to_float(data[p+i*6+0 : p+i*6+2])
                y = aom_half_to_float(data[p+i*6+2 : p+i*6+4])
                z = aom_half_to_float(data[p+i*6+4 : p+i*6+6])
            else:
                x, y, z = struct.unpack('<fff', data[p+i*12:p+i*12+12])
            mesh.normals.append((x, y, z))
        p += nv * v_stride

        # Read UVs if not secondary and has TEXCOORDSA (0x0002)
        if not mesh.is_secondary and (flags & 0x0002):
            uv_stride = 4 if is_half else 8
            for i in range(nv):
                if is_half:
                    u = aom_half_to_float(data[p+i*4+0 : p+i*4+2])
                    v = aom_half_to_float(data[p+i*4+2 : p+i*4+4])
                else:
                    u, v = struct.unpack('<ff', data[p+i*8:p+i*8+8])
                mesh.uvs.append((u, v))
            p += nv * uv_stride

        # Read face materials & triangle indices if not secondary and has MATERIAL (0x0040)
        if not mesh.is_secondary and (flags & 0x0040):
            for i in range(nf):
                fm = struct.unpack('<H', data[p+i*2 : p+i*2+2])[0]
                mesh.face_mats.append(fm)
            p += nf * 2

            for i in range(nf):
                a, b, c = struct.unpack('<HHH', data[p+i*6 : p+i*6+6])
                mesh.indices.extend([a, b, c])
            p += nf * 6

            for i in range(nv):
                vm = struct.unpack('<H', data[p+i*2 : p+i*2+2])[0]
                mesh.vert_mats.append(vm)
            p += nv * 2

        model.meshes.append(mesh)

        # Advance to next tag
        next_tag = len(data)
        for t in [b'MESI', b'MTRL']:
            idx = data.find(t, pos + 4)
            if idx != -1 and idx < next_tag:
                next_tag = idx
        pos = next_tag

    # Parse materials
    while pos + 4 <= len(data) and data[pos:pos+4] == b'MTRL':
        mid, mflags, mres, name_len = struct.unpack('<IIfI', data[pos+4:pos+20])
        ambient = struct.unpack('<3f', data[pos+20:pos+32])
        diffuse = struct.unpack('<3f', data[pos+32:pos+44])
        specular = struct.unpack('<3f', data[pos+44:pos+56])
        emissive = struct.unpack('<3f', data[pos+56:pos+68])
        name_bytes = data[pos+68:pos+68+name_len]
        mat_name = name_bytes.decode('latin1', 'ignore').strip('\x00').strip()

        mat = BrgMaterial()
        mat.id = mid
        mat.flags = mflags
        mat.name = mat_name
        mat.ambient = ambient
        mat.diffuse = diffuse
        mat.specular = specular
        model.materials.append(mat)

        next_tag = len(data)
        for t in [b'MTRL']:
            idx = data.find(t, pos + 4)
            if idx != -1 and idx < next_tag:
                next_tag = idx
        pos = next_tag

    return model


def convert_brg_to_gltf(model, output_path=None):
    """
    Converts a parsed BrgModel into glTF 2.0 JSON + binary buffer (.glb or .gltf).
    Includes base mesh, morph targets for all keyframes, and morph animation channel.
    """
    if not model.meshes:
        raise ValueError("Cannot convert empty BRG model")

    base_mesh = model.meshes[0]
    num_verts = len(base_mesh.vertices)
    num_indices = len(base_mesh.indices)

    # 1. Prepare Binary Buffer
    # Layout:
    # 0: base positions (float32 * 3 * nv)
    # 1: base normals   (float32 * 3 * nv)
    # 2: base uvs       (float32 * 2 * nv)
    # 3: indices        (uint16 * num_indices)
    # 4..N: morph targets (pos delta float32 * 3 * nv, norm delta float32 * 3 * nv)
    # N+1: anim key times
    # N+2: anim morph weights

    bin_data = bytearray()

    def align4(b):
        while len(b) % 4 != 0:
            b.append(0)

    # Base positions
    pos_offset = len(bin_data)
    pos_min = [float('inf')] * 3
    pos_max = [float('-inf')] * 3
    for v in base_mesh.vertices:
        for i in range(3):
            pos_min[i] = min(pos_min[i], v[i])
            pos_max[i] = max(pos_max[i], v[i])
        bin_data.extend(struct.pack('<3f', *v))
    pos_len = len(bin_data) - pos_offset
    align4(bin_data)

    # Base normals
    norm_offset = len(bin_data)
    for n in base_mesh.normals:
        bin_data.extend(struct.pack('<3f', *n))
    norm_len = len(bin_data) - norm_offset
    align4(bin_data)

    # Base UVs
    uv_offset = len(bin_data)
    uv_min = [float('inf')] * 2
    uv_max = [float('-inf')] * 2
    if base_mesh.uvs:
        for uv in base_mesh.uvs:
            uv_min[0] = min(uv_min[0], uv[0])
            uv_min[1] = min(uv_min[1], uv[1])
            uv_max[0] = max(uv_max[0], uv[0])
            uv_max[1] = max(uv_max[1], uv[1])
            bin_data.extend(struct.pack('<2f', *uv))
    else:
        uv_min = [0.0, 0.0]
        uv_max = [0.0, 0.0]
        bin_data.extend(b'\x00' * (num_verts * 8))
    uv_len = len(bin_data) - uv_offset
    align4(bin_data)

    # Indices
    idx_offset = len(bin_data)
    for idx in base_mesh.indices:
        bin_data.extend(struct.pack('<H', idx))
    idx_len = len(bin_data) - idx_offset
    align4(bin_data)

    # Morph Targets
    morph_frames = [m for m in model.meshes[1:] if m.is_secondary]
    morph_target_accessors = []

    for frame in morph_frames:
        t_pos_offset = len(bin_data)
        t_pos_min = [float('inf')] * 3
        t_pos_max = [float('-inf')] * 3
        for vi in range(num_verts):
            delta_pos = (
                frame.vertices[vi][0] - base_mesh.vertices[vi][0],
                frame.vertices[vi][1] - base_mesh.vertices[vi][1],
                frame.vertices[vi][2] - base_mesh.vertices[vi][2],
            )
            for k in range(3):
                t_pos_min[k] = min(t_pos_min[k], delta_pos[k])
                t_pos_max[k] = max(t_max_target := t_pos_max[k], delta_pos[k])
            bin_data.extend(struct.pack('<3f', *delta_pos))
        t_pos_len = len(bin_data) - t_pos_offset
        align4(bin_data)

        t_norm_offset = len(bin_data)
        for vi in range(num_verts):
            delta_norm = (
                frame.normals[vi][0] - base_mesh.normals[vi][0],
                frame.normals[vi][1] - base_mesh.normals[vi][1],
                frame.normals[vi][2] - base_mesh.normals[vi][2],
            )
            bin_data.extend(struct.pack('<3f', *delta_norm))
        t_norm_len = len(bin_data) - t_norm_offset
        align4(bin_data)

        morph_target_accessors.append({
            'pos_offset': t_pos_offset,
            'pos_len': t_pos_len,
            'pos_min': t_pos_min,
            'pos_max': t_pos_max,
            'norm_offset': t_norm_offset,
            'norm_len': t_norm_len
        })

    # Animation samplers (if morph frames exist)
    has_anim = len(morph_frames) > 0 and model.anim_time > 0.0
    anim_time_offset = 0
    anim_time_len = 0
    anim_weights_offset = 0
    anim_weights_len = 0
    num_keys = len(morph_frames) + 1

    if has_anim:
        # Keyframe timestamps: 0.0 to anim_time
        anim_time_offset = len(bin_data)
        time_step = model.anim_time / max(1, num_keys - 1)
        for ki in range(num_keys):
            t = ki * time_step
            bin_data.extend(struct.pack('<f', t))
        anim_time_len = len(bin_data) - anim_time_offset
        align4(bin_data)

        # Morph weights per keyframe:
        # Key 0: [0, 0, ...] (base)
        # Key 1: [1, 0, ...] (frame 1 active)
        # Key 2: [0, 1, ...] (frame 2 active)
        anim_weights_offset = len(bin_data)
        num_targets = len(morph_frames)
        for ki in range(num_keys):
            weights = [0.0] * num_targets
            if ki > 0:
                weights[ki - 1] = 1.0
            for w in weights:
                bin_data.extend(struct.pack('<f', w))
        anim_weights_len = len(bin_data) - anim_weights_offset
        align4(bin_data)

    # 2. Build glTF JSON Structure
    buffer_views = [
        {"buffer": 0, "byteOffset": pos_offset, "byteLength": pos_len, "target": 34962},       # 0: POS
        {"buffer": 0, "byteOffset": norm_offset, "byteLength": norm_len, "target": 34962},     # 1: NORM
        {"buffer": 0, "byteOffset": uv_offset, "byteLength": uv_len, "target": 34962},         # 2: UV
        {"buffer": 0, "byteOffset": idx_offset, "byteLength": idx_len, "target": 34963},       # 3: IDX
    ]

    accessors = [
        {"bufferView": 0, "byteOffset": 0, "componentType": 5126, "count": num_verts, "type": "VEC3", "min": pos_min, "max": pos_max}, # 0: POS
        {"bufferView": 1, "byteOffset": 0, "componentType": 5126, "count": num_verts, "type": "VEC3"},                                  # 1: NORM
        {"bufferView": 2, "byteOffset": 0, "componentType": 5126, "count": num_verts, "type": "VEC2", "min": uv_min, "max": uv_max},  # 2: UV
        {"bufferView": 3, "byteOffset": 0, "componentType": 5123, "count": num_indices, "type": "SCALAR"},                             # 3: IDX
    ]

    targets = []
    for mt in morph_target_accessors:
        p_bv = len(buffer_views)
        buffer_views.append({"buffer": 0, "byteOffset": mt['pos_offset'], "byteLength": mt['pos_len'], "target": 34962})
        p_acc = len(accessors)
        accessors.append({"bufferView": p_bv, "byteOffset": 0, "componentType": 5126, "count": num_verts, "type": "VEC3", "min": mt['pos_min'], "max": mt['pos_max']})

        n_bv = len(buffer_views)
        buffer_views.append({"buffer": 0, "byteOffset": mt['norm_offset'], "byteLength": mt['norm_len'], "target": 34962})
        n_acc = len(accessors)
        accessors.append({"bufferView": n_bv, "byteOffset": 0, "componentType": 5126, "count": num_verts, "type": "VEC3"})

        targets.append({"POSITION": p_acc, "NORMAL": n_acc})

    # Material
    materials = []
    tex_name = model.materials[0].name.lower() if model.materials else model.name.lower()
    materials.append({
        "name": model.materials[0].name if model.materials else "Material_0",
        "pbrMetallicRoughness": {
            "baseColorFactor": [1.0, 1.0, 1.0, 1.0],
            "metallicFactor": 0.1,
            "roughnessFactor": 0.8
        },
        "extras": {
            "texture": tex_name + ".ddt"
        }
    })

    mesh_dict = {
        "name": model.name + "_Mesh",
        "primitives": [{
            "attributes": {
                "POSITION": 0,
                "NORMAL": 1,
                "TEXCOORD_0": 2
            },
            "indices": 3,
            "material": 0
        }]
    }
    if targets:
        mesh_dict["primitives"][0]["targets"] = targets
        mesh_dict["weights"] = [0.0] * len(targets)

    # Animations
    animations = []
    if has_anim:
        t_bv = len(buffer_views)
        buffer_views.append({"buffer": 0, "byteOffset": anim_time_offset, "byteLength": anim_time_len})
        t_acc = len(accessors)
        accessors.append({"bufferView": t_bv, "byteOffset": 0, "componentType": 5126, "count": num_keys, "type": "SCALAR", "min": [0.0], "max": [model.anim_time]})

        w_bv = len(buffer_views)
        buffer_views.append({"buffer": 0, "byteOffset": anim_weights_offset, "byteLength": anim_weights_len})
        w_acc = len(accessors)
        accessors.append({"bufferView": w_bv, "byteOffset": 0, "componentType": 5126, "count": num_keys * len(morph_frames), "type": "SCALAR"})

        animations.append({
            "name": "Default",
            "channels": [{
                "sampler": 0,
                "target": {"node": 0, "path": "weights"}
            }],
            "samplers": [{
                "input": t_acc,
                "interpolation": "LINEAR",
                "output": w_acc
            }]
        })

    gltf_json = {
        "asset": {"version": "2.0", "generator": "AoM-Native-Port BRG to glTF Converter (ptasev spec)"},
        "scenes": [{"nodes": [0]}],
        "nodes": [{
            "name": model.name,
            "mesh": 0
        }],
        "meshes": [mesh_dict],
        "materials": materials,
        "accessors": accessors,
        "bufferViews": buffer_views,
        "buffers": [{"byteLength": len(bin_data)}]
    }

    if animations:
        gltf_json["animations"] = animations

    # Package as GLB (Binary glTF)
    json_bytes = json.dumps(gltf_json, separators=(',', ':')).encode('utf-8')
    while len(json_bytes) % 4 != 0:
        json_bytes += b' '

    # GLB Header (12 bytes)
    # Magic: 0x46546C67 ("glTF")
    # Version: 2
    # Length: total file size
    total_len = 12 + 8 + len(json_bytes) + 8 + len(bin_data)
    glb_header = struct.pack('<4sII', b'glTF', 2, total_len)

    # Chunk 0: JSON (type 0x4E4F534A "JSON")
    chunk0_hdr = struct.pack('<II', len(json_bytes), 0x4E4F534A)

    # Chunk 1: BIN (type 0x004E4942 "BIN\0")
    chunk1_hdr = struct.pack('<II', len(bin_data), 0x004E4942)

    glb_content = glb_header + chunk0_hdr + json_bytes + chunk1_hdr + bytes(bin_data)

    if output_path:
        with open(output_path, 'wb') as f:
            f.write(glb_content)

    return glb_content, gltf_json


def main():
    parser = argparse.ArgumentParser(description="Convert AoM BRG files to glTF 2.0 / GLB")
    parser.add_argument("input", help="Path to .brg file or entry name inside archive")
    parser.add_argument("-o", "--output", help="Output .glb or .gltf path", default=None)
    parser.add_argument("--bar", help="Path to models.bar archive", default=None)
    args = parser.parse_args()

    data = None
    name = os.path.splitext(os.path.basename(args.input))[0]

    if args.bar or not os.path.exists(args.input):
        bar_path = args.bar
        if not bar_path:
            for bp in ['/sdcard/AoM/models/models.bar', '/sdcard/AoM/models/models2.bar']:
                if os.path.exists(bp):
                    bar_path = bp
                    break
        if bar_path:
            bar = BarArchive(bar_path)
            entry = bar.find(args.input)
            if not entry:
                entry = bar.find(args.input + '.brg')
            if entry:
                data = bar.extract(entry)
                name = entry.filename
            else:
                print(f"Error: Entry '{args.input}' not found in {bar_path}")
                sys.exit(1)
        else:
            print("Error: No BAR archive found.")
            sys.exit(1)
    else:
        with open(args.input, 'rb') as f:
            data = f.read()

    print(f"Parsing BRG '{name}' ({len(data)} bytes)...")
    model = parse_brg(data, name)
    print(f"Loaded {len(model.meshes)} mesh(es), {len(model.materials)} material(s)")
    for i, m in enumerate(model.meshes):
        print(f"  Mesh {i}: nv={m.num_vertices}, nf={m.num_faces}, is_sec={m.is_secondary}")
    if model.num_meshes > 1:
        print(f"  Animation: frames={model.num_frames}, duration={model.anim_time:.3f}s, fps={model.fps:.1f}")

    out_file = args.output or (os.path.splitext(args.input)[0] + ".glb")
    glb, j = convert_brg_to_gltf(model, out_file)
    print(f"Successfully generated GLB '{out_file}' ({len(glb)} bytes) with {len(j['accessors'])} accessors.")


if __name__ == '__main__':
    main()
