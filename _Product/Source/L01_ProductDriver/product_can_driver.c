#include "product_can_driver.h"

#include <stddef.h>
#include <string.h>

#include "fsl_mcan.h"

#define PRODUCT_CAN_STD_ID_MAX             (0x7FFU)
#define PRODUCT_CAN_STD_ID_SHIFT           (18U)
#define PRODUCT_CAN_FILTER_OFFSET          (0x000U)
#define PRODUCT_CAN_RX_FIFO_OFFSET         (0x010U)
#define PRODUCT_CAN_TX_BUFFER_OFFSET       (0x120U)

static ProductCanFrame_t s_tx_queue[PRODUCT_CAN_TX_QUEUE_DEPTH];
static ProductCanFrame_t s_rx_queue[PRODUCT_CAN_RX_FIFO_DEPTH];
static volatile uint8_t s_tx_head;
static volatile uint8_t s_tx_tail;
static volatile uint8_t s_tx_count;
static uint8_t s_rx_head;
static uint8_t s_rx_tail;
static uint8_t s_rx_count;
static ProductCanDiagnostics_t s_diagnostics;
static bool s_initialized;
static bool s_previous_bus_off;

static uint8_t NextIndex(uint8_t index, uint8_t depth)
{
    index++;
    return (index >= depth) ? 0U : index;
}

static bool ConfigureMessageRam(void)
{
    mcan_memory_config_t memory = {0};
    mcan_frame_filter_config_t standard_filter = {0};
    mcan_rx_fifo_config_t rx_fifo = {0};
    mcan_tx_buffer_config_t tx_buffer = {0};
    mcan_std_filter_element_config_t filter_element = {0};

    standard_filter.address = PRODUCT_CAN_FILTER_OFFSET;
    standard_filter.idFormat = kMCAN_FrameIDStandard;
    standard_filter.listSize = 1U;
    standard_filter.remFrame = kMCAN_rejectFrame;
    standard_filter.nmFrame = kMCAN_reject0;

    rx_fifo.address = PRODUCT_CAN_RX_FIFO_OFFSET;
    rx_fifo.elementSize = PRODUCT_CAN_RX_FIFO_DEPTH;
    rx_fifo.watermark = 0U;
    rx_fifo.opmode = kMCAN_FifoBlocking;
    rx_fifo.datafieldSize = kMCAN_8ByteDatafield;

    tx_buffer.address = PRODUCT_CAN_TX_BUFFER_OFFSET;
    tx_buffer.dedicatedSize = 1U;
    tx_buffer.fqSize = 0U;
    tx_buffer.mode = kMCAN_txFifo;
    tx_buffer.datafieldSize = kMCAN_8ByteDatafield;

    memory.baseAddr = PRODUCT_CAN_MESSAGE_RAM_BASE;
    memory.stdFilterCfg = &standard_filter;
    memory.rxFifo0Cfg = &rx_fifo;
    memory.txBufferCfg = &tx_buffer;

    /* The configured objects end below 0x200 in the reserved 4 KiB region. */
    (void)memset((void *)PRODUCT_CAN_MESSAGE_RAM_BASE, 0,
                 PRODUCT_CAN_MESSAGE_RAM_BYTES);
    if (MCAN_SetMessageRamConfig(CAN0, &memory) != kStatus_Success)
    {
        return false;
    }

    /* GFC and filter RAM are protected configuration and require INIT mode. */
    MCAN_EnterInitialMode(CAN0);
    /* Enable completion reporting for the single dedicated TX buffer. */
    CAN0->TXBTIE |= 1U;
    CAN0->GFC = (CAN0->GFC & ~(CAN_GFC_ANFE_MASK | CAN_GFC_RRFE_MASK)) |
                CAN_GFC_ANFE(kMCAN_reject0) |
                CAN_GFC_RRFE(kMCAN_rejectFrame);

    filter_element.sfec = kMCAN_storeinFifo0;
    filter_element.sft = kMCAN_range;
    filter_element.sfid1 = 0U;
    filter_element.sfid2 = PRODUCT_CAN_STD_ID_MAX;
    MCAN_SetSTDFilterElement(CAN0, &standard_filter, &filter_element, 0U);
    MCAN_EnterNormalMode(CAN0);
    return true;
}

