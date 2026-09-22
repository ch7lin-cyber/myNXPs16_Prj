#include "product_tc_k_table.h"

#include <stddef.h>

/*
 * K-type table storage.
 *
 * Measurement row units:
 *   {input_min_uV, input_max_uV, slope_q, intercept_q}
 *   temperature_mC = (slope_q * input_uV + intercept_q) / 1000000
 *
 * CJC row units:
 *   {input_min_mC, input_max_mC, slope_q, intercept_q}
 *   cjc_uV = (slope_q * input_mC + intercept_q) / 1000000
 *
 * The first matrix covers -200 to 1300 degree C in 20 degree C segments.
 * The second matrix covers the approved CJC range in 10 degree C segments.
 * Keep PRODUCT_TC_K_TABLE_READY at zero until all rows and boundary tests are
 * present. Returning NULL prevents incomplete calibration data being used.
 */
#define PRODUCT_TC_K_TABLE_READY (0U)

#if (PRODUCT_TC_K_TABLE_READY == 1U)
static const PiecewiseLinearSegment_t s_tc_k_measurement_segments[] =
{
    /* {input_min_uV, input_max_uV, slope_q, intercept_q}, */
};

static const PiecewiseLinearSegment_t s_tc_k_cjc_segments[] =
{
    /* {input_min_mC, input_max_mC, slope_q, intercept_q}, */
};

static const PiecewiseLinearTable_t s_tc_k_measurement_table =
{
    s_tc_k_measurement_segments,
    (uint16_t)(sizeof(s_tc_k_measurement_segments) /
               sizeof(s_tc_k_measurement_segments[0])),
    PRODUCT_TC_K_COEFFICIENT_SCALE
};

static const PiecewiseLinearTable_t s_tc_k_cjc_table =
{
    s_tc_k_cjc_segments,
    (uint16_t)(sizeof(s_tc_k_cjc_segments) /
               sizeof(s_tc_k_cjc_segments[0])),
    PRODUCT_TC_K_COEFFICIENT_SCALE
};
#endif

const PiecewiseLinearTable_t *ProductTcKTable_GetMeasurementTable(void)
{
#if (PRODUCT_TC_K_TABLE_READY == 1U)
    return &s_tc_k_measurement_table;
#else
    return NULL;
#endif
}

const PiecewiseLinearTable_t *ProductTcKTable_GetCjcTable(void)
{
#if (PRODUCT_TC_K_TABLE_READY == 1U)
    return &s_tc_k_cjc_table;
#else
    return NULL;
#endif
}

bool ProductTcKTable_IsReady(void)
{
    return (ProductTcKTable_GetMeasurementTable() != NULL) &&
           (ProductTcKTable_GetCjcTable() != NULL);
}



/***** ch add @2026/09/22 ***/


//CJC compensate -20.0~109.0[deg]
const s32 sas32_cjc_table_K[CJC_DEGREE_GROUP_NUMBER][2] =
{
    // y= ax + b ;
    {386, -77830}, {392, -78440}, {397, -79445}, {401, -80625}, {405, -82200},
    {409, -84215}, {411, -85425}, {413, -86785}, {415, -88375}, {416, -89310},
    {415, -88325}, {414, -87190}, {413, -85965},
};


// K type
const s32 sas32_uV_split_table_K[MID_TC_K_GROUP_SPLIT_NUMBER] =
{
    42,  309,  650, 1059, 1531, 2062, 2646, 3280, 3957, 4673,//10
    5422, 6200, 6998, 7812, 8636, 9467, 10296, 11120, 11935, 12740, //20
    13540, 14338, 15140, 15947, 16761, 17582, 18409, 19240, 20074, 20913, //30
    21754, 22597, 23443, 24291, 25141, 25992, 26844, 27697, 28550, 29403, //40
    30255, 31105, 31955, 32802, 33647, 34489, 35329, 36165, 36998, 37828, //50
    38653, 39475, 40293, 41108, 41918, 42724, 43526, 44324, 45118, 45908, //60
    46694, 47476, 48253, 49026, 49795, 50559, 51319, 52073, 52823, 53567, //70
    54305, 55038, 55765, 56486, 57200, 57908, 58610, 59306
};

const s32 sas32_uV_temp_table_K[MID_TC_K_GROUP_NUMBER][2] =
{
    // y= ax + b ; {a,b} [10^(-5),10^(-3)]
    {7479, -222741}, {5865, -217843}, {4890, -211575}, {4237, -204701}, {3766, -197516}, //5
    {3425, -190507}, {3155, -183375}, {2954, -176797}, {2793, -170452}, {2670, -164715}, //10
    {2571, -159359}, {2506, -155343}, {2457, -151916}, {2427, -149585}, {2407, -147865}, //15
    {2413, -148444}, {2427, -149901}, {2454, -152894}, {2484, -156474}, {2500, -158512}, //20
    {2506, -159317}, {2494, -157582}, {2478, -155157}, {2457, -151810}, {2436, -148290}, //25
    {2418, -145120}, {2407, -143089}, {2398, -141372}, {2384, -138560}, {2378, -137301}, //30
    {2372, -135996}, {2364, -134191}, {2358, -132784}, {2353, -131568}, {2350, -130813}, //35
    {2347, -130034}, {2345, -129495}, {2345, -129497}, {2345, -129495}, {2347, -130083}, //40
    {2353, -131900}, {2353, -131901}, {2361, -134459}, {2367, -136429}, {2375, -139125}, //45
    {2381, -141195}, {2392, -145080}, {2401, -148337}, {2410, -151653}, {2424, -156956}, //50
    {2433, -160443}, {2445, -165180}, {2454, -168797}, {2469, -174956}, {2481, -179987}, //55
    {2494, -185542}, {2506, -190767}, {2519, -196531}, {2532, -202401}, {2545, -208366}, //60
    {2558, -214437}, {2574, -222038}, {2587, -228321}, {2601, -235181}, {2618, -243647}, //65
    {2632, -250726}, {2653, -261501}, {2667, -268794}, {2688, -279887}, {2710, -291683}, //70
    {2729, -302004}, {2751, -314113}, {2774, -326931}, {2801, -342190}, {2825, -355923}, //75
    {2839, -364096}, {2874, -384460}
};




/***************************/
