# Commercial feature research: openFruit

## Reference

XLN Audio's official XO support documentation describes lane-level nudging, independent groove settings, accent patterns with random variation, and linked A/B patterns. Those ideas informed the timing and dynamics additions below.

- [XO interface overview](https://support.xlnaudio.com/hc/en-us/articles/16920363887645-Interface-Overview)
- [XO getting-started guide](https://assets.xlnaudio.com/documents/xo-getting-started.pdf)
- [XO product page](https://www.xlnaudio.com/products/xo)

The reference is for sequencer workflow only. openFruit keeps its own sample engine, four-page patterns, and Song Mode.

## Capability gaps and implemented changes

1. **Per-track nudge.** The sequencer had per-step timing offsets but no whole-lane offset. Added host-automatable track nudge (−50% to +50% of one step), applied when scheduling the lane.
2. **Per-track groove amount.** Swing was global. Added a host-automatable groove amount per track that delays alternating hits by a fraction of a step.
3. **Periodic accentuator.** Step velocity was editable, but there was no repeating accent pattern. Added global host-automatable accent amount and interval; the scheduled event velocity is boosted on each interval without changing stored step velocities.

## Verification

- Release AU, VST3, LV2, and Standalone targets built successfully.
- CTest passed: **37/37** after the timing controls were moved into the labeled Track Settings callout.
- Refreshed and inspected assets/openFruit-screenshot.png, assets/openFruit-song-mode-screenshot.png, assets/openFruit-browser-screenshot.png, and assets/openFruit-track-settings-screenshot.png. The new per-track nudge and groove controls are visible with labels in the settings capture.
