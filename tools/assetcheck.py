"""The scene and asset half of the offline game check: verifies the map's Scene, Layer, AnimatorParam
and ShaderProperty entries (sdk/map/Scene.h, sdk/map/UnityRendering.h) in copies of the game's scene
and asset files. It reads with UnityPy (pinned in tools/assetcheck-requirements.txt) and never runs
game code. tools/gamecheck.ps1 copies the files and runs it isolated (python -I); sdk/systems/scene.md
says which files hold what.

usage:
  assetcheck.py --data <copied Phasmophobia_Data> --entries <tsv>            check the entries
  assetcheck.py --data <copied Phasmophobia_Data> --entries <tsv> --needs    the files the check reads
  assetcheck.py --data <copied Phasmophobia_Data> --entries <tsv> --record <json>
  assetcheck.py --snapshot <json> --entries <tsv>                            check a recorded snapshot

The entries are what `gamecheck --scene-entries` writes: key, kind (scene, layer, animator or shader),
1 when required or 0, scene, path, component and name, tab separated. The check prints
`OK <key> <detail>` or `MISSING <key> <why>` for each and exits 1 when a required one is missing, 2 on
input it cannot read (an unreadable entry line, file or snapshot, or anything else that stops the
reading).
--needs prints the data files the check reads that are already known, one name per line: a file's
own references are known only once it is there, so the caller copies and asks again until nothing is
added. --record writes what the check reads from the files (the snapshot) instead of checking it; the
tests check a recorded snapshot, which needs no UnityPy.

A snapshot is JSON: {"layers": [32 names, "" for none], "places": {scene: {"objects": [{"name",
"parent" (an index, -1 for none), "components": [type names]}], "controllers": {name: [parameter
names]}}}, "shaders": {scene: {shader name: [[property names] for each shader of that name]}}}.
Objects are listed parent first and children in their Transform's order. A place is a scene by the
name the build lists it under, or "Resources/<path>" for a prefab Resources.Load loads; one missing
from the snapshot is not in the build. A scene's shaders are those of its shared assets file
(sharedassets<n>.assets for the build's scene n) and of the shared assets files that one refers to.
"""

import argparse
import json
import os
import re
import struct
import sys

KINDS = ("scene", "layer", "animator", "shader")
SHARED_ASSETS = re.compile(r"sharedassets[0-9]+\.assets")
RESOURCES = "Resources/"


class BadInput(Exception):
    """Input the check cannot read: exit 2 with the reason."""


def read_entries(path):
    entries = []
    try:
        with open(path, encoding="utf-8", newline="") as f:
            lines = f.read().split("\n")
    except OSError as e:
        raise BadInput(f"cannot read {path}: {e.strerror}")
    for number, line in enumerate(lines, 1):
        if not line:
            continue
        cols = line.split("\t")
        if len(cols) != 7 or cols[1] not in KINDS or cols[2] not in ("0", "1"):
            raise BadInput(f"{path}:{number}: not an entry line")
        key, kind, required, scene, path_, component, name = cols
        entries.append({"key": key, "kind": kind, "required": required == "1", "scene": scene,
                        "path": path_, "component": component, "name": name})
    return entries


# The check, on a snapshot.

def find_all(objects, children, root, path):
    """Every object Transform.Find(path) could reach from `root`: it takes the first of these."""
    found = [root]
    for segment in path.split("/"):
        found = [c for o in found for c in children.get(o, []) if objects[c]["name"] == segment]
    return found


def path_of(objects, index):
    """The object's path from its root; a parent chain that loops stops where it comes back."""
    names, seen = [], set()
    while index >= 0 and index not in seen:
        seen.add(index)
        names.append(objects[index]["name"])
        index = objects[index]["parent"]
    return "/".join(reversed(names))


def check_scene(entry, place):
    objects = place["objects"]
    children = {}
    for i, o in enumerate(objects):
        children.setdefault(o["parent"], []).append(i)
    roots = [i for i, o in enumerate(objects) if entry["component"] in o["components"]]
    if not roots:
        return False, f"no object in {entry['scene']} carries {entry['component']}"
    for root in roots:
        found = find_all(objects, children, root, entry["path"])
        if len(found) != 1:
            why = "nothing" if not found else f"{len(found)} objects, and Transform.Find takes the first,"
            return False, f"{why} at {entry['path']} under {path_of(objects, root)} in {entry['scene']}"
    plural = "s" if len(roots) > 1 else ""
    return True, f"{entry['scene']}: under {len(roots)} {entry['component']} object{plural}"


