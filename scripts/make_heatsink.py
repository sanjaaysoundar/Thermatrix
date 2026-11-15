#!/usr/bin/env python3
# scripts/make_heatsink.py
# Generates the conforming tapered heat sink CAD model, watertight STL,
# 3-level meshes, cube benchmark models, and fin-sweep meshes using Gmsh.
#
# Geometry Specification (FIX 3 & FIX 5):
#   Base: 60 x 60 x 4 mm
#   Fins: 9 Tapered plate fins (2.0 mm root to 1.2 mm tip, 25 mm tall)
#   Gaps: 4.0 mm at root (about 3 to 4 mm), 4.8 mm at tip
#   Source patch: 10 x 10 mm on underside (z=0), offset dx=-10 mm, dy=+5 mm from center
#   Underside outside patch: adiabatic
#   All other exposed faces: convective Robin

import os
import sys
import collections
import gmsh

def build_heat_sink(model_name, lc_min, lc_max, n_fins=9, pitch=0.0060, y0=0.0060):
    gmsh.model.add(model_name)

    # 1. Base Box: 60 x 60 x 4 mm
    L_x = 0.060
    L_y = 0.060
    H_base = 0.004
    base = gmsh.model.occ.addBox(0.0, 0.0, 0.0, L_x, L_y, H_base)

    # 2. Tapered Plate Fins (25 mm tall, 2.0 mm root to 1.2 mm tip)
    H_fin = 0.025
    Z_top = H_base + H_fin
    t_root_half = 0.0010   # 1.0 mm (root thickness = 2.0 mm)
    t_tip_half  = 0.0006   # 0.6 mm (tip thickness = 1.2 mm)

    fins = []
    for i in range(n_fins):
        yc = y0 + i * pitch
        # 4 vertices of the trapezoid in y-z plane at x = 0
        p1 = gmsh.model.occ.addPoint(0.0, yc - t_root_half, H_base)
        p2 = gmsh.model.occ.addPoint(0.0, yc + t_root_half, H_base)
        p3 = gmsh.model.occ.addPoint(0.0, yc + t_tip_half,  Z_top)
        p4 = gmsh.model.occ.addPoint(0.0, yc - t_tip_half,  Z_top)

        l1 = gmsh.model.occ.addLine(p1, p2)
        l2 = gmsh.model.occ.addLine(p2, p3)
        l3 = gmsh.model.occ.addLine(p3, p4)
        l4 = gmsh.model.occ.addLine(p4, p1)

        cl = gmsh.model.occ.addCurveLoop([l1, l2, l3, l4])
        s = gmsh.model.occ.addPlaneSurface([cl])
        ext = gmsh.model.occ.extrude([(2, s)], L_x, 0.0, 0.0)
        for ent in ext:
            if ent[0] == 3:
                fins.append(ent)

    # 3. Fuse base and fins into a single solid
    fuse_res, _ = gmsh.model.occ.fuse([(3, base)], fins)
    solid_vol = fuse_res[0][1]

    # 4. Fragment the 10x10 mm source patch on the bottom (z = 0)
    # Base center: (0.030, 0.030). Offset dx=-10 mm, dy=+5 mm -> Center (0.020, 0.035)
    # Patch box on bottom: x in [0.015, 0.025], y in [0.030, 0.040]
    patch_s = gmsh.model.occ.addRectangle(0.015, 0.030, 0.0, 0.010, 0.010)
    gmsh.model.occ.fragment([(3, solid_vol)], [(2, patch_s)])
    gmsh.model.occ.synchronize()

    # 5. Classify Boundary Surfaces
    patch_tags = []
    adiab_tags = []
    conv_tags = []

    for _, s_tag in gmsh.model.getEntities(2):
        com = gmsh.model.occ.getCenterOfMass(2, s_tag)
        if abs(com[2]) < 1e-6:
            # Bottom surface at z = 0
            if (0.015 - 1e-5 <= com[0] <= 0.025 + 1e-5 and
                0.030 - 1e-5 <= com[1] <= 0.040 + 1e-5):
                patch_tags.append(s_tag)
            else:
                adiab_tags.append(s_tag)
        else:
            # Exposed convective surface
            conv_tags.append(s_tag)

    # 6. Physical Groups (Solid=1, Patch=10, Adiabatic=20, Convective=30)
    gmsh.model.addPhysicalGroup(3, [solid_vol], tag=1, name='Solid')
    gmsh.model.addPhysicalGroup(2, patch_tags,  tag=10, name='SourcePatch')
    gmsh.model.addPhysicalGroup(2, adiab_tags,  tag=20, name='Adiabatic')
    gmsh.model.addPhysicalGroup(2, conv_tags,   tag=30, name='Convective')

    # 7. Mesh Generation
    gmsh.option.setNumber('Mesh.CharacteristicLengthMin', lc_min)
    gmsh.option.setNumber('Mesh.CharacteristicLengthMax', lc_max)
    gmsh.option.setNumber('Mesh.Algorithm', 6)     # Frontal-Delaunay for 2D
    gmsh.option.setNumber('Mesh.Algorithm3D', 1)   # Delaunay for 3D
    gmsh.option.setNumber('Mesh.Optimize', 1)      # Netgen optimizer for element quality
    gmsh.option.setNumber('Mesh.MshFileVersion', 2.2)

    gmsh.model.mesh.generate(3)
    return solid_vol, patch_tags, adiab_tags, conv_tags

