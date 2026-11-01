// HardwareMock.cpp – Mock adapter implementations [ALL SIMULATED]
// THERMATRIX C++17 backend

#include "HardwareMock.h"

#include <cmath>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <thread>

namespace thermatrix {
namespace hw {

// ─── utility ─────────────────────────────────────────────────────────────────

static std::string isoNow() {
    auto now  = std::chrono::system_clock::now();
    auto tt   = std::chrono::system_clock::to_time_t(now);
    std::tm buf{};
#ifdef _WIN32
    gmtime_s(&buf, &tt);
#else
    gmtime_r(&tt, &buf);
#endif
    std::ostringstream oss;
    oss << std::put_time(&buf, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str() + " [SIMULATED]";
}

// ─── base MockDevice ─────────────────────────────────────────────────────────

bool MockDevice::discover() {
    // Always succeeds in simulation; real code would query USB/serial bus.
    return true; // [SIMULATED]
}

bool MockDevice::connect(int timeout_ms) {
    state_ = ConnectState::Connecting;
    ++connect_call_count_;

    // Simulate a recoverable timeout on the first connect attempt for JTAG
    if (type_ == DeviceType::JTAGAdapter && connect_call_count_ == 1) {
        // [SIMULATED] deliberate timeout to demonstrate recoverable failure
        std::this_thread::sleep_for(std::chrono::milliseconds(5)); // tiny sim delay
        state_       = ConnectState::Error;
        last_error_  = "[SIMULATED] Timeout connecting to JTAG adapter after " +
                       std::to_string(timeout_ms) + " ms. Retrying...";
        return false;
    }

    // Subsequent attempts succeed
    state_      = ConnectState::Connected;
    last_error_ = "";
    return true;
}

void MockDevice::disconnect() {
    state_      = ConnectState::Disconnected;
    last_error_ = "";
}

// ─── MCU mock ────────────────────────────────────────────────────────────────

std::vector<Capability> McuMock::capabilities() const {
    return {
        {"Type",         "ARM Cortex-M4 microcontroller"},
        {"Part",         "STM32F407VGT6 [SIMULATED]"},
        {"Flash",        "1 MB"},
        {"SRAM",         "192 KB"},
        {"Clock",        "168 MHz"},
        {"Host interface","USB Full-Speed (FS) via USB-CDC"},
        {"Voltage",      "3.3 V core / 5 V USB"},
        {"JTAG pins",    "TDI, TDO, TCK, TMS, TRST, GND"},
        {"Simulation",   "All values synthetic"}
    };
}

std::optional<TelemetryMsg> McuMock::readTelemetry() {
    if (!isConnected()) return std::nullopt;
    // [SIMULATED] Drift temperature slowly
    sim_temp_ += 0.05 * (std::sin(sim_temp_ * 0.1) + 0.5);
    TelemetryMsg msg;
    msg.device_id = id_;
    msg.type      = "temperature";
    msg.value     = sim_temp_;
    msg.unit      = "°C";
    msg.timestamp = isoNow();
    return msg;
}

// ─── FPGA mock ───────────────────────────────────────────────────────────────

std::vector<Capability> FpgaMock::capabilities() const {
    return {
        {"Type",          "Xilinx Artix-7 FPGA"},
        {"Part",          "XC7A35T-1CPG236C [SIMULATED]"},
        {"Logic cells",   "33 280"},
        {"BRAM",          "1 800 Kb"},
        {"DSP slices",    "90"},
        {"Host interface","USB 3.0 via FT601 FIFO bridge"},
        {"JTAG interface","On-chip JTAG TAP (accessed via J-Link) [SIMULATED]"},
        {"Voltage",       "1.0 V core / 1.8 V IO / 3.3 V USB"},
        {"Simulation",    "All values synthetic"}
    };
}

std::optional<TelemetryMsg> FpgaMock::readTelemetry() {
    if (!isConnected()) return std::nullopt;
    // [SIMULATED] toggle voltage slightly
    sim_voltage_ = 1.0 + 0.01 * std::sin(sim_voltage_ * 3.0);
    TelemetryMsg msg;
    msg.device_id = id_;
    msg.type      = "core_voltage";
    msg.value     = sim_voltage_;
    msg.unit      = "V";
    msg.timestamp = isoNow();
    return msg;
}

// ─── JTAG adapter mock ───────────────────────────────────────────────────────

std::vector<Capability> JTagMock::capabilities() const {
    return {
        {"Type",           "JTAG / SWD Debug Adapter"},
        {"Part",           "SEGGER J-Link BASE [SIMULATED]"},
        {"Protocol",       "IEEE 1149.1 JTAG + SWD"},
        {"Speed",          "Up to 50 MHz adaptive clocking [SIMULATED]"},
        {"Host interface", "USB 2.0 HID"},
        {"Voltage",        "1.2–5 V target, self-powered via USB"},
        {"JTAG signals",   "TCK (output), TMS (output), TDI (output), TDO (input), TRST# (output)"},
        {"Level shifting", "On-board level shifters for 1.8 V / 3.3 V targets"},
        {"Why not compute","JTAG is a debug/programming interface (IEEE 1149.1). "
                           "It provides boundary scan, halt/step, register and memory "
                           "access. Its maximum bandwidth is ~several MB/s and protocol "
                           "overhead is high; it cannot substitute for a compute "
                           "interconnect such as USB, SPI, or PCIe."},
        {"Simulation",     "All values synthetic"}
    };
}

std::optional<TelemetryMsg> JTagMock::readTelemetry() {
    if (!isConnected()) return std::nullopt;
    TelemetryMsg msg;
    msg.device_id = id_;
    msg.type      = "jtag_status";
    msg.value     = 1.0; // 1 = TAP in IDLE state [SIMULATED]
    msg.unit      = "(TAP state: Run-Test/Idle)";
    msg.timestamp = isoNow();
    return msg;
}

// ─── hardware manager ────────────────────────────────────────────────────────

HardwareMgr::HardwareMgr() {}

std::string HardwareMgr::connectAll() {
    std::ostringstream log;
    log << "[SIMULATED] Connecting to hardware devices...\n";

    for (auto* dev : std::initializer_list<MockDevice*>{&mcu_, &fpga_, &jtag_}) {
        bool disc = dev->discover();
        log << "  " << dev->id() << ": discover=" << (disc ? "OK" : "FAIL") << "\n";

        bool ok = dev->connect();
        if (!ok) {
            // Recoverable: retry once
            log << "  " << dev->id() << ": connect FAILED – "
                << dev->lastError() << "\n";
            ok = dev->connect();
            log << "  " << dev->id() << ": retry " << (ok ? "SUCCEEDED" : "FAILED") << "\n";
        } else {
            log << "  " << dev->id() << ": connected [SIMULATED]\n";
        }
    }
    log_ = log.str();
    return log_;
}

} // namespace hw
} // namespace thermatrix
