#!/usr/bin/env python3
# scripts/make_heatsink.py
# Generates the conforming tapered heat sink CAD model, watertight STL,
# and three conforming tetrahedral mesh levels using Gmsh (Python API).
#
# Geometry Specification (FIX 3):
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

def build_heat_sink(model_name, lc_min, lc_max):
    gmsh.model.add(model_name)

    # 1. Base Box: 60 x 60 x 4 mm
    L_x = 0.060
    L_y = 0.060
    H_base = 0.004
    base = gmsh.model.occ.addBox(0.0, 0.0, 0.0, L_x, L_y, H_base)

    # 2. 9 Tapered Plate Fins (25 mm tall, 2.0 mm root to 1.2 mm tip)
    H_fin = 0.025
    Z_top = H_base + H_fin # 0.029 m
    t_root_half = 0.0010   # 1.0 mm (root thickness = 2.0 mm)
    t_tip_half  = 0.0006   # 0.6 mm (tip thickness = 1.2 mm)
    pitch = 0.0060         # 6.0 mm pitch (gap at root = 4.0 mm)
    y0 = 0.0060            # First fin center: 6.0 mm

    fins = []
    for i in range(9):
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
    # Dimensions & Analytical Properties
    # -----------------------------------------------------------------
    L_x = 0.060
    L_y = 0.060
    H_base = 0.004
    H_fin = 0.025
    H_total = H_base + H_fin
    n_fins = 9
    t_root = 0.0020
    t_tip  = 0.0012

    V_base = L_x * L_y * H_base # 1.44e-5 m^3
    A_trap = 0.5 * (t_root + t_tip) * H_fin # 4.0e-5 m^2
    V_fins = n_fins * (L_x * A_trap) # 2.16e-5 m^3
    V_analytical = V_base + V_fins # 3.60e-5 m^3

    # Analytical Exposed Area:
    # Base sides: 2*(60*4) + 2*(60*4) = 960 mm^2
    # Base top: 60*60 - 9*(60*2) = 2520 mm^2
    # Fins slant: 9 * 2 * (60 * sqrt(25^2 + 0.4^2)) = 27003.456 mm^2
    # Fins trapezoids: 9 * 2 * 40 = 720 mm^2
    # Fins tips: 9 * (60 * 1.2) = 648 mm^2
    A_conv_analytical = 0.031851456 # m^2

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
    # Generate Coarse Mesh & Watertight STL
    # -----------------------------------------------------------------
    print("\n[1/3] Generating Coarse Conforming Tetrahedral Mesh...")
    build_heat_sink("Coarse", 0.0035, 0.0045)
    gmsh.option.setNumber('Mesh.SaveAll', 1)
    gmsh.write("data/heatsink.stl")
    gmsh.write("bin/data/heatsink.stl")
    gmsh.option.setNumber('Mesh.SaveAll', 0)
    gmsh.write("data/heatsink_coarse.msh")
    gmsh.write("bin/data/heatsink_coarse.msh")

    nodes_c = len(gmsh.model.mesh.getNodes()[0])
    tets_c  = len(gmsh.model.mesh.getElementsByType(4)[0])
    print(f"      Coarse Mesh: {nodes_c} nodes, {tets_c} tetrahedra")

    # Watertightness Verification of data/heatsink.stl
    n_tri, n_edg, n_non_man = check_stl_watertight("data/heatsink.stl")
    print(f"      STL Watertightness Check: {n_tri} facets, {n_edg} unique edges")
    if n_non_man == 0:
        print("      [PASS] 100% Watertight: every edge is shared by exactly 2 triangles!")
    else:
        print(f"      [FAIL] Found {n_non_man} non-manifold edges in data/heatsink.stl")
        sys.exit(1)

    # -----------------------------------------------------------------
    # Generate Medium Mesh
    # -----------------------------------------------------------------
    print("\n[2/3] Generating Medium Conforming Tetrahedral Mesh...")
    build_heat_sink("Medium", 0.0022, 0.0030)
    gmsh.option.setNumber('Mesh.SaveAll', 0)
    gmsh.write("data/heatsink_medium.msh")
    gmsh.write("bin/data/heatsink_medium.msh")

    nodes_m = len(gmsh.model.mesh.getNodes()[0])
    tets_m  = len(gmsh.model.mesh.getElementsByType(4)[0])
    print(f"      Medium Mesh: {nodes_m} nodes, {tets_m} tetrahedra")

    # -----------------------------------------------------------------
    # Generate Fine Mesh
    # -----------------------------------------------------------------
    print("\n[3/3] Generating Fine Conforming Tetrahedral Mesh...")
    build_heat_sink("Fine", 0.0015, 0.0020)
    gmsh.option.setNumber('Mesh.SaveAll', 0)
    gmsh.write("data/heatsink_fine.msh")
    gmsh.write("bin/data/heatsink_fine.msh")

    nodes_f = len(gmsh.model.mesh.getNodes()[0])
    tets_f  = len(gmsh.model.mesh.getElementsByType(4)[0])
    print(f"      Fine Mesh:   {nodes_f} nodes, {tets_f} tetrahedra")

    gmsh.finalize()
    print("\n[SUCCESS] Geometry generation, watertight STL, and 3-level meshes completed.")

if __name__ == "__main__":
    main()
