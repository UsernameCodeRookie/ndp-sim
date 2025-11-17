/**
 * @file main.cpp
 * @brief Event-driven RISC-V simulator with RVV backend support
 *
 * Highly parametrizable simulator that:
 * - Loads program definitions from external JSON files
 * - Supports both scalar (RV32I) and vector (RVV) instructions
 * - Executes via event-driven pipeline with full tracing
 * - Generates detailed statistics and performance analysis
 *
 * Usage:
 *   main --program <program.json>
 *   main --program programs/mac_example.json
 *   main --program programs/rvv_alu_example.json [--trace] [--verbose]
 *   main --help
 *
 * Program Definition Format (JSON):
 * {
 *   "name": "Program Name",
 *   "description": "...",
 *   "core_config": { ... },
 *   "vector_config": { ... },
 *   "memory_config": { ... },
 *   "simulation_config": { ... },
 *   "data_memory": [ ... ],
 *   "instructions": [ ... ]
 * }
 */

#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "comp/core/core.h"
#include "comp/rvv/rvv_backend.h"
#include "comp/rvv/rvv_interface.h"
#include "scheduler.h"
#include "trace.h"

using namespace Architecture;
using namespace EventDriven;

/**
 * @brief Simplified JSON parser for program definition
 *
 * Handles basic JSON parsing without external dependencies
 */
class SimpleJsonParser {
 public:
  static uint32_t parseHex(const std::string& hex_str) {
    // Handle 0x prefix
    std::string trimmed = hex_str;
    if (trimmed.length() >= 2 &&
        (trimmed.substr(0, 2) == "0x" || trimmed.substr(0, 2) == "0X")) {
      trimmed = trimmed.substr(2);
    }
    if (trimmed.empty()) return 0;
    try {
      return std::stoul(trimmed, nullptr, 16);
    } catch (...) {
      return 0;
    }
  }

  static int64_t parseInt(const std::string& num_str) {
    try {
      return std::stoll(num_str);
    } catch (...) {
      return 0;
    }
  }
};

/**
 * @brief Configuration loaded from JSON file
 */
struct ProgramConfig {
  std::string name;
  std::string description;

  // Scalar Core Configuration
  uint32_t num_instruction_lanes = 4;
  uint32_t num_registers = 32;
  uint32_t num_read_ports = 8;
  uint32_t num_write_ports = 6;
  bool use_regfile_forwarding = true;
  uint32_t alu_period = 1;
  uint32_t bru_period = 1;
  uint32_t num_bru_units = 4;
  uint32_t mlu_period = 3;
  uint32_t dvu_period = 8;
  uint32_t lsu_period = 2;

  // Vector Configuration
  bool enable_rvv = true;
  uint32_t vector_issue_width = 4;
  uint32_t vlen = 256;
  uint32_t vector_register_count = 32;

  // Memory Configuration
  uint32_t instruction_memory_size = 4096;
  uint32_t data_memory_size = 8192;

  // Simulation Configuration
  uint64_t max_cycles = 10000;
  bool enable_tracing = true;
  bool verbose = false;
  std::string trace_output = "trace.log";
  bool collect_statistics = true;

  // RVV Configuration
  uint16_t vl = 8;
  uint8_t sew = 0;
  uint8_t lmul = 0;

  // Instruction data
  std::vector<std::pair<uint32_t, uint32_t>> instructions;  // address, binary
  std::vector<std::pair<uint32_t, uint32_t>> data_memory;   // address, value
};

/**
 * @brief Load program configuration from JSON file
 */
