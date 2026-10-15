#!/usr/bin/env python3
"""Writes the two model files the Model Import room reads: Content/Models/Import/cottage.x (DirectX text format) and cottage.fbx (FBX 6.1 ASCII).

Both describe the same cottage: a mesh with two materials (cream walls, a rust roof) and a child frame that holds the chimney, a second
mesh with a third material, so the importers' node trees, per-polygon materials and hierarchy transforms all have something to show.
Nothing is downloaded: the geometry is generated here. Run from the repository root:  python3 tools/gen_models.py
"""
import os

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Content", "Models", "Import")

# Right-handed, Y up, faces listed counter-clockwise seen from outside.
def box(x0, y0, z0, x1, y1, z1):
    v = [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0), (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)]
    faces = [(4, 5, 6, 7), (1, 0, 3, 2), (5, 1, 2, 6), (0, 4, 7, 3), (3, 7, 6, 2), (0, 1, 5, 4)]   # +z, -z, +x, -x, +y, -y
    return v, faces


def cottage():
    """Walls (material 0) and a gabled roof (material 1) as one polygon soup."""
    positions, faces, materials = [], [], []
    wv, wf = box(-1.0, 0.0, -0.8, 1.0, 1.0, 0.8)
    base = len(positions)
    positions += wv
    for f in wf:
        faces.append(tuple(base + i for i in f))
        materials.append(0)
    # Roof: eaves at y = 1 (a little wider than the walls), ridge at y = 1.65 along x.
    a, b, c, d = (-1.15, 1.0, -0.95), (1.15, 1.0, -0.95), (1.15, 1.0, 0.95), (-1.15, 1.0, 0.95)
    r0, r1 = (-1.15, 1.65, 0.0), (1.15, 1.65, 0.0)
    base = len(positions)
    positions += [a, b, c, d, r0, r1]
    A, B, C, D, R0, R1 = [base + i for i in range(6)]
    faces += [(D, C, R1, R0), (B, A, R0, R1), (A, D, R0), (C, B, R1)]   # +z slope, -z slope, -x gable, +x gable
    materials += [1, 1, 1, 1]
    return positions, faces, materials


def sub(p, q): return (p[0] - q[0], p[1] - q[1], p[2] - q[2])
def cross(p, q): return (p[1] * q[2] - p[2] * q[1], p[2] * q[0] - p[0] * q[2], p[0] * q[1] - p[1] * q[0])
def norm(p):
    n = (p[0] ** 2 + p[1] ** 2 + p[2] ** 2) ** 0.5
    return (p[0] / n, p[1] / n, p[2] / n)
def face_normal(positions, f): return norm(cross(sub(positions[f[1]], positions[f[0]]), sub(positions[f[2]], positions[f[0]])))


MATERIALS = [("Walls", (0.93, 0.86, 0.70), 8.0), ("Roof", (0.66, 0.26, 0.18), 6.0), ("Brick", (0.55, 0.30, 0.25), 4.0)]
CHIMNEY_AT = (0.55, 1.35, -0.25)


