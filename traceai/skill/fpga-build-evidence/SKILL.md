---
name: fpga-build-evidence
description: Audit FPGA release artifact hashes, embedded XSA bitstream pairing, and supplied timing or DRC metrics. Use when checking reproducible build evidence before handing off a release; it does not program a device.
---

# FPGA build evidence

Read the release's maintained source, tool version, part/package/speed grade and final routed-report provenance. Identify which checkpoint produced the handed-off BIT and XSA. An earlier project run may have different timing from a post-route optimized checkpoint.

Use [scripts/verify_bundle.py](scripts/verify_bundle.py) for explicit artifact SHA256, XSA/BIT byte pairing and a supplied JSON timing/DRC summary. Pass actual paths; do not infer board memory maps or serial connections from filenames. The helper reads files and writes a result only.

```text
python scripts/verify_bundle.py --root <artifact-root> --manifest <manifest.json> --bit <file.bit> --xsa <file.xsa> --status <hardware_status.json> --output <audit.json>
```

The manifest uses `artifacts` as a map of root-relative file names to SHA256 strings. Keep paths within that root. The optional status JSON supplies `wns_ns`, `whs_ns`, `drc_errors`, and `drc_critical`. Missing or non-finite supplied metrics fail validation. Optional omission means unexamined, not passed. Include both BIT and XSA or neither.

Read original setup/hold, clock/CDC, route, exceptions and unconstrained-path reports before making a design signoff claim. A nonnegative WNS covers only correctly constrained paths. Do not create broad timing exceptions to clear a report. If source, constraints, model data or IP configuration changes, require new applicable build and test evidence.

For FPGA SoCs, additionally verify the platform/BSP came from the paired XSA and review address, clock, cache, DMA and ELF memory ownership. This helper cannot infer that compatibility. Treat host tests, RTL simulation, build completion, programming and board acceptance as separate states.

State which checks ran and which inputs were used. Preserve failed attempts and raw reports. Request missing artifacts only when required for the requested conclusion, and avoid substituting old successful records for current inputs. This workflow supplies no authorization to program, erase, open a serial port or publish files.

Validated behavior and limitations are recorded in [../tests/result.json](../tests/result.json). The relative test-report link is resolved from this folder's parent; projects can retain their own equivalent evidence when reusing this skill.