bool ProductCanDriver_Init(void)
{
    mcan_config_t config;

    s_initialized = false;
    s_tx_head = 0U;
    s_tx_tail = 0U;
    s_tx_count = 0U;
    s_rx_head = 0U;
    s_rx_tail = 0U;
    s_rx_count = 0U;
    s_previous_bus_off = false;
    (void)memset(&s_diagnostics, 0, sizeof(s_diagnostics));

    MCAN_GetDefaultConfig(&config);
    config.baudRateA = PRODUCT_CAN_BITRATE_BPS;
    config.enableCanfdNormal = false;
    config.enableCanfdSwitch = false;
    config.enableLoopBackInt = (PRODUCT_CAN_INTERNAL_LOOPBACK != 0);
    config.enableLoopBackExt = false;
    config.enableBusMon = false;

    /* Use a timing solution for the actual 15 MHz MCAN source clock. */
    if (!MCAN_CalculateImprovedTimingValues(
            config.baudRateA, PRODUCT_CAN_SOURCE_CLOCK_HZ,
            &config.timingConfig))
    {
        return false;
    }

    MCAN_Init(CAN0, &config, PRODUCT_CAN_SOURCE_CLOCK_HZ);
    if (!ConfigureMessageRam())
    {
        MCAN_Deinit(CAN0);
        return false;
    }

    MCAN_ClearStatusFlag(CAN0, 0xFFFFFFFFU);
    s_initialized = true;
    return true;
}

bool ProductCanDriver_Send(const ProductCanFrame_t *frame)
{
    uint8_t index;

    if (!s_initialized || (frame == NULL) ||
        (frame->identifier > PRODUCT_CAN_STD_ID_MAX) ||
        (frame->length > PRODUCT_CAN_MAX_DATA_LENGTH))
    {
        s_diagnostics.tx_invalid_frame++;
        return false;
    }
    if (s_tx_count >= PRODUCT_CAN_TX_QUEUE_DEPTH)
    {
        s_diagnostics.tx_queue_full++;
        return false;
    }

    index = s_tx_head;
    s_tx_queue[index] = *frame;
    s_tx_head = NextIndex(s_tx_head, PRODUCT_CAN_TX_QUEUE_DEPTH);
    s_tx_count++;
    s_diagnostics.tx_queued++;
    s_diagnostics.tx_queue_pending = s_tx_count;
    return true;
}

bool ProductCanDriver_Receive(ProductCanFrame_t *frame)
{
    if ((frame == NULL) || (s_rx_count == 0U))
    {
        return false;
    }
    *frame = s_rx_queue[s_rx_tail];
    s_rx_tail = NextIndex(s_rx_tail, PRODUCT_CAN_RX_FIFO_DEPTH);
    s_rx_count--;
    return true;
}

static void ProcessRx(void)
{
    while ((CAN0->RXF0S & CAN_RXF0S_F0FL_MASK) != 0U)
    {
        mcan_rx_buffer_frame_t received = {0};
        ProductCanFrame_t frame = {0};
        uint8_t index;
        uint8_t length;

        if (MCAN_ReadRxFifo(CAN0, kMCAN_Fifo0, &received) != kStatus_Success)
        {
            break;
        }
        if ((received.xtd != 0U) || (received.rtr != 0U) ||
            (received.fdf != 0U) || (received.brs != 0U) ||
            (received.dlc > PRODUCT_CAN_MAX_DATA_LENGTH))
        {
            s_diagnostics.rx_invalid_frame++;
            continue;
        }

        length = (uint8_t)received.dlc;
        frame.identifier = (uint16_t)(received.id >> PRODUCT_CAN_STD_ID_SHIFT);
        frame.length = length;
        if (length > 0U)
        {
            (void)memcpy(frame.data, received.data, length);
        }

        if (s_rx_count >= PRODUCT_CAN_RX_FIFO_DEPTH)
        {
            s_diagnostics.rx_queue_dropped++;
            continue;
        }
        index = s_rx_head;
        s_rx_queue[index] = frame;
        s_rx_head = NextIndex(s_rx_head, PRODUCT_CAN_RX_FIFO_DEPTH);
        s_rx_count++;
        s_diagnostics.rx_received++;
    }
}