class ProgramLoader {
 public:
  static ProgramConfig loadFromFile(const std::string& filename) {
    ProgramConfig config;

    std::ifstream file(filename);
    if (!file.is_open()) {
      throw std::runtime_error("Cannot open program file: " + filename);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json_content = buffer.str();

    // Simple manual JSON parsing
    parseJson(json_content, config);

    return config;
  }

 private:
  static void parseJson(const std::string& json, ProgramConfig& config) {
    // Parse name
    config.name = extractString(json, "\"name\"");
    config.description = extractString(json, "\"description\"");

    // Parse core_config
    auto core_config = extractObject(json, "core_config");
    if (!core_config.empty()) {
      config.num_instruction_lanes =
          extractInt(core_config, "num_instruction_lanes", 4);
      config.alu_period = extractInt(core_config, "alu_period", 1);
      config.bru_period = extractInt(core_config, "bru_period", 1);
      config.num_bru_units = extractInt(core_config, "num_bru_units", 4);
      config.mlu_period = extractInt(core_config, "mlu_period", 3);
      config.dvu_period = extractInt(core_config, "dvu_period", 8);
      config.lsu_period = extractInt(core_config, "lsu_period", 2);
    }

    // Parse vector_config
    auto vec_config = extractObject(json, "vector_config");
    if (!vec_config.empty()) {
      config.enable_rvv = extractBool(vec_config, "enable_rvv", true);
      config.vector_issue_width =
          extractInt(vec_config, "vector_issue_width", 4);
      config.vlen = extractInt(vec_config, "vlen", 256);
    }

    // Parse simulation_config
    auto sim_config = extractObject(json, "simulation_config");
    if (!sim_config.empty()) {
      config.max_cycles = extractInt(sim_config, "max_cycles", 10000);
      config.enable_tracing = extractBool(sim_config, "enable_tracing", true);
      config.verbose = extractBool(sim_config, "verbose", false);
      config.trace_output = extractString(sim_config, "\"trace_output\"");
    }

    // Parse rvv_config
    auto rvv_cfg = extractObject(json, "rvv_config");
    if (!rvv_cfg.empty()) {
      config.vl = extractInt(rvv_cfg, "vl", 8);
      config.sew = extractInt(rvv_cfg, "sew", 0);
      config.lmul = extractInt(rvv_cfg, "lmul", 0);
    }

    // Parse instructions
    parseInstructionsArray(json, config);

    // Parse data_memory
    parseDataMemoryArray(json, config);
  }

  static std::string extractString(const std::string& json,
                                   const std::string& key) {
    size_t pos = json.find(key);
    if (pos == std::string::npos) return "";

    // Find the colon
    size_t colon_pos = json.find(':', pos);
    if (colon_pos == std::string::npos) return "";

    // Find opening quote
    size_t quote_start = json.find('"', colon_pos);
    if (quote_start == std::string::npos) return "";

    // Find closing quote
    size_t quote_end = json.find('"', quote_start + 1);
    if (quote_end == std::string::npos) return "";

    return json.substr(quote_start + 1, quote_end - quote_start - 1);
  }

  static int64_t extractInt(const std::string& json, const std::string& key,
                            int64_t default_val) {
    size_t pos = json.find("\"" + key + "\"");
    if (pos == std::string::npos) return default_val;

    // Find colon
    size_t colon_pos = json.find(':', pos);
    if (colon_pos == std::string::npos) return default_val;

    // Extract number
    size_t num_start = colon_pos + 1;
    while (num_start < json.length() && std::isspace(json[num_start])) {
      num_start++;
    }

    size_t num_end = num_start;
    while (num_end < json.length() &&
           (std::isdigit(json[num_end]) || json[num_end] == '-')) {
      num_end++;
    }

    if (num_start >= json.length()) return default_val;

    try {
      return std::stoll(json.substr(num_start, num_end - num_start));
    } catch (...) {
      return default_val;
    }
  }

  static bool extractBool(const std::string& json, const std::string& key,
                          bool default_val) {
    size_t pos = json.find("\"" + key + "\"");
    if (pos == std::string::npos) return default_val;

    // Find colon
    size_t colon_pos = json.find(':', pos);
    if (colon_pos == std::string::npos) return default_val;

    // Look for true/false
    std::string rest = json.substr(colon_pos);
    if (rest.find("true") != std::string::npos) return true;
    if (rest.find("false") != std::string::npos) return false;

    return default_val;
  }

  static std::string extractObject(const std::string& json,
                                   const std::string& key) {
    size_t pos = json.find("\"" + key + "\"");
    if (pos == std::string::npos) return "";

    // Find opening brace
    size_t brace_start = json.find('{', pos);
    if (brace_start == std::string::npos) return "";

    // Find matching closing brace
    int depth = 0;
    for (size_t i = brace_start; i < json.length(); i++) {
      if (json[i] == '{') depth++;
      if (json[i] == '}') depth--;

      if (depth == 0) {
        return json.substr(brace_start, i - brace_start + 1);
      }
    }

    return "";
  }

  static void parseInstructionsArray(const std::string& json,
                                     ProgramConfig& config) {
    size_t pos = json.find("\"instructions\"");
    if (pos == std::string::npos) return;

    // Find opening bracket
    size_t bracket_start = json.find('[', pos);
    if (bracket_start == std::string::npos) return;

    // Find matching closing bracket by counting
    size_t bracket_end = bracket_start;
    int bracket_count = 0;
    for (size_t i = bracket_start; i < json.length(); i++) {
      if (json[i] == '[') bracket_count++;
      if (json[i] == ']') {
        bracket_count--;
        if (bracket_count == 0) {
          bracket_end = i;
          break;
        }
      }
    }
    if (bracket_end == bracket_start) return;

    std::string array_str =
        json.substr(bracket_start, bracket_end - bracket_start + 1);

    int inst_count = 0;
    // Parse each instruction object
    size_t obj_start = array_str.find('{');
    while (obj_start != std::string::npos) {
      size_t obj_end = array_str.find('}', obj_start);
      if (obj_end == std::string::npos) break;

      std::string obj = array_str.substr(obj_start, obj_end - obj_start + 1);

      // Check if it's a comment line (skip objects with only type:"comment")
      bool is_comment_only = (obj.find("\"type\"") != std::string::npos &&
                              obj.find("\"type\"") < obj.find("\"address\""));
      if (is_comment_only) {
        // Skip comment entries
        obj_start = array_str.find('{', obj_end + 1);
        continue;
      }

      // Extract address and binary
      uint32_t addr = 0;
      uint32_t binary = 0;

      // Parse address
      size_t addr_pos = obj.find("\"address\"");
      if (addr_pos != std::string::npos) {
        size_t colon_pos = obj.find(':', addr_pos);
        size_t comma_pos = obj.find(',', colon_pos);
        if (comma_pos == std::string::npos) {
          comma_pos = obj.find('}', colon_pos);
        }
        std::string addr_str =
            obj.substr(colon_pos + 1, comma_pos - colon_pos - 1);
        // Trim whitespace
        addr_str.erase(0, addr_str.find_first_not_of(" \t\n\r"));
        addr_str.erase(addr_str.find_last_not_of(" \t\n\r") + 1);
        try {
          addr = std::stoul(addr_str);
        } catch (...) {
        }
      }

      // Parse binary - handle "0x..." format
      size_t bin_pos = obj.find("\"binary\"");
      if (bin_pos != std::string::npos) {
        // Find the opening quote of the value
        size_t quote_start = obj.find('"', bin_pos + 9);  // Skip past "binary"
        if (quote_start != std::string::npos) {
          size_t quote_end = obj.find('"', quote_start + 1);
          if (quote_end != std::string::npos) {
            std::string bin_str =
                obj.substr(quote_start + 1, quote_end - quote_start - 1);
            binary = SimpleJsonParser::parseHex(bin_str);
          }
        }
      }

      if (addr != 0 || binary != 0) {
        config.instructions.push_back({addr, binary});
        inst_count++;
      }

      obj_start = array_str.find('{', obj_end + 1);
    }
  }

  static void parseDataMemoryArray(const std::string& json,
                                   ProgramConfig& config) {
    size_t pos = json.find("\"data_memory\"");
    if (pos == std::string::npos) return;

    size_t bracket_start = json.find('[', pos);
    if (bracket_start == std::string::npos) return;

    // Find matching closing bracket by counting
    size_t bracket_end = bracket_start;
    int bracket_count = 0;
    for (size_t i = bracket_start; i < json.length(); i++) {
      if (json[i] == '[') bracket_count++;
      if (json[i] == ']') {
        bracket_count--;
        if (bracket_count == 0) {
          bracket_end = i;
          break;
        }
      }
    }
    if (bracket_end == bracket_start) return;

    std::string array_str =
        json.substr(bracket_start, bracket_end - bracket_start + 1);

    // Parse each data object
    size_t obj_start = array_str.find('{');
    while (obj_start != std::string::npos) {
      size_t obj_end = array_str.find('}', obj_start);
      if (obj_end == std::string::npos) break;

      std::string obj = array_str.substr(obj_start, obj_end - obj_start + 1);

      // Extract address and values
      uint32_t addr = 0;
      std::vector<uint32_t> values;

      // Parse address - handle "0x000" or "0x010" format
      size_t addr_pos = obj.find("\"address\"");
      if (addr_pos != std::string::npos) {
        size_t colon_pos = obj.find(':', addr_pos);
        size_t quote_start = obj.find('"', colon_pos);
        size_t quote_end = obj.find('"', quote_start + 1);
        if (quote_start != std::string::npos &&
            quote_end != std::string::npos) {
          std::string addr_str =
              obj.substr(quote_start + 1, quote_end - quote_start - 1);
          try {
            if (addr_str.length() >= 2 && addr_str.substr(0, 2) == "0x") {
              addr = std::stoul(addr_str, nullptr, 16);
            } else {
              addr = std::stoul(addr_str);
            }
          } catch (...) {
            addr = 0;
          }
        }
      }

      // Parse values array
      size_t val_pos = obj.find("\"values\"");
      if (val_pos != std::string::npos) {
        size_t bracket_s = obj.find('[', val_pos);
        size_t bracket_e = obj.find(']', bracket_s);
        std::string val_str =
            obj.substr(bracket_s + 1, bracket_e - bracket_s - 1);

        size_t comma_pos = 0;
        while (comma_pos < val_str.length()) {
          size_t next_comma = val_str.find(',', comma_pos);
          if (next_comma == std::string::npos) next_comma = val_str.length();

          std::string num_str =
              val_str.substr(comma_pos, next_comma - comma_pos);
          num_str.erase(0, num_str.find_first_not_of(" \t\n\r"));
          num_str.erase(num_str.find_last_not_of(" \t\n\r") + 1);

          if (!num_str.empty()) {
            try {
              values.push_back(std::stoul(num_str));
            } catch (...) {
            }
          }

          comma_pos = next_comma + 1;
        }
      }

      // Add to config
      for (uint32_t val : values) {
        config.data_memory.push_back({addr, val});
        addr += 4;
      }

      obj_start = array_str.find('{', obj_end + 1);
    }
  }
};

/**
 * @brief Main event-driven simulator
 */
class RVVSimulator {
 public:
  explicit RVVSimulator(const ProgramConfig& config) : config_(config) {}