def write_x():
    p, faces, mats = cottage()
    cv, cf = box(-0.15, 0.0, -0.15, 0.15, 0.7, 0.15)
    lines = ["xof 0303txt 0032", "// Written by tools/gen_models.py for the CNA 3D Museum. A cottage: walls and a roof in one mesh, a chimney in a child frame.", ""]

    def mesh(name, positions, fs, ms, mat_defs):
        # DirectX is left-handed with clockwise front faces: mirror z and reverse each face so the importer's basis change gives the cottage back.
        lp = [(x, y, -z) for (x, y, z) in positions]
        out = [f"  Mesh {name} {{", f"    {len(lp)};"]
        out += [f"    {x:.6f};{y:.6f};{z:.6f};" + ("," if i < len(lp) - 1 else ";") for i, (x, y, z) in enumerate(lp)]
        rf = [tuple(reversed(f)) for f in fs]
        out.append(f"    {len(rf)};")
        out += [f"    {len(f)};" + ",".join(str(i) for i in f) + (";," if i < len(rf) - 1 else ";;") for i, f in enumerate(rf)]
        normals = [face_normal(lp, f) for f in rf]
        out += ["    MeshNormals {", f"      {len(normals)};"]
        out += [f"      {x:.6f};{y:.6f};{z:.6f};" + ("," if i < len(normals) - 1 else ";") for i, (x, y, z) in enumerate(normals)]
        out.append(f"      {len(rf)};")
        out += [f"      {len(f)};" + ",".join([str(i)] * len(f)) + (";," if i < len(rf) - 1 else ";;") for i, f in enumerate(rf)]
        out.append("    }")
        out += ["    MeshMaterialList {", f"      {len(mat_defs)};", f"      {len(ms)};", "      " + ",".join(str(m) for m in ms) + ";;"]
        for name_m, (r, g, b), shine in mat_defs:
            out += [f"      Material {name_m} {{", f"        {r:.6f};{g:.6f};{b:.6f};1.000000;;", f"        {shine:.6f};", "        0.200000;0.200000;0.200000;;", "        0.000000;0.000000;0.000000;;", "      }"]
        out += ["    }", "  }"]
        return out

    lines += ["Frame Cottage {", "  FrameTransformMatrix {", "    1.000000,0.000000,0.000000,0.000000,", "    0.000000,1.000000,0.000000,0.000000,", "    0.000000,0.000000,1.000000,0.000000,", "    0.000000,0.000000,0.000000,1.000000;;", "  }"]
    lines += mesh("CottageMesh", p, faces, mats, MATERIALS[:2])
    x, y, z = CHIMNEY_AT
    lines += ["  Frame Chimney {", "    FrameTransformMatrix {", "      1.000000,0.000000,0.000000,0.000000,", "      0.000000,1.000000,0.000000,0.000000,", "      0.000000,0.000000,1.000000,0.000000,",
              f"      {x:.6f},{y:.6f},{-z:.6f},1.000000;;", "    }"]
    lines += ["  " + l for l in mesh("ChimneyMesh", cv, cf, [0] * len(cf), MATERIALS[2:])]
    lines += ["  }", "}", ""]
    with open(os.path.join(OUT, "cottage.x"), "w", newline="\n") as f:
        f.write("\n".join(lines))