static void UpdateErrorDiagnostics(void)
{
    uint32_t interrupt_status = CAN0->IR;
    uint32_t protocol_status = CAN0->PSR;
    uint32_t error_status = CAN0->ECR;
    uint32_t clear_flags = 0U;
    bool bus_off = ((protocol_status & CAN_PSR_BO_MASK) != 0U);
    uint8_t lec = (uint8_t)((protocol_status & CAN_PSR_LEC_MASK) >>
                            CAN_PSR_LEC_SHIFT);

    s_diagnostics.bus_off = bus_off;
    s_diagnostics.tx_error_count = (uint8_t)(error_status & CAN_ECR_TEC_MASK);
    s_diagnostics.rx_error_count =
        (uint8_t)((error_status & CAN_ECR_REC_MASK) >> CAN_ECR_REC_SHIFT);
    s_diagnostics.tx_in_flight = (MCAN_IsTransmitRequestPending(CAN0, 0U) != 0U);
    s_diagnostics.tx_queue_pending = s_tx_count;

    if (bus_off)
    {
        s_diagnostics.controller_state = PRODUCT_CAN_STATE_BUS_OFF;
    }
    else if ((protocol_status & CAN_PSR_EP_MASK) != 0U)
    {
        s_diagnostics.controller_state = PRODUCT_CAN_STATE_ERROR_PASSIVE;
    }
    else if ((protocol_status & CAN_PSR_EW_MASK) != 0U)
    {
        s_diagnostics.controller_state = PRODUCT_CAN_STATE_ERROR_WARNING;
    }
    else
    {
        s_diagnostics.controller_state = PRODUCT_CAN_STATE_ERROR_ACTIVE;
    }

    if ((interrupt_status & CAN_IR_BO_MASK) != 0U)
    {
        clear_flags |= CAN_IR_BO_MASK;
    }
    /* BO interrupt signals both entry and exit; count entries from PSR. */
    if (bus_off && !s_previous_bus_off)
    {
        s_diagnostics.bus_off_events++;
    }
    if (s_previous_bus_off && !bus_off)
    {
        s_diagnostics.bus_off_recoveries++;
    }
    s_previous_bus_off = bus_off;

    if ((interrupt_status & CAN_IR_EW_MASK) != 0U)
    {
        s_diagnostics.error_warning_events++;
        clear_flags |= CAN_IR_EW_MASK;
    }
    if ((interrupt_status & CAN_IR_EP_MASK) != 0U)
    {
        s_diagnostics.error_passive_events++;
        clear_flags |= CAN_IR_EP_MASK;
    }

    if ((interrupt_status & (CAN_IR_PEA_MASK | CAN_IR_PED_MASK)) != 0U)
    {
        s_diagnostics.protocol_error_events++;
        if (lec <= 6U)
        {
            s_diagnostics.last_protocol_error = lec;
        }
        clear_flags |= interrupt_status & (CAN_IR_PEA_MASK | CAN_IR_PED_MASK);
    }

    if ((interrupt_status & CAN_IR_RF0L_MASK) != 0U)
    {
        s_diagnostics.rx_fifo_lost = true;
        s_diagnostics.rx_fifo_lost_events++;
        clear_flags |= CAN_IR_RF0L_MASK;
    }

    if (clear_flags != 0U)
    {
        MCAN_ClearStatusFlag(CAN0, clear_flags);
    }
}

static void ProcessTx(void)
{
    if (MCAN_GetStatusFlag(CAN0, kMCAN_TxTransmitCompleteFlag) != 0U)
    {
        MCAN_ClearStatusFlag(CAN0, kMCAN_TxTransmitCompleteFlag);
        s_diagnostics.tx_completed++;
    }

    /* Keep queued frames bounded during bus-off; send them after recovery. */
    if (s_diagnostics.bus_off || (s_tx_count == 0U) ||
        (MCAN_IsTransmitRequestPending(CAN0, 0U) != 0U))
    {
        return;
    }

    {
        ProductCanFrame_t frame = s_tx_queue[s_tx_tail];
        uint8_t data[PRODUCT_CAN_MAX_DATA_LENGTH] = {0};
        mcan_tx_buffer_frame_t tx = {0};

        if (frame.length > 0U)
        {
            (void)memcpy(data, frame.data, frame.length);
        }
        tx.id = (uint32_t)frame.identifier << PRODUCT_CAN_STD_ID_SHIFT;
        tx.xtd = kMCAN_FrameIDStandard;
        tx.rtr = kMCAN_FrameTypeData;
        tx.fdf = 0U;
        tx.brs = 0U;
        tx.dlc = frame.length;
        tx.data = data;
        tx.size = frame.length;

        if (MCAN_WriteTxBuffer(CAN0, 0U, &tx) == kStatus_Success)
        {
            MCAN_TransmitAddRequest(CAN0, 0U);
            s_tx_tail = NextIndex(s_tx_tail, PRODUCT_CAN_TX_QUEUE_DEPTH);
            s_tx_count--;
        }
    }
}

void ProductCanDriver_Process(void)
{
    if (!s_initialized)
    {
        return;
    }

    UpdateErrorDiagnostics();
    ProcessRx();
    ProcessTx();
    s_diagnostics.tx_in_flight =
        (MCAN_IsTransmitRequestPending(CAN0, 0U) != 0U);
    s_diagnostics.tx_queue_pending = s_tx_count;
}

bool ProductCanDriver_GetDiagnostics(ProductCanDiagnostics_t *diagnostics)
{
    if ((diagnostics == NULL) || !s_initialized)
    {
        return false;
    }
    *diagnostics = s_diagnostics;
    return true;
}

