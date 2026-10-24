#include "product_pv_processing_adapter.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>

typedef struct { ProductPvSourceSample samples[2]; bool available[2]; unsigned calls[2]; } Sources;
static bool Provider(void *context, uint16_t source, ProductPvSourceSample *sample)
{
    Sources *s=context;
    assert(source<2); ++s->calls[source];
    if (!s->available[source]) return false;
    *sample=s->samples[source]; return true;
}
int main(void)
{
    const FbLinearizationPoint points[]={{0,1000},{1000000,1001000}};
    ProductPvProcessingConfig c={0};
    ProductPvProcessingAdapter a, b;
    Sources sources={{{200000,90,1,PRODUCT_PV_MILLICELSIUS,true},
                      {300000,90,1,PRODUCT_PV_MILLICELSIUS,true}}, {true,true}, {0,0}};
    ProductPvProcessingOutput out;
    c.source[0]=0; c.source[1]=1; c.maximum_age_ms=100; c.maximum_skew_ms=10;
    c.linearization[0].mode=FB_INPUT_LINEARIZATION_VALUE;
    c.linearization[0].parameters.curve=(FbLinearizationConfig){points,2,FB_LINEARIZATION_CLAMP};
    c.linearization[1].mode=FB_INPUT_LINEARIZATION_BYPASS;
    c.switch_config=(FbInputSwitchOverConfig){-100000,1000000,200000,400000,FB_SWITCH_USE_OTHER};
    assert(ProductPvProcessing_Init(&a,&c,Provider,&sources));
    assert(ProductPvProcessing_Process(&a,100,true,false)==PRODUCT_PV_OK);
    ProductPvProcessing_GetOutput(&a,&out);
    assert(out.pv==201000 && out.selected==FB_SWITCH_SOURCE1 && sources.calls[1]==0);
    assert(sources.samples[0].value==200000); /* snapshot unchanged */
    c.switch_enabled=true; assert(ProductPvProcessing_SetConfig(&a,&c));
    ProductPvProcessing_Process(&a,100,true,false); ProductPvProcessing_GetOutput(&a,&out);
    assert(out.pv==250500 && out.selected==FB_SWITCH_BOTH && !out.degraded);
    sources.samples[0].valid=false;
    assert(ProductPvProcessing_Process(&a,100,true,false)==PRODUCT_PV_OK);
    ProductPvProcessing_GetOutput(&a,&out); assert(out.pv==300000 && out.degraded);
    sources.samples[0].valid=true; sources.samples[1].timestamp_ms=79;
    assert(ProductPvProcessing_Process(&a,100,true,false)==PRODUCT_PV_INVALID_INPUT);
    ProductPvProcessing_GetOutput(&a,&out);
    assert(!out.valid && out.pv==0 && out.sources[0].status==PRODUCT_PV_SOURCE_UNALIGNED);
    sources.samples[1].timestamp_ms=80;
    assert(ProductPvProcessing_Process(&a,100,true,false)==PRODUCT_PV_OK); /* skew equality */
    sources.samples[0].timestamp_ms=0; sources.samples[1].timestamp_ms=0;
    assert(ProductPvProcessing_Process(&a,100,true,false)==PRODUCT_PV_OK); /* age equality */
    assert(ProductPvProcessing_Process(&a,101,true,false)==PRODUCT_PV_INVALID_INPUT);
    sources.samples[0].timestamp_ms=UINT32_MAX-5; sources.samples[1].timestamp_ms=UINT32_MAX-5;
    assert(ProductPvProcessing_Process(&a,5,true,false)==PRODUCT_PV_OK); /* wrap age11 */
    sources.samples[0].timestamp_ms=sources.samples[1].timestamp_ms=100;
    assert(ProductPvProcessing_Process(&a,99,true,false)==PRODUCT_PV_INVALID_INPUT); /* future */
    sources.samples[0].unit=PRODUCT_PV_UNSUPPORTED_UNIT;
    sources.available[1]=false;
    assert(ProductPvProcessing_Process(&a,100,true,false)==PRODUCT_PV_INVALID_INPUT);
    ProductPvProcessing_GetOutput(&a,&out);
    assert(out.sources[0].status==PRODUCT_PV_SOURCE_UNIT && out.sources[1].status==PRODUCT_PV_SOURCE_UNAVAILABLE);
    sources.samples[0].unit=PRODUCT_PV_MILLICELSIUS; sources.available[1]=true;
    /* Transaction rejects bad config and preserves old config/history. */
    ProductPvProcessingConfig bad=c; bad.maximum_age_ms=0;
    assert(!ProductPvProcessing_SetConfig(&a,&bad) && a.config.maximum_age_ms==100);
    bad=c; bad.linearization[0].parameters.curve.count=1;
    assert(!ProductPvProcessing_SetConfig(&a,&bad));
    /* KEEP preserves BOTH history across a source failure. */
    c.switch_config.fail_action=FB_SWITCH_KEEP_SOURCE;
    assert(ProductPvProcessing_SetConfig(&a,&c));
    ProductPvProcessing_Process(&a,100,true,false);
    sources.samples[1].valid=false;
    assert(ProductPvProcessing_Process(&a,100,true,false)==PRODUCT_PV_INVALID_INPUT);
    ProductPvProcessing_GetOutput(&a,&out); assert(out.selected==FB_SWITCH_BOTH);
    assert(ProductPvProcessing_Init(&b,&c,Provider,&sources));
    assert(ProductPvProcessing_Process(&b,100,true,false)==PRODUCT_PV_OK); /* initial KEEP source1 */
    assert(ProductPvProcessing_Process(&a,100,true,true)==PRODUCT_PV_RESET);
    assert(ProductPvProcessing_Process(&a,100,true,false)==PRODUCT_PV_OK);
    assert(ProductPvProcessing_Process(&a,100,false,false)==PRODUCT_PV_DISABLED);
    /* Exact bypass vs explicit rejection of lossy float conversion. */
    c.switch_enabled=false; sources.samples[0].value=16777217;
    assert(ProductPvProcessing_SetConfig(&a,&c));
    assert(ProductPvProcessing_Process(&a,100,true,false)==PRODUCT_PV_INVALID_INPUT);
    ProductPvProcessing_GetOutput(&a,&out); assert(out.sources[0].status==PRODUCT_PV_SOURCE_PRECISION);
    c.linearization[0].mode=FB_INPUT_LINEARIZATION_BYPASS;
    assert(ProductPvProcessing_SetConfig(&a,&c));
    ProductPvProcessing_Process(&a,100,true,false); ProductPvProcessing_GetOutput(&a,&out);
    assert(out.pv==16777217);
    /* Out-of-range curve output is rejected before integer conversion. */
    { const FbLinearizationPoint huge[]={{0,2147483648.0f},{1,2147483648.0f}};
      c.linearization[0].mode=FB_INPUT_LINEARIZATION_VALUE;
      c.linearization[0].parameters.curve=(FbLinearizationConfig){huge,2,FB_LINEARIZATION_CLAMP};
      assert(ProductPvProcessing_SetConfig(&a,&c)); sources.samples[0].value=0;
      assert(ProductPvProcessing_Process(&a,100,true,false)==PRODUCT_PV_INVALID_INPUT);
      ProductPvProcessing_GetOutput(&a,&out); assert(out.sources[0].status==PRODUCT_PV_SOURCE_RANGE);
    }
    ProductPvProcessingAdapter empty={0};
    assert(ProductPvProcessing_Process(&empty,0,true,false)==PRODUCT_PV_NOT_INITIALIZED);
    assert(!ProductPvProcessing_GetOutput(&empty,&out));
    assert(ProductPvProcessing_Process(NULL,0,true,false)==PRODUCT_PV_INVALID_ARGUMENT);
    assert(!ProductPvProcessing_Init(NULL,&c,Provider,&sources));
    assert(!ProductPvProcessing_ValidateConfig(NULL));
    assert(!ProductPvProcessing_GetOutput(&a,NULL));
    puts("L05 PV processing: linearization -> switch, freshness/units, apply and lifecycle PASS");
    return 0;
}
