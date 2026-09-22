#include "product_tc_e_table.h"

#include <stddef.h>

#if PRODUCT_TC_E_TABLE_COMPLETE

#if (PRODUCT_TC_E_MEASUREMENT_SEGMENT_COUNT == 0U)
#error "The E measurement segment count must not be zero"
#endif

typedef struct
{
    int32_t slope;
    int32_t intercept_millicelsius;
} ProductTcEMeasurementCoefficient_t;

typedef struct
{
    int32_t slope;
    int32_t intercept_deci_microvolts;
} ProductTcECjcCoefficient_t;

/* Values are input_uV + PRODUCT_TC_E_INPUT_SHIFT_UV. */
static const int32_t s_tc_e_measurement_boundaries_shifted_uv
    [PRODUCT_TC_E_MEASUREMENT_BOUNDARY_COUNT] =
{
    45, 1148, 2300, 3492, 4720, 5985, 7285, 8619, 9985, 11381,
    12803, 14251, 15721, 17212, 18720, 20245, 21784, 23336, 24900, 26474,
    28057, 29648, 31246, 32850, 34459, 36072, 37687, 39305, 40924, 42543,
    44162, 45779, 47393, 49005,
};

/* temperature_mC = slope * shifted_uV / scale + intercept_mC. */
static const ProductTcEMeasurementCoefficient_t
    s_tc_e_measurement_coefficients[PRODUCT_TC_E_MEASUREMENT_SEGMENT_COUNT] =
{
    {1813, -40758}, {1736, -39876}, {1678, -38552}, {1629, -36842}, {1581, -34582},
    {1538, -32001}, {1499, -29160}, {1464, -26150}, {1433, -23056}, {1406, -19992},
    {1381, -16787}, {1361, -13940}, {1341, -10799}, {1326, -8213}, {1311, -5402},
    {1300, -3178}, {1289, -787}, {1279, 1539}, {1271, 3530}, {1263, 5644},
    {1257, 7332}, {1252, 8810}, {1247, 10367}, {1243, 11681}, {1240, 12716},
    {1238, 13433}, {1236, 14187}, {1235, 14584}, {1235, 14585}, {1235, 14595},
    {1237, 13715}, {1239, 12796}, {1241, 11845},
};

/* cjc_deci_uV = slope * shifted_temperature_deci_C + intercept. */
static const ProductTcECjcCoefficient_t
    s_tc_e_cjc_coefficients[PRODUCT_TC_E_CJC_SEGMENT_COUNT] =
{
    {570, -115250}, {582, -116470}, {591, -118270}, {601, -121285}, {609, -124485},
    {619, -129480}, {628, -134890}, {637, -141195}, {645, -147575}, {655, -156575},
    {663, -164575}, {671, -173380}, {679, -182970},
};

static PiecewiseLinearSegment_t
    s_tc_e_measurement_segments[PRODUCT_TC_E_MEASUREMENT_SEGMENT_COUNT];
static PiecewiseLinearSegment_t
    s_tc_e_cjc_segments[PRODUCT_TC_E_CJC_SEGMENT_COUNT];
static bool s_tables_initialized;

static const PiecewiseLinearTable_t s_tc_e_measurement_table =
{
    s_tc_e_measurement_segments,
    PRODUCT_TC_E_MEASUREMENT_SEGMENT_COUNT,
    PRODUCT_TC_E_MEASUREMENT_COEFFICIENT_SCALE
};

static const PiecewiseLinearTable_t s_tc_e_cjc_table =
{
    s_tc_e_cjc_segments,
    PRODUCT_TC_E_CJC_SEGMENT_COUNT,
    PRODUCT_TC_E_CJC_COEFFICIENT_SCALE
};

static void InitializeTables(void)
{
    uint16_t index;

    if (s_tables_initialized)
    {
        return;
    }

    for (index = 0U; index < PRODUCT_TC_E_MEASUREMENT_SEGMENT_COUNT; index++)
    {
        int32_t slope = s_tc_e_measurement_coefficients[index].slope;
        s_tc_e_measurement_segments[index].x_min =
            s_tc_e_measurement_boundaries_shifted_uv[index] -
            PRODUCT_TC_E_INPUT_SHIFT_UV;
        s_tc_e_measurement_segments[index].x_max =
            s_tc_e_measurement_boundaries_shifted_uv[index + 1U] -
            PRODUCT_TC_E_INPUT_SHIFT_UV;
        s_tc_e_measurement_segments[index].slope = slope;
        s_tc_e_measurement_segments[index].intercept =
            ((int64_t)slope * PRODUCT_TC_E_INPUT_SHIFT_UV) +
            ((int64_t)s_tc_e_measurement_coefficients[index]
                 .intercept_millicelsius *
             PRODUCT_TC_E_MEASUREMENT_COEFFICIENT_SCALE);
    }

    for (index = 0U; index < PRODUCT_TC_E_CJC_SEGMENT_COUNT; index++)
    {
        int32_t slope = s_tc_e_cjc_coefficients[index].slope;
        s_tc_e_cjc_segments[index].x_min =
            PRODUCT_TC_E_CJC_MIN_MILLICELSIUS +
            ((int32_t)index * PRODUCT_TC_E_CJC_INTERVAL_MC);
        s_tc_e_cjc_segments[index].x_max =
            s_tc_e_cjc_segments[index].x_min + PRODUCT_TC_E_CJC_INTERVAL_MC;
        s_tc_e_cjc_segments[index].slope = slope;
        s_tc_e_cjc_segments[index].intercept =
            (((int64_t)slope * PRODUCT_TC_E_CJC_INPUT_SHIFT_DECICELSIUS) +
             s_tc_e_cjc_coefficients[index].intercept_deci_microvolts) *
            100LL;
    }

    s_tables_initialized = true;
}

const PiecewiseLinearTable_t *ProductTcETable_GetMeasurementTable(void)
{
    InitializeTables();
    return &s_tc_e_measurement_table;
}

const PiecewiseLinearTable_t *ProductTcETable_GetCjcTable(void)
{
    InitializeTables();
    return &s_tc_e_cjc_table;
}

bool ProductTcETable_IsReady(void)
{
    InitializeTables();
    return PiecewiseLinearTable_IsValid(&s_tc_e_measurement_table) &&
           PiecewiseLinearTable_IsValid(&s_tc_e_cjc_table);
}

#else

const PiecewiseLinearTable_t *ProductTcETable_GetMeasurementTable(void)
{
    return NULL;
}

const PiecewiseLinearTable_t *ProductTcETable_GetCjcTable(void)
{
    return NULL;
}

bool ProductTcETable_IsReady(void)
{
    return false;
}

#endif /* PRODUCT_TC_E_TABLE_COMPLETE */
