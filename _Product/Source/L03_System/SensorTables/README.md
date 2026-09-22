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

## Thermocouple table status

| Type | File stem | Rated range (degree C) | Status |
|---|---|---:|---|
| K | `product_tc_k_table` | -200 to 1300 | Complete |
| J | `product_tc_j_table` | -200 to 1200 | Awaiting coefficients |
| T | `product_tc_t_table` | -200 to 400 | Awaiting coefficients |
| E | `product_tc_e_table` | 0 to 600 | Awaiting coefficients |
| N | `product_tc_n_table` | -200 to 1300 | Awaiting coefficients |
| R | `product_tc_r_table` | 0 to 1700 | Awaiting coefficients |
| S | `product_tc_s_table` | 0 to 1700 | Awaiting coefficients |
| B | `product_tc_b_table` | 100 to 1800 | Awaiting coefficients |
| L | `product_tc_l_table` | -200 to 850 | Awaiting coefficients |
| U | `product_tc_u_table` | -200 to 500 | Awaiting coefficients |
| TXK | `product_tc_txk_table` | -150 to 800 | Awaiting coefficients |
| C | `product_tc_c_table` | 0 to 2300 | Awaiting coefficients |
| D | `product_tc_d_table` | 0 to 2300 | Awaiting coefficients |

For each incomplete type, fill every `TODO: USER TABLE DATA` block and the
measurement constants in its header. Keep `PRODUCT_TC_<TYPE>_TABLE_COMPLETE`
at `0U` while editing; set it to `1U` only after validation.

## RTD table status

| Type | File stem | Rated range (degree C) | Status |
|---|---|---:|---|
| Pt100 | `product_rtd_pt100_table` | -200 to 850 | Awaiting coefficients |
| JPt100 | `product_rtd_jpt100_table` | -20 to 400 | Awaiting coefficients |
| Ni120 | `product_rtd_ni120_table` | -80 to 300 | Awaiting coefficients |
| Cu50 | `product_rtd_cu50_table` | -50 to 150 | Awaiting coefficients |
| Pt1000 | `product_rtd_pt1000_table` | -200 to 850 | Awaiting coefficients |

RTD table input is calibrated resistance in milliohm. Fill each
`TODO: USER TABLE DATA` block and the segment count in its header. Keep
`PRODUCT_RTD_<TYPE>_TABLE_COMPLETE` at `0U` while editing; set it to `1U`
only after validation.
