# SPDX-License-Identifier: GPL-3.0-only
# minColorAE. Copyright (C) 2026 cbkow.
#
# The minColor proof scene: builds a .blend from nothing, so the footage the release
# is proven on can be regenerated exactly.
#
#   Blender -b --factory-startup -P proof/blender/make_scene.py -- <out.blend> [--preview <png> <frame>]
#
# A Geometry Nodes cloner: a field of bevelled bars with slanted tops on a grid, their
# heights staggered by two travelling waves, under a low camera with shallow depth of
# field and motion blur. Colour is the point of it:
#   - every bar's albedo is a hue around the full saturation edge of ACEScg (the AP1
#     primaries sit near the spectral locus), a few rows stepping down in saturation;
#   - some bars glow at HDR intensities (up to ~30x diffuse white) as the wave passes;
#   - a row of thin "laser" rods emits true spectral colours, 450 to 650 nm, from the
#     CIE 1931 colour-matching functions: most lie outside ACEScg, so the EXR carries
#     negative values, the hardest case for every gamut rail downstream.
# The blend file's working space is ACEScg, so the EXRs are linear ACEScg (minColor's
# default working space). Renders: Cycles on the GPU, 1920x1080, 24 fps, frames 1-120.

import math
import sys

import bpy
import bmesh

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
OUT_BLEND = argv[0] if argv else "minColor_proof.blend"
PREVIEW = argv[argv.index("--preview") + 1:] if "--preview" in argv else None   # png frame [samples]

# ---------------------------------------------------------------- scene, colour
for ob in list(bpy.data.objects):
    bpy.data.objects.remove(ob, do_unlink=True)
bpy.ops.wm.set_working_color_space(working_space="ACEScg", convert_colors=False)   # the property is read-only; the menu's operator sets it
assert bpy.data.colorspace.working_space == "ACEScg"
sc = bpy.context.scene
sc.name = "minColor proof"
sc.frame_start, sc.frame_end = 1, 120
sc.render.fps = 24
sc.view_settings.view_transform = "AgX"   # what the Blender viewport and the preview show; EXRs stay scene-linear
sc.view_settings.look = "None"

# ---------------------------------------------------------------- the bar
def make_bar():
    """A 0.11 x 0.11 x 6 bar, origin 4.5 below its top (so bottoms never show), top face slanted down along -Y."""
    me = bpy.data.meshes.new("bar")
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co.x *= 0.11
        v.co.y *= 0.11
        v.co.z = (v.co.z + 0.5) * 6.0 - 4.5
        if v.co.z > 1.0:                        # the top: slant it
            v.co.z -= 0.09 if v.co.y < 0 else 0.0
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new("bar (cloner source)", me)
    sc.collection.objects.link(ob)
    bev = ob.modifiers.new("bevel", "BEVEL")
    bev.width, bev.segments, bev.limit_method = 0.006, 2, "ANGLE"
    for p in me.polygons:
        p.use_smooth = True
    ob.hide_render = True
    ob.hide_viewport = True
    ob.location = (0, 0, -50)
    return ob

bar = make_bar()

