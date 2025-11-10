# 统一架构说明 (Unified Architecture)

## 概述

本项目已完成软硬件解耦和架构统一，现在使用单一的 TPU 和 LSU 类，通过宏控制可选的 DRAMsim3 集成。

## 核心组件

### 1. TPU (Tensor Processing Unit)
**文件**: `src/components/tpu.h`

统一的 TPU 类，提供硬件原语（Hardware Primitives）：

```cpp
class SystolicArrayTPU {
  // 硬件原语
  size_t getArraySize();                                    // 获取阵列大小
  std::shared_ptr<MACUnit> getMAC(size_t row, size_t col); // 访问 MAC 单元
  void resetAllMACs();                                      // 重置所有 MAC
  std::shared_ptr<LoadStoreUnit> getLSU();                  // 获取 LSU
  
  // 内存操作
  void writeMemory(uint32_t address, int32_t data);         // 写内存
  int32_t readMemory(uint32_t address);                     // 读内存
  void writeMemoryBlock(uint32_t base, const std::vector<int>& data);
  std::vector<int> readMemoryBlock(uint32_t base, size_t size);
};
```

**特性**：
- 仅提供低层硬件原语，不包含算法逻辑
- 4x4 脉动阵列（MAC 单元）
- 通过 LSU 访问统一内存
- 支持可选的 DRAMsim3 集成（通过宏控制）

### 2. LSU (Load-Store Unit)
**文件**: `src/components/lsu.h`

统一的 LSU 类，支持片上内存和可选的 DRAMsim3：

```cpp
class LoadStoreUnit {
  // 直接内存访问
  void directWrite(uint32_t address, int32_t data);
  int32_t directRead(uint32_t address);
  
  // 端口访问（用于流水线）
  // Port: req_in, resp_out, ready, valid, done
};
```

**特性**：
- 8 个内存 bank，每个 8KB 容量
- 支持标量和向量加载/存储
- Bank 冲突检测和排队
- 可选的 DRAMsim3 片外内存模拟

### 3. Operators (算子)

**基类**: `src/operators/base.h`

```cpp
class OperatorBase {
  enum class ExecutionBackend { CPU, TPU, AUTO };
  
  void setBackend(ExecutionBackend backend);
  void bindTPU(std::shared_ptr<SystolicArrayTPU> tpu);
  virtual void compute() = 0;
};
```

**已实现的算子**：
- `gemm.h` - 矩阵乘法 (GEMM)

**设计原则**：
- 算子包含算法逻辑
- 通过 TPU 原语调用硬件
- 支持 CPU fallback
- 软硬件完全解耦

## DRAMsim3 集成

### 编译时控制

通过 `USE_DRAMSIM3` 宏控制是否启用 DRAMsim3：

```cmake
# 在 CMakeLists.txt 中
target_compile_definitions(your_target PRIVATE USE_DRAMSIM3)
```

### 代码示例

```cpp
#ifdef USE_DRAMSIM3
// TPU 构造时传入 DRAMsim3 配置
auto tpu = std::make_shared<SystolicArrayTPU>(
    "TPU", scheduler, period, 4,
    "path/to/dramsim3.config",
    "output_dir"
);
#else
// 标准 TPU（片上内存）
auto tpu = std::make_shared<SystolicArrayTPU>(
    "TPU", scheduler, period, 4
);
#endif
```

## 文件组织

### 已删除的过时文件
以下文件已被统一版本替代并删除：
- ✗ `tpu_primitives.h` → 合并到 `tpu.h`
- ✗ `tpu_dramsim3.h` → 合并到 `tpu.h` (宏控制)
- ✗ `lsu_dramsim3.h` → 合并到 `lsu.h` (宏控制)
- ✗ `tpu_old.h`, `lsu_old.h` - 备份文件

### 当前架构
```
src/
├── components/
│   ├── tpu.h            # 统一 TPU (支持可选 DRAMsim3)
│   ├── lsu.h            # 统一 LSU (支持可选 DRAMsim3)
│   ├── dramsim3_wrapper.h
│   └── ...
└── operators/
    ├── base.h           # 算子基类
    ├── gemm.h           # GEMM 算子
    └── tile_config.h    # Tile配置
```

## 使用示例

### 1. 创建 TPU 和算子

```cpp
// 创建事件调度器
EventDriven::EventScheduler scheduler;

// 创建 TPU（片上内存）
auto tpu = std::make_shared<SystolicArrayTPU>(
    "TPU", scheduler, 1, 4
);
tpu->start();

// 创建 GEMM 算子
auto gemm = std::make_shared<GEMMOperator<int>>("GEMM_TPU");
gemm->bindTPU(tpu);
gemm->setBackend(OperatorBase::ExecutionBackend::TPU);
```

### 2. 准备数据

```cpp
// 准备输入矩阵
std::vector<std::vector<int>> A = {{1, 0}, {0, 1}};
std::vector<std::vector<int>> B = {{2, 3}, {4, 5}};

gemm->setInputs(A, B);
```

### 3. 执行计算

```cpp
// 在 TPU 上计算
gemm->compute();

// 获取结果
auto C = gemm->getOutput();
```

### 4. TPU 原语直接使用

```cpp
// 写入数据到内存
tpu->writeMemory(0x1000, 42);

// 读取数据
int value = tpu->readMemory(0x1000);

// 访问 MAC 单元
auto mac = tpu->getMAC(0, 0);
mac->setInputA(3);
mac->setInputB(4);

// 重置所有累加器
tpu->resetAllMACs();
```

## 架构优势

### 1. 软硬件解耦
- **硬件层** (TPU/LSU): 仅提供原语，无算法逻辑
- **软件层** (Operators): 实现算法，调用硬件原语
- 清晰的抽象边界

### 2. 统一接口
- 单一的 TPU 和 LSU 类
- 宏控制可选特性（DRAMsim3）
- 减少代码重复

### 3. 灵活部署
- CPU/TPU 后端切换
- 可选的 DRAMsim3 集成
- 易于扩展新算子

### 4. 易于测试
- 硬件原语可独立测试
- 算子可在 CPU 上验证
- 清晰的测试边界

## 测试验证

### 运行测试

```bash
cd build

# 测试 TPU 原语
./test_tpu_primitives.exe

# 测试 GEMM 算子
./gemm_operator_tpu_example.exe
```

### 测试覆盖

✓ TPU 内存读写
✓ MAC 单元访问和重置
✓ GEMM 算子（4x4, 8x8, 16x16, 非方阵）
✓ TPU vs CPU 结果一致性
✓ 性能对比

## 未来扩展

### 添加新算子

1. 继承 `OperatorBase`
2. 实现 `compute()` 方法
3. 使用 TPU 原语实现算法
4. 提供 CPU fallback

### 示例：新增 ReLU 算子

```cpp
class ReLUOperator : public OperatorBase {
 public:
  void compute() override {
    if (backend_ == ExecutionBackend::TPU && tpu_) {
      computeOnTPU();
    } else {
      computeOnCPU();
    }
  }

 private:
  void computeOnTPU() {
    // 使用 tpu_->readMemory() / writeMemory()
    // 实现 ReLU: y = max(0, x)
  }
  
  void computeOnCPU() {
    // CPU 实现
  }
};
```

## 总结

本架构实现了：
- ✅ 单一 TPU 类和单一 LSU 类
- ✅ 宏控制 DRAMsim3 集成
- ✅ 软硬件完全解耦
- ✅ 清晰的硬件原语接口
- ✅ 灵活的算子框架
- ✅ 已删除所有过时文件

这为后续开发提供了清晰、简洁、可扩展的基础架构。