  void run() {
    printHeader();

    // Create scheduler
    auto scheduler = std::make_unique<EventScheduler>();

    // Setup tracing
    if (config_.enable_tracing) {
      EventDriven::Tracer::getInstance().initialize(config_.trace_output, true);
      EventDriven::Tracer::getInstance().setVerbose(config_.verbose);
      EventDriven::Tracer::getInstance().addComponentFilter("SCore");
      if (config_.enable_rvv) {
        EventDriven::Tracer::getInstance().addComponentFilter("RVV");
      }
    }

    // Create Scalar Core
    SCore::Config core_config;
    core_config.num_instruction_lanes = config_.num_instruction_lanes;
    core_config.num_registers = config_.num_registers;
    core_config.num_read_ports = config_.num_read_ports;
    core_config.num_write_ports = config_.num_write_ports;
    core_config.use_regfile_forwarding = config_.use_regfile_forwarding;
    core_config.alu_period = config_.alu_period;
    core_config.bru_period = config_.bru_period;
    core_config.num_bru_units = config_.num_bru_units;
    core_config.mlu_period = config_.mlu_period;
    core_config.dvu_period = config_.dvu_period;
    core_config.lsu_period = config_.lsu_period;
    core_config.vector_issue_width = config_.vector_issue_width;
    core_config.vlen = config_.vlen;

    auto core = std::make_shared<SCore>("SCore-0", *scheduler, core_config);

    // Create RVV Backend if enabled
    std::shared_ptr<RVVBackend> rvv_backend;
    if (config_.enable_rvv) {
      rvv_backend = std::make_shared<RVVBackend>("RVV-Backend", *scheduler, 1,
                                                 config_.vlen);
      core->setRVVInterface(rvv_backend);

      // Configure RVV
      RVVConfigState rvv_config;
      rvv_config.vl = config_.vl;
      rvv_config.sew = config_.sew;
      rvv_config.lmul = config_.lmul;
      rvv_config.vstart = 0;
      rvv_config.ma = false;
      rvv_config.ta = false;
      rvv_backend->setConfigState(rvv_config);

      // Initialize vector registers
      auto vrf = rvv_backend->getVRF();
      if (vrf) {
        std::vector<uint8_t> v_init(config_.vlen / 8, 0x0F);
        for (int i = 0; i < 32; i++) {
          vrf->write(i, v_init);
        }
      }
    }

    // Load program
    loadProgram(core);

    // Initialize core
    core->initialize();

    // Run simulation
    printSimulationStart();
    runSimulation(*scheduler, core, rvv_backend);

    // Print results
    printResults(core, rvv_backend);

    if (config_.enable_tracing) {
      EventDriven::Tracer::getInstance().dump();
      std::cout << "\nTrace saved to: " << config_.trace_output << std::endl;
    }
  }

