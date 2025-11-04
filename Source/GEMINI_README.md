# Gemini Assistant Guide for the VATInstancing Plugin

This document is the primary technical guide for AI assistants working on this plugin. Its purpose is to ensure all generated or modified code adheres strictly to the established architecture and design philosophy. Read and adhere to these principles before making any changes.

## 1. Core Philosophy & Architecture

The system is built on a **decoupled, ID-centric, push-based model**. The primary goal is robustness and clear separation of concerns, especially between the game world and editor preview worlds.

-   **Decoupled**: Components do not know about the internal workings of the rendering system, and vice-versa. They communicate through a central registry using a unique ID.
-   **ID-centric**: The `FVATProxyId` (a `uint64`) is the single source of truth for identifying an instance. All operations (register, update, unregister) are keyed by this ID. We do **not** pass component pointers beyond the initial registration call.
-   **Push-based**: The `UVATInstancedProxyComponent` is responsible for *pushing* its state changes (transform, custom data) to the rendering system. The rendering system is passive and only acts when its API is called.

### Architectural Components & Data Flow

1.  **`UVATInstancedProxyComponent` (The "Client")**
    *   **Responsibility**: The public-facing component attached to Actors. It holds the visual configuration data (mesh, materials, etc., encapsulated in a `FBatchKey`) and the instance-specific data (transform, custom VAT data).
    *   **Lifecycle**:
        *   `InitializeComponent`: Creates a unique, persistent `ProxyId`.
        *   `OnRegister`: **The single point of registration.** Calls `VATInstanceRegistry::RegisterProxy()`. This happens universally (game and editor).
        *   `OnUnregister`: **The single point of unregistration.** Calls `VATInstanceRegistry::UnregisterProxy()`.
        *   `BeginPlay`/`EndPlay`: **MUST NOT** be used for registration/unregistration.

2.  **`VATInstanceRegistry` (The "Router")**
    *   **Responsibility**: A static, **stateless** router. It is the sole entry point for all API calls from proxy components.
    *   **Logic**: It inspects the `UWorld` from the component making the call and forwards the request to the appropriate renderer (`UVatiRenderSubsystem` for game worlds, or a `UVATInstanceRenderer` for preview worlds).
    *   **State**: It holds **NO** state about instances. It only holds pointers to the active renderers.

3.  **`IVATInstanceRendererInterface` (The "Contract")**
    *   **Responsibility**: Defines the abstract API that all renderers must implement (`RegisterProxy`, `UnregisterProxy`, `UpdateProxyVisuals`, etc.).
    *   All calls take `FVATProxyId` as the primary identifier.

4.  **`UVatiRenderSubsystem` (The "Game Renderer")**
    *   **Responsibility**: Manages all VAT instances for the main game world. It is a `UWorldSubsystem`.
    *   **State**: It is **stateful**. It maintains the mapping from `FVATProxyId` to the actual instance data and the `UInstancedStaticMeshComponent` (ISMC) that renders it.

5.  **`UVATInstanceRenderer` (The "Preview Renderer")**
    *   **Responsibility**: A lightweight, `UObject`-based renderer for non-game worlds (e.g., Blueprint editor preview).
    *   **State**: It is **stateful** and self-contained. It is created, managed, and owned by the editor code that requires a preview.

## 2. Hard Rules & Design Decisions

-   **RULE 1: Understand Animation Data Structures.**
    *   **Reason**: To correctly play animations by name and avoid bugs.
    *   **Implementation**: In `UMyAnimToTextureDataAsset`, there are two key arrays:
        *   `TArray<FAnim2TextureAnimSequenceInfo> AnimSequences`: This is the **design-time** data. It is configured by artists and contains the `TObjectPtr<UAnimSequence>`. **This is the array you search to find an animation by its name.**
        *   `TArray<FAnim2TextureAnimInfo> Animations`: This is the **runtime** data, generated at bake time. It contains only frame ranges and has no reference to the original `UAnimSequence`.
    *   **CRITICAL**: The relationship between these two arrays is their **index**. The animation at `AnimSequences[i]` corresponds to the runtime data at `Animations[i]`. To play an animation by name, you must first find its index in `AnimSequences` and then use that index to access the data in `Animations`.

-   **RULE 2: State Belongs to Renderers.**
    *   **Reason**: To maintain a clean architecture.
    *   **Implementation**: Never add instance-tracking maps or other state to `VATInstanceRegistry`. All state related to rendered instances (e.g., `TMap<FVATProxyId, FProxyInstanceInfo>`) belongs inside the renderer classes (`UVatiRenderSubsystem`, `UVATInstanceRenderer`).