def check_layer(entry, layers):
    indexes = [i for i, name in enumerate(layers) if name == entry["name"]]
    if len(indexes) == 1:
        return True, str(indexes[0])
    if not indexes:
        return False, f"no layer is named {entry['name']}"
    return False, f"layers {', '.join(map(str, indexes))} are all named {entry['name']}"


def check_animator(entry, place):
    controllers = place["controllers"].get(entry["component"])
    if not controllers:
        return False, f"no Animator in {entry['scene']} uses a controller named {entry['component']}"
    for parameters in controllers:
        if entry["name"] not in parameters:
            return False, f"a controller named {entry['component']} in {entry['scene']} has no {entry['name']}"
    plural = "s" if len(controllers) > 1 else ""
    return True, f"{entry['scene']}: in {len(controllers)} controller{plural} named {entry['component']}"


def check_shader(entry, shaders):
    if shaders is None:
        return False, f"{entry['scene']} is not a scene of this build"
    found = shaders.get(entry["component"])
    if not found:
        return False, f"no shader named {entry['component']} is in {entry['scene']}'s assets"
    for properties in found:
        if entry["name"] not in properties:
            return False, f"a shader named {entry['component']} in {entry['scene']}'s assets has no {entry['name']}"
    plural = "s" if len(found) > 1 else ""
    return True, f"{entry['scene']}: in {len(found)} shader{plural} named {entry['component']}"


def check(entries, snapshot):
    """The report's lines, and whether a required entry is missing."""
    lines, failed = [], False
    for entry in entries:
        if entry["kind"] == "layer":
            ok, detail = check_layer(entry, snapshot["layers"])
        elif entry["kind"] == "shader":
            ok, detail = check_shader(entry, snapshot.get("shaders", {}).get(entry["scene"]))
        else:
            place = snapshot["places"].get(entry["scene"])
            if place is None:
                ok, detail = False, f"{entry['scene']} is not a scene or Resources prefab of this build"
            elif entry["kind"] == "scene":
                ok, detail = check_scene(entry, place)
            else:
                ok, detail = check_animator(entry, place)
        lines.append(f"{'OK' if ok else 'MISSING'} {entry['key']} {detail}")
        failed = failed or (not ok and entry["required"])
    return lines, failed


# Reading the files.

def load(data, name):
    """The serialized file `name` in `data`, read with UnityPy."""
    import UnityPy  # only reading the files needs it

    path = os.path.join(data, name)
    if not os.path.isfile(path):
        raise BadInput(f"{path} is missing; copy it first (--needs lists it)")
    env = UnityPy.load(path)
    for f in env.files.values():
        if getattr(f, "objects", None) is not None:
            return f
    raise BadInput(f"{path} is not a serialized file")


class Build:
    """What globalgamemanagers says of the build: its scenes, layers and Resources prefabs."""

    def __init__(self, data):
        self.data = data
        managers = load(data, "globalgamemanagers")
        self.externals = [os.path.basename(e.path) for e in managers.externals]
        self.scenes, self.layers, self.container = [], [""] * 32, []
        for obj in managers.objects.values():
            kind = obj.type.name
            if kind == "BuildSettings":
                self.scenes = list(obj.read_typetree().get("scenes", []))
            elif kind == "TagManager":
                for i, name in enumerate(obj.read_typetree().get("layers", [])[:32]):
                    self.layers[i] = name or ""
            elif kind == "ResourceManager":
                self.container = list(obj.read_typetree().get("m_Container", []))

    def place_files(self, scene):
        """The files and the root objects' path ids of a place (None: every object of the file)."""
        if scene.startswith(RESOURCES):
            wanted = scene[len(RESOURCES):].lower()
            out = {}
            for path, ref in self.container:
                file_id = ref["m_FileID"]
                if path.lower() == wanted and 0 < file_id <= len(self.externals):
                    out.setdefault(self.externals[file_id - 1], []).append(ref["m_PathID"])
            return out
        return {f"level{i}": None for i, s in enumerate(self.scenes)
                if os.path.splitext(os.path.basename(s))[0] == scene}

    def shared_assets(self, scene):
        """The shared assets files of the scene named `scene` (one per time the build lists it)."""
        return [f"sharedassets{i}.assets" for i, s in enumerate(self.scenes)
                if os.path.splitext(os.path.basename(s))[0] == scene]