 private:
  ProgramConfig config_;

  void loadProgram(std::shared_ptr<SCore> core) {
    std::cout << "\nLoading Program: " << config_.name << std::endl;
    std::cout << "Description: " << config_.description << "\n" << std::endl;

    // Load instructions
    std::cout << "Loading " << config_.instructions.size() << " instructions\n";
    for (const auto& [addr, binary] : config_.instructions) {
      core->loadInstruction(addr, binary);
    }

    // Load data
    std::cout << "Loading " << config_.data_memory.size() << " data values\n";
    for (const auto& [addr, value] : config_.data_memory) {
      core->loadData(addr, value);
      if (config_.verbose && addr < 0x30) {
        std::cout << "  [0x" << std::hex << addr << std::dec << "] = " << value
                  << "\n";
      }
    }

    std::cout << std::endl;
  }

  void printHeader() const {
    std::cout
        << "\n╔════════════════════════════════════════════════════════╗\n"
        << "║  Event-Driven RISC-V Simulator with RVV Support       ║\n"
        << "║  Configuration: External Program File                 ║\n"
        << "╚════════════════════════════════════════════════════════╝\n"
        << std::endl;

    std::cout << "Scalar Core:\n"
              << "  Issue Width:     " << config_.num_instruction_lanes
              << " lanes/cycle\n"
              << "  ALU Latency:     " << config_.alu_period << " cycle(s)\n"
              << "  BRU Units:       " << config_.num_bru_units << "\n"
              << "  MLU Latency:     " << config_.mlu_period << " cycle(s)\n"
              << "  DVU Latency:     " << config_.dvu_period << " cycle(s)\n\n";

    if (config_.enable_rvv) {
      std::cout << "Vector Backend:\n"
                << "  Status:          ENABLED\n"
                << "  VLEN:            " << config_.vlen << " bits\n"
                << "  VL:              " << config_.vl << " elements\n"
                << "  SEW:             " << (8 << config_.sew) << " bits\n"
                << "  LMUL:            " << (1 << config_.lmul) << "x\n\n";
    }

    std::cout << "Simulation:\n"
              << "  Max Cycles:      " << config_.max_cycles << "\n"
              << "  Tracing:         "
              << (config_.enable_tracing ? "ENABLED" : "DISABLED") << "\n"
              << "  Verbose:         " << (config_.verbose ? "ON" : "OFF")
              << "\n\n";
  }