def build_cube(model_name, lc_min, lc_max):
    gmsh.model.add(model_name)
    box = gmsh.model.occ.addBox(0.0, 0.0, 0.0, 0.060, 0.060, 0.060)
    patch_s = gmsh.model.occ.addRectangle(0.015, 0.030, 0.0, 0.010, 0.010)
    gmsh.model.occ.fragment([(3, box)], [(2, patch_s)])
    gmsh.model.occ.synchronize()

    patch_tags = []
    adiab_tags = []
    conv_tags = []
    for _, s_tag in gmsh.model.getEntities(2):
        com = gmsh.model.occ.getCenterOfMass(2, s_tag)
        if abs(com[2]) < 1e-6:
            if 0.015 - 1e-5 <= com[0] <= 0.025 + 1e-5 and 0.030 - 1e-5 <= com[1] <= 0.040 + 1e-5:
                patch_tags.append(s_tag)
            else:
                adiab_tags.append(s_tag)
        else:
            conv_tags.append(s_tag)

    gmsh.model.addPhysicalGroup(3, [box],       tag=1,  name='Solid')
    gmsh.model.addPhysicalGroup(2, patch_tags, tag=10, name='SourcePatch')
    gmsh.model.addPhysicalGroup(2, adiab_tags, tag=20, name='Adiabatic')
    gmsh.model.addPhysicalGroup(2, conv_tags,  tag=30, name='Convective')

    gmsh.option.setNumber('Mesh.CharacteristicLengthMin', lc_min)
    gmsh.option.setNumber('Mesh.CharacteristicLengthMax', lc_max)
    gmsh.option.setNumber('Mesh.Algorithm', 6)
    gmsh.option.setNumber('Mesh.Algorithm3D', 1)
    gmsh.option.setNumber('Mesh.Optimize', 1)
    gmsh.option.setNumber('Mesh.MshFileVersion', 2.2)

    gmsh.model.mesh.generate(3)
    return box

def check_stl_watertight(stl_path):
    with open(stl_path, 'r') as f:
        lines = f.readlines()

    triangles = []
    cur_tri = []
    for line in lines:
        parts = line.strip().split()
        if not parts:
            continue
        if parts[0] == 'vertex':
            cur_tri.append(tuple(round(float(x), 7) for x in parts[1:4]))
            if len(cur_tri) == 3:
                triangles.append(cur_tri)
                cur_tri = []

    edges = collections.defaultdict(int)
    for t in triangles:
        for i in range(3):
            p1, p2 = t[i], t[(i + 1) % 3]
            edge = tuple(sorted([p1, p2]))
            edges[edge] += 1

    non_manifold = [e for e, count in edges.items() if count != 2]
    return len(triangles), len(edges), len(non_manifold)

