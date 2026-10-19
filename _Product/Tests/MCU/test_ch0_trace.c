/* Reuse the integration mock and original checks, then exercise trace limits. */
#define main integration_main
#include "../Integration/test_product_fb_schedule_integration.c"
#undef main

int main(int argc, char **argv)
{
    uint32_t i;
    static const int32_t values[] = {100, 200, 300, 400, 500, 600};
    assert(integration_main() == 0);
    assert(ProductFbScheduleIntegration_Init(UINT32_MAX - 10U));
    for (i = 0U; i < 6U; ++i)
    {
        g_snapshot.sample_sequence = i;
        g_snapshot.engineering_value = values[i];
        g_snapshot.valid = true;
        ProductFbScheduleIntegration_Process(9U + i * 20U);
    }
    assert(g_product_fb_ch0_trace.count == 6U);
    assert(g_product_fb_ch0_trace.records[5].sample_count == 5U);
    assert(g_product_fb_ch0_trace.records[5].average == 400.0);
    assert(g_product_fb_ch0_trace.records[0].elapsed_ms == 20U);
    assert(g_product_fb_ch0_trace.records[0].cycles_valid == 0U);
    ProductFbScheduleIntegration_Process(129U);
    assert(g_product_fb_ch0_trace.records[6].accepted == 0U);
    g_snapshot.valid = false;
    ProductFbScheduleIntegration_Process(149U);
    assert(g_product_fb_ch0_trace.records[7].sample_count == 0U);
    g_snapshot.valid = true;
    ProductFbScheduleIntegration_Process(169U);
    assert(g_product_fb_ch0_trace.records[8].sample_count == 1U);
    if (argc == 2)
    {
        FILE *csv = fopen(argv[1], "w");
        assert(csv != NULL);
        fputs("init_ms,now_ms,elapsed_ms,lateness_ms,sequence,pv,provider_ok,input_valid,accepted,sample_count,output_valid,average,status,cycles,cycles_valid\n", csv);
        for (i = 0U; i < g_product_fb_ch0_trace.count; ++i)
        {
            ProductFbCh0TraceRecord_t r = g_product_fb_ch0_trace.records[i];
            fprintf(csv, "%lu,%lu,%lu,%lu,%lu,%ld,%lu,%lu,%lu,%lu,%lu,%.17g,%lu,%lu,%lu\n",
                (unsigned long)g_product_fb_ch0_trace.init_ms,
                (unsigned long)r.now_ms, (unsigned long)r.elapsed_ms,
                (unsigned long)r.lateness_ms, (unsigned long)r.sequence,
                (long)r.pv, (unsigned long)r.provider_ok,
                (unsigned long)r.input_valid, (unsigned long)r.accepted,
                (unsigned long)r.sample_count, (unsigned long)r.output_valid,
                r.average, (unsigned long)r.status, (unsigned long)r.cycles,
                (unsigned long)r.cycles_valid);
        }
        assert(fclose(csv) == 0);
    }
    for (i = 9U; i < 520U; ++i)
    {
        ProductFbScheduleIntegration_Process(9U + i * 20U);
    }
    assert(g_product_fb_ch0_trace.count == 512U);
    assert(g_product_fb_ch0_trace.capture == 0U);
    assert(GetDiagnostics().callback_count == 520U);
    g_product_fb_ch0_trace.capture = 1U;
    ProductFbScheduleIntegration_Process(10409U);
    assert(g_product_fb_ch0_trace.count == 512U);
    assert(ProductFbScheduleIntegration_Init(0U));
    assert(g_product_fb_ch0_trace.count == 0U);
    puts("PASS: trace capacity, freeze, reinit, wrap, window and recovery (HOST ONLY)");
    return 0;
}
