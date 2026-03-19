#include "uart_screen.hpp"
#include "bsp_tjc_hmi.hpp"
#include "bsp_usart.h"
#include "tx_api.h"

#include <cmath>
#include <cstring>

namespace {

static constexpr uint8_t  PAGE_HOME  = 0U;
static constexpr uint8_t  PAGE_PARAM = 1U;
static constexpr uint8_t  PAGE_NYQ   = 2U;
static constexpr uint8_t  PAGE_BODE  = 3U;
static constexpr uint8_t  PAGE_SOH   = 4U;
static constexpr uint8_t  PAGE_INVALID = 0xFFU;

static constexpr uint8_t  EVT_PRESS   = 0x01U;

static constexpr uint8_t  WAVE_COMP_ID = 1U;
static constexpr uint8_t  WAVE_CH      = 0U;
static constexpr uint16_t WAVE_POINTS  = 512U;

static constexpr uint16_t COLOR_BLACK  = 0U;
static constexpr uint16_t COLOR_GREEN  = 2016U;
static constexpr uint16_t COLOR_YELLOW = 65504U;
static constexpr uint16_t COLOR_RED    = 63488U;

/* ===================== Field Test Tunables =====================
 * 你后续主要改这一区域即可（对象名、按钮ID、阈值、倍率策略参数）
 */
static constexpr char CMD_SENDME[] = "sendme";
static constexpr char CMD_GET_CUR_PAGE[]       = "get dp";
static constexpr char CMD_BKCMD_VERBOSE[]      = "bkcmd=3";
static constexpr char CMD_GET_PAGE1_CELLS[]    = "get page1.t0.txt";
static constexpr char CMD_GET_PAGE1_CAPACITY[] = "get page1.t1.txt";
static constexpr char CMD_GET_PAGE1_BRAND[]    = "get page1.b1.txt";

static constexpr uint32_t PAGE1_QUERY_TIMEOUT_TICKS = 400U;
static constexpr uint32_t PAGE_PROBE_PERIOD_TICKS = 300U;
static constexpr uint8_t  PAGE4_VALUE_DECIMALS = 0U;

/* Page0 按钮映射（b0~b3） */
static constexpr uint8_t HOME_BTN_TO_PAGE1 = 0U;
static constexpr uint8_t HOME_BTN_TO_PAGE2 = 1U;
static constexpr uint8_t HOME_BTN_TO_PAGE3 = 2U;
static constexpr uint8_t HOME_BTN_TO_PAGE4 = 3U;

/* Page1/2/3/4 Home/功能按钮映射 */
static constexpr uint8_t PAGE1_BTN_HOME = 3U;   /* page1.b3 */
static constexpr uint8_t PAGE2_BTN_ADD  = 1U;   /* page2.b1 */
static constexpr uint8_t PAGE2_BTN_ADDT = 2U;   /* page2.b2 */
static constexpr uint8_t PAGE2_BTN_CLR  = 3U;   /* page2.b3 */
static constexpr uint8_t PAGE2_BTN_HOME = 0U;   /* page2.b0 */
static constexpr uint8_t PAGE3_BTN_ADD  = 1U;   /* page3.b1 */
static constexpr uint8_t PAGE3_BTN_ADDT = 2U;   /* page3.b2 */
static constexpr uint8_t PAGE3_BTN_CLR  = 0U;   /* page3.b0 */
static constexpr uint8_t PAGE3_BTN_HOME = 3U;   /* page3.b3 */
static constexpr uint8_t PAGE4_BTN_HOME = 5U;   /* page4.b5 */

/* Page1 -> 倍率策略阈值 */
static constexpr uint32_t CELLS_HIGH_TH = 6U;
static constexpr uint32_t CAPACITY_HIGH_TH_MAH = 5000U;
static constexpr uint32_t CAPACITY_MID_TH_MAH  = 3000U;
static constexpr float    GAIN_VOLTAGE_HIGH_CELLS = 0.5f;
static constexpr float    GAIN_VOLTAGE_NORMAL     = 1.0f;
static constexpr float    GAIN_CURRENT_HIGH_CAP   = 0.7f;
static constexpr float    GAIN_CURRENT_MID_CAP    = 0.9f;
static constexpr float    GAIN_CURRENT_LOW_CAP    = 1.1f;
static constexpr char     BRAND_TATTU[]           = "Tattu";
static constexpr float    BRAND_TATTU_AMP_FACTOR  = 1.05f;
static constexpr float    AMP_SCALE_MIN           = 0.2f;
static constexpr float    AMP_SCALE_MAX           = 2.0f;

/* 波形幅值范围 */
static constexpr float    WAVE_AMP_BASE = 90.0f;
static constexpr float    WAVE_AMP_MIN  = 10.0f;
static constexpr float    WAVE_AMP_MAX  = 120.0f;

/* Page4 阈值：两个阻值三档判定 */
static constexpr float R_OHM_LOW_TH  = 30.0f;
static constexpr float R_OHM_HIGH_TH = 50.0f;
static constexpr float R_CT_LOW_TH   = 20.0f;
static constexpr float R_CT_HIGH_TH  = 40.0f;

/* Page4 健康分扣分策略 */
static constexpr int PENALTY_R_OHM_LOW  = 20;
static constexpr int PENALTY_R_OHM_HIGH = 35;
static constexpr int PENALTY_R_CT_LOW   = 20;
static constexpr int PENALTY_R_CT_HIGH  = 35;

/* Page4 底部健康等级分段 */
static constexpr int SCORE_EXCELLENT_TH = 90;
static constexpr int SCORE_GOOD_TH      = 75;
static constexpr int SCORE_NORMAL_TH    = 60;
static constexpr int SCORE_WEAK_TH      = 40;

/* Page4 控件对象名 */
static constexpr char OBJ_PAGE4_R_OHM_VAL[] = "page4.t2";
static constexpr char OBJ_PAGE4_R_CT_VAL[]  = "page4.t3";
static constexpr char OBJ_PAGE4_R_OHM_LOW[]    = "page4.t14";
static constexpr char OBJ_PAGE4_R_OHM_NORMAL[] = "page4.t13";
static constexpr char OBJ_PAGE4_R_OHM_HIGH[]   = "page4.t12";
static constexpr char OBJ_PAGE4_R_CT_LOW[]     = "page4.t11";
static constexpr char OBJ_PAGE4_R_CT_NORMAL[]  = "page4.t9";
static constexpr char OBJ_PAGE4_R_CT_HIGH[]    = "page4.t8";
static constexpr char OBJ_PAGE4_LV_EXCELLENT[] = "page4.t7";
static constexpr char OBJ_PAGE4_LV_GOOD[]      = "page4.t6";
static constexpr char OBJ_PAGE4_LV_NORMAL[]    = "page4.t5";
static constexpr char OBJ_PAGE4_LV_WEAK[]      = "page4.t4";
static constexpr char OBJ_PAGE4_LV_BAD[]       = "page4.t15";

typedef struct
{
    uint32_t cells;
    uint32_t capacity_mah;
    char     brand[24];
    float    gain_voltage;
    float    gain_current;
    float    wave_amp_scale;
} page1_params_t;

typedef enum
{
    PAGE1_QUERY_IDLE = 0,
    PAGE1_QUERY_WAIT_CELLS,
    PAGE1_QUERY_WAIT_CAPACITY,
    PAGE1_QUERY_WAIT_BRAND,
} page1_query_state_e;

static TX_MUTEX g_screen_mutex;
static bool     g_screen_inited = false;

static uint8_t  g_wave_buf[WAVE_POINTS];

static volatile uint8_t g_current_page = PAGE_INVALID;

static page1_params_t g_page1_params =
{
    .cells         = 1U,
    .capacity_mah  = 1000U,
    .brand         = "UNKNOWN",
    .gain_voltage  = 1.0f,
    .gain_current  = 1.0f,
    .wave_amp_scale = 1.0f,
};

static page1_query_state_e g_page1_query_state = PAGE1_QUERY_IDLE;
static ULONG               g_page1_query_deadline = 0U;
static bool                g_page1_cells_valid = false;
static bool                g_page1_capacity_valid = false;
static bool                g_page1_brand_valid = false;
static ULONG               g_page1_last_update_tick = 0U;
static ULONG               g_last_page_probe_tick = 0U;
static ULONG               g_hmi_rx_frame_count = 0U;
static uint8_t             g_hmi_last_frame_type = 0U;

static float g_latest_r_ohm = 35.0f;
static float g_latest_r_ct  = 25.0f;

static uint8_t  g_rx_frame[128];
static uint16_t g_rx_len = 0U;
static uint8_t  g_ff_count = 0U;

static bool ScreenLock(void)
{
    return (TX_SUCCESS == tx_mutex_get(&g_screen_mutex, TX_WAIT_FOREVER));
}

static void ScreenUnlock(void)
{
    tx_mutex_put(&g_screen_mutex);
}

static uint32_t FloatToX100(float v)
{
    if (v <= 0.0f)
    {
        return 0U;
    }
    return (uint32_t)(v * 100.0f + 0.5f);
}

static bool ParseUintFromText(const uint8_t *data, uint16_t len, uint32_t *out)
{
    if ((nullptr == data) || (nullptr == out) || (len == 0U))
    {
        return false;
    }

    uint32_t val = 0U;
    bool got_digit = false;

    for (uint16_t i = 0U; i < len; ++i)
    {
        uint8_t c = data[i];
        if ((c >= (uint8_t)'0') && (c <= (uint8_t)'9'))
        {
            got_digit = true;
            val = val * 10U + (uint32_t)(c - (uint8_t)'0');
        }
        else if ((c == (uint8_t)' ') || (c == (uint8_t)'\t') || (c == (uint8_t)'\r') || (c == (uint8_t)'\n'))
        {
            if (got_digit)
            {
                break;
            }
        }
        else
        {
            if (!got_digit)
            {
                return false;
            }
            break;
        }
    }

    if (!got_digit)
    {
        return false;
    }

    *out = val;
    return true;
}

static void LogPage1TextFrame(const uint8_t *data, uint16_t len)
{
    char txt[32];
    uint16_t copy_len = len;
    if (copy_len >= sizeof(txt))
    {
        copy_len = (uint16_t)(sizeof(txt) - 1U);
    }

    for (uint16_t i = 0U; i < copy_len; ++i)
    {
        uint8_t c = data[i];
        txt[i] = ((c >= 0x20U) && (c <= 0x7EU)) ? (char)c : '.';
    }
    txt[copy_len] = '\0';

    BSP_Printf(COM_DEBUG,
               "[HMI] Page1 text: state=%d len=%u txt=%s\r\n",
               (int)g_page1_query_state,
               (unsigned)len,
               txt);
}

static void UpdateGainProfileFromPage1(void)
{
    /* Example policy: tune later in lab by calibration table. */
    g_page1_params.gain_voltage = (g_page1_params.cells >= CELLS_HIGH_TH) ? GAIN_VOLTAGE_HIGH_CELLS : GAIN_VOLTAGE_NORMAL;

    if (g_page1_params.capacity_mah >= CAPACITY_HIGH_TH_MAH)
    {
        g_page1_params.gain_current = GAIN_CURRENT_HIGH_CAP;
    }
    else if (g_page1_params.capacity_mah >= CAPACITY_MID_TH_MAH)
    {
        g_page1_params.gain_current = GAIN_CURRENT_MID_CAP;
    }
    else
    {
        g_page1_params.gain_current = GAIN_CURRENT_LOW_CAP;
    }

    g_page1_params.wave_amp_scale = g_page1_params.gain_current;

    if (0 == std::strcmp(g_page1_params.brand, BRAND_TATTU))
    {
        g_page1_params.wave_amp_scale *= BRAND_TATTU_AMP_FACTOR;
    }

    if (g_page1_params.wave_amp_scale < AMP_SCALE_MIN)
    {
        g_page1_params.wave_amp_scale = AMP_SCALE_MIN;
    }
    if (g_page1_params.wave_amp_scale > AMP_SCALE_MAX)
    {
        g_page1_params.wave_amp_scale = AMP_SCALE_MAX;
    }

    uint32_t gain_v_x100 = FloatToX100(g_page1_params.gain_voltage);
    uint32_t gain_i_x100 = FloatToX100(g_page1_params.gain_current);
    uint32_t amp_x100 = FloatToX100(g_page1_params.wave_amp_scale);

    BSP_Printf(COM_DEBUG,
               "[HMI] Page1 params: brand=%s cells=%lu cap=%lu, gainV=%lu.%02lu gainI=%lu.%02lu amp=%lu.%02lu\r\n",
               g_page1_params.brand,
               (unsigned long)g_page1_params.cells,
               (unsigned long)g_page1_params.capacity_mah,
               (unsigned long)(gain_v_x100 / 100U),
               (unsigned long)(gain_v_x100 % 100U),
               (unsigned long)(gain_i_x100 / 100U),
               (unsigned long)(gain_i_x100 % 100U),
               (unsigned long)(amp_x100 / 100U),
               (unsigned long)(amp_x100 % 100U));
}

static void StartPage1Query(void)
{
    if (g_page1_query_state != PAGE1_QUERY_IDLE)
    {
        return;
    }

    g_page1_query_state = PAGE1_QUERY_WAIT_CELLS;
    g_page1_query_deadline = tx_time_get() + PAGE1_QUERY_TIMEOUT_TICKS;
    TjcHmi::SendCmd(CMD_GET_PAGE1_CELLS);  /* 电池节数 */
}

static void ContinuePage1QueryAfterNumber(uint32_t value)
{
    if (g_page1_query_state == PAGE1_QUERY_WAIT_CELLS)
    {
        g_page1_params.cells = value;
        g_page1_cells_valid = true;
        g_page1_last_update_tick = tx_time_get();
        BSP_Printf(COM_DEBUG, "[HMI] Page1 ret cells=%lu\r\n", (unsigned long)value);
        g_page1_query_state = PAGE1_QUERY_WAIT_CAPACITY;
        g_page1_query_deadline = tx_time_get() + PAGE1_QUERY_TIMEOUT_TICKS;
        TjcHmi::SendCmd(CMD_GET_PAGE1_CAPACITY); /* 电池容量 */
        return;
    }

    if (g_page1_query_state == PAGE1_QUERY_WAIT_CAPACITY)
    {
        g_page1_params.capacity_mah = value;
        g_page1_capacity_valid = true;
        g_page1_last_update_tick = tx_time_get();
        BSP_Printf(COM_DEBUG, "[HMI] Page1 ret capacity=%lu\r\n", (unsigned long)value);
        g_page1_query_state = PAGE1_QUERY_WAIT_BRAND;
        g_page1_query_deadline = tx_time_get() + PAGE1_QUERY_TIMEOUT_TICKS;
        TjcHmi::SendCmd(CMD_GET_PAGE1_BRAND); /* 电池品牌 */
        return;
    }
}

static void FinishPage1QueryWithBrand(const uint8_t *data, uint16_t len)
{
    if (g_page1_query_state != PAGE1_QUERY_WAIT_BRAND)
    {
        return;
    }

    uint16_t copy_len = len;
    if (copy_len >= sizeof(g_page1_params.brand))
    {
        copy_len = (uint16_t)(sizeof(g_page1_params.brand) - 1U);
    }

    if ((copy_len > 0U) && (nullptr != data))
    {
        std::memcpy(g_page1_params.brand, data, copy_len);
    }
    g_page1_params.brand[copy_len] = '\0';
    g_page1_brand_valid = true;
    g_page1_last_update_tick = tx_time_get();
    BSP_Printf(COM_DEBUG, "[HMI] Page1 ret brand=%s\r\n", g_page1_params.brand);

    g_page1_query_state = PAGE1_QUERY_IDLE;
    UpdateGainProfileFromPage1();
}

static void HandlePage1StringResponse(const uint8_t *data, uint16_t len)
{
    if (g_page1_query_state == PAGE1_QUERY_WAIT_BRAND)
    {
        FinishPage1QueryWithBrand(data, len);
        return;
    }

    uint32_t num = 0U;
    if (ParseUintFromText(data, len, &num))
    {
        ContinuePage1QueryAfterNumber(num);
        return;
    }

    BSP_Printf(COM_DEBUG,
               "[HMI] Page1 text parse fail at state=%d\r\n",
               (int)g_page1_query_state);
    g_page1_query_state = PAGE1_QUERY_IDLE;
}

static void HandlePage1QueryTimeout(void)
{
    if (g_page1_query_state == PAGE1_QUERY_IDLE)
    {
        return;
    }

    ULONG now = tx_time_get();
    if (now < g_page1_query_deadline)
    {
        return;
    }

    BSP_Printf(COM_DEBUG,
               "[HMI] Page1 query timeout at state=%d rxfrm=%lu last=0x%02X\r\n",
               (int)g_page1_query_state,
               (unsigned long)g_hmi_rx_frame_count,
               (unsigned)g_hmi_last_frame_type);
    g_page1_query_state = PAGE1_QUERY_IDLE;
}

static void GenerateSinWave(uint8_t *out, uint16_t points)
{
    if (nullptr == out)
    {
        return;
    }

    float amp = WAVE_AMP_BASE * g_page1_params.wave_amp_scale;
    if (amp < WAVE_AMP_MIN) amp = WAVE_AMP_MIN;
    if (amp > WAVE_AMP_MAX) amp = WAVE_AMP_MAX;

    for (uint16_t i = 0; i < points; ++i)
    {
        float v = (std::sin((float)(i + 1U) * 3.1415926f / 50.0f) + 1.0f) * amp;
        int n = (int)v;
        if (n < 0) n = 0;
        if (n > 255) n = 255;
        out[i] = (uint8_t)n;
    }
}

static void DoDynamicWave(void)
{
    if (!ScreenLock()) return;
    TjcHmi::ClearWave(WAVE_COMP_ID, WAVE_CH);
    GenerateSinWave(g_wave_buf, WAVE_POINTS);
    for (uint16_t i = 0; i < WAVE_POINTS; ++i)
    {
        TjcHmi::AddWave(WAVE_COMP_ID, WAVE_CH, g_wave_buf[i]);
    }
    ScreenUnlock();
}

static void DoStaticWave(void)
{
    if (!ScreenLock()) return;
    TjcHmi::ClearWave(WAVE_COMP_ID, WAVE_CH);
    GenerateSinWave(g_wave_buf, WAVE_POINTS);
    TjcHmi::AddWaveFast(WAVE_COMP_ID, WAVE_CH, g_wave_buf, WAVE_POINTS);
    ScreenUnlock();
}

static void DoClearWave(void)
{
    if (!ScreenLock()) return;
    TjcHmi::ClearWave(WAVE_COMP_ID, WAVE_CH);
    ScreenUnlock();
}

enum class Level3 : uint8_t
{
    LOW = 0U,
    NORMAL,
    HIGH,
};

static Level3 ClassifyROhm(float r_ohm)
{
    if (r_ohm < R_OHM_LOW_TH) return Level3::LOW;
    if (r_ohm > R_OHM_HIGH_TH) return Level3::HIGH;
    return Level3::NORMAL;
}

static Level3 ClassifyRCt(float r_ct)
{
    if (r_ct < R_CT_LOW_TH) return Level3::LOW;
    if (r_ct > R_CT_HIGH_TH) return Level3::HIGH;
    return Level3::NORMAL;
}

static void HighlightTriState(const char *low_id, const char *normal_id, const char *high_id, Level3 level)
{
    TjcHmi::SetColor(low_id, COLOR_BLACK);
    TjcHmi::SetColor(normal_id, COLOR_BLACK);
    TjcHmi::SetColor(high_id, COLOR_BLACK);

    if (level == Level3::LOW)
    {
        TjcHmi::SetColor(low_id, COLOR_RED);
    }
    else if (level == Level3::NORMAL)
    {
        TjcHmi::SetColor(normal_id, COLOR_GREEN);
    }
    else
    {
        TjcHmi::SetColor(high_id, COLOR_RED);
    }
}

static int CalcScoreFromResistance(float r_ohm, float r_ct)
{
    int penalty = 0;

    if (r_ohm < R_OHM_LOW_TH) penalty += PENALTY_R_OHM_LOW;
    else if (r_ohm > R_OHM_HIGH_TH) penalty += PENALTY_R_OHM_HIGH;

    if (r_ct < R_CT_LOW_TH) penalty += PENALTY_R_CT_LOW;
    else if (r_ct > R_CT_HIGH_TH) penalty += PENALTY_R_CT_HIGH;

    int score = 100 - penalty;
    if (score < 0) score = 0;
    return score;
}

static void HighlightHealthLevelByScore(int score)
{
    static const char * const levels[] =
    {
        OBJ_PAGE4_LV_EXCELLENT, /* 优秀 */
        OBJ_PAGE4_LV_GOOD,      /* 良好 */
        OBJ_PAGE4_LV_NORMAL,    /* 一般 */
        OBJ_PAGE4_LV_WEAK,      /* 较差 */
        OBJ_PAGE4_LV_BAD,       /* 劣质 */
    };

    for (uint32_t i = 0; i < (sizeof(levels) / sizeof(levels[0])); ++i)
    {
        TjcHmi::SetColor(levels[i], COLOR_BLACK);
    }

    if (score >= SCORE_EXCELLENT_TH) TjcHmi::SetColor(OBJ_PAGE4_LV_EXCELLENT, COLOR_GREEN);
    else if (score >= SCORE_GOOD_TH) TjcHmi::SetColor(OBJ_PAGE4_LV_GOOD, COLOR_GREEN);
    else if (score >= SCORE_NORMAL_TH) TjcHmi::SetColor(OBJ_PAGE4_LV_NORMAL, COLOR_YELLOW);
    else if (score >= SCORE_WEAK_TH) TjcHmi::SetColor(OBJ_PAGE4_LV_WEAK, COLOR_RED);
    else TjcHmi::SetColor(OBJ_PAGE4_LV_BAD, COLOR_RED);
}

static const char *TouchEventToStr(uint8_t event)
{
    if (event == 0x01U) return "Pressed";
    if (event == 0x00U) return "Released";
    return "Unknown";
}

static const char *Page0CmpName(uint8_t cmp)
{
    if (cmp == HOME_BTN_TO_PAGE1) return "电池类型设定";
    if (cmp == HOME_BTN_TO_PAGE2) return "尼奎斯特图";
    if (cmp == HOME_BTN_TO_PAGE3) return "波特图";
    if (cmp == HOME_BTN_TO_PAGE4) return "健康度评估";
    return "UnknownCmp";
}

static const char *Page1CmpName(uint8_t cmp)
{
    if (cmp == 0U) return "电池节数";
    if (cmp == 1U) return "电池品牌";
    if (cmp == 2U) return "电池容量";
    if (cmp == PAGE1_BTN_HOME) return "主页返回";
    return "UnknownCmp";
}

static void OnPageChanged(uint8_t new_page)
{
    if (g_current_page == new_page)
    {
        return;
    }

    g_current_page = new_page;
    BSP_Printf(COM_DEBUG, "[HMI] page changed -> %u\r\n", (unsigned)new_page);

    if (new_page == PAGE_PARAM)
    {
        StartPage1Query();
    }
    else if (new_page == PAGE_SOH)
    {
        App_UpdateHealthResult(g_latest_r_ohm, g_latest_r_ct, 0);
    }
}

static void HandleTouchEvent(uint8_t page, uint8_t cmp, uint8_t event)
{
    OnPageChanged(page);

    if (page == PAGE_HOME)
    {
        BSP_Printf(COM_DEBUG, "[HMI] P0 touch cmp=%u(%s) event=%s\r\n",
                   (unsigned)cmp, Page0CmpName(cmp), TouchEventToStr(event));
    }
    else if (page == PAGE_PARAM)
    {
        BSP_Printf(COM_DEBUG, "[HMI] P1 touch cmp=%u(%s) event=%s\r\n",
                   (unsigned)cmp, Page1CmpName(cmp), TouchEventToStr(event));
    }

    if (event != EVT_PRESS)
    {
        return;
    }

    if (page == PAGE_HOME)
    {
        if (cmp == HOME_BTN_TO_PAGE1 || cmp == HOME_BTN_TO_PAGE2 ||
            cmp == HOME_BTN_TO_PAGE3 || cmp == HOME_BTN_TO_PAGE4)
        {
            TjcHmi::SendCmd(CMD_SENDME);
            if (cmp == HOME_BTN_TO_PAGE4)
            {
                App_UpdateHealthResult(g_latest_r_ohm, g_latest_r_ct, 0);
            }
        }
        return;
    }

    if (page == PAGE_PARAM)
    {
        if (cmp == PAGE1_BTN_HOME)
        {
            TjcHmi::SendCmd(CMD_SENDME);
            return;
        }

        /* Any setting click on page1 can change data, refresh params. */
        StartPage1Query();
        return;
    }

    if (page == PAGE_NYQ)
    {
        if (cmp == PAGE2_BTN_ADD) DoDynamicWave();  /* b1 添加波形 */
        else if (cmp == PAGE2_BTN_ADDT) DoStaticWave(); /* b2 静态波形 */
        else if (cmp == PAGE2_BTN_CLR) DoClearWave();  /* b3 清空 */
        else if (cmp == PAGE2_BTN_HOME) { TjcHmi::SendCmd(CMD_SENDME); } /* b0 Home */
        return;
    }

    if (page == PAGE_BODE)
    {
        if (cmp == PAGE3_BTN_ADD) DoDynamicWave();  /* b1 添加波形 */
        else if (cmp == PAGE3_BTN_ADDT) DoStaticWave(); /* b2 静态波形 */
        else if (cmp == PAGE3_BTN_CLR) DoClearWave();  /* b0 清空 */
        else if (cmp == PAGE3_BTN_HOME) { TjcHmi::SendCmd(CMD_SENDME); } /* b3 Home */
        return;
    }

    if (page == PAGE_SOH)
    {
        if (cmp == PAGE4_BTN_HOME)
        {
            TjcHmi::SendCmd(CMD_SENDME);
        }
    }
}

static void HandleFrameCompleted(const uint8_t *frame, uint16_t frame_len)
{
    if ((nullptr == frame) || (frame_len < 1U))
    {
        return;
    }

    uint8_t type = frame[0];
    g_hmi_rx_frame_count++;
    g_hmi_last_frame_type = type;

    if ((type == 0x65U) && (frame_len >= 4U))
    {
        HandleTouchEvent(frame[1], frame[2], frame[3]);
        return;
    }

    if ((type == 0x66U) && (frame_len >= 2U))
    {
        OnPageChanged(frame[1]);
        return;
    }

    if ((type == 0x71U) && (frame_len >= 5U))
    {
        uint32_t val = ((uint32_t)frame[1]) |
                       ((uint32_t)frame[2] << 8) |
                       ((uint32_t)frame[3] << 16) |
                       ((uint32_t)frame[4] << 24);
        if (g_page1_query_state != PAGE1_QUERY_IDLE)
        {
            ContinuePage1QueryAfterNumber(val);
            return;
        }

        if (val <= (uint32_t)PAGE_SOH)
        {
            OnPageChanged((uint8_t)val);
            return;
        }
        return;
    }

    if ((type == 0x70U) && (frame_len >= 1U))
    {
        const uint8_t *payload = &frame[1];
        uint16_t payload_len = (uint16_t)(frame_len - 1U);
        LogPage1TextFrame(payload, payload_len);
        HandlePage1StringResponse(payload, payload_len);
        return;
    }
}

static void FeedRxByte(uint8_t b)
{
    if (g_rx_len >= sizeof(g_rx_frame))
    {
        g_rx_len = 0U;
        g_ff_count = 0U;
    }

    g_rx_frame[g_rx_len++] = b;

    if (b == 0xFFU)
    {
        g_ff_count++;
        if ((g_ff_count == 3U) && (g_rx_len >= 4U))
        {
            uint16_t frame_len = (uint16_t)(g_rx_len - 3U);
            HandleFrameCompleted(g_rx_frame, frame_len);
            g_rx_len = 0U;
            g_ff_count = 0U;
        }
    }
    else
    {
        g_ff_count = 0U;
    }
}

} /* namespace */