  void printSimulationStart() const {
    std::cout << "Starting Simulation\n"
              << "═══════════════════════════════════════════════════════\n"
              << std::endl;
  }

  void runSimulation(EventScheduler& scheduler, std::shared_ptr<SCore> core,
                     std::shared_ptr<RVVBackend> rvv) {
    uint64_t cycles = 0;
    uint64_t prev_dispatched = 0;

    while (scheduler.getPendingEventCount() > 0 &&
           cycles < config_.max_cycles) {
      scheduler.runFor(1);

      uint64_t curr_dispatched = core->getInstructionsDispatched();
      if (curr_dispatched > prev_dispatched && config_.verbose) {
        uint64_t dispatched_this_cycle = curr_dispatched - prev_dispatched;
        std::cout << "Cycle " << std::setw(5) << cycles << ": "
                  << dispatched_this_cycle << " instruction(s) dispatched\n";
        prev_dispatched = curr_dispatched;
      }

      cycles++;
    }

    std::cout << "\nSimulation completed in " << cycles << " cycles"
              << std::endl;
  }

  void printResults(std::shared_ptr<SCore> core,
                    std::shared_ptr<RVVBackend> rvv) const {
    std::cout
        << "\n╔════════════════════════════════════════════════════════╗\n"
        << "║  RESULTS                                               ║\n"
        << "╚════════════════════════════════════════════════════════╝\n"
        << std::endl;

    std::cout << "Scalar Core:\n"
              << "  Instructions Dispatched: "
              << core->getInstructionsDispatched() << "\n"
              << "  Instructions Retired:    " << core->getInstructionsRetired()
              << "\n\n";

    if (config_.enable_rvv && rvv) {
      std::cout << "Vector Backend:\n"
                << "  Decode Count:    " << rvv->getDecodeCount() << "\n"
                << "  Dispatch Count:  " << rvv->getDispatchCount() << "\n"
                << "  Execute Count:   " << rvv->getExecuteCount() << "\n"
                << "  Retire Count:    " << rvv->getRetireCount() << "\n"
                << "  Total UOPs:      " << rvv->getTotalUopsGenerated()
                << "\n\n";
    }

    std::cout << "Status: ✓ COMPLETED\n";
  }
};

void printUsage(const std::string& program_name) {
  std::cout
      << "\nEvent-Driven RISC-V Simulator with RVV Support\n"
      << "\nUsage: " << program_name << " [OPTIONS]\n"
      << "\nOptions:\n"
      << "  --program <file>      Load program from JSON file (required)\n"
      << "  --trace               Enable event tracing\n"
      << "  --verbose             Verbose output during execution\n"
      << "  --help                Show this message\n"
      << "\nExamples:\n"
      << "  " << program_name << " --program programs/mac_example.json\n"
      << "  " << program_name
      << " --program programs/rvv_alu_example.json --trace\n"
      << std::endl;
}

int main(int argc, char* argv[]) {
  try {
    std::string program_file;
    bool enable_trace = false;
    bool verbose = false;

    // Parse arguments
    for (int i = 1; i < argc; i++) {
      std::string arg = argv[i];

      if (arg == "--program" && i + 1 < argc) {
        program_file = argv[++i];
      } else if (arg == "--trace") {
        enable_trace = true;
      } else if (arg == "--verbose") {
        verbose = true;
      } else if (arg == "--help") {
        printUsage(argv[0]);
        return 0;
      }
    }

    // Check if program file is specified
    if (program_file.empty()) {
      std::cerr << "Error: --program <file> is required\n";
      printUsage(argv[0]);
      return 1;
    }

    // Load configuration
    ProgramConfig config = ProgramLoader::loadFromFile(program_file);

    // Override with command line options
    if (enable_trace) {
      config.enable_tracing = true;
    }
    if (verbose) {
      config.verbose = true;
    }

    // Run simulator
    RVVSimulator simulator(config);
    simulator.run();

    std::cout << "\n✓ Simulation completed successfully!\n" << std::endl;
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
}