def main():
    print("=================================================================")
    print(" THERMATRIX Heat Sink CAD, STL & Tetrahedral Meshes Generator")
    print("=================================================================")

    os.makedirs("data", exist_ok=True)
    os.makedirs("bin/data", exist_ok=True)

    gmsh.initialize()
    gmsh.option.setNumber("General.Verbosity", 1) # Quiet output

    # -----------------------------------------------------------------
    # Dimensions & Analytical Properties (9 Fins)
    # -----------------------------------------------------------------
    L_x = 0.060
    L_y = 0.060
    H_base = 0.004
    H_fin = 0.025
    H_total = H_base + H_fin
    n_fins = 9
    t_root = 0.0020
    t_tip  = 0.0012

    V_base = L_x * L_y * H_base
    A_trap = 0.5 * (t_root + t_tip) * H_fin
    V_fins = n_fins * (L_x * A_trap)
    V_analytical = V_base + V_fins
    A_conv_analytical = 0.031851456

    print("\n--- Final Heat Sink Specifications ---")
    print(f"  Base Dimensions: {L_x*1e3:.1f} x {L_y*1e3:.1f} x {H_base*1e3:.1f} mm")
    print(f"  Fin Count:       {n_fins} tapered plate fins")
    print(f"  Fin Height:      {H_fin*1e3:.1f} mm (Total Height = {H_total*1e3:.1f} mm)")
    print(f"  Fin Thickness:   Root = {t_root*1e3:.1f} mm, Tip = {t_tip*1e3:.1f} mm")
    print(f"  Fin Pitch:       6.0 mm (Gap at root = 4.0 mm, Gap at tip = 4.8 mm)")
    print(f"  Source Patch:    10.0 x 10.0 mm on underside, center offset dx=-10mm, dy=+5mm")
    print(f"  Analytical Solid Volume:   {V_analytical:.6e} m^3 ({V_analytical*1e6:.1f} mm^3)")
    print(f"  Analytical Exposed Area:   {A_conv_analytical:.6e} m^2 ({A_conv_analytical*1e6:.1f} mm^2)")

    # -----------------------------------------------------------------
    # 1. Generate 9-Fin Heat Sink (Coarse, Medium, Fine)
    # -----------------------------------------------------------------
    print("\n[1/4] Generating 9-Fin Conforming Tetrahedral Meshes...")
    build_heat_sink("Coarse_9Fins", 0.0035, 0.0045, n_fins=9, pitch=0.0060, y0=0.0060)
    gmsh.option.setNumber('Mesh.SaveAll', 1)
    gmsh.write("data/heatsink.stl")
    gmsh.write("bin/data/heatsink.stl")
    gmsh.option.setNumber('Mesh.SaveAll', 0)
    gmsh.write("data/heatsink_coarse.msh")
    gmsh.write("bin/data/heatsink_coarse.msh")
    print(f"      Coarse Mesh: {len(gmsh.model.mesh.getNodes()[0])} nodes, {len(gmsh.model.mesh.getElementsByType(4)[0])} tets")

    # STL Watertightness Check
    n_tri, n_edg, n_non_man = check_stl_watertight("data/heatsink.stl")
    print(f"      STL Watertightness Check: {n_tri} facets, {n_edg} unique edges")
    if n_non_man == 0:
        print("      [PASS] 100% Watertight: every edge is shared by exactly 2 triangles!")
    else:
        print(f"      [FAIL] Found {n_non_man} non-manifold edges in data/heatsink.stl")
        sys.exit(1)

    build_heat_sink("Medium_9Fins", 0.0022, 0.0030, n_fins=9, pitch=0.0060, y0=0.0060)
    gmsh.option.setNumber('Mesh.SaveAll', 0)
    gmsh.write("data/heatsink_medium.msh")
    gmsh.write("bin/data/heatsink_medium.msh")
    print(f"      Medium Mesh: {len(gmsh.model.mesh.getNodes()[0])} nodes, {len(gmsh.model.mesh.getElementsByType(4)[0])} tets")

    build_heat_sink("Fine_9Fins", 0.0015, 0.0020, n_fins=9, pitch=0.0060, y0=0.0060)
    gmsh.option.setNumber('Mesh.SaveAll', 0)
    gmsh.write("data/heatsink_fine.msh")
    gmsh.write("bin/data/heatsink_fine.msh")
    print(f"      Fine Mesh:   {len(gmsh.model.mesh.getNodes()[0])} nodes, {len(gmsh.model.mesh.getElementsByType(4)[0])} tets")

    # -----------------------------------------------------------------
    # 2. Generate 60x60x60 mm Solid Cube (Coarse, Medium, Fine)
    # -----------------------------------------------------------------
    print("\n[2/4] Generating 60x60x60 mm Solid Cube Meshes...")
    build_cube("Cube_Coarse", 0.007, 0.010)
    gmsh.option.setNumber('Mesh.SaveAll', 1)
    gmsh.write("data/cube.stl")
    gmsh.write("bin/data/cube.stl")
    gmsh.option.setNumber('Mesh.SaveAll', 0)
    gmsh.write("data/cube_coarse.msh")
    gmsh.write("bin/data/cube_coarse.msh")
    print(f"      Cube Coarse: {len(gmsh.model.mesh.getNodes()[0])} nodes, {len(gmsh.model.mesh.getElementsByType(4)[0])} tets")

    build_cube("Cube_Medium", 0.004, 0.006)
    gmsh.option.setNumber('Mesh.SaveAll', 0)
    gmsh.write("data/cube_medium.msh")
    gmsh.write("bin/data/cube_medium.msh")
    print(f"      Cube Medium: {len(gmsh.model.mesh.getNodes()[0])} nodes, {len(gmsh.model.mesh.getElementsByType(4)[0])} tets")

    build_cube("Cube_Fine", 0.0025, 0.0035)
    gmsh.option.setNumber('Mesh.SaveAll', 0)
    gmsh.write("data/cube_fine.msh")
    gmsh.write("bin/data/cube_fine.msh")
    print(f"      Cube Fine:   {len(gmsh.model.mesh.getNodes()[0])} nodes, {len(gmsh.model.mesh.getElementsByType(4)[0])} tets")

    # -----------------------------------------------------------------
    # 3. Generate 3-Fin and 5-Fin Meshes for Complexity Study
    # -----------------------------------------------------------------
    print("\n[3/4] Generating Fin Sweep Meshes (3 Fins, 5 Fins)...")
    # 3 Fins: pitch = 15 mm, y0 = 15 mm -> fins at 15, 30, 45 mm
    build_heat_sink("Heatsink_3Fins", 0.0025, 0.0035, n_fins=3, pitch=0.0150, y0=0.0150)
    gmsh.option.setNumber('Mesh.SaveAll', 0)
    gmsh.write("data/heatsink_3fins.msh")
    gmsh.write("bin/data/heatsink_3fins.msh")
    print(f"      3-Fin Mesh:  {len(gmsh.model.mesh.getNodes()[0])} nodes, {len(gmsh.model.mesh.getElementsByType(4)[0])} tets")

    # 5 Fins: pitch = 10 mm, y0 = 10 mm -> fins at 10, 20, 30, 40, 50 mm
    build_heat_sink("Heatsink_5Fins", 0.0025, 0.0035, n_fins=5, pitch=0.0100, y0=0.0100)
    gmsh.option.setNumber('Mesh.SaveAll', 0)
    gmsh.write("data/heatsink_5fins.msh")
    gmsh.write("bin/data/heatsink_5fins.msh")
    print(f"      5-Fin Mesh:  {len(gmsh.model.mesh.getNodes()[0])} nodes, {len(gmsh.model.mesh.getElementsByType(4)[0])} tets")

    gmsh.finalize()
    print("\n[SUCCESS] Geometry generation, watertight STL, and all meshes completed.")

if __name__ == "__main__":
    main()
