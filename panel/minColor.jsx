// SPDX-License-Identifier: GPL-3.0-only
// minColorAE. Copyright (C) 2026 cbkow.
//
// minColor panel (ScriptUI). Workflow only: it adds the minColor effects and keeps
// the project's sidecar folder; nothing a frame renders depends on it, and every
// effect works the same without it.
//
// 100% manual: it never reads or reports the project's colour settings. AE's
// scripting answers for them (config name, working space) can be stale or wrong
// (a silent fallback keeps the old label), and minColor does no colour
// detection anyway. No timers, no idle work; it changes the project only from a
// button and reports only what that click did.
//
// Sidecar: <project folder>/minColor/ holds the viewport shim (and, later, the
// In/Out presets). Pin Viewport Shim writes the shim there and makes it the
// project's OCIO config; AE stores that path absolutely, so after moving the
// project, press it again. For Adobe-engine projects the macOS Fix effect, added
// by hand on a guide layer, does the shim's job instead.
//
// ExtendScript groups an unparenthesised nested ?: from the LEFT; always parenthesise.
//
// The build fills the two @...@ tokens (panel/CMakeLists.txt): the version and the
// shim's text, so the panel and the shim it writes can never drift apart.

(function (thisObj) {
 var LOG = "/tmp/mincolor_panel.log";   /* a throw while a panel loads is a modal dialog; log instead */
 function log(s) {
   try { var f = new File(LOG); if (!f.exists) return; f.lineFeed = "Unix"; f.open("e"); f.seek(0, 2); f.writeln(new Date().toTimeString().substr(0, 8) + " " + s); f.close(); } catch (e) {}
 }
 try {
  var VERSION = "@MINCOLOR_VERSION@";
  var SHIM_NAME = "mincolor-viewport-shim.ocio";
  var SHIM_TEXT = @MINCOLOR_SHIM_JS@;
  var MN_OUTPUT = "ski.bialkow minColor Output";
  var MN_MACFIX = "ski.bialkow minColor macOS Fix";

  var win;
  if (thisObj instanceof Panel) win = thisObj;
  else {
    try { if ($.global.__minColorAEWin) $.global.__minColorAEWin.close(); } catch (e) {}
    win = new Window("palette", "minColor", undefined, { resizeable: true });
    $.global.__minColorAEWin = win;   /* a DoScript-launched palette needs a reference to outlive the script */
  }
  win.orientation = "column"; win.alignChildren = ["fill", "top"]; win.spacing = 6; win.margins = 10;

  // ---- file helpers ------------------------------------------------------------
  function readText(file) {
    if (!file.exists) return null;
    file.encoding = "UTF-8";
    if (!file.open("r")) return null;
    var s = file.read(); file.close(); return s;
  }
  function writeText(file, s) {
    file.encoding = "UTF-8"; file.lineFeed = "Unix";
    if (!file.open("w")) return false;
    var ok = file.write(s); file.close(); return ok;
  }

  /* Pin Viewport Shim: write minColor/mincolor-viewport-shim.ocio next to the saved
     .aep (only when missing: AE caches a parsed config per path for the session,
     and the copy may have been edited), make it the project's OCIO config, and
     turn the OCIO engine on. On an Adobe-engine project that switch changes how
     footage is interpreted; the button's tip says so. */
  function pinShim() {
    var out = [], f = app.project.file;
    if (!f) { out.push("Save the project first: the shim goes in a minColor folder next to it. Nothing changed."); return out; }
    var dir = new Folder(f.parent.fsName + "/minColor");
    if (!dir.exists && !dir.create()) { out.push("Could not create " + dir.fsName + ". Nothing changed."); return out; }
    var shim = new File(dir.fsName + "/" + SHIM_NAME);
    if (!shim.exists) {
      if (!writeText(shim, SHIM_TEXT)) { out.push("Could not write " + shim.fsName + ". Nothing changed."); return out; }
      out.push("Wrote minColor/" + SHIM_NAME + ".");
    } else if (readText(shim) !== SHIM_TEXT) {
      out.push("minColor/" + SHIM_NAME + " differs from this panel's (" + VERSION + "); kept it. Delete it and press again for a fresh copy.");
    } else {
      out.push("minColor/" + SHIM_NAME + " is already there.");
    }
    try {
      app.project.ocioConfigurationFile = shim.fsName;
      if (app.project.colorManagementSystem !== 1) app.project.colorManagementSystem = 1;
      out.push("Set it as the project's OCIO config.");
      out.push("Check Project Settings > Color: Working Color Space = minColor Output, and the viewer display for this machine.");
    } catch (e) {
      out.push("After Effects refused the config: " + e.toString());
    }
    return out;
  }

  // ---- layers ------------------------------------------------------------------------
  function hasEffect(layer, matchName) {
    var parade;
    try { parade = layer.property("ADBE Effect Parade"); } catch (e) { return false; }   /* cameras, lights */
    if (!parade) return false;
    for (var i = 1; i <= parade.numProperties; i++) if (parade.property(i).matchName === matchName) return true;
    return false;
  }
  function findLayers(comp, matchName) {
    var hits = [];
    for (var i = 1; i <= comp.numLayers; i++) if (hasEffect(comp.layer(i), matchName)) hits.push(comp.layer(i));
    return hits;
  }

  /* Add Output: an adjustment layer at the top of the active comp carrying minColor
     Output at its defaults; below a macOS Fix layer if the comp has one (the Fix
     re-encodes the Output's pixels, so it must stay above). One Output per comp: an
     existing one is reported, not duplicated. */
  function addOutput() {
    var out = [];
    var comp = app.project.activeItem;
    if (!(comp instanceof CompItem)) { out.push("Open or select a comp first; nothing added."); return out; }
    var existing = findLayers(comp, MN_OUTPUT);
    if (existing.length) {
      out.push("\"" + comp.name + "\" already has minColor Output on layer " + existing[0].index +
               " (\"" + existing[0].name + "\"); nothing added.");
      return out;
    }
    var fixes = findLayers(comp, MN_MACFIX);
    app.beginUndoGroup("minColor: Add Output");
    try {
      var l = comp.layers.addSolid([1, 1, 1], "minColor Output", comp.width, comp.height, comp.pixelAspect, comp.duration);
      l.adjustmentLayer = true;
      l.property("ADBE Effect Parade").addProperty(MN_OUTPUT);
      if (fixes.length) l.moveAfter(fixes[fixes.length - 1]);   /* below the lowest Fix layer */
      out.push("Added minColor Output to \"" + comp.name + "\" on layer " + l.index +
               (fixes.length ? ", under the macOS Fix layer." : ", at the top."));
    } catch (e) {
      out.push("Add Output failed: " + e.toString());
    }
    app.endUndoGroup();
    return out;
  }

  // ---- UI ----------------------------------------------------------------------------
  /* Pill buttons in the AE-native (Spectrum 2) theme: an accent for the main action,
     a quiet outline for the rest. Fills only: AE remaps non-neutral pens. */
  function flatButton(parent, label, opts) {
    opts = opts || {};
    var b = parent.add("iconbutton", undefined, undefined, { style: "toolbutton" });
    b.textLabel = label; b.hov = false; b.dn = false;
    b.preferredSize.height = 24; b.alignment = ["fill", "center"];
    if (opts.tip) b.helpTip = opts.tip;
    b.onDraw = function () {
      var g = this.graphics, s = this.size;
      function pill(x, y, w, h, col) {   /* caps + rect as ONE path, ONE fill (separate fills stack alpha) */
        g.newPath();
        g.ellipsePath(x, y, h, h);
        g.ellipsePath(x + w - h, y, h, h);
        g.rectPath(x + h / 2, y, w - h, h);
        g.fillPath(g.newBrush(g.BrushType.SOLID_COLOR, col));
      }
      var accent = [0.00784, 0.39608, 0.86275, 1];
      if (opts.primary) {
        var k = this.dn ? 0.68 : (this.hov ? 0.82 : 1);   /* Spectrum accent darkens on hover */
        var fill = [accent[0] * k, accent[1] * k, accent[2] * k, 1];
        pill(0, 0, s[0], s[1], fill);
      } else {
        var rimA = this.dn ? 0.34 : (this.hov ? 0.32 : 0.26);
        var center = this.dn ? [0.24, 0.24, 0.24, 1] : (this.hov ? [0.30, 0.30, 0.30, 1] : [0.13, 0.13, 0.13, 1]);
        pill(0, 0, s[0], s[1], [1, 1, 1, rimA]);
        pill(2, 2, s[0] - 4, s[1] - 4, center);
      }
      var f = ScriptUI.newFont("dialog", opts.primary ? "BOLD" : "REGULAR", 11);
      var ts = g.measureString(this.textLabel, f);
      var textCol = opts.primary ? [0.97, 0.97, 0.97, 1] : [0.86, 0.86, 0.86, 1];
      g.drawString(this.textLabel, g.newPen(g.PenType.SOLID_COLOR, textCol, 1),
                   Math.max(2, (s[0] - ts.width) / 2), Math.max(0, (s[1] - ts.height) / 2 - 1), f);
    };
    b.addEventListener("mouseover", function () { this.hov = true;  try { this.window.update(); } catch (e) {} });
    b.addEventListener("mouseout",  function () { this.hov = false; this.dn = false; try { this.window.update(); } catch (e) {} });
    b.addEventListener("mousedown", function () { this.dn = true;  try { this.window.update(); } catch (e) {} });
    b.addEventListener("mouseup",   function () { this.dn = false; try { this.window.update(); } catch (e) {} });
    return b;
  }

  var bAdd = flatButton(win, "Add Output", { primary: true,
    tip: "Adjustment layer with minColor Output at the top of the active comp (under a macOS Fix layer if there is one)" });
  var bShim = flatButton(win, "Pin Viewport Shim", {
    tip: "Writes the macOS viewport shim into minColor/ next to the project and makes it the project's OCIO config.\n" +
         "Turns OCIO on: on an Adobe-engine project that changes how footage is interpreted.\n" +
         "Press again after moving the project." });
  var report = win.add("statictext", undefined, "", { multiline: true });
  report.preferredSize = [240, 64];
  var ver = win.add("statictext", undefined, "minColor " + VERSION);
  ver.graphics.foregroundColor = ver.graphics.newPen(ver.graphics.PenType.SOLID_COLOR, [0.55, 0.55, 0.55, 1], 1);

  function run(fn) {
    var lines;
    try { lines = fn(); } catch (e) { lines = ["Error: " + e.toString() + " (line " + e.line + ")"]; log("ERR " + e.toString() + " line " + e.line); }
    report.text = lines.join("\n");
    log(lines.join(" | "));
    try { win.update(); } catch (e) {}
  }
  bAdd.onClick = function () { this.active = false; run(addOutput); };   /* active=false: ScriptUI keeps a pressed look otherwise */
  bShim.onClick = function () { this.active = false; run(pinShim); };

  win.layout.layout(true);
  win.onResizing = win.onResize = function () { try { this.layout.resize(); } catch (e) {} };
  if (win instanceof Window) { win.center(); win.show(); }
 } catch (eTop) { log("LOAD " + eTop.toString() + " line " + eTop.line); }
})(this);
