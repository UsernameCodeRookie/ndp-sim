# 附录 B: 代码示例集

## 概览

本附录收集了论文中提到的主要代码示例，可直接使用或参考。

## 第一章: 事件驱动相关示例

### 示例1.1: 简单的事件调度

```cpp
#include "event.h"
#include "scheduler.h"

using namespace EventDriven;

int main() {
    EventScheduler scheduler;
    
    // 安排事件
    for (int i = 0; i < 10; i++) {
        auto event = std::make_shared<LambdaEvent>(
            i * 100,  // 时间
            [i](EventScheduler& s) {
                std::cout << "Event " << i << " at time " 
                          << s.getCurrentTime() << "\n";
            },
            i  // 优先级
        );
        scheduler.scheduleEvent(event);
    }
    
    // 运行模拟
    scheduler.run();
    
    return 0;
}
```

**输出**:
```
Event 0 at time 0
Event 1 at time 100
Event 2 at time 200
...
Event 9 at time 900
```

### 示例1.2: 周期性事件

```cpp
EventScheduler scheduler;

// 每100个周期执行一次
auto timer = std::make_shared<PeriodicEvent>(
    0,      // 开始时间
    100,    // 周期
    [](EventScheduler& s, uint64_t time) {
        std::cout << "Periodic event at time " << time << "\n";
    },
    10      // 重复10次
);
scheduler.scheduleEvent(timer);
scheduler.run();
```

## 第二章: 建模相关示例

### 示例2.1: 创建简单组件

```cpp
class Counter : public Component {
private:
    uint64_t count_;
    
public:
    Counter(const std::string& name, EventScheduler& scheduler)
        : Component(name, scheduler), count_(0) {
        // 创建端口
        addPort("input", PortDirection::INPUT);
        addPort("output", PortDirection::OUTPUT);
    }
    
    void process() {
        auto input = getPort("input");
        if (input && input->hasData()) {
            auto packet = input->read();
            count_++;
            
            // 发送输出
            auto output = getPort("output");
            output->write(packet);
        }
    }
    
    uint64_t getCount() const { return count_; }
};
```

### 示例2.2: 连接组件

```cpp
EventScheduler scheduler;

// 创建组件
auto comp1 = std::make_shared<Counter>("counter1", scheduler);
auto comp2 = std::make_shared<Counter>("counter2", scheduler);

// 连接
auto conn = std::make_shared<Connection>("link", scheduler);
conn->addSourcePort(comp1->getPort("output"));
conn->addDestinationPort(comp2->getPort("input"));

// 数据流: comp1.output → link → comp2.input
```

### 示例2.3: 创建流水线

```cpp
Pipeline pipeline("my_pipe", scheduler, 1, 3);

// 第0级: 乘以2
pipeline.setStageFunction(0, [](auto data) {
    auto int_data = std::dynamic_pointer_cast<IntDataPacket>(data);
    if (int_data) {
        int_data->value *= 2;
    }
    return data;
});

// 第1级: 加10
pipeline.setStageFunction(1, [](auto data) {
    auto int_data = std::dynamic_pointer_cast<IntDataPacket>(data);
    if (int_data) {
        int_data->value += 10;
    }
    return data;
});

// 第2级: 右移1
pipeline.setStageFunction(2, [](auto data) {
    auto int_data = std::dynamic_pointer_cast<IntDataPacket>(data);
    if (int_data) {
        int_data->value >>= 1;
    }
    return data;
});

// 运行10个周期
for (int i = 0; i < 10; i++) {
    pipeline.tick();
}

std::cout << "Processed: " << pipeline.getTotalProcessed() << "\n";
```

### 示例2.4: 带暂停条件的流水线

```cpp
Pipeline pipeline("with_stall", scheduler, 1, 2);

// 设置暂停条件(当值>50时暂停)
pipeline.setStageStallPredicate(1, [](auto data) {
    auto int_data = std::dynamic_pointer_cast<IntDataPacket>(data);
    return int_data && int_data->value > 50;
});
```

## 第三章: Coral NPU建模示例

### 示例3.1: ALU单元

```cpp
class ALU : public Component {
private:
    Pipeline execute_pipeline_;
    
public:
    ALU(const std::string& name, EventScheduler& scheduler)
        : Component(name, scheduler),
          execute_pipeline_(name + "_pipeline", scheduler, 1, 3) {
        
        addPort("operand_a", PortDirection::INPUT);
        addPort("operand_b", PortDirection::INPUT);
        addPort("opcode", PortDirection::INPUT);
        addPort("result", PortDirection::OUTPUT);
        
        // 配置流水线
        setupPipeline();
    }
    
private:
    void setupPipeline() {
        // 第0级: 读操作数
        execute_pipeline_.setStageFunction(0, [this](auto data) {
            // 操作数已经在包中
            return data;
        });
        
        // 第1级: 执行
        execute_pipeline_.setStageFunction(1, [this](auto data) {
            auto result = std::dynamic_pointer_cast<IntDataPacket>(data);
            if (result) {
                // 执行操作
                result->value = doALUOp(result->value);
            }
            return data;
        });
        
        // 第2级: 写回
        execute_pipeline_.setStageFunction(2, [this](auto data) {
            auto output = getPort("result");
            output->write(data);
            return data;
        });
    }
    
    int32_t doALUOp(int32_t operand) {
        // 简化的ALU操作
        return operand + 1;
    }
};
```