-   **RULE 3: Understand VAT Texture Data Layout and Shader Calculation.**
    - Implementation (Texture Data Layout):
        - The BonePositionTexture and BoneRotationTexture have a total of NumFrames + 1 rows.
        - Rows 0 to NumFrames - 1 store the delta pose for each animation frame, calculated as DeltaPose(n) = Pose(n) - RefPose.
        - The very last row (at index NumFrames) stores the base RefPose itself.
    - Implementation (Shader Calculation):
        - The vertex shader performs two texture lookups to calculate the final vertex pose:
        - 1. Sample the DeltaPose using the frame data from the component (e.g., DeltaPose = Texture2DSample(BonePositionTexture, UV_for_frame_n)).
        - 2. Sample the RefPose from the last row of the texture (e.g., RefPose = Texture2DSample(BonePositionTexture, UV_for_last_row)).
        - The final pose is computed by adding them: FinalPose = RefPose + DeltaPose. If the shader receives a frame value of 0.0 (a common case in previews or on
        initialization), it correctly samples the delta for the first frame and adds it to the base RefPose, resulting in a valid, non-distorted pose.
    - Implementation (CPU-side Frame Calculation):
        - The UVATInstancedProxyComponent pre-calculates the normalized vertical texture coordinate on the CPU as UV.y = AbsoluteFrame / (NumFrames + 1).

-   **RULE 4: The Registry is the Only Entry Point.**
    *   **Reason**: To enforce the decoupled architecture.
    *   **Implementation**: A `UVATInstancedProxyComponent` must never try to get a direct pointer to a renderer. All communication must go through the static methods of `VATInstanceRegistry`.

-   **RULE 5: Understand Animation End/Transition Logic.**
    *   **Reason**: To ensure animations transition smoothly and end correctly according to design intent (e.g., looping vs. freezing on the last frame).
    *   **Implementation**: The logic resides in `UVATInstancedProxyComponent::UpdateAnimation`.
        *   **Smooth Transition**: When an animation is configured to transition to another (`bTransitionToNextOnEnd` is true), the switch is initiated `TotalBlendDuration` *before* the animation's calculated end time. This allows the new animation to start blending in while the old one is still playing its final moments.
        *   **Freeze on Last Frame**: If an animation is not set to transition, it will play to its end, and its state will be frozen on the final frame (`bIsPlayingAnimation` is set to `false`).
        *   **Looping**: Looping is achieved by setting the `NextAnimIndexToPlayOnEnd` to the same index as the currently playing animation. The transition logic handles this as a seamless loop.

-   **RULE 6: Use `ensure(false)` for Editor-Only Pointer Checks.**
    *   **Reason**: To provide immediate feedback to developers in the editor via a breakpoint when a pointer is unexpectedly null, without impacting runtime performance. Runtime checks are considered unnecessary if the logic is thoroughly tested.
    *   **Implementation**: All critical pointer checks that should "never" fail in a correctly configured setup must be wrapped in a `WITH_EDITOR` macro. Inside the macro, use `ensure(false)` to trigger a non-fatal assertion that breaks in the debugger but doesn't crash the editor. This is preferred over logging, which is often missed.

-   **RULE 7: Overlay Materials Drive Animated Effects via DMIs.**
    *   **Purpose**: To apply complex, animated visual effects (e.g., outlines, fresnel, distortion) that are perfectly synchronized with the base vertex animation.
    *   **Implementation**: Use the `UVATInstancedProxyComponent::SetOverlayMaterial` function.
    *   **Core Mechanism**: When called with `CreateDMIandOverwritePara = true` (the default and common case), this function performs several key actions:
        1.  It creates a **Dynamic Material Instance (DMI)** from the supplied overlay material.
        2.  It automatically populates this DMI with the correct VAT data (`BonePositionTexture`, `BoneRotationTexture`, bounding box info) from the component's `VisualTypeAsset`.
    *   **Intended Workflow**: The overlay material should be authored as a **Material Layer** (see `Content/Materials/ML_BoneAnimBlend.uasset` for an example) that uses the VAT parameters to manipulate attributes like `WorldPositionOffset` and `Normal`. This allows the overlay effect to be driven by the exact same animation data as the base mesh.
    *   **Static Overlays**: Setting `CreateDMIandOverwritePara` to `false` is for the rare case where you want to apply a simple, static material that does not need to be animated.

-   **RULE 8: Material Changes Must Be Committed Manually.**
    *   **Reason**: To prevent inefficient, repeated updates to the rendering system when changing multiple materials in a single frame. The `TickComponent` function is kept clean and focused only on animation updates.
    *   **Implementation**: The functions `SetMaterialForSlot` and `SetOverlayMaterial` do **not** immediately notify the rendering system. They only update the local material arrays and set an internal `bBatchKeyDirty` flag.
    *   **Required Action**: After setting one or more materials, you **must** call the `CommitMaterialChanges()` function. This function checks the dirty flag, constructs a new `FBatchKey` with the updated materials, and then efficiently notifies the `VATInstanceRegistry` of the change. This coalesces multiple material changes into a single, efficient update.


## 3. Code Style & Conventions
-   **Headers**: Include only what is necessary. Use forward declarations (`class UMyClass;`) in header files whenever possible to reduce compile times.

## 4. Documentation Maintenance
-   If you gain new insights into the project architecture during development, you should ask whether to add this new information to the documentation.
-   When modifying existing rules or designs, you must also update this document to reflect the changes.
