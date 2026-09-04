# 📋 Changelog — BlueLine Core (Open Source Edition)

All notable changes to the open-source core edition of BlueLine will be documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [1.2.0] — 2026-09-04

### 🌟 Major Release Highlights

```
   VANILLA WIRE (CURVED)                     BLUELINE MANHATTAN ROUTER (90°)
   [Node A]                                  [Node A]
      (Out) --..                                (Out) ──────┐
                ``--..                                      │
                      ``-- (In)                             ▼ (Knot Centered)
                          [Node B]                          └───► (In)
                                                                 [Node B]
```

```
   GENETIC GRAPH CLEANING & PURE FLOW
   [Pure Math] ───────┐
                      ▼
   [Exec In] ───► [Exec Node] ───► [Next Step]
   (Horizontal Flow Preserved, Comment Boxes Locked)
```

---

### 🔀 Pathfinding & Manhattan Routing (`Shift+R`)
* **Knot Pin Centering Math:** Corrected reroute knot placement to center precisely on the 32×32 knot geometry `(CornerX - 16, CornerY - 16)`, eliminating 16-pixel offset kinks on 90° bends.
* **Parallel Connection Staggering:** Added automatic horizontal staggering (`StaggerOffset`) for multiple connections running between the same source and target node pairs, preventing overlaid collinear wires.
* **Reroute Node Exemption:** Nodes of type `UK2Node_Knot` are now explicitly excluded from being processed as source/target blocks, preserving user-placed reroutes during batch routing passes.
* **Proximity Spacing Gate:** Enforces `MinRigidifySpacing` (default 100px) so closely packed nodes are not cluttered with redundant bends.

### 🧬 Evolutionary Graph Cleaner (`Shift+C`)
* **Topological BFS Flow:** Structures chaotic graph logic into clean left-to-right columns based on dependency depth.
* **Genetic Algorithm Crossing Optimizer:** Heuristically evaluates line intersections across parallel execution branches to minimize crossings.
* **Pure Node Flow Logic:** Pure nodes (math, getters, conversions) are placed cleanly to the left of their direct consumer execution nodes.
* **Comment Box Position Locking:** Existing `UEdGraphNode_Comment` containers and their encapsulated nodes retain their layout positions during whole-graph cleaning.
* **Directed Cycle Handling:** Full protection against infinite recursion or stack overflows when processing feedback loops or cyclic execution chains.

### 🧲 Magnet Auto-Format (`Shift+Q`)
* **Cycle-Safe Ancestor Checking:** Implemented lambda ancestor traversal (`IsAncestor`) in alignment planning to eliminate circular parent-child deadlocks.
* **Graph Context Transaction:** Added explicit `GraphContext->Modify()` call ensuring full Undo/Redo (`Ctrl+Z`) transaction fidelity.

### 🏷️ Smart Tags & Semantic Analysis (`Shift+T`)
* **Knot Traversal in Clustering:** The semantic cluster detection engine now traverses through `UK2Node_Knot` reroute chains, ensuring long logic sequences are categorized as single unified blocks.
* **Comment Node GUID Initialization:** Generated comment boxes now call `CreateNewGuid()` and fire `Graph->NotifyGraphChanged()`, resolving editor staleness issues.
* **Focus Fallback Integration:** Switched property customization graph resolution to `FBlueLineContextUtils::GetCurrentGraphFromFocus()`.
* **Action Menu Clutter Prevention:** Internal test nodes (`UK2Node_KingSafety`, `UK2Node_AWSTag`, `UK2Node_TagDemo`) are hidden from user Blueprint search menus unless `bExposeDemoNodes` is explicitly enabled.

### 🧪 Automated Testing Suite
* Added 10 automated unit and regression tests running under the `BlueLine.Graph.*` and `BlueLine.SmartTags.*` namespaces:
  * `BlueLine.Graph.Analyzer.BasicMetrics`
  * `BlueLine.Graph.Analyzer.ClusterDetection`
  * `BlueLine.Graph.Analyzer.BoundsCalculation`
  * `BlueLine.Graph.Analyzer.WireCrossings`
  * `BlueLine.Graph.Formatter.ExecAlignment`
  * `BlueLine.Graph.Cleaner.CyclesAndComments`
  * `BlueLine.Graph.Cleaner.PureNodeFlow`
  * `BlueLine.Graph.Routing.KnotCenteringAndPinPos`
  * `BlueLine.SmartTags.AutoTag.PersistsMetadata`
  * `BlueLine.SmartTags.AutoTag.RespectsBlueprintColorEditSetting`

### ⚡ Multi-Engine Compatibility
* **Removed Hardcoded Engine Pin:** Removed `"EngineVersion": "5.7.0"` from `BlueLineCore.uplugin`, establishing tested compatibility from **UE 5.5 floor** through **UE 5.8+**.
* **Clean Build Verification:** Verified zero compilation errors and zero linker warnings across Win64 targets on UE 5.5, UE 5.7, and UE 5.8.

### 🪟 UI & Hotkeys
* **Standard Chords:**
  * `Shift + Q`: Magnet Auto-Format Selection
  * `Shift + R`: Rigidify Wires (Manhattan)
  * `Shift + C`: Clean Graph
  * `Shift + T`: Auto-Tag Graph
  * `Shift + Alt + W`: Toggle Wire Style
* **Dynamic MainFrame Binding:** Modules listen to `OnModulesChanged` to dynamically bind hotkeys if `MainFrame` loads after plugin initialization.
* **Context Menu Extender:** Context menu automatically appends `Module.GetPluginCommands()` so hotkeys function while the right-click menu is open.

---

## [1.1.0] — 2026-02-15

### Added
* Semantic tag clustering engine (`FBlueLineSmartTagAnalyzer`) for automatic graph categorization into Combat, Movement, UI, AI, and Networking tags.
* "Spawn Messy Demo" right-click context menu action for live stress-testing formatting tools.
* Pin-level wire crossing calculations using pin offsets instead of whole-node center points.
* Shared endpoint exemption in line intersection math.

### Changed
* Refactored wire snapping (`FBlueLineWireSnapper`) to track `DragSourcePin` and prevent invalid pin snaps.
* Renamed toggle wire style shortcut to `Shift+Alt+W` to avoid conflicts with player ejection in PIE.

---

## [1.0.0] — 2026-02-01

### Added
* Initial open-source release of BlueLine Core under the MIT License.
* Orthogonal Manhattan wire router (`FBlueLineManhattanRouter`).
* Automatic connection interceptor (`FBlueLineConnectionInterceptor`).
* Selection-only Magnet auto-formatter (`BlueLineFormatter`).
* Graph complexity metric analyzer (`FBlueLineGraphAnalyzer`).
* Shared team theme data asset system (`UBlueLineThemeData`).
* Runtime 3D world text debug visualizer (`UBlueLineDebugLib`).