# ---------------------------------------------------------------- geometry nodes
def gn_cloner(source):
    ng = bpy.data.node_groups.new("minColor cloner", "GeometryNodeTree")
    ng.interface.new_socket("Geometry", in_out="OUTPUT", socket_type="NodeSocketGeometry")
    N, L = ng.nodes, ng.links
    def node(t, x, y, **kw):
        n = N.new(t); n.location = (x, y)
        for k, v in kw.items(): setattr(n, k, v)
        return n
    def math(op, a, b=None, x=0, y=0):
        n = node("ShaderNodeMath", x, y, operation=op)
        for i, v in enumerate((a, b)):
            if v is None: continue
            if isinstance(v, (int, float)): n.inputs[i].default_value = v
            else: L.new(v, n.inputs[i])
        return n.outputs[0]

    grid = node("GeometryNodeMeshGrid", -1400, 0)
    grid.inputs["Size X"].default_value, grid.inputs["Size Y"].default_value = 11.0, 9.0   # wide enough that no edge shows
    grid.inputs["Vertices X"].default_value, grid.inputs["Vertices Y"].default_value = 80, 64
    pos = node("GeometryNodeInputPosition", -1400, -300)
    sep = node("ShaderNodeSeparateXYZ", -1200, -300); L.new(pos.outputs[0], sep.inputs[0])
    t = node("GeometryNodeInputSceneTime", -1400, -500).outputs["Seconds"]
    X, Y = sep.outputs["X"], sep.outputs["Y"]

    # height: two travelling waves, quantised a little for the staircase look
    w1 = math("SINE", math("ADD", math("MULTIPLY", X, 1.3, -1000, -200), math("MULTIPLY", t, 1.7, -1000, -400), -850, -300), None, -700, -300)
    w2 = math("SINE", math("ADD", math("MULTIPLY", Y, 0.9, -1000, -600), math("MULTIPLY", t, -1.1, -1000, -800), -850, -700), None, -700, -700)
    h = math("ADD", math("MULTIPLY", w1, 0.65, -550, -300), math("MULTIPLY", w2, 0.4, -550, -700), -400, -500)
    hq = math("MULTIPLY", math("FLOOR", math("MULTIPLY", h, 7.0, -250, -500), None, -100, -500), 1.0 / 7.0, 50, -500)   # staircase steps
    off = node("ShaderNodeCombineXYZ", 200, -500); L.new(hq, off.inputs["Z"])
    setp = node("GeometryNodeSetPosition", 400, 0)
    L.new(grid.outputs["Mesh"], setp.inputs["Geometry"]); L.new(off.outputs[0], setp.inputs["Offset"])

    # the clones
    info = node("GeometryNodeObjectInfo", 400, 300); info.inputs["Object"].default_value = source
    inst = node("GeometryNodeInstanceOnPoints", 650, 0)
    L.new(setp.outputs[0], inst.inputs["Points"]); L.new(info.outputs["Geometry"], inst.inputs["Instance"])

    # colour: hue across the field and slowly in time; saturation full except a few bands
    hue = math("FRACT", math("ADD", math("ADD", math("MULTIPLY", X, 0.16, 200, -900), math("MULTIPLY", Y, 0.07, 200, -1050), 380, -950),
                                 math("MULTIPLY", t, 0.03, 200, -1200), 550, -1000), None, 700, -1000)
    band = math("FRACT", math("MULTIPLY", Y, 0.21, 200, -1350), None, 380, -1350)            # 0..1 across bands of rows
    sat = math("MAXIMUM", math("GREATER_THAN", band, 0.08, 550, -1350), 0.35, 700, -1350)  # most rows 1.0, a narrow band at 0.35
    hsv = node("FunctionNodeCombineColor", 900, -1100, mode="HSV")
    L.new(hue, hsv.inputs[0]); L.new(sat, hsv.inputs[1]); hsv.inputs[2].default_value = 0.85
    store_c = node("GeometryNodeStoreNamedAttribute", 900, 0, data_type="FLOAT_COLOR", domain="INSTANCE")
    store_c.inputs["Name"].default_value = "mc_albedo"
    L.new(inst.outputs[0], store_c.inputs["Geometry"]); L.new(hsv.outputs[0], store_c.inputs["Value"])

    # glow: a random 7 % of the bars, brightest where the first wave crests (HDR, up to ~30x)
    rnd = node("FunctionNodeRandomValue", 700, -1600, data_type="FLOAT"); rnd.inputs["Seed"].default_value = 7
    pick = math("GREATER_THAN", rnd.outputs["Value"], 0.93, 900, -1600)
    crest = math("POWER", math("MAXIMUM", w1, 0.0, 900, -1800), 3.0, 1050, -1800)
    glow = math("MULTIPLY", math("MULTIPLY", pick, crest, 1200, -1700), 30.0, 1350, -1700)
    store_g = node("GeometryNodeStoreNamedAttribute", 1100, 0, data_type="FLOAT", domain="INSTANCE")
    store_g.inputs["Name"].default_value = "mc_glow"
    L.new(store_c.outputs[0], store_g.inputs["Geometry"]); L.new(glow, store_g.inputs["Value"])

    out = node("NodeGroupOutput", 1350, 0); L.new(store_g.outputs[0], out.inputs[0])
    return ng

