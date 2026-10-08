# SUPERSEDED (2026-10-08, ReIncarnation M0)

`2026-09-26-camd-mysprintf-x86_64.diff` (the G7 local fix) is superseded
by upstream commit `28ec43a517` ("camd: format cluster names through a
va_list", Jaime Dias, 2026-10-05, on AROS master). The same change is
now carried in the Vulkan4AROS patch carriage for both ABIs (M0):

- `src/abi-patches/v1/aros/0073-camd-cluster-names-va_list.diff` (+ SERIES)
- `patches/camd-cluster-names-va_list.v11.patch`

This file is kept as the historical record of the G7 proof.
