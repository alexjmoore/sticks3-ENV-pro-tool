---
type: concept
title: M5StickS3 Two-Way Button Navigation
description: Non-blocking single-tap and double-tap gesture detection for responsive multi-screen cycling on M5Unified devices.
tags: [m5sticks3, ui, gestures, buttons]
timestamp: 2026-09-17
---

# M5StickS3 Two-Way Button Navigation

## Overview
With 9 distinct views (sensors, environmental summaries, historical charts, power diagnostics, and Matter status), linear cycling requires too many button presses. To allow bidirectional navigation on single-button interfaces (Button A), a non-blocking double-tap timer is implemented.

## Key Invariants & Details
1. **Double-Click Detection Timing**:
   - `DOUBLE_CLICK_TIME` window set to **280 ms**.
   - First click sets `pendingBtnASingleClick = true` and records `lastBtnAPressTime`.
   - If a second click occurs within 280 ms, double-click triggers immediately (`stepViewBack()`), clearing the pending flag. This produces **0 ms post-click latency** on double-tap navigation.
   - If 280 ms elapses without a second click, the single click triggers (`stepView()`).

2. **Wakeup Exemption**:
   - When the display is asleep (`!displayOn`), Button A or Button B press wakes the screen without modifying `currentView` or queuing a click.

3. **Immediate View Redraw**:
   - UI redraws are decoupled from the 5-second background sensor polling interval.
   - `ui.updateDisplay()` is invoked immediately upon view change, waking, or IMU auto-rotation for zero perceived latency.

## Related Concepts
- [M5StickS3 Thermal & Battery Optimization Invariants](./m5sticks3-power-optimization.md)
- [ESP32-S3 Native Matter-over-Wi-Fi Architecture & Endpoints](./matter-esp32-architecture.md)
