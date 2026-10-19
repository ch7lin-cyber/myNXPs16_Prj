# Halt target and set g_product_fb_ch0_trace.capture=0 before sourcing.
# Writes ch0_trace.csv in GDB's working directory; requires no GDB Python.
define export-ch0-trace
  if g_product_fb_ch0_trace.capture != 0 || g_product_fb_ch0_trace.schema_version != 1 || g_product_fb_ch0_trace.count == 0 || g_product_fb_ch0_trace.count > 512
    echo ERROR: freeze a nonempty schema-1 trace before export.\n
  else
    set logging file ch0_trace.csv
    set logging overwrite on
    set logging redirect on
    set logging on
    printf "init_ms,now_ms,elapsed_ms,lateness_ms,sequence,pv,provider_ok,input_valid,accepted,sample_count,output_valid,average,status,cycles,cycles_valid\n"
    set $i = 0
    while $i < g_product_fb_ch0_trace.count
      set $r = &g_product_fb_ch0_trace.records[$i]
      printf "%u,%u,%u,%u,%u,%d,%u,%u,%u,%u,%u,%.17g,%u,%u,%u\n", g_product_fb_ch0_trace.init_ms, $r->now_ms, $r->elapsed_ms, $r->lateness_ms, $r->sequence, $r->pv, $r->provider_ok, $r->input_valid, $r->accepted, $r->sample_count, $r->output_valid, $r->average, $r->status, $r->cycles, $r->cycles_valid
      set $i = $i + 1
    end
    set logging off
    set logging redirect off
    echo Exported ch0_trace.csv in GDB working directory.\n
  end
end
export-ch0-trace
