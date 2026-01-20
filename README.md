# BlueLine Plugin
### Clean Wires. Shared Tags. Pure Logic.

![Unreal Engine 5.6+](https://img.shields.io/badge/Unreal%20Engine-5.6%2B-blue) ![Platform Windows|Mac|Linux](https://img.shields.io/badge/Platform-Win%20|%20Mac%20|%20Linux-lightgrey) ![u](https://img.shields.io/badge/License-GPL3-blue)

**BlueLine** is a lightweight, modular C++ plugin for Unreal Engine 5 designed to solve three specific problems in professional development workflows:
1.  **Graph Readability:** Replaces messy Bezier "noodles" with clean, 90-degree orthogonal wiring without the performance cost of A* pathfinding.
2.  **Team Collaboration:** Provides "Soft Formatting" tools that align nodes without reorganizing the entire graph, preventing massive Git/Perforce diffs.
3.  **Data Visualization:** Transforms standard Gameplay Tags in the Details Panel into colored, readable chips, synchronized across the entire team via Data Assets.

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

## 📄 License

MIT License. Free to use in commercial and personal projects.
