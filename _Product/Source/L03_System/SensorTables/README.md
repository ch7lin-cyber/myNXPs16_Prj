# Product sensor tables

Each sensor type owns one `product_<sensor>_table.c/.h` pair. Conversion code
remains in PlatformCore; this directory contains product coefficient data only.

## Naming

- Measurement: `Product<sensor>Table_GetMeasurementTable()`
- Cold junction: `Product<sensor>Table_GetCjcTable()` (thermocouples only)
- Readiness: `Product<sensor>Table_IsReady()`

## Fixed-point row

```c
{x_min, x_max, slope_q, intercept_q}
```

```text
y = (slope_q * x + intercept_q) / coefficient_scale
```

Units:

| Table | x | y |
|---|---|---|
| TC measurement | uV | 0.001 degree C |
| TC CJC | 0.001 degree C | uV |
| RTD measurement | milliohm | 0.001 degree C |

Adjacent rows must be monotonic and cover their boundary without a gap.
Incomplete tables must return `NULL` and must not be registered for runtime use.
