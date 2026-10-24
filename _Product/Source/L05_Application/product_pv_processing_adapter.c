#include "product_pv_processing_adapter.h"
#include <limits.h>
#include <math.h>
#include <stddef.h>

static bool Policy(FbLinearizationPolicy p)
{ return p==FB_LINEARIZATION_CLAMP || p==FB_LINEARIZATION_ERROR; }
static bool CurveValid(const FbInputLinearizationConfig *c)
{
    size_t i;
    const FbSignalLinearizationParameters *p=&c->parameters;
    if (c->mode==FB_INPUT_LINEARIZATION_BYPASS) return true;
    if (c->mode!=FB_INPUT_LINEARIZATION_VALUE && c->mode!=FB_INPUT_LINEARIZATION_DEVIATION)
        return false;
    if (p->curve.table==NULL || p->curve.count<2 || p->curve.count>PRODUCT_PV_CURVE_MAX_POINTS ||
        !Policy(p->curve.policy) || (p->offset_enabled && !isfinite(p->offset)) ||
        (p->limits_enabled && (!Policy(p->limit_policy) || !isfinite(p->limit_low) ||
         !isfinite(p->limit_high) || p->limit_low>p->limit_high))) return false;
    for (i=0;i<p->curve.count;++i)
        if (!isfinite(p->curve.table[i].x) || !isfinite(p->curve.table[i].y) ||
            (i!=0 && p->curve.table[i].x<=p->curve.table[i-1].x)) return false;
    return true;
}
bool ProductPvProcessing_ValidateConfig(const ProductPvProcessingConfig *c)
{
    if (c==NULL || c->maximum_age_ms==0 || c->maximum_age_ms>INT32_MAX ||
        c->maximum_skew_ms>c->maximum_age_ms || !CurveValid(&c->linearization[0])) return false;
    if (c->switch_enabled) {
        const FbInputSwitchOverConfig *s=&c->switch_config;
        if (!CurveValid(&c->linearization[1]) || s->sensor_low>s->switch_low ||
            s->switch_low>s->switch_high || s->switch_high>s->sensor_high ||
            (s->fail_action!=FB_SWITCH_USE_OTHER && s->fail_action!=FB_SWITCH_KEEP_SOURCE)) return false;
    }
    return true;
}
bool ProductPvProcessing_Init(ProductPvProcessingAdapter *a,
    const ProductPvProcessingConfig *c, ProductPvSourceProvider provider, void *context)
{
    if (a==NULL) return false;
    *a=(ProductPvProcessingAdapter){0};
    if (provider==NULL || !ProductPvProcessing_ValidateConfig(c)) return false;
    a->config=*c; a->provider=provider; a->provider_context=context; a->initialized=true;
    a->output.selected=FB_SWITCH_NONE; a->output.status=PRODUCT_PV_RESET;
    return true;
}
bool ProductPvProcessing_SetConfig(ProductPvProcessingAdapter *a,
    const ProductPvProcessingConfig *c)
{
    if (a==NULL || !a->initialized || !ProductPvProcessing_ValidateConfig(c)) return false;
    a->config=*c; FbInputSwitchOver_Init(&a->switch_fb);
    a->output=(ProductPvProcessingOutput){0}; a->output.selected=FB_SWITCH_NONE;
    a->output.status=PRODUCT_PV_RESET; return true;
}
static void Source(ProductPvProcessingAdapter *a, unsigned i, uint32_t now)
{
    ProductPvSourceDiagnostics *d=&a->output.sources[i];
    const FbInputLinearizationConfig *c=&a->config.linearization[i];
    FbSignalLinearizationInput in;
    FbSignalLinearizationOutput out;
    double rounded;
    d->status=PRODUCT_PV_SOURCE_UNAVAILABLE;
    if (!a->provider(a->provider_context,a->config.source[i],&d->raw)) return;
    if (!d->raw.valid) { d->status=PRODUCT_PV_SOURCE_INVALID; return; }
    if (d->raw.unit!=PRODUCT_PV_MILLICELSIUS) { d->status=PRODUCT_PV_SOURCE_UNIT; return; }
    d->age_ms=now-d->raw.timestamp_ms;
    if (d->age_ms>a->config.maximum_age_ms) { d->status=PRODUCT_PV_SOURCE_STALE; return; }
    if (c->mode==FB_INPUT_LINEARIZATION_BYPASS) d->processed=d->raw.value;
    else {
        in=(FbSignalLinearizationInput){(float)d->raw.value,true,true};
        if ((double)in.value!=(double)d->raw.value) { d->status=PRODUCT_PV_SOURCE_PRECISION; return; }
        d->linearization_status=FbInputLinearization_Execute(c,&in,&out);
        if (!out.valid) { d->status=PRODUCT_PV_SOURCE_LINEARIZATION; return; }
        rounded=out.value>=0 ? floor((double)out.value+0.5) : ceil((double)out.value-0.5);
        if (!isfinite(rounded) || rounded<INT32_MIN || rounded>INT32_MAX) {
            d->status=PRODUCT_PV_SOURCE_RANGE; return;
        }
        d->processed=(int32_t)rounded;
    }
    d->valid=true; d->status=PRODUCT_PV_SOURCE_OK;
}
ProductPvProcessingStatus ProductPvProcessing_Process(ProductPvProcessingAdapter *a,
    uint32_t now, bool enable, bool reset)
{
    FbInputSwitchOverInput in={0};
    FbInputSwitchOverOutput out;
    unsigned i;
    if (a==NULL) return PRODUCT_PV_INVALID_ARGUMENT;
    a->output=(ProductPvProcessingOutput){0}; a->output.selected=FB_SWITCH_NONE;
    a->output.timestamp_ms=now;
    if (!a->initialized) { a->output.status=PRODUCT_PV_NOT_INITIALIZED; return a->output.status; }
    if (!enable || reset) {
        FbInputSwitchOver_Init(&a->switch_fb);
        a->output.status=!enable ? PRODUCT_PV_DISABLED : PRODUCT_PV_RESET;
        return a->output.status;
    }
    Source(a,0,now);
    if (a->config.switch_enabled) {
        uint32_t age0, age1, skew;
        Source(a,1,now);
        age0=a->output.sources[0].age_ms; age1=a->output.sources[1].age_ms;
        skew=age0>=age1 ? age0-age1 : age1-age0;
        if (a->output.sources[0].valid && a->output.sources[1].valid && skew>a->config.maximum_skew_ms)
            for (i=0;i<2;++i) {
                a->output.sources[i].valid=false;
                a->output.sources[i].status=PRODUCT_PV_SOURCE_UNALIGNED;
            }
        for (i=0;i<2;++i) {
            in.value[i]=a->output.sources[i].processed;
            in.valid[i]=a->output.sources[i].valid;
        }
        in.enable=true;
        a->output.switch_status=FbInputSwitchOver_Execute(&a->switch_fb,&a->config.switch_config,&in,&out);
        a->output.pv=out.value; a->output.valid=out.valid;
        a->output.degraded=out.degraded; a->output.selected=out.selected;
    } else {
        a->output.valid=a->output.sources[0].valid;
        if (a->output.valid) { a->output.pv=a->output.sources[0].processed; a->output.selected=FB_SWITCH_SOURCE1; }
    }
    a->output.status=a->output.valid ? PRODUCT_PV_OK : PRODUCT_PV_INVALID_INPUT;
    return a->output.status;
}
bool ProductPvProcessing_GetOutput(const ProductPvProcessingAdapter *a, ProductPvProcessingOutput *o)
{
    if (a==NULL || o==NULL || !a->initialized) return false;
    *o=a->output; return true;
}
