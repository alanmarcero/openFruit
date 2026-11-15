# openFruit dB Output Control Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use $executing-plans to implement this plan task-by-task.

**Goal:** Add a saved, automatable −24 to +12 dB output knob to openFruit without changing existing volume automation.

**Architecture:** Add an APVTS output parameter and rotary control. Send its target to FruitEngine, which smooths and applies gain after the existing master-volume and soft-saturation stage. Keep the legacy linear Master Volume and track parameters.

**Tech Stack:** C++20, JUCE 8.0.9, CMake, Catch2.

**Spec:** `/Users/alan.marcero/Development/random-question-claude/audio-plugin-output-db-controls-design-2026-11-15.md`

## Global Constraints

- Output range is −24 to +12 dB, with a 0 dB default.
- Preserve the `master_vol` and per-track volume parameter IDs and ranges.
- Use the project's existing JUCE/CMake dependencies and keep the output stage allocation-free.
- Bump the project version from 1.4.0 to 1.5.0.

## Review Focus

- 0 dB must leave output at unity; −6 dB must scale output by about 0.501 — assert against deterministic sample audio.
- Output automation must smooth without a click — test a target change and bound the adjacent-sample step.
- Output dB must be saved and restored — test the APVTS parameter state.
- Older sessions without the parameter must use 0 dB — the new parameter default provides the fallback.
- Existing Master Volume automation must retain its linear range and effect — assert the old parameter ID and range remain.

---

### Task 1: Add automatable dB output control

**Files:**
- Modify: `Tests/FruitEngineTests.cpp`
- Create: `Tests/PluginParameterTests.cpp`
- Modify: `Source/Plugin/Parameters.h`
- Modify: `Source/Plugin/Parameters.cpp`
- Modify: `Source/Plugin/PluginProcessor.cpp`
- Modify: `Source/Plugin/Panels.h`
- Modify: `Source/Plugin/Panels.cpp`
- Modify: `Source/Core/FruitEngine.h`
- Modify: `Source/Core/FruitEngine.cpp`
- Modify: `CMakeLists.txt`
- Modify: `README.md`
- Modify: `.github/workflows/build.yml`

**Interfaces:**
- Produces: `openfruit::params::kOutputDb`, the APVTS ID `output_db`, bounded from −24 to +12 dB with 0 dB default.
- Produces: `FruitEngine::setOutputDb(float)`, which updates the smoothed post-saturation output target.

- [x] **Step 1: Write the failing engine test** using a deterministic sample, comparing 0 dB output with −6 dB output and checking a smoothed parameter step.
- [x] **Step 2: Run the engine test** and confirm it fails because the output control is not implemented.
- [x] **Step 3: Write the failing APVTS test** for the new automatable parameter, its 0 dB default, state recall, and preservation of `master_vol`.
- [x] **Step 4: Run the parameter test** and confirm it fails because `output_db` is absent.
- [x] **Step 5: Add the parameter and UI knob, pass its target from PluginProcessor to FruitEngine, and apply smoothed gain after soft saturation.**
- [x] **Step 6: Bump the project version to 1.5.0, document the Output control, and publish releases from version tags only.**
- [x] **Step 7: Run the full CTest suite.** Expected: zero failures.
- [x] **Step 8: Commit the feature and plan** with the personal noreply Git identity.
