# SM to H-ISM Converter

Unreal Engine editor plugin that converts selected Static Mesh Actors into
Instanced Static Mesh (ISM) or Hierarchical Instanced Static Mesh (HISM)
components, and back, from the level editor's actor context menu or from
Blueprint/C++.

Built for scenes with lots of repeated static geometry (vegetation, rocks,
props) where converting to instances by hand is tedious and error-prone,
especially once materials use per-instance Custom Primitive Data (CPD).

---

## Features

- **One-click conversion** from the level editor's right-click menu:
  `Convert to ISM`, `Convert to HISM`, `Reset To Static Mesh`.
- **Automatic grouping** of selected actors by mesh, materials, level, and
  (see below) Custom Primitive Data compatibility — each group becomes one
  ISM/HISM component.
- **ISM vs HISM choice**: HISM includes BVH culling, better for many small
  objects (rocks, vegetation); plain ISM suits fewer, larger objects
  (buildings, set pieces).
- **Fully reversible**: `Reset To Static Mesh` recreates individual
  `AStaticMeshActor`s from the instances, restoring transform, materials,
  shadow/rendering/LOD/lightmap settings, and Custom Primitive Data.
- **Correct handling of Custom Primitive Data with arbitrary materials**
  (see [How CPD is handled](#how-custom-primitive-data-is-handled) below) —
  this is the part that needs the most care and is covered in detail
  further down.
- **Material safety check**: automatically enables `Used with Instanced
  Static Meshes` on any material that's missing it, so instancing doesn't
  silently break rendering.
- **Full Undo/Redo support** via `FScopedTransaction`.
- Available both as an editor menu command and as a Blueprint-callable
  function library (`Convert Static Mesh To ISM` / `Revert ISM To Static
  Mesh Actors`).

---

## Installation

1. Copy the plugin folder into your project's `Plugins/` directory (or your
   engine's `Engine/Plugins/` for an engine-wide install).
2. Regenerate project files / rebuild.
3. Enable **SM Converter** in `Edit > Plugins` if it isn't enabled
   automatically.

---

## How to Use

### From the level editor

1. Select one or more Static Mesh Actors in the level.
2. Right-click → **SM to Instances** → **Convert to ISM** (or **Convert to
   HISM**).
3. To undo the conversion, select the created ISM/HISM actor(s) and choose
   **SM to Instances** → **Reset To Static Mesh**.

Selected actors are grouped automatically — you don't need to pre-sort them
by mesh or material, the plugin does this for you (see grouping rules
below).

### From Blueprint

Use the **SM to ISM** function library:

- `Convert Static Mesh To ISM (Settings, out Result)`
- `Revert ISM To Static Mesh Actors (Settings, out Result)`

Both take a settings struct (`FSMtoISMSettingsBP` / `FISMtoSMSettingsBP`)
exposing the same options as the C++ API below, and return a result struct
with counts and a -readable log — useful for driving the conversion
from an editor utility widget or a build script.

### From C++

```cpp
#include "SMtoISMConverter.h"

FSMtoISMSettings Settings;
Settings.bUseHISM = true;
Settings.bDeleteSourceActors = true;

FSMtoISMResult Result = FSMtoISMConverter::Convert(Settings);
```

---

## Settings Reference

| Setting | Default | Notes |
|---|---|---|
| `bReadFromSource` | `true` | Read shadow/rendering/LOD settings from each source actor instead of the values below. |
| `bDeleteSourceActors` | `true` | Delete original actors after a successful conversion. |
| `bUseHISM` | `true` | HISM (BVH culling) vs plain ISM. |
| `bCastShadow` / `bCastDynamicShadow` / `bCastStaticShadow` / `bCastContactShadow` / `bSelfShadowOnly` | — | Used only when `bReadFromSource = false`. |
| `bReceivesDecals` / `bRenderCustomDepth` / `CustomDepthStencil` / `BoundsScale` | — | Rendering overrides, same rule as above. |
| `ForcedLOD` / `MinLOD` / `MaxDrawDistance` | — | LOD overrides. |
| `bOverrideLightmapRes` / `OverrideLightmapRes` / `LightmapUVChannel` | — | Lightmap overrides (always applied, not gated by `bReadFromSource`). |
| `bDeleteSourceComponent` (revert) | `true` | Deletes the ISM/HISM container actor after reverting. |
| `bForceMovable` (revert) | `true` | Forces the recreated Static Mesh Actors to Movable; if `false`, mobility is copied from the ISM/HISM component. |

---

## How Custom Primitive Data is handled

This is the part that isn't obvious from the outside and is worth
understanding before relying on the plugin with production materials.

### The problem

A material can read a given Custom Primitive Data slot in two different
ways:

- **Per Instance Custom Data** node → the value can be different for every
  instance inside the same ISM/HISM component.
- **Generic Custom Primitive Data** (the "component-level" case, including
  a material that doesn't read that slot via a per-instance node at all) →
  the value is **shared by the whole component** — the engine has no way to
  give two instances in the same ISM/HISM different values for that slot.

A converter that ignores this distinction will, for any material using the
second kind, silently produce wrong visuals as soon as two actors with
different CPD values get grouped into the same component — because only
one of the two values can "win".

### What the plugin does about it

1. **Classification** (`FSMtoISMConverter::DetermineCPDDataModes`): for
   every CPD index on a source component, the plugin inspects every
   material assigned to it and checks whether *all* of them read that
   index through a `Per Instance Custom Data` (or `...3Vector`) node. If
   so, the index is safe to vary per instance. Otherwise (including
   materials that don't reference CPD via any node) it's treated as
   component-level.
2. **Grouping key**: two source actors only end up in the same ISM/HISM if,
   in addition to matching mesh/materials/level, their component-level CPD
   values are identical. If they differ, they get separate ISM/HISM
   components automatically — no data loss, no manual pre-sorting needed.
3. **Writing**: per-instance indices are written with
   `SetCustomData()` on the specific instance (this data is a normal
   serialized `UPROPERTY`, no extra handling needed). Component-level
   indices are written once per group.
4. **Reverting**: on `Reset To Static Mesh`, the same classification runs
   again against the ISM/HISM component's own materials, so each recreated
   actor gets its per-instance values back from `PerInstanceSMCustomData`
   and its component-level values back from the group's shared CPD.

### The persistence issue (and how it's solved)

`UPrimitiveComponent::SetCustomPrimitiveDataFloat()` is explicitly
documented by Epic as **run-time only — it does not serialize**. A value
set this way renders correctly immediately in the editor viewport, but is
lost the moment the level is closed and reopened, Play In Editor starts, or
the game is packaged, because none of those go through the code path that
originally set it.

Both directions of the conversion need this data to survive beyond the
current editor session, so the plugin attaches a small helper component,
**`USMConvertedCPDComponent`**, to any actor that has component-level CPD
to keep:

- It stores the values as a normal `UPROPERTY` (`BakedCustomPrimitiveData`)
  — this is the one copy that actually gets serialized.
- It reapplies them to the owning primitive in `OnRegister()`, which fires
  on editor level load, on entering PIE, and in packaged builds — a single
  mechanism covers all three.
- In editor builds, it also reapplies on `PostEditChangeProperty()`, so
  editing the array in the Details panel is reflected immediately.

It's attached in two places:

- On the **container actor** created by `Convert()`, holding the group's
  component-level values — only when there's at least one (materials using
  only Per Instance Custom Data don't get this component at all, since
  `PerInstanceSMCustomData` already persists on its own).
- On **every actor recreated** by `ConvertBack()`, holding all of that
  instance's CPD values (after reverting there's no more "per instance"
  concept, everything becomes plain component-level CPD on that actor).

**If you need to hand-tune a CPD value after a conversion**, edit it on
this component's `BakedCustomPrimitiveData` array, not anywhere else. It's
the only copy guaranteed to persist and be picked up again; editing the
primitive's live value directly will look right until the next time the
component reapplies its own array, at which point your edit is lost.

---

## Tips

- If you see a `[WARN] ... is missing Per Instance Custom Data for CPD
  indices [...]` in the Output Log (filter by `SMConverter`) after a
  conversion, it means a material doesn't expose a per-instance node for a
  CPD index it uses — the value is still preserved correctly (as
  component-level data), it just can't vary per instance within that ISM.
  Not necessarily an error, just worth knowing.
- If instances render with the wrong material after conversion, check the
  `[WARN] '...' non ha 'Used with Instanced Static Meshes'` log line — the
  plugin fixes this automatically, but the material asset needs saving
  afterwards if you want the fix to persist outside this session.

---

## Limitations

- Only `UStaticMeshComponent` sources are supported (not Skeletal Mesh or
  other primitive types).
- Grouping is per-level: actors in different sublevels are never grouped
  together even if otherwise identical.
- Component-level CPD values are part of the grouping key, so a scene with
  many *unique* component-level CPD values per object will produce many
  small ISM groups rather than one large one — this is a correctness
  trade-off, not a bug (see [How Custom Primitive Data is
  handled](#how-custom-primitive-data-is-handled)).

---