#if PRODUCT_CAN_TEST_BUILD
#include "product_can_test.h"
/* Intentionally compiled with the driver so IDE source lists need no changes. */
volatile ProductCanTestWatch_t g_can_test_watch;
static uint32_t s_test_target, s_test_deadline_start, s_test_baseline_completed;
static bool s_test_waiting;
static ProductCanFrame_t TestFrame(uint16_t id, uint32_t sequence)
{
    ProductCanFrame_t frame = {0};
    frame.identifier = id;
    frame.length = 8U;
    frame.data[0] = (uint8_t)sequence;
    frame.data[1] = (uint8_t)(sequence >> 8U);
    frame.data[2] = (uint8_t)(sequence >> 16U);
    frame.data[3] = (uint8_t)(sequence >> 24U);
    frame.data[4] = 0xA5U; frame.data[5] = 0x5AU;
    frame.data[6] = 0x00U; frame.data[7] = 0xFFU;
    return frame;
}
void ProductCanTest_Process(uint32_t now_ms)
{
    ProductCanDiagnostics_t diagnostics;
    ProductCanFrame_t frame, expected;
    uint32_t command = g_can_test_watch.command;
    if (!ProductCanDriver_GetDiagnostics(&diagnostics))
    {
        g_can_test_watch.state = 3U;
        return;
    }
    g_can_test_watch.driver = diagnostics;
    if (command != 0U)
    {
        g_can_test_watch.command = 0U;
        if (command == 5U) { g_can_test_watch.state = 0U; return; }
        if (command == 4U)
        {
            /* Explicit bench request only: clear INIT to start the hardware
             * recovery sequence. PSR.BO clearing must be observed separately. */
            if (diagnostics.bus_off)
            {
                MCAN_EnterNormalMode(CAN0);
                g_can_test_watch.recovery_requests++;
            }
            return;
        }
        if ((command > 3U) || (g_can_test_watch.state == 1U) ||
            ((command == 1U) != (PRODUCT_CAN_INTERNAL_LOOPBACK != 0)) ||
            diagnostics.tx_in_flight || (diagnostics.tx_queue_pending != 0U))
        { g_can_test_watch.state = 3U; return; }
        s_test_target = g_can_test_watch.requested_frames;
        if (s_test_target == 0U) { s_test_target = 1U; }
        if (s_test_target > 100000U) { g_can_test_watch.state = 3U; return; }
        g_can_test_watch.role = command;
        g_can_test_watch.matched = 0U;
        g_can_test_watch.submitted = 0U;
        g_can_test_watch.unexpected = 0U;
        g_can_test_watch.timeouts = 0U;
        g_can_test_watch.state = 1U;
        s_test_baseline_completed = diagnostics.tx_completed;
        s_test_deadline_start = now_ms;
        s_test_waiting = false;
    }
    if (g_can_test_watch.state != 1U) { return; }
    while (ProductCanDriver_Receive(&frame))
    {
        uint16_t rx_id = (g_can_test_watch.role == 2U) ? 0x322U : 0x321U;
        expected = TestFrame(rx_id, g_can_test_watch.matched);
        if ((frame.identifier != expected.identifier) || (frame.length != 8U) ||
            (memcmp(frame.data, expected.data, 8U) != 0) ||
            (g_can_test_watch.matched >= s_test_target) ||
            ((g_can_test_watch.role != 3U) && !s_test_waiting))
        {
            g_can_test_watch.unexpected++;
            g_can_test_watch.state = 3U;
            return;
        }
        if (g_can_test_watch.role == 3U)
        {
            frame.identifier = 0x322U;
            if (!ProductCanDriver_Send(&frame)) { g_can_test_watch.state = 3U; return; }
            g_can_test_watch.submitted++;
        }
        g_can_test_watch.matched++;
        s_test_waiting = false;
        s_test_deadline_start = now_ms;
    }
    if ((g_can_test_watch.role != 3U) && !s_test_waiting &&
        (g_can_test_watch.submitted < s_test_target))
    {
        frame = TestFrame(0x321U, g_can_test_watch.submitted);
        if (!ProductCanDriver_Send(&frame)) { g_can_test_watch.state = 3U; return; }
        g_can_test_watch.submitted++;
        s_test_waiting = true;
        s_test_deadline_start = now_ms;
    }
    if ((g_can_test_watch.matched == s_test_target) &&
        ((diagnostics.tx_completed - s_test_baseline_completed) == s_test_target))
    { g_can_test_watch.state = 2U; }
    else if ((uint32_t)(now_ms - s_test_deadline_start) >= 5000U)
    { g_can_test_watch.timeouts++; g_can_test_watch.state = 3U; }
}
#endif