void App_UartScreen_Init(void)
{
    if (g_screen_inited)
    {
        return;
    }

    tx_mutex_create(&g_screen_mutex, (CHAR *)"UART_SCREEN_M", TX_INHERIT);
    TjcHmi::Init(COM_TJC);

    g_current_page = PAGE_INVALID;
    g_page1_query_state = PAGE1_QUERY_IDLE;
    g_page1_cells_valid = false;
    g_page1_capacity_valid = false;
    g_page1_brand_valid = false;
    g_page1_last_update_tick = 0U;
    g_last_page_probe_tick = 0U;
    g_hmi_rx_frame_count = 0U;
    g_hmi_last_frame_type = 0U;
    g_rx_len = 0U;
    g_ff_count = 0U;

    /* Ask HMI to return command status for debug stage. */
    TjcHmi::SendCmd(CMD_BKCMD_VERBOSE);

    /* Ask HMI to report current page once at startup. */
    TjcHmi::SendCmd(CMD_SENDME);
    TjcHmi::SendCmd(CMD_GET_CUR_PAGE);

    g_screen_inited = true;
}

void App_UartScreen_Task(void)
{
    if (!g_screen_inited)
    {
        return;
    }

    uint8_t rx_byte = 0U;
    if (FSP_SUCCESS == BSP_Serial_Read(COM_TJC, &rx_byte, 1U))
    {
        FeedRxByte(rx_byte);
    }

    ULONG now = tx_time_get();
    if (((g_current_page == PAGE_INVALID) || ((now - g_last_page_probe_tick) >= PAGE_PROBE_PERIOD_TICKS)) &&
        (g_page1_query_state == PAGE1_QUERY_IDLE))
    {
        TjcHmi::SendCmd(CMD_GET_CUR_PAGE);
        g_last_page_probe_tick = now;
    }

    HandlePage1QueryTimeout();
}

