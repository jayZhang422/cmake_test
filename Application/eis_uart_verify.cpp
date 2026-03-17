#include "eis_uart_verify.hpp"

#include "bsp_algorithm.hpp"
#include "dsp_preprocess.hpp"
#define EIS_SIGNAL_QUALITY_DEFINED
#include "afe_control.hpp"
#include "eis_safety_monitor.hpp"
#include "osl_calibration.hpp"
#include "bsp_usart.h"
#undef EIS_SIGNAL_QUALITY_DEFINED

#include <stdint.h>
#include <math.h>

namespace EIS
{
SafetyMonitor::ThresholdConfig SafetyMonitor::_cfg = {5.0f, 1.0f, 10.0f};
}

namespace
{
constexpr uint32_t EIS_POINTS                = 1024U;
constexpr uint32_t EIS_PACKET_BYTES          = EIS_POINTS * 2U * sizeof(uint16_t);
constexpr float    EIS_SAMPLE_RATE_HZ        = 100000.0f;
constexpr uint32_t EIS_REFERENCE_CYCLES      = 10U;
constexpr float    EIS_REFERENCE_FREQ_HZ     = (EIS_SAMPLE_RATE_HZ * (float) EIS_REFERENCE_CYCLES) / (float) EIS_POINTS;
constexpr float    EXPECTED_IMPEDANCE_MAG    = (1000.0f / 1500.0f);
constexpr float    EXPECTED_IMPEDANCE_PHASE  = -45.0f;
constexpr float    PASS_MAG_ERR_THRESHOLD    = 0.01f;
constexpr float    PASS_PHASE_ERR_THRESHOLD  = 1.0f;
constexpr float    ADC_VREF_V                = 3.3f;
constexpr float    ADC_PGA_GAIN              = 1.0f;
constexpr float    SCALE_U                   = 1000000.0f;
constexpr float    SCALE_MDEG                = 1000.0f;
constexpr float    FFT_RATIO_PASS_THRESHOLD  = 20.0f;

alignas(4) static uint8_t  g_packet_buf[EIS_PACKET_BYTES];
alignas(4) static uint16_t g_voltage_raw[EIS_POINTS];
alignas(4) static uint16_t g_current_raw[EIS_POINTS];
alignas(4) static float    g_voltage_signal[EIS_POINTS];
alignas(4) static float    g_current_signal[EIS_POINTS];
alignas(4) static float    g_ref_sin[EIS_POINTS];
alignas(4) static float    g_ref_cos[EIS_POINTS];
alignas(4) static float    g_fft_input[EIS_POINTS];
alignas(4) static float    g_fft_mag[EIS_POINTS / 2U];

static uint16_t load_u16_le(const uint8_t * p_data)
{
    return (uint16_t) p_data[0] | (uint16_t) ((uint16_t) p_data[1] << 8);
}

static void decode_packet(const uint8_t * p_packet)
{
    const uint8_t * p_voltage = p_packet;
    const uint8_t * p_current = p_packet + (EIS_POINTS * sizeof(uint16_t));

    for (uint32_t i = 0; i < EIS_POINTS; ++i)
    {
        g_voltage_raw[i] = load_u16_le(&p_voltage[i * 2U]);
        g_current_raw[i] = load_u16_le(&p_current[i * 2U]);
    }
}

static float absf(float value)             
{
    return (value < 0.0f) ? -value : value;
}

static float normalize_phase_deg(float phase_deg)
{
    float result = phase_deg;
    while (result > 180.0f)
    {
        result -= 360.0f;
    }

    while (result < -180.0f)
    {
        result += 360.0f;
    }

    return result;
}

static long to_fixed(float value, float scale)
{
    if (value >= 0.0f)
    {
        return (long) (value * scale + 0.5f);
    }
    return (long) (value * scale - 0.5f);
}

static void run_safety_validation(float v_dc, float i_rms, const ImpedanceResult * p_z_result)
{
    EIS::SafetyMonitor::ThresholdConfig cfg = {5.0f, 1.0f, 10.0f};
    EIS::SafetyMonitor::Init(cfg);

    EIS::SafetyStatus_e td_state = EIS::SafetyMonitor::CheckTimeDomain(v_dc, i_rms);
    EIS::SafetyStatus_e zi_state = EIS::SafetyMonitor::CheckImpedance(p_z_result);

    uint32_t chk_over_voltage = (EIS::SafetyMonitor::CheckTimeDomain(6.0f, 0.1f) == EIS::SAFETY_ERR_OVER_VOLTAGE) ? 1U : 0U;
    uint32_t chk_over_current = (EIS::SafetyMonitor::CheckTimeDomain(1.0f, 2.0f) == EIS::SAFETY_ERR_OVER_CURRENT) ? 1U : 0U;

    ImpedanceResult z_disconnect = {0.0f, 0.0f, 20.0f, 0.0f};
    uint32_t chk_disconnect = (EIS::SafetyMonitor::CheckImpedance(&z_disconnect) == EIS::SAFETY_ERR_PROBE_DISCONNECT) ? 1U : 0U;

    ImpedanceResult z_nan = {NAN, 0.0f, NAN, 0.0f};
    uint32_t chk_nan = (EIS::SafetyMonitor::CheckImpedance(&z_nan) == EIS::SAFETY_ERR_MATH_NAN) ? 1U : 0U;

    uint32_t pass = ((td_state == EIS::SAFETY_OK) &&
                     (zi_state == EIS::SAFETY_OK) &&
                     (chk_over_voltage == 1U) &&
                     (chk_over_current == 1U) &&
                     (chk_disconnect == 1U) &&
                     (chk_nan == 1U)) ? 1U : 0U;

    BSP_Printf(COM_DEBUG,
               "EIS_SAFE,Td=%d,Zi=%d,Ov=%lu,Oc=%lu,Od=%lu,Nn=%lu,Ps=%lu\r\n",
               (int) td_state,
               (int) zi_state,
               (unsigned long) chk_over_voltage,
               (unsigned long) chk_over_current,
               (unsigned long) chk_disconnect,
               (unsigned long) chk_nan,
               (unsigned long) pass);
}

static void run_agc_validation(void)
{
    uint32_t pass = 1U;
    uint8_t gain_idx = 0U;

    EIS::AgcStateMachine agc_seq;
    agc_seq.Init(0U, 3U, 1U, 2U);

    EIS::AgcState_e s0 = agc_seq.Process(EIS::SIGNAL_OK, &gain_idx);
    pass &= ((s0 == EIS::AGC_STATE_STABLE) && (gain_idx == 1U)) ? 1U : 0U;

    EIS::AgcState_e s1 = agc_seq.Process(EIS::SIGNAL_CLIPPED, &gain_idx);
    pass &= ((s1 == EIS::AGC_STATE_JUST_CHANGED) && (gain_idx == 0U)) ? 1U : 0U;

    EIS::AgcState_e s2 = agc_seq.Process(EIS::SIGNAL_OK, &gain_idx);
    pass &= ((s2 == EIS::AGC_STATE_BLANKING) && (gain_idx == 0U)) ? 1U : 0U;

    EIS::AgcState_e s3 = agc_seq.Process(EIS::SIGNAL_OK, &gain_idx);
    pass &= ((s3 == EIS::AGC_STATE_BLANKING) && (gain_idx == 0U)) ? 1U : 0U;

    EIS::AgcState_e s4 = agc_seq.Process(EIS::SIGNAL_TOO_WEAK, &gain_idx);
    pass &= ((s4 == EIS::AGC_STATE_JUST_CHANGED) && (gain_idx == 1U)) ? 1U : 0U;

    EIS::AgcStateMachine agc_limit_low;
    agc_limit_low.Init(0U, 2U, 0U, 1U);
    EIS::AgcState_e l0 = agc_limit_low.Process(EIS::SIGNAL_CLIPPED, &gain_idx);
    pass &= ((l0 == EIS::AGC_STATE_LIMIT_WARNING) && (gain_idx == 0U)) ? 1U : 0U;

    EIS::AgcStateMachine agc_limit_high;
    agc_limit_high.Init(0U, 2U, 2U, 1U);
    EIS::AgcState_e l1 = agc_limit_high.Process(EIS::SIGNAL_TOO_WEAK, &gain_idx);
    pass &= ((l1 == EIS::AGC_STATE_LIMIT_WARNING) && (gain_idx == 2U)) ? 1U : 0U;

    BSP_Printf(COM_DEBUG,
               "EIS_AGC,S0=%d,S1=%d,S2=%d,S3=%d,S4=%d,L0=%d,L1=%d,Ps=%lu\r\n",
               (int) s0,
               (int) s1,
               (int) s2,
               (int) s3,
               (int) s4,
               (int) l0,
               (int) l1,
               (unsigned long) pass);
}

static void run_osl_validation(void)
{
    EIS::CalibPoint table[2] = {
        {900.0f, 1.0f, 0.0f, 100.0f, 0.0f},
        {1100.0f, 3.0f, 0.0f, 200.0f, 0.0f}
    };

    EIS::OslCalibration osl;
    osl.Init(table, 2U);

    float real_part = 12.0f;
    float imag_part = 0.0f;
    osl.ApplyCompensation(1000.0f, &real_part, &imag_part);

    const float expected_real = 10.7142857f;
    const float expected_imag = 0.0f;
    float err_real = absf(real_part - expected_real);
    float err_imag = absf(imag_part - expected_imag);
    float max_err = (err_real > err_imag) ? err_real : err_imag;
    uint32_t pass = (max_err <= 0.0005f) ? 1U : 0U;

    BSP_Printf(COM_DEBUG,
               "EIS_OSL,Ru=%ld,Xu=%ld,Eu=%ld,Ps=%lu\r\n",
               to_fixed(real_part, SCALE_U),
               to_fixed(imag_part, SCALE_U),
               to_fixed(max_err, SCALE_U),
               (unsigned long) pass);
}

static void run_fft_validation(void)
{
    static StaticFFT<EIS_POINTS> fft_engine;

    EisRefGenerator::Generate(g_fft_input, EIS_POINTS, EIS_REFERENCE_FREQ_HZ, EIS_SAMPLE_RATE_HZ, BSP_ALG_SIN);
    fft_engine.Execute(g_fft_input, g_fft_mag);

    uint32_t peak_idx = 1U;
    uint32_t second_idx = 1U;
    float peak_val = g_fft_mag[1U];
    float second_val = 0.0f;

    for (uint32_t i = 2U; i < (EIS_POINTS / 2U); ++i)
    {
        float value = g_fft_mag[i];
        if (value > peak_val)
        {
            second_val = peak_val;
            second_idx = peak_idx;
            peak_val = value;
            peak_idx = i;
        }
        else if (value > second_val)
        {
            second_val = value;
            second_idx = i;
        }
    }

    float ratio = peak_val / ((second_val > 1e-12f) ? second_val : 1e-12f);
    float ratio_limited = (ratio > 999999.0f) ? 999999.0f : ratio;
    uint32_t pass = ((peak_idx == EIS_REFERENCE_CYCLES) && (ratio >= FFT_RATIO_PASS_THRESHOLD)) ? 1U : 0U;

    BSP_Printf(COM_DEBUG,
               "EIS_FFT,Pk=%lu,Sk=%lu,Rm=%ld,Ps=%lu\r\n",
               (unsigned long) peak_idx,
               (unsigned long) second_idx,
               to_fixed(ratio_limited, 1000.0f),
               (unsigned long) pass);
}

static void run_eis_validation_once(void)
{
    float v_dc = 0.0f;
    float i_dc = 0.0f;

    bool v_clipped = EIS::DspPreprocess::ConvertRawToVoltage(g_voltage_raw,
                                                              g_voltage_signal,
                                                              EIS_POINTS,
                                                              ADC_VREF_V,
                                                              ADC_PGA_GAIN);
    bool i_clipped = EIS::DspPreprocess::ConvertRawToVoltage(g_current_raw,
                                                              g_current_signal,
                                                              EIS_POINTS,
                                                              ADC_VREF_V,
                                                              ADC_PGA_GAIN);

    EIS::DspPreprocess::RemoveDcOffset(g_voltage_signal, EIS_POINTS, &v_dc);
    EIS::DspPreprocess::RemoveDcOffset(g_current_signal, EIS_POINTS, &i_dc);

    float v_rms = EIS::DspPreprocess::CalculateRMS(g_voltage_signal, EIS_POINTS);
    float i_rms = EIS::DspPreprocess::CalculateRMS(g_current_signal, EIS_POINTS);

    EIS::SignalQuality_e v_quality = EIS::DspPreprocess::EvaluateSignalQuality(v_clipped, v_rms, ADC_PGA_GAIN);
    EIS::SignalQuality_e i_quality = EIS::DspPreprocess::EvaluateSignalQuality(i_clipped, i_rms, ADC_PGA_GAIN);

    EisRefGenerator::Generate(g_ref_sin, EIS_POINTS, EIS_REFERENCE_FREQ_HZ, EIS_SAMPLE_RATE_HZ, BSP_ALG_SIN);
    EisRefGenerator::Generate(g_ref_cos, EIS_POINTS, EIS_REFERENCE_FREQ_HZ, EIS_SAMPLE_RATE_HZ, BSP_ALG_COS);

    float vr = DigitalLockIn::Demodulate(g_voltage_signal, g_ref_sin, EIS_POINTS);
    float vi = DigitalLockIn::Demodulate(g_voltage_signal, g_ref_cos, EIS_POINTS);
    float ir = DigitalLockIn::Demodulate(g_current_signal, g_ref_sin, EIS_POINTS);
    float ii = DigitalLockIn::Demodulate(g_current_signal, g_ref_cos, EIS_POINTS);

    ImpedanceResult z_result = {0.0f, 0.0f, 0.0f, 0.0f};
    ImpedanceSolver::Calculate(vr, vi, ir, ii, &z_result);

    float mag_err_abs    = absf(z_result.Magnitude - EXPECTED_IMPEDANCE_MAG);
    float phase_err_deg  = normalize_phase_deg(z_result.Phase_deg - EXPECTED_IMPEDANCE_PHASE);
    float phase_err_abs  = absf(phase_err_deg);
    uint32_t pass_result = (mag_err_abs <= PASS_MAG_ERR_THRESHOLD && phase_err_abs <= PASS_PHASE_ERR_THRESHOLD) ? 1U : 0U;

    long vr_u      = to_fixed(vr, SCALE_U);
    long vi_u      = to_fixed(vi, SCALE_U);
    long ir_u      = to_fixed(ir, SCALE_U);
    long ii_u      = to_fixed(ii, SCALE_U);
    long mag_u     = to_fixed(z_result.Magnitude, SCALE_U);
    long phase_md  = to_fixed(z_result.Phase_deg, SCALE_MDEG);
    long magerr_u  = to_fixed(mag_err_abs, SCALE_U);
    long pherr_md  = to_fixed(phase_err_abs, SCALE_MDEG);
    long vdc_u     = to_fixed(v_dc, SCALE_U);
    long idc_u     = to_fixed(i_dc, SCALE_U);

    BSP_Printf(COM_DEBUG,
               "EIS_RES,Mu=%ld,Pm=%ld,Eu=%ld,Em=%ld,Vq=%d,Iq=%d,Ps=%lu\r\n",
               mag_u,
               phase_md,
               magerr_u,
               pherr_md,
               (int) v_quality,
               (int) i_quality,
               (unsigned long) pass_result);

    BSP_Printf(COM_DEBUG,
               "EIS_IQ,Vru=%ld,Viu=%ld,Iru=%ld,Iiu=%ld,Vdu=%ld,Idu=%ld\r\n",
               vr_u,
               vi_u,
               ir_u,
               ii_u,
               vdc_u,
               idc_u);

    run_safety_validation(v_dc, i_rms, &z_result);
    run_agc_validation();
    run_osl_validation();
    run_fft_validation();
}
} // namespace

extern "C" void Run_eis_uart_verify_server(void)
{
    uint32_t fs_hz = (uint32_t) (EIS_SAMPLE_RATE_HZ + 0.5f);
    uint32_t fref_mhz = (uint32_t) (EIS_REFERENCE_FREQ_HZ * 1000.0f + 0.5f);

    BSP_Serial_Init(COM_DEBUG);
    BSP_Printf(COM_DEBUG,
               "EIS UART verifier ready. N=%lu,Fs=%luHz,Fref_mHz=%lu,Packet=%lu bytes\r\n",
               (unsigned long) EIS_POINTS,
               (unsigned long) fs_hz,
               (unsigned long) fref_mhz,
               (unsigned long) EIS_PACKET_BYTES);

    while (1)
    {
        fsp_err_t err = BSP_Serial_Read(COM_DEBUG, g_packet_buf, EIS_PACKET_BYTES);
        if (FSP_SUCCESS != err)
        {
            // BSP_Printf(COM_DEBUG, "EIS_RX_ERR,%d\r\n", (int) err);
            // continue;
        }

        decode_packet(g_packet_buf);
        run_eis_validation_once();
    }
}