field_me = bpy.data.meshes.new("field")
field = bpy.data.objects.new("bar field (Geometry Nodes)", field_me)
sc.collection.objects.link(field)
gn = field.modifiers.new("cloner", "NODES"); gn.node_group = gn_cloner(bar)

# ---------------------------------------------------------------- materials
def bar_material():
    m = bpy.data.materials.new("bar: albedo + glow")
    m.use_nodes = True
    N, L = m.node_tree.nodes, m.node_tree.links
    bsdf = N["Principled BSDF"]
    bsdf.inputs["Roughness"].default_value = 0.38
    a = N.new("ShaderNodeAttribute"); a.attribute_type = "INSTANCER"; a.attribute_name = "mc_albedo"; a.location = (-500, 200)
    g = N.new("ShaderNodeAttribute"); g.attribute_type = "INSTANCER"; g.attribute_name = "mc_glow"; g.location = (-500, -200)
    L.new(a.outputs["Color"], bsdf.inputs["Base Color"])
    L.new(a.outputs["Color"], bsdf.inputs["Emission Color"])
    L.new(g.outputs["Fac"], bsdf.inputs["Emission Strength"])
    return m

bar.data.materials.append(bar_material())

# CIE 1931 2-degree colour-matching functions, the multi-lobe Gaussian fit of
# Wyman, Sloan and Shirley (JCGT 2013).
def cie_xyz(nm):
    def g(x, mu, s1, s2): return math.exp(-0.5 * ((x - mu) / (s1 if x < mu else s2)) ** 2)
    x = 1.056 * g(nm, 599.8, 37.9, 31.0) + 0.362 * g(nm, 442.0, 16.0, 26.7) - 0.065 * g(nm, 501.1, 20.4, 26.2)
    y = 0.821 * g(nm, 568.8, 46.9, 40.5) + 0.286 * g(nm, 530.9, 16.3, 31.1)
    z = 1.217 * g(nm, 437.0, 11.8, 36.0) + 0.681 * g(nm, 459.0, 26.0, 13.8)
    return x, y, z

XYZ_TO_AP1 = ((1.6410233797, -0.3248032942, -0.2364246952),   # ACEScg, its own D60 white (no adaptation:
              (-0.6636628587, 1.6153315917, 0.0167563477),     # a laser's light is not a reflectance)
              (0.0117218943, -0.0082844420, 0.9883948585))

def laser_material(nm, strength):
    X, Y, Z = cie_xyz(nm)
    rgb = [sum(XYZ_TO_AP1[r][c] * v for c, v in enumerate((X, Y, Z))) for r in range(3)]
    peak = max(abs(v) for v in rgb)
    rgb = [v / peak for v in rgb]   # largest channel 1 (by luminance, 450 nm would need 53x the blue of 570 nm)
    m = bpy.data.materials.new(f"laser {nm} nm")
    m.use_nodes = True
    N, L = m.node_tree.nodes, m.node_tree.links
    for n in list(N): N.remove(n)
    out = N.new("ShaderNodeOutputMaterial")
    em = N.new("ShaderNodeEmission"); em.inputs["Strength"].default_value = strength
    cc = N.new("ShaderNodeCombineColor")   # Combine Color takes negative channels; a colour swatch would clamp them
    for i, v in enumerate(rgb): cc.inputs[i].default_value = v
    L.new(cc.outputs[0], em.inputs["Color"]); L.new(em.outputs[0], out.inputs["Surface"])
    m["ACEScg"] = rgb
    return m, rgb

