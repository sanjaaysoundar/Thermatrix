#pragma once
// HardwareMock.h – Mock adapters for MCU, FPGA, and JTAG debug adapter
// THERMATRIX C++17 backend
//
// All behavior is SIMULATED. No real hardware is connected.
// Labels: [SIMULATED] or [MOCK] used throughout.
//
// Design rationale:
//   A modular host-connected workbench connects:
//     1. Microcontroller (MCU) – e.g. STM32F4 – for real-time sensor acquisition
//     2. FPGA – e.g. Xilinx Artix-7 – for parallel signal processing / matrix ops
//     3. JTAG debug adapter – e.g. SEGGER J-Link – for firmware programming/debug
//
// JTAG is a debug/programming interface (IEEE 1149.1), NOT a general compute
// interface. It provides boundary scan, breakpoint insertion, memory access, and
// register inspection. It cannot be used as a substitute for USB, SPI, or UART
// for streaming data; its bandwidth (~MHz) and protocol overhead make it
// unsuitable for real-time telemetry.
//
// Interface table is documented in docs/block_diagram.md.

#include <string>
#include <vector>
#include <functional>
#include <optional>
#include <chrono>

namespace thermatrix {
namespace hw {

// ─── device types ────────────────────────────────────────────────────────────

enum class DeviceType { MCU, FPGA, JTAGAdapter };

static inline const char* deviceTypeName(DeviceType t) {
    switch(t) {
        case DeviceType::MCU:         return "MCU (STM32F4) [SIMULATED]";
        case DeviceType::FPGA:        return "FPGA (Xilinx Artix-7) [SIMULATED]";
        case DeviceType::JTAGAdapter: return "JTAG Debug Adapter (SEGGER J-Link) [SIMULATED]";
    }
    return "Unknown";
}

enum class ConnectState { Disconnected, Connecting, Connected, Error };

static inline const char* connectStateName(ConnectState s) {
    switch(s) {
        case ConnectState::Disconnected: return "Disconnected";
        case ConnectState::Connecting:   return "Connecting...";
        case ConnectState::Connected:    return "Connected";
        case ConnectState::Error:        return "Error";
    }
    return "Unknown";
}

/// Telemetry message from a device [SIMULATED].
struct TelemetryMsg {
    std::string    device_id;
    std::string    type;          ///< e.g. "temperature", "voltage", "jtag_status"
    double         value;
    std::string    unit;
    std::string    timestamp;     ///< ISO-8601 string [SIMULATED]
};

/// Reported capability.
struct Capability {
    std::string name;
    std::string value;
};

// ─── base mock adapter ───────────────────────────────────────────────────────

class MockDevice {
public:
    explicit MockDevice(DeviceType type, const std::string& id)
        : type_(type), id_(id) {}
    virtual ~MockDevice() = default;

    DeviceType         type()  const { return type_; }
    const std::string& id()    const { return id_; }
    ConnectState       state() const { return state_; }
    const std::string& lastError() const { return last_error_; }

    /// Discover device (check if it would appear on the bus). [SIMULATED]
    virtual bool discover();

    /// Connect to device with optional timeout_ms. [SIMULATED]
    /// Returns true on success. May fail with timeout to demonstrate recoverable failure.
    virtual bool connect(int timeout_ms = 500);

    /// Disconnect. [SIMULATED]
    virtual void disconnect();

    /// Report capabilities. [SIMULATED]
    virtual std::vector<Capability> capabilities() const = 0;

    /// Read one telemetry message. [SIMULATED]
    virtual std::optional<TelemetryMsg> readTelemetry() = 0;

    bool isConnected() const { return state_ == ConnectState::Connected; }

protected:
    DeviceType   type_;
    std::string  id_;
    ConnectState state_{ConnectState::Disconnected};
    std::string  last_error_;
    int          connect_call_count_{0}; ///< Used to simulate intermittent failures
};

// ─── MCU mock ────────────────────────────────────────────────────────────────

class McuMock : public MockDevice {
public:
    McuMock() : MockDevice(DeviceType::MCU, "MCU-STM32F4-001") {}
    std::vector<Capability> capabilities() const override;
    std::optional<TelemetryMsg> readTelemetry() override;
private:
    double sim_temp_{25.0}; ///< Simulated sensor temperature [°C]
};

// ─── FPGA mock ───────────────────────────────────────────────────────────────

class FpgaMock : public MockDevice {
public:
    FpgaMock() : MockDevice(DeviceType::FPGA, "FPGA-ARTIX7-001") {}
    std::vector<Capability> capabilities() const override;
    std::optional<TelemetryMsg> readTelemetry() override;
private:
    double sim_voltage_{1.0}; ///< Simulated core voltage [V]
};

// ─── JTAG adapter mock ───────────────────────────────────────────────────────

class JTagMock : public MockDevice {
public:
    JTagMock() : MockDevice(DeviceType::JTAGAdapter, "JTAG-JLINK-001") {}
    std::vector<Capability> capabilities() const override;
    std::optional<TelemetryMsg> readTelemetry() override;
};

// ─── device manager ─────────────────────────────────────────────────────────

/// Manages all three mock devices.
class HardwareMgr {
public:
    HardwareMgr();

    McuMock&   mcu()  { return mcu_; }
    FpgaMock&  fpga() { return fpga_; }
    JTagMock&  jtag() { return jtag_; }

    /// Connect all devices, return summary.
    std::string connectAll();

    /// Log line for most recent operation.
    const std::string& lastLog() const { return log_; }

private:
    McuMock   mcu_;
    FpgaMock  fpga_;
    JTagMock  jtag_;
    std::string log_;
};

} // namespace hw
} // namespace thermatrix
