Summary: Options that do nothing until enabled, by an environment variable, a registry value or a user-supplied renderer
Range: 2000-2999

<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->
# options

Taken by `macfixes`. Each patch adds something a user or an app embedding
Wine has to ask for. Until then, programs see no difference.

**Dormant** means a program sees nothing different until the option is
turned on. Each patch here must keep to that:

- **Code paths.** Every changed path checks the switch first, and the
  check is cheap. Nothing may run per frame or per call until something
  turns the option on.
- **Placeholders.** A patch may install inert files the option needs,
  such as placeholder DLLs Wine only loads when their file is in the
  prefix (`find_builtin_without_file`). Loading one must behave as
  before: it fails the same way a missing DLL does. Its header says so.
- **Exports.** New exports that nothing calls until a renderer uses them
  are fine.
- **Enabling.** The header's `Why` says how to turn the option on.

Check dormancy by measuring, not by reading: the review found per-frame
work behind one "option".

## The switches

| Switch | Where | Patch | Versions |
|---|---|---|---|
| `WINEMSYNC=1` (10.0 also `WINEESYNC=1`) | environment | `ntdll-msync` | all |
| `WINE_SIMULATE_WRITECOPY=1` | environment | `ntdll-simulate-writecopy` | all |
| `WINENCPU=<n>` | environment | `ntdll-cpu-count-cap` | all |
| `CX_FWD_COMPAT_GL_CTX=1` | environment | `winemac-fwd-compat-gl-context` | 10.0 |
| `WINEDLLPATH_D3DMETAL`, `_CNCD`, `_D9VK`, `_DXVK`, `_DXMT` | environment, set by a launcher | `ntdll-renderer-dll-paths` | all (D3DMetal 10.0 only) |
| `SIKARUGIR_KEEP_METAL_HUD`, `SIKARUGIR_D2R_FORCE_VKD3D` | environment | `ntdll-launcher-metal-hud`, `ntdll-d2r-renderer-paths` | all |
| `CX_APPLEGPTK_LIBD3DSHARED_PATH` | environment | `ntdll-d3dmetal-code-regions` | 10.0 |
| `CX_D3DMETALPATH` | environment | `kernelbase-nvngx-d3dmetal` | 10.0 |
| `HKCU\Software\Wine\AppDefaults\<exe>\CommandLineAppend` | registry | `kernelbase-commandline-append` | all |
| `…\Mac Driver\ColorDepth` (per program only) | registry | `gdi32-per-program-color-depth` | 11.22 |
| `WINEMAC_PLUGINS` | environment | `winemac-plugins` | 11.22 |
| `…\Mac Driver\ActivationPolicy`, `WINEMAC_ACTIVATION_POLICY` | registry, environment | `winemac-activation-policy` | 11.22 |
| `…\Mac Driver\HostWindows` | registry | `winemac-host-windows`, `winemac-host-displays` | 11.22 |

The `CX_` and `SIKARUGIR_` names are CrossOver's and Sikarugir's (user,
2026-11-25): launchers that already set them work unchanged. The D3DMetal
switches work on 10.0 only. D3DMetal 3.0 needs Wine 10.4's GSBASE model,
which WineHQ changed in 10.5 (measured: it crashes creating its DXGI
factory on 11.22).

The set's patches apply in order number, together with the other sets
a flavour takes. Layout and header format: [patches/README.md](../../README.md).
