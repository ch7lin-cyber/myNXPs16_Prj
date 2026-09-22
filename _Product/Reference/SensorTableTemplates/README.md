# Thermocouple table parameter template

Use `product_tc_x_table.c/.h` as the source for each unfinished TC type. The
completed K-type files in `Source/L03_System/SensorTables` are the reference
implementation.

## Planned files and rated ranges

| TC type | Target file stem | Rated range | Extended flag range |
|---|---|---:|---:|
| J | `product_tc_j_table` | -200 to 1200 C | -220 to 1220 C |
| T | `product_tc_t_table` | -200 to 400 C | -220 to 420 C |
| E | `product_tc_e_table` | 0 to 600 C | -20 to 620 C |
| N | `product_tc_n_table` | -200 to 1300 C | -220 to 1320 C |
| R | `product_tc_r_table` | 0 to 1700 C | -20 to 1720 C |
| S | `product_tc_s_table` | 0 to 1700 C | -20 to 1720 C |
| B | `product_tc_b_table` | 100 to 1800 C | 80 to 1820 C |
| L | `product_tc_l_table` | -200 to 850 C | -220 to 870 C |
| U | `product_tc_u_table` | -200 to 500 C | -220 to 520 C |
| TXK | `product_tc_txk_table` | -150 to 800 C | -170 to 820 C |
| C | `product_tc_c_table` | 0 to 2300 C | -20 to 2320 C |
| D | `product_tc_d_table` | 0 to 2300 C | -20 to 2320 C |

K type is already complete and is not generated from this template again.

## Data to enter for each type

Search for `TODO: USER TABLE DATA` and enter:

1. Rated minimum and maximum temperature in 0.001 degree C.
2. Measurement segment count and input-uV shift.
3. `segment_count + 1` shifted-uV boundaries.
4. One measurement `{a, b}` row for each segment.
5. CJC segment count.
6. One CJC `{a, b}` row for each 10 degree C segment.

Keep `PRODUCT_TC_X_TABLE_COMPLETE` equal to `0U` while entering data. Change it
to `1U` only after the row counts, monotonic boundaries, endpoint results and
continuity have been checked.

## Fixed-point equations

Measurement:

```text
shifted_uV = input_uV + input_shift_uV
temperature_mC = a * shifted_uV / 100 + b_mC
```

Cold-junction compensation:

```text
shifted_temperature_deci_C = temperature_deci_C + 200
cjc_deci_uV = a * shifted_temperature_deci_C + b_deci_uV
```

Do not add the CJC voltage inside the table. The conversion service performs:

```text
calibrated_TC_uV + CJC_uV -> measurement table -> temperature
```
