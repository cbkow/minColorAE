// SPDX-License-Identifier: GPL-3.0-only
// minColorAE. Copyright (C) 2026 cbkow.
//
// minColor panel (ScriptUI). Workflow only: it adds the minColor effects and keeps
// the project's sidecar folder; nothing a frame renders depends on it, and every
// effect works the same without it.
//
// Passive: no timers, no idle work. It reads the project when it gains focus
// (onActivate) and after its own buttons, and it changes the project only from a
// button, one undo group per click.
//
// Sidecar: <project folder>/minColor/ holds the viewport shim (and, later, the
// In/Out presets). In an OCIO project already pinned to a minColor viewport
// shim, a button press re-points the pin at the sidecar copy when it points
// anywhere else (a moved project, another machine). AE wants an absolute path
// there, so that is the one path the panel keeps current; everything else in
// the folder is found relative to the .aep. An Adobe-engine project is never
// switched to OCIO: there the macOS Fix effect, added by hand on a guide layer,
// does the shim's job.
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
  function baseName(path) { return String(path).replace(/\\/g, "/").replace(/^.*\//, ""); }

  // ---- project facts (read only) -------------------------------------------------
  function sidecarFolder() {
    var f = app.project.file;
    return f ? new Folder(f.parent.fsName + "/minColor") : null;
  }
  function colourFacts() {
    var p = app.project, cms = -1, cfg = "";
    try { cms = p.colorManagementSystem; } catch (e) {}
    try { cfg = p.ocioConfigurationFile || ""; } catch (e) {}
    return { ocio: cms === 1, cms: cms, config: cfg, shimPinned: cms === 1 && baseName(cfg) === SHIM_NAME };
  }

  // ---- the sidecar's one job in step 1: keep the shim pin pointing at it ----------
  /* Returns lines for the report. Only acts on a project that is OCIO and pinned to a
     file named like the shim; writes the shim into minColor/ when it is missing there. */
  function maintainShim() {
    var c = colourFacts(), out = [];
    if (!c.shimPinned) return out;
    var dir = sidecarFolder();
    if (!dir) { out.push("Viewport shim pinned; save the project to give it a minColor folder."); return out; }
    if (!dir.exists && !dir.create()) { out.push("Could not create " + dir.fsName); return out; }
    var shim = new File(dir.fsName + "/" + SHIM_NAME);
    if (!shim.exists) {
      if (!writeText(shim, SHIM_TEXT)) { out.push("Could not write " + shim.fsName); return out; }
      out.push("Wrote minColor/" + SHIM_NAME + ".");
    } else if (readText(shim) !== SHIM_TEXT) {
      /* AE caches a parsed config per path for the session, and the user may have
         edited theirs: report, never overwrite */
      out.push("Note: minColor/" + SHIM_NAME + " differs from this panel's (" + VERSION + "); left as it is.");
    }
    if (new File(c.config).fsName !== shim.fsName) {
      try {
        app.project.ocioConfigurationFile = shim.fsName;
        out.push("Re-pinned the viewport shim to minColor/" + SHIM_NAME + " (was " + c.config + ").");
      } catch (e) {
        out.push("Could not re-pin the viewport shim: " + e.toString());
      }
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
      return out.concat(maintainShim());
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
      out = out.concat(maintainShim());
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

  var facts = win.add("statictext", undefined, "", { multiline: true });
  facts.preferredSize = [240, 64];
  var bAdd = flatButton(win, "Add Output", { primary: true,
    tip: "Adjustment layer with minColor Output at the top of the active comp" });
  var report = win.add("statictext", undefined, "", { multiline: true });
  report.preferredSize = [240, 64];
  var ver = win.add("statictext", undefined, "minColor " + VERSION);
  ver.graphics.foregroundColor = ver.graphics.newPen(ver.graphics.PenType.SOLID_COLOR, [0.55, 0.55, 0.55, 1], 1);

  function refreshFacts() {
    var lines = [], p = app.project;
    if (!p) { facts.text = "No project."; return; }
    lines.push(p.file ? "Project: " + p.file.name.replace(/%20/g, " ") : "Project: not saved yet");
    var c = colourFacts();
    if (c.cms === 0) lines.push("Colour engine: Adobe (use macOS Fix on a guide layer)");
    else if (c.ocio) lines.push("Colour engine: OCIO, " + (c.shimPinned ? "viewport shim" : baseName(c.config) || "built-in config"));
    else lines.push("Colour engine: not readable");
    if (c.ocio) {
      /* "None" = AE converts nothing at all (import, viewer, output); it happens when a
         config is pinned that lacks the old working space's name */
      var ws = ""; try { ws = p.workingSpace; } catch (e) {}
      lines.push("Working space: " + (ws || "unknown") +
                 (ws === "None" ? " (set it in Project Settings > Color)" : ""));
    }
    var dir = sidecarFolder();
    lines.push("Folder: " + (!dir ? "none until the project is saved" : (dir.exists ? "minColor/" : "minColor/ not created yet")));
    facts.text = lines.join("\n");
  }
  function run(fn) {
    var lines;
    try { lines = fn(); } catch (e) { lines = ["Error: " + e.toString() + " (line " + e.line + ")"]; log("ERR " + e.toString() + " line " + e.line); }
    report.text = lines.join("\n");
    log(lines.join(" | "));
    refreshFacts();
    try { win.update(); } catch (e) {}
  }
  bAdd.onClick = function () { this.active = false; run(addOutput); };   /* active=false: ScriptUI keeps a pressed look otherwise */
  win.onActivate = function () { try { refreshFacts(); } catch (e) {} };

  refreshFacts();
  win.layout.layout(true);
  win.onResizing = win.onResize = function () { try { this.layout.resize(); } catch (e) {} };
  if (win instanceof Window) { win.center(); win.show(); }
 } catch (eTop) { log("LOAD " + eTop.toString() + " line " + eTop.line); }
})(this);