def external_name(f, file_id):
    if file_id == 0:
        return f.name
    if 0 < file_id <= len(f.externals):
        return os.path.basename(f.externals[file_id - 1].path)
    return None


class Reader:
    """Reads a place's objects, and the files they refer to, from the data folder."""

    def __init__(self, data):
        self.data, self.files, self.wanted = data, {}, []

    def file(self, name):
        if name not in self.files:
            if name not in self.wanted:
                self.wanted.append(name)
            self.files[name] = load(self.data, name) if os.path.isfile(os.path.join(self.data, name)) else None
        return self.files[name]

    def target(self, f, pptr):
        """The object a PPtr points at, or None when its file is not here (wanted noted)."""
        name = external_name(f, pptr.file_id)
        other = self.file(name) if name and pptr.path_id else None
        return (other, other.objects.get(pptr.path_id)) if other else (None, None)

    def script_name(self, f, obj):
        # A MonoBehaviour's typetree is stripped: m_Script is the PPtr after m_GameObject (12 bytes)
        # and m_Enabled (1 byte, aligned to 4), at byte 16.
        raw = obj.get_raw_data()
        if len(raw) < 28:
            return None
        file_id, path_id = struct.unpack_from("<iq", raw, 16)
        name = external_name(f, file_id)
        other = self.file(name) if name else None
        script = other.objects.get(path_id) if other else None
        if script is None or script.type.name != "MonoScript":
            return None
        s = script.read()
        return f"{s.m_Namespace}.{s.m_ClassName}" if s.m_Namespace else s.m_ClassName

    def controller(self, f, pptr, depth=0):
        """The parameters of the controller a PPtr points at, through override controllers."""
        other, obj = self.target(f, pptr)
        if obj is None or depth > 4:
            return None
        if obj.type.name == "AnimatorOverrideController":
            return self.controller(other, obj.read().m_Controller, depth + 1)
        if obj.type.name != "AnimatorController":
            return None
        tree = obj.read_typetree()
        names = {int(h): n for h, n in tree["m_TOS"]}
        values = tree["m_Controller"]["m_Values"]["data"]["m_ValueArray"]
        return tree["m_Name"], [names.get(v["m_ID"], "") for v in values]

    def shaders(self, name):
        """The shaders of the shared assets file `name` and of the shared assets files it refers to:
        {shader name: [[property names], ...]}. Files not here yet are noted as wanted."""
        out, seen, pending = {}, set(), [name]
        while pending:
            current = pending.pop()
            if current in seen:
                continue
            seen.add(current)
            f = self.file(current)
            if f is None:
                continue
            pending.extend(n for n in (os.path.basename(e.path) for e in f.externals) if SHARED_ASSETS.fullmatch(n))
            for obj in f.objects.values():
                if obj.type.name != "Shader":
                    continue
                form = obj.read_typetree().get("m_ParsedForm", {})
                props = [p["m_Name"] for p in form.get("m_PropInfo", {}).get("m_Props", [])]
                out.setdefault(form.get("m_Name", ""), []).append(props)
        return out

    def place(self, f, roots):
        """The objects under the root GameObjects (every root of the file when None), and the
        controllers their Animators use."""
        objects, controllers = [], {}

        def transform_of(go):
            for c in go.m_Component:
                pptr = getattr(c, "component", c)
                o = f.objects.get(pptr.path_id) if pptr.file_id == 0 else None
                if o is not None and o.type.name in ("Transform", "RectTransform"):
                    return o.read()
            return None

        def describe(go):
            """The GameObject's components, noting the controllers of its Animators."""
            components = []
            for c in go.m_Component:
                pptr = getattr(c, "component", c)
                o = f.objects.get(pptr.path_id) if pptr.file_id == 0 else None
                if o is None:
                    continue
                if o.type.name == "MonoBehaviour":
                    name = self.script_name(f, o)
                    if name:
                        components.append(name)
                    continue
                components.append(o.type.name)
                if o.type.name == "Animator":
                    found = self.controller(f, o.read().m_Controller)
                    if found:
                        controllers.setdefault(found[0], []).append(found[1])
            return components

        # Parent first and children in their Transform's order, walked with a stack rather than
        # recursion, and each GameObject once: a malformed file whose hierarchy loops still ends.
        visited = set()

        def add(root_id):
            stack = [(root_id, -1)]
            while stack:
                path_id, parent = stack.pop()
                go_obj = f.objects.get(path_id)
                if path_id in visited or go_obj is None or go_obj.type.name != "GameObject":
                    continue
                visited.add(path_id)
                go = go_obj.read()
                index = len(objects)
                objects.append({"name": go.m_Name, "parent": parent, "components": describe(go)})
                t = transform_of(go)
                children = []
                for child in (t.m_Children if t else []):
                    child_t = f.objects.get(child.path_id)
                    if child_t is not None:
                        children.append((child_t.read().m_GameObject.path_id, index))
                stack.extend(reversed(children))

        if roots is None:
            roots = []
            for o in f.objects.values():
                if o.type.name in ("Transform", "RectTransform"):
                    t = o.read()
                    if not t.m_Father.path_id:
                        roots.append(t.m_GameObject.path_id)
        for path_id in roots:
            add(path_id)
        return {"objects": objects, "controllers": controllers}