void App_UartScreen_RequestPage1Refresh(void)
{
    if (!g_screen_inited)
    {
        return;
    }

    if ((g_current_page != PAGE_PARAM) && (g_current_page != PAGE_INVALID))
    {
        return;
    }

    StartPage1Query();
}

void App_UartScreen_DumpPage1(void)
{
    uint32_t gain_v_x100 = FloatToX100(g_page1_params.gain_voltage);
    uint32_t gain_i_x100 = FloatToX100(g_page1_params.gain_current);
    uint32_t amp_x100 = FloatToX100(g_page1_params.wave_amp_scale);

    BSP_Printf(COM_DEBUG,
               "[HMI] P1 dump: st=%d pg=%u cells=%lu(v=%u) cap=%lu(v=%u) brand=%s(v=%u)\r\n",
               (int)g_page1_query_state,
               (unsigned)g_current_page,
               (unsigned long)g_page1_params.cells,
               (unsigned)g_page1_cells_valid,
               (unsigned long)g_page1_params.capacity_mah,
               (unsigned)g_page1_capacity_valid,
               g_page1_params.brand,
               (unsigned)g_page1_brand_valid);

    BSP_Printf(COM_DEBUG,
               "[HMI] P1 gain: V=%lu.%02lu I=%lu.%02lu A=%lu.%02lu tick=%lu rxfrm=%lu last=0x%02X\r\n",
               (unsigned long)(gain_v_x100 / 100U),
               (unsigned long)(gain_v_x100 % 100U),
               (unsigned long)(gain_i_x100 / 100U),
               (unsigned long)(gain_i_x100 % 100U),
               (unsigned long)(amp_x100 / 100U),
               (unsigned long)(amp_x100 % 100U),
               (unsigned long)g_page1_last_update_tick,
               (unsigned long)g_hmi_rx_frame_count,
               (unsigned)g_hmi_last_frame_type);
}

