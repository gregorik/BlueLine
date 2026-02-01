# BlueLine Plugin
### Clean Wires. Shared Tags. Pure Logic.

![image](https://img.shields.io/badge/-Unreal%20Engine-313131?style=for-the-badge&logo=unreal-engine&logoColor=blue) ![image](https://img.shields.io/badge/C%2B%2B-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=blue) ![image](https://img.shields.io/badge/Python-FFD43B?style=for-the-badge&logo=python&logoColor=blue) ![image](https://img.shields.io/badge/json-5E5C5C?style=for-the-badge&logo=json&logoColor=white) ![image](https://img.shields.io/badge/MIT-green?style=for-the-badge) ![alt text](https://img.shields.io/github/stars/gregorik/InstantOrganicCaves) ![alt text](https://img.shields.io/badge/Support-Patreon-red) [![YouTube](https://img.shields.io/badge/YouTube-Subscribe-red?style=flat&logo=youtube)](https://www.youtube.com/@agregori) [![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/C0C616ULD4)

[Example video 1](https://www.youtube.com/watch?v=qFOMJrigYo0) <br>
[Update video 1](https://www.youtube.com/watch?v=pUQSMOLOd9c)

<img width="1280" height="720" alt="bluel111" src="https://github.com/user-attachments/assets/c17413bf-5ded-4ce3-98f0-129d0a77e44a" />


**BlueLine** is a lightweight, modular editor productivity & visualization plugin for Unreal Engine 5.7+. At its current state (0.2), it solves the "Spaghetti Code" problem in Blueprints by enforcing strict, circuit-board-style layouts and semantically coloring data.

Its current three pillars are:

   1. Pathfinding ReRouting (Clean Wires):
       * Instead of the default curved splines that overlap messily, this plugin inserts "Knot" (Reroute) nodes to force wires into grid-snapped 90-degree angles.
       * It builds on existing UE5 logic to more intuitively "Straighten" existing connections via the Context Menu or a hotkey (Shift+Q).
       * A context menu is available on <i>entry/exit node pins</i> that enables selective wire Rerouting / Rigidifying even if no other nodes are selected.
       * BlueLine <i>connection interception</i> toggle: new connections automatically use Pathfinding Rerouting.

   2. Smart Tag Customization (Visual Semantics):
       * It intercepts FGameplayTag pins in the Graph Editor. Instead of a plain grey text box, it renders a colored badge and a dropdown picker.
       * The colors are driven by a central Theme Data Asset. For example, you can define Status.Damage as Red and Status.Heal as Green.

   3. Unified Runtime Debugging:
       * It provides a blueprint library (BlueLineDebugLib) to draw debug text in the 3D world.
       * Crucially, this debug text automatically matches the colors defined in your Editor Theme. If Status.Fire is Orange in your Blueprint graph, it appears as Orange floating text above your burning character.
                                                                                                                                                                                                                                                                                                                                                         █
  In short: It forces your visual scripting to be as organized and strictly typed as written code, while unifying your team's visual language across Editor and Game.


---

## 🏗 Installation

1.  **Download:** Clone or extract this repository into your project's `Plugins` folder:
    `YourProject/Plugins/BlueLine`
2.  **Regenerate:** Right-click your `.uproject` file and select **Generate Visual Studio Project Files**.
3.  **Compile:** Open the solution in Visual Studio/Rider and build your project.
4.  **Enable:** Launch the Editor, go to **Edit > Plugins**, and ensure **BlueLine** is enabled.

---

## 🔌 Graph Enhancements

BlueLine replaces the standard graph rendering pipeline with a high-performance "Manhattan" style renderer.

### 1. Manhattan Wiring
*   **Orthogonal Routing:** Wires turn at 90-degree angles.
*   **Ghost Wires:** Wires passing *behind* nodes are automatically rendered with reduced opacity (35%) to reduce visual noise.
*   **Performance:** Uses simple geometric heuristics instead of pathfinding, ensuring zero input lag even in massive Animation Blueprints.

### 2. "Soft Magnet" Formatting (`Shift + Q`)
Unlike other auto-formatters that rearrange your entire graph (destroying your specific layout), BlueLine uses a **Selection-Only** approach.
*   **Usage:** Select a group of nodes and press `Shift + Q`.
*   **Behavior:** Nodes align grid-relative to their **input connections**.
*   **The "Anti-Diff" Philosophy:** BlueLine never touches nodes you haven't selected. This ensures your Commit History remains clean and readable.

### 3. Hotkeys
| Key | Action | Description |
| :--- | :--- | :--- |
| **Shift + Q** | **Magnet Align** | Aligns selected nodes to the grid relative to their inputs. |
| **F8** | **Toggle Style** | Instantly switches between BlueLine wires and Standard Bezier curves. |

---

## 🏷️ Smart Tags & Visualization

BlueLine overrides the Details Panel customization for `FGameplayTag`, replacing the text string with a colored "Chip" widget.

### 1. Setup (Team Color Sync)
Colors are **not** stored in local user preferences. they are stored in a Data Asset so the whole team sees the same colors.

1.  Create a **DataAsset** derived from `BlueLineThemeData`.
2.  Name it `DA_BlueLineDefault` and place it in `/Game/BlueLine/` (Content/BlueLine/).
    *   *Note: This path ensures the plugin finds it automatically without configuration.*
3.  Add Tag Styles:
    *   **Tag:** `Status.Debuff`
    *   **Color:** Red
    *   **Apply To Children:** True
    *   *(Result: `Status.Debuff.Fire` and `Status.Debuff.Ice` will automatically inherit Red).*

### 2. Runtime Debugging (C++)
Debug colors shouldn't disappear when you hit PIE (Play In Editor). Use the static library to draw colored tags on the HUD or in World space.

```cpp
#include "Debug/BlueLineDebugLib.h"

// Draws the tag text at location, using the color defined in your Theme Data Asset.
UBlueLineDebugLib::DrawBlueLineDebugTag(this, MyGameplayTag, GetActorLocation());

// Or get the color manually for your own UI widgets:
FLinearColor TagColor = UBlueLineDebugLib::GetColorForTag(MyGameplayTag);
```

### 3. Blueprint Support
The library is exposed to Blueprint as **"Draw BlueLine Debug Tag"**.

---

## ⚙️ Configuration

### Shared Team Settings (Source Control)
*   **Asset:** `UBlueLineThemeData`
*   **Location:** `/Game/BlueLine/DA_BlueLineDefault`
*   **Contains:** Tag Colors, Wire Thickness, Bubble Sizes.
*   *Check this file into Git/Perforce.*

### Local User Preferences (Local Only)
*   **Location:** Editor Preferences > Plugins > BlueLine Graph
*   **Contains:**
    *   `Enable Manhattan Routing` (Toggle off if you prefer vanilla curves).
    *   `Dim Wires Behind Nodes`.
    *   `Magnet Snap Sensitivity`.
*   *These settings affect only your machine.*

---

## 🧩 Architecture (For Contributors)

The plugin is split into three distinct modules to ensure proper packaging behavior (Game vs. Editor).

### 1. `BlueLineCore` (Runtime)
*   **Type:** `Runtime` (Ships with game).
*   **Responsibilities:**
    *   Defines `UBlueLineThemeData`.
    *   Handles runtime color lookup logic.
    *   Contains `DrawDebug` helpers.
*   **Dependencies:** `GameplayTags`, `Core`.

### 2. `BlueLineSmartTags` (Editor)
*   **Type:** `Editor` (Stripped from game).
*   **Responsibilities:**
    *   Implements `IPropertyTypeCustomization`.
    *   Draws the "Chip" widget in details panels.
*   **Dependencies:** `BlueLineCore`, `PropertyEditor`, `Slate`.

### 3. `BlueLineGraph` (Editor)
*   **Type:** `Editor` (Stripped from game).
*   **Responsibilities:**
    *   Implements `FConnectionDrawingPolicy`.
    *   Handles wire geometry calculation.
    *   Handles "Magnet" formatting logic.
*   **Dependencies:** `BlueLineCore`, `GraphEditor`, `UnrealEd`.

---

## ❓ Troubleshooting

**Q: The wires look like standard Unreal wires?**
A: Press **F8** to toggle the style. Also, check *Editor Preferences > BlueLine Graph* to ensure "Enable Manhattan Routing" is checked.

**Q: My Debug Tags are Magenta/Pink?**
A: This indicates the plugin cannot find the Theme Data. Ensure you have created a `BlueLineThemeData` asset at `/Game/BlueLine/DA_BlueLineDefault`.

**Q: I get linker errors when building?**
A: Ensure your project has `GameplayTags` enabled in your `.uproject` file.

---
## Changelog Jan 2026

#### **🚀 New Features**
*   **Magnet Formatting (`Shift+Q`):** Added a non-destructive formatter that aligns selected nodes grid-relative to their input connections without altering the rest of the graph.
*   **Manhattan Router (`Shift+R`):** Implemented `FBlueLineManhattanRouter`. Instead of drawing custom wires, this tool physically inserts `UK2Node_Knot` (Reroute Nodes) to force wires into orthogonal 90-degree shapes.
    *   *Includes:* Type propagation (prevents Wildcard errors), Left-to-Right flow enforcement, and "Knot Collision" prevention.
*   **Smart Tag Chips (Details Panel):** Custom `IPropertyTypeCustomization` for `FBlueLineTagStyle`. Displays a colored chip, a native Gameplay Tag dropdown, and a live color preview strip.
*   **Colored Graph Pins:** Implemented `FBlueLineGraphPinFactory`. Blueprint nodes with Gameplay Tag pins now feature a high-visibility colored border matching the project's Data Asset configuration.
*   **Runtime Debug Library:** Added `UBlueLineDebugLib::DrawBlueLineDebugTag` to render colored debug text in the game world using the shared editor team colors.
*   **Auto-Discovery:** Logic added to `BlueLineDebugLib` to automatically scan the Asset Registry for the `BlueLineThemeData` asset, removing the requirement for hardcoded file paths.

#### **🔧 Improvements & Refactors**
*   **UE 5.7 Compatibility:**
    *   Refactored all Slate rendering logic to use strict `FVector2f` (floats) instead of `FVector2D`.
    *   Updated `FPaintGeometry` constructors to use `FSlateLayoutTransform`.
*   **Tag Picker UI:** Switched from a custom button implementation to `IGameplayTagsEditorModule::MakeGameplayTagWidget`, restoring native functionalities like "Add New Tag," "Rename," and "Search."
*   **Pin Visualization:** Updated `SBlueLineGraphPinEnhanced` to include visual stubs and "Connection Count" badges for non-tag pins.

#### **🐛 Bug Fixes**
*   **Crash Fix (Undo):** Fixed a crash occurring when checking `Undo` after routing wires. The fix ensures `UK2Node_Knot` pin types are finalized *before* being registered in the transaction history.
*   **Crash Fix (Startup):** Fixed a crash on engine launch caused by the Pin Factory attempting to access `UGameplayTagsManager` before it was allocated. Added `GetIfAllocated()` safety checks.
*   **Connectivity:** Fixed `Shift+R` creating tangled knots ("Spaghetti explosion") by filtering out backward-looping connections and enforcing minimum node spacing.
*   **Linker Errors:** Resolved LNK2019 errors by correctly adding dependencies (`GameplayTags`, `GameplayTagsEditor`, `Kismet`, `ToolMenus`, `AssetRegistry`) to `Build.cs`.

#### **❌ Deprecated / Removed**
*   **F8 (Wire Rendering):** Removed `FBlueLineConnectionPolicy`. UE 5.7 removed the `CreateConnectionPolicy` hook from the Global Factory, making custom wire drawing via plugins unsupported without engine modification. Replaced by the **Manhattan Router** logic.
*   **Toolbar Button:** Removed the "AutoFormat" toolbar button. Clicking the button caused the Graph Editor to lose focus, breaking the selection-context logic necessary for the formatter to work. The workflow is now Hotkey-only (`Shift+Q` / `Shift+R`).

---

### **v0.1 - v0.2 (Development History)**
*   *Initial prototype attempts consisting of monolithic modules.*
*   Attempted to use `FExtender` for toolbar buttons (Failed in UE5, moved to `UToolMenus`).
*   Attempted to use `BlueprintAssist`-style global formatting (abandoned for Selection-Only approach to preserve Git Diff history).
*   Attempted `A*` Pathfinding for wires (abandoned due to performance cost in large Animation Blueprints, switched to Geometric Heuristics).
*   Initial split into `Core` (Runtime) and `Editor` modules to fix packaging errors.


---
## 📄 License

MIT License.