def read_snapshot(data, entries):
    """The snapshot of the places the entries name, and every data file the reading wanted."""
    build = Build(data)
    reader = Reader(data)
    wanted = ["globalgamemanagers"]
    places, shaders = {}, {}
    for scene in sorted({e["scene"] for e in entries if e["kind"] == "shader"}):
        files = build.shared_assets(scene)
        if not files:
            continue
        found = {}
        for name in files:
            for shader, lists in reader.shaders(name).items():
                found.setdefault(shader, []).extend(lists)
        shaders[scene] = found
    for scene in sorted({e["scene"] for e in entries if e["kind"] in ("scene", "animator")}):
        files = build.place_files(scene)
        if not files:
            continue
        place = {"objects": [], "controllers": {}}
        for name, roots in files.items():
            f = reader.file(name)
            if f is None:
                continue
            part = reader.place(f, roots)
            offset = len(place["objects"])
            for o in part["objects"]:
                place["objects"].append({**o, "parent": o["parent"] + offset if o["parent"] >= 0 else -1})
            for controller, lists in part["controllers"].items():
                place["controllers"].setdefault(controller, []).extend(lists)
        places[scene] = place
    wanted += [w for w in reader.wanted if w not in wanted]
    missing = [w for w in wanted if not os.path.isfile(os.path.join(data, w))]
    return {"layers": build.layers, "places": places, "shaders": shaders}, wanted, missing


def main(argv):
    parser = argparse.ArgumentParser(prog="assetcheck", description="Checks scene facts in the game's files.")
    parser.add_argument("--entries", required=True)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--data")
    source.add_argument("--snapshot")
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--needs", action="store_true")
    mode.add_argument("--record")
    args = parser.parse_args(argv)
    try:
        entries = read_entries(args.entries)
        if args.snapshot:
            if args.needs or args.record:
                raise BadInput("--needs and --record read the files: give --data")
            try:
                with open(args.snapshot, encoding="utf-8") as f:
                    snapshot = json.load(f)
            except (OSError, ValueError) as e:
                raise BadInput(f"cannot read {args.snapshot}: {e}")
        else:
            if not os.path.isfile(os.path.join(args.data, "globalgamemanagers")):
                raise BadInput(f"{args.data} has no globalgamemanagers")
            snapshot, wanted, missing = read_snapshot(args.data, entries)
            if args.needs:
                print("\n".join(wanted))
                return 0
            if missing:
                raise BadInput(f"not copied yet: {', '.join(missing)}")
            if args.record:
                with open(args.record, "w", encoding="utf-8", newline="\n") as f:
                    json.dump(snapshot, f, indent=1)
                return 0
        lines, failed = check(entries, snapshot)
    except BadInput as e:
        print(f"assetcheck: {e}", file=sys.stderr)
        return 2
    except Exception as e:  # anything else the files or the snapshot did not let it read
        print(f"assetcheck: cannot read the input: {type(e).__name__}: {e}", file=sys.stderr)
        return 2
    print("\n".join(lines))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