def write_fbx():
    p, faces, mats = cottage()
    cv, cf = box(-0.15, 0.0, -0.15, 0.15, 0.7, 0.15)

    def model(name, positions, fs, ms, translation, parent_material):
        verts = ",".join(f"{x:g},{y:g},{z:g}" for x, y, z in positions)
        poly = []
        for f in fs:
            poly += [str(i) for i in f[:-1]] + [str(-(f[-1] + 1))]
        normals = []
        for f in fs:
            n = face_normal(positions, f)
            normals += [f"{n[0]:g},{n[1]:g},{n[2]:g}"] * len(f)
        out = [f'\tModel: "Model::{name}", "Mesh" {{', "\t\tVersion: 232", "\t\tProperties60:  {",
               f'\t\t\tProperty: "Lcl Translation", "Lcl Translation", "A+",{translation[0]:g},{translation[1]:g},{translation[2]:g}',
               '\t\t\tProperty: "Lcl Rotation", "Lcl Rotation", "A+",0,0,0', '\t\t\tProperty: "Lcl Scaling", "Lcl Scaling", "A+",1,1,1', "\t\t}",
               "\t\tMultiLayer: 0", "\t\tMultiTake: 1", "\t\tShading: Y", '\t\tCulling: "CullingOff"',
               f"\t\tVertices: {verts}", f"\t\tPolygonVertexIndex: {','.join(poly)}", "\t\tGeometryVersion: 124",
               "\t\tLayerElementNormal: 0 {", "\t\t\tVersion: 101", '\t\t\tName: ""', '\t\t\tMappingInformationType: "ByPolygonVertex"', '\t\t\tReferenceInformationType: "Direct"',
               f"\t\t\tNormals: {','.join(normals)}", "\t\t}",
               "\t\tLayerElementMaterial: 0 {", "\t\t\tVersion: 101", '\t\t\tName: ""', '\t\t\tMappingInformationType: "ByPolygon"', '\t\t\tReferenceInformationType: "IndexToDirect"',
               f"\t\t\tMaterials: {','.join(str(m) for m in ms)}", "\t\t}",
               "\t\tLayer: 0 {", "\t\t\tVersion: 100", "\t\t\tLayerElement:  {", '\t\t\t\tType: "LayerElementNormal"', "\t\t\t\tTypedIndex: 0", "\t\t\t}",
               "\t\t\tLayerElement:  {", '\t\t\t\tType: "LayerElementMaterial"', "\t\t\t\tTypedIndex: 0", "\t\t\t}", "\t\t}", "\t}"]
        return out

    def material(name, rgb, shine):
        return [f'\tMaterial: "Material::{name}", "" {{', "\t\tVersion: 102", '\t\tShadingModel: "phong"', "\t\tMultiLayer: 0", "\t\tProperties60:  {", '\t\t\tProperty: "ShadingModel", "KString", "", "phong"',
                f'\t\t\tProperty: "DiffuseColor", "Color", "A+",{rgb[0]:g},{rgb[1]:g},{rgb[2]:g}', '\t\t\tProperty: "SpecularColor", "Color", "A+",0.2,0.2,0.2', '\t\t\tProperty: "EmissiveColor", "Color", "A+",0,0,0',
                '\t\t\tProperty: "Opacity", "double", "",1', f'\t\t\tProperty: "Shininess", "double", "",{shine:g}', "\t\t}", "\t}"]

    L = ["; FBX 6.1.0 project file", "; Written by tools/gen_models.py for the CNA 3D Museum.", "; ----------------------------------------------------", "",
         "FBXHeaderExtension:  {", "\tFBXHeaderVersion: 1003", "\tFBXVersion: 6100", "\tCreator: \"CNA\"", "}", "",
         "Definitions:  {", "\tVersion: 100", "\tCount: 5", '\tObjectType: "Model" {', "\t\tCount: 2", "\t}", '\tObjectType: "Material" {', "\t\tCount: 3", "\t}", '\tObjectType: "Deformer" {', "\t\tCount: 0", "\t}", "}", "",
         "Objects:  {"]
    L += model("Cottage", p, faces, mats, (0, 0, 0), None)
    L += model("Chimney", cv, cf, [0] * len(cf), CHIMNEY_AT, None)
    for name, rgb, shine in MATERIALS:
        L += material(name, rgb, shine)
    L += ["}", "", "Relations:  {", '\tModel: "Model::Cottage", "Mesh" {', "\t}", '\tModel: "Model::Chimney", "Mesh" {', "\t}"]
    for name, _, _ in MATERIALS:
        L += [f'\tMaterial: "Material::{name}", "" {{', "\t}"]
    L += ["}", "", "Connections:  {", '\tConnect: "OO", "Model::Cottage", "Model::Scene"', '\tConnect: "OO", "Model::Chimney", "Model::Cottage"',
          '\tConnect: "OO", "Material::Walls", "Model::Cottage"', '\tConnect: "OO", "Material::Roof", "Model::Cottage"', '\tConnect: "OO", "Material::Brick", "Model::Chimney"', "}", "",
          "Takes:  {", '\tCurrent: ""', "}", ""]
    with open(os.path.join(OUT, "cottage.fbx"), "w", newline="\n") as f:
        f.write("\n".join(L))


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    write_x()
    write_fbx()
    for n in ("cottage.x", "cottage.fbx"):
        print(n, os.path.getsize(os.path.join(OUT, n)), "bytes")