### 示例3.2: 寄存器文件

```cpp
class RegFile : public Component {
private:
    static constexpr size_t NUM_REGS = 32;
    std::array<uint32_t, NUM_REGS> registers_;
    
public:
    RegFile(const std::string& name, EventScheduler& scheduler)
        : Component(name, scheduler) {
        
        // 创建读/写端口
        for (int i = 0; i < 4; i++) {
            addPort("read_addr_" + std::to_string(i), 
                    PortDirection::INPUT);
            addPort("read_data_" + std::to_string(i), 
                    PortDirection::OUTPUT);
        }
        
        for (int i = 0; i < 2; i++) {
            addPort("write_addr_" + std::to_string(i), 
                    PortDirection::INPUT);
            addPort("write_data_" + std::to_string(i), 
                    PortDirection::INPUT);
        }
        
        // 初始化寄存器
        registers_.fill(0);
    }
    
    void tick() {
        // 处理读操作
        for (int i = 0; i < 4; i++) {
            auto addr_port = getPort("read_addr_" + std::to_string(i));
            if (addr_port && addr_port->hasData()) {
                auto addr_packet = 
                    std::dynamic_pointer_cast<IntDataPacket>(
                        addr_port->read()
                    );
                if (addr_packet) {
                    int reg_idx = addr_packet->value;
                    auto data_port = getPort("read_data_" + std::to_string(i));
                    auto data = std::make_shared<IntDataPacket>(
                        registers_[reg_idx]
                    );
                    data_port->write(data);
                }
            }
        }
        
        // 处理写操作
        for (int i = 0; i < 2; i++) {
            auto addr_port = getPort("write_addr_" + std::to_string(i));
            auto data_port = getPort("write_data_" + std::to_string(i));
            
            if (addr_port && addr_port->hasData() &&
                data_port && data_port->hasData()) {
                
                auto addr_packet = 
                    std::dynamic_pointer_cast<IntDataPacket>(
                        addr_port->read()
                    );
                auto data_packet = 
                    std::dynamic_pointer_cast<IntDataPacket>(
                        data_port->read()
                    );
                
                if (addr_packet && data_packet) {
                    int reg_idx = addr_packet->value;
                    registers_[reg_idx] = data_packet->value;
                }
            }
        }
    }
    
    uint32_t readReg(int idx) const {
        return registers_[idx];
    }
    
    void writeReg(int idx, uint32_t value) {
        registers_[idx] = value;
    }
};
```

### 示例3.3: 简单的指令流水线

```cpp
class SimplePipeline : public Component {
private:
    Pipeline instr_pipeline_;
    uint64_t pc_;
    
public:
    SimplePipeline(const std::string& name, EventScheduler& scheduler)
        : Component(name, scheduler),
          instr_pipeline_(name + "_pipe", scheduler, 1, 5),
          pc_(0) {
        
        addPort("instr_in", PortDirection::INPUT);
        addPort("result_out", PortDirection::OUTPUT);
        
        setupPipeline();
    }
    
private:
    void setupPipeline() {
        // Stage 0: Fetch
        instr_pipeline_.setStageFunction(0, [this](auto data) {
            // 取指: 返回当前PC的指令
            auto instr = std::make_shared<IntDataPacket>(pc_);
            pc_++;
            return instr;
        });
        
        // Stage 1: Decode
        instr_pipeline_.setStageFunction(1, [](auto data) {
            // 译码: 解析指令字段
            return data;
        });
        
        // Stage 2: Execute
        instr_pipeline_.setStageFunction(2, [](auto data) {
            // 执行: 计算结果
            auto instr = std::dynamic_pointer_cast<IntDataPacket>(data);
            if (instr) {
                instr->value = instr->value * 2;  // 简化操作
            }
            return data;
        });
        
        // Stage 3: Memory
        instr_pipeline_.setStageFunction(3, [](auto data) {
            // 内存: 访存操作
            return data;
        });
        
        // Stage 4: Writeback
        instr_pipeline_.setStageFunction(4, [this](auto data) {
            // 写回: 更新寄存器
            auto output = getPort("result_out");
            output->write(data);
            return data;
        });
    }
};
```

## 完整的集成示例

### 示例: 完整的处理器模拟

```cpp
int main() {
    using namespace EventDriven;
    using namespace Architecture;
    
    EventScheduler scheduler;
    
    // 创建处理器组件
    auto regfile = std::make_shared<RegFile>("regfile", scheduler);
    auto alu = std::make_shared<ALU>("alu", scheduler);
    auto pipeline = std::make_shared<SimplePipeline>("pipe", scheduler);
    
    // 连接组件
    // (这里应该创建Connection对象进行连接)
    
    // 安排初始事件
    scheduler.scheduleEvent(std::make_shared<LambdaEvent>(
        0,
        [&scheduler](EventScheduler& s) {
            // 初始化程序
            std::cout << "Simulation started at time " 
                      << s.getCurrentTime() << "\n";
        }
    ));
    
    // 运行模拟
    scheduler.run();
    
    return 0;
}
```

---

**参见**: 附录A获得完整API参考