void App_UpdateHealthResult(float r_ohm, float r_ct, int health_score)
{
    (void)health_score;

    g_latest_r_ohm = r_ohm;
    g_latest_r_ct = r_ct;

    if (!ScreenLock()) return;

    /* 1) Display two resistance values. */
    TjcHmi::SetFloat(OBJ_PAGE4_R_OHM_VAL, r_ohm, PAGE4_VALUE_DECIMALS);
    TjcHmi::SetFloat(OBJ_PAGE4_R_CT_VAL, r_ct, PAGE4_VALUE_DECIMALS);

    /* 2) Top tri-state highlight by each resistance. */
    HighlightTriState(OBJ_PAGE4_R_OHM_LOW, OBJ_PAGE4_R_OHM_NORMAL, OBJ_PAGE4_R_OHM_HIGH, ClassifyROhm(r_ohm));
    HighlightTriState(OBJ_PAGE4_R_CT_LOW, OBJ_PAGE4_R_CT_NORMAL, OBJ_PAGE4_R_CT_HIGH, ClassifyRCt(r_ct));

    /* 3) Bottom health grade highlight by combined score. */
    int score = CalcScoreFromResistance(r_ohm, r_ct);
    HighlightHealthLevelByScore(score);

    ScreenUnlock();

    int32_t r_ohm_x100 = (int32_t)(r_ohm * 100.0f);
    int32_t r_ct_x100 = (int32_t)(r_ct * 100.0f);
    BSP_Printf(COM_DEBUG,
               "[HMI] Page4 update: r_ohm=%ld.%02ld r_ct=%ld.%02ld score=%d dec=%u\r\n",
               (long)(r_ohm_x100 / 100),
               (long)(r_ohm_x100 >= 0 ? (r_ohm_x100 % 100) : -(r_ohm_x100 % 100)),
               (long)(r_ct_x100 / 100),
               (long)(r_ct_x100 >= 0 ? (r_ct_x100 % 100) : -(r_ct_x100 % 100)),
               score,
               (unsigned)PAGE4_VALUE_DECIMALS);
}

void App_DrawEISWaveforms(uint8_t* nyquist_data, uint8_t* bode_data, uint16_t points)
{
    if (!ScreenLock()) return;

    const uint8_t *sel = nyquist_data;
    if ((g_current_page == PAGE_BODE) && (nullptr != bode_data))
    {
        sel = bode_data;
    }
    else if ((g_current_page == PAGE_NYQ) && (nullptr != nyquist_data))
    {
        sel = nyquist_data;
    }

    if ((nullptr != sel) && (points > 0U))
    {
        TjcHmi::ClearWave(1U, 0U);
        TjcHmi::AddWaveFast(1U, 0U, sel, points);
    }

    ScreenUnlock();
}