lasers = []
for i, nm in enumerate((450, 470, 490, 505, 520, 540, 570, 590, 610, 630, 650)):
    # upright glowing rods standing in the back rows of the field
    bpy.ops.mesh.primitive_cylinder_add(vertices=24, radius=0.022, depth=7.0, location=(-3.3 + i * 0.66, 3.4 + 0.25 * (i % 2), 0.5))
    rod = bpy.context.active_object
    rod.name = f"laser {nm} nm"
    m, rgb = laser_material(nm, 8.0)
    rod.data.materials.append(m)
    lasers.append((nm, rgb))

# ---------------------------------------------------------------- light, world, camera
sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
sun.data.energy, sun.data.angle = 2.2, math.radians(1.5)
sun.data.color = (1.0, 0.92, 0.82)
sun.rotation_euler = (math.radians(52), 0, math.radians(-38))
sc.collection.objects.link(sun)
fill = bpy.data.objects.new("fill", bpy.data.lights.new("fill", "AREA"))
fill.data.energy, fill.data.size = 250.0, 6.0
fill.data.color = (0.75, 0.85, 1.0)
fill.location, fill.rotation_euler = (5.5, -3.0, 4.0), (math.radians(60), 0, math.radians(55))
sc.collection.objects.link(fill)
world = bpy.data.worlds.new("dusk"); sc.world = world
world.use_nodes = True
world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.012, 0.014, 0.022, 1.0)

cam = bpy.data.objects.new("camera", bpy.data.cameras.new("camera"))
sc.collection.objects.link(cam); sc.camera = cam
cam.location = (4.4, -8.2, 4.4)
aim = bpy.data.objects.new("camera aim", None); sc.collection.objects.link(aim)
aim.location = (-0.4, 1.4, 0.6)
track = cam.constraints.new("TRACK_TO"); track.target = aim
track.track_axis, track.up_axis = "TRACK_NEGATIVE_Z", "UP_Y"
cam.data.lens = 45
cam.data.dof.use_dof = True
cam.data.dof.focus_object = aim
cam.data.dof.aperture_fstop = 2.0

# ---------------------------------------------------------------- render
r = sc.render
r.engine = "CYCLES"
r.resolution_x, r.resolution_y, r.resolution_percentage = 1920, 1080, 100
r.use_motion_blur = True
r.motion_blur_shutter = 0.5
cy = sc.cycles
cy.device = "GPU"
cy.samples = 160
cy.use_denoising = True
cy.max_bounces = 8
prefs = bpy.context.preferences.addons["cycles"].preferences
prefs.compute_device_type = "METAL" if sys.platform == "darwin" else "OPTIX"
prefs.get_devices()
for d in prefs.devices:
    d.use = d.type != "CPU"
im = r.image_settings
im.file_format = "OPEN_EXR"
im.color_depth = "32"
im.exr_codec = "ZIP"
im.color_mode = "RGBA"
r.filepath = "//exr/minColor_proof_####"

for nm, rgb in lasers:
    print(f"laser {nm} nm ACEScg {rgb[0]:+.3f} {rgb[1]:+.3f} {rgb[2]:+.3f}")
bpy.ops.wm.save_as_mainfile(filepath=OUT_BLEND)
print("saved", OUT_BLEND)

if PREVIEW:   # one frame at half size through Blender's AgX view, for a look
    png, frame = PREVIEW[0], int(PREVIEW[1])
    sc.frame_set(frame)
    r.resolution_percentage = 50
    cy.samples = int(PREVIEW[2]) if len(PREVIEW) > 2 else 64
    im.file_format, im.color_depth = "PNG", "8"
    r.filepath = png
    bpy.ops.render.render(write_still=True)
    print("preview", png)
