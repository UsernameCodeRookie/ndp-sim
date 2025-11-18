# 附录 A: API参考

## 概览

本附录提供了框架中所有主要类和接口的API参考。

## 事件系统 API (第一章相关)

### class EventScheduler

事件驱动调度器的核心类。

```cpp
class EventScheduler {
public:
    // 核心方法
    void run();
    void scheduleEvent(std::shared_ptr<Event> event);
    void run(uint64_t max_time);
    
    // 查询方法
    uint64_t getCurrentTime() const;
    bool hasEvents() const;
    size_t getEventCount() const;
    
    // 统计方法
    uint64_t getTotalEventsExecuted() const;
    void printStatistics() const;
    
    // 调度器配置
    void enableTracing(bool enable);
    void setMaximumTime(uint64_t max_time);
};
```

### class Event

事件的基类，所有事件都应继承此类。

```cpp
class Event {
public:
    using EventID = uint64_t;
    
    Event(uint64_t time, int priority = 0, 
          EventType type = EventType::GENERIC,
          const std::string& name = "Event");
    
    virtual void execute(EventScheduler& scheduler) = 0;
    
    // 属性查询
    uint64_t getTime() const;
    int getPriority() const;
    EventType getType() const;
    const std::string& getName() const;
    EventID getID() const;
    bool isCancelled() const;
    
    // 事件控制
    void cancel();
    
    // 比较操作符
    bool operator<(const Event& other) const;
};
```

### class LambdaEvent

使用Lambda函数定义的事件。

```cpp
class LambdaEvent : public Event {
public:
    using Callback = std::function<void(EventScheduler&)>;
    
    LambdaEvent(uint64_t time, Callback callback, 
                int priority = 0,
                const std::string& name = "LambdaEvent");
    
    void execute(EventScheduler& scheduler) override;
};
```

### class PeriodicEvent

周期性重复执行的事件。

```cpp
class PeriodicEvent : public Event,
                      public std::enable_shared_from_this<PeriodicEvent> {
public:
    using Callback = std::function<void(EventScheduler&, uint64_t)>;
    
    PeriodicEvent(uint64_t start_time, uint64_t period, 
                  Callback callback,
                  uint64_t repeat_count = 0,
                  const std::string& name = "PeriodicEvent");
    
    void execute(EventScheduler& scheduler) override;
    
    uint64_t getPeriod() const;
    uint64_t getExecutionCount() const;
};
```

## 体系结构建模 API (第二章相关)

### class Port

端口类，用于组件间的通信。

```cpp
class Port {
public:
    Port(const std::string& name, PortDirection direction, 
         Component* owner);
    
    // 属性查询
    const std::string& getName() const;
    PortDirection getDirection() const;
    Component* getOwner() const;
    Connection* getConnection() const;
    
    // 连接管理
    void setConnection(Connection* conn);
    bool isConnected() const;
    
    // 数据操作
    void setData(std::shared_ptr<DataPacket> data);
    std::shared_ptr<DataPacket> getData() const;
    bool hasData() const;
    void clearData();
    
    // 读写操作
    virtual std::shared_ptr<DataPacket> read();
    virtual void write(std::shared_ptr<DataPacket> data);
};
```

### class Component

体系结构组件的基类。

```cpp
class Component {
public:
    Component(const std::string& name, 
              EventDriven::EventScheduler& scheduler);
    
    // 属性查询
    const std::string& getName() const;
    EventDriven::EventScheduler& getScheduler();
    bool isEnabled() const;
    
    // 状态控制
    void setEnabled(bool enabled);
    
    // 端口管理
    void addPort(const std::string& port_name, 
                 PortDirection direction);
    std::shared_ptr<Port> getPort(const std::string& name);
    const std::unordered_map<std::string, std::shared_ptr<Port>>& 
        getPorts() const;
    
    // 生命周期方法
    virtual void initialize();
    virtual void reset();
};
```

### class Connection

连接两个端口的基类。

```cpp
class Connection {
public:
    Connection(const std::string& name, 
               EventDriven::EventScheduler& scheduler);
    
    // 属性查询
    const std::string& getName() const;
    uint64_t getLatency() const;
    void setLatency(uint64_t latency);
    
    // 端口管理
    void addSourcePort(std::shared_ptr<Port> port);
    void addDestinationPort(std::shared_ptr<Port> port);
    
    const std::vector<std::shared_ptr<Port>>& 
        getSourcePorts() const;
    const std::vector<std::shared_ptr<Port>>& 
        getDestinationPorts() const;
    
    // 数据传输
    virtual void propagate();
};
```

### class Pipeline : public TickingComponent

流水线类。

```cpp
class Pipeline : public Architecture::TickingComponent {
public:
    using StageFunction = std::function<std::shared_ptr<
        Architecture::DataPacket>(
        std::shared_ptr<Architecture::DataPacket>)>;
    
    Pipeline(const std::string& name, 
             EventDriven::EventScheduler& scheduler,
             uint64_t period, size_t num_stages = 3,
             uint64_t default_stage_latency = 0);
    
    // 阶段配置
    void setStageFunction(size_t stage_index, 
                          StageFunction func);
    void setStageStallPredicate(
        size_t stage_index,
        std::function<bool(std::shared_ptr<
            Architecture::DataPacket>)> predicate);
    void setStageLatency(size_t stage_index, 
                        uint64_t latency);
    uint64_t getStageLatency(size_t stage_index) const;
    
    bool setStage(size_t stage_index,
                  std::shared_ptr<Architecture::Stage> stage);
    
    // 流水线操作
    void tick() override;
    void flush();
    bool isEmpty() const;
    bool isFull() const;
    size_t getOccupancy() const;
    
    // 统计信息
    size_t getNumStages() const;
    uint64_t getTotalProcessed() const;
    uint64_t getTotalStalls() const;
    bool isStalled() const;
    void printStatistics() const;
};
```

### class Stage

流水线级的基类。

```cpp
class Stage : public Component {
public:
    Stage(const std::string& name, 
          EventDriven::EventScheduler& scheduler);
    
    // 处理方法
    virtual std::shared_ptr<DataPacket> process(
        std::shared_ptr<DataPacket> data) = 0;
    
    // 暂停检查
    virtual bool shouldStall(
        std::shared_ptr<DataPacket> data) const;
    
    // 延迟查询
    virtual uint64_t getLatency() const;
    
    // 重置
    virtual void reset();
};
```

### class DataPacket

数据包的基类。

```cpp
class DataPacket {
public:
    virtual ~DataPacket() = default;
    
    uint64_t timestamp;  // 创建时间戳
    bool valid;          // 数据有效性
};

class IntDataPacket : public DataPacket {
public:
    IntDataPacket(int32_t value = 0) : value(value) {}
    
    int32_t value;
};
```

## 特殊连接 API

### class ReadyValidConnection

实现Ready-Valid握手协议的连接。

```cpp
class ReadyValidConnection : public Connection {
public:
    ReadyValidConnection(const std::string& name,
                        EventDriven::EventScheduler& scheduler);
    
    void propagate() override;
    
    // 控制信号
    void setReady(std::shared_ptr<Port> port, bool ready);
    void setValid(std::shared_ptr<Port> port, bool valid);
};
```

### class CreditConnection

实现信用流控制的连接。

```cpp
class CreditConnection : public Connection {
public:
    CreditConnection(const std::string& name,
                     EventDriven::EventScheduler& scheduler,
                     size_t initial_credits = 1);
    
    void propagate() override;
    
    void returnCredit();
    size_t getAvailableCredits() const;
};
```

## 时钟相关 API

### class TickingComponent

周期驱动的组件基类。

```cpp
class TickingComponent : public Component {
public:
    TickingComponent(const std::string& name,
                    EventDriven::EventScheduler& scheduler,
                    uint64_t period = 1);
    
    // 时钟方法
    virtual void tick() = 0;
    
    // 时钟控制
    uint64_t getPeriod() const;
    void setPeriod(uint64_t period);
    
    // 时钟事件调度
    void scheduleClockTick(uint64_t time);
};
```

## 追踪与调试 API

### class TraceEvent

用于追踪的宏和类。

```cpp
// 追踪宏定义 (在trace.h中)

#define TRACE_COMPUTE(time, component, event_type, message) \
    { /* 追踪计算事件 */ }

#define TRACE_MEMORY(time, component, addr, op, message) \
    { /* 追踪内存事件 */ }

#define TRACE_COMMUNICATION(time, src, dst, data, message) \
    { /* 追踪通信事件 */ }

#define TRACE_PIPELINE(time, component, stage, message) \
    { /* 追踪流水线事件 */ }
```

## 使用示例

### 创建简单事件

```cpp
// 方法1: 继承Event类
class MyEvent : public Event {
public:
    MyEvent(uint64_t time) : Event(time) {}
    void execute(EventScheduler& scheduler) override {
        std::cout << "My event executed at " 
                  << scheduler.getCurrentTime() << "\n";
    }
};

// 方法2: 使用LambdaEvent
scheduler.scheduleEvent(std::make_shared<LambdaEvent>(
    1000,
    [](EventScheduler& s) {
        std::cout << "Lambda event at " << s.getCurrentTime() << "\n";
    }
));
```

### 创建组件

```cpp
class MyComponent : public Component {
public:
    MyComponent(const std::string& name, 
                EventScheduler& scheduler)
        : Component(name, scheduler) {
        addPort("input", PortDirection::INPUT);
        addPort("output", PortDirection::OUTPUT);
    }
    
    void initialize() override {
        std::cout << getName() << " initialized\n";
    }
};

// 使用
MyComponent comp("my_comp", scheduler);
comp.initialize();
```

### 创建流水线

```cpp
Pipeline pipeline("my_pipeline", scheduler, 1, 3);

// 设置第0级的处理函数
pipeline.setStageFunction(0, [](auto data) {
    auto result = std::dynamic_pointer_cast<IntDataPacket>(data);
    if (result) {
        result->value *= 2;
    }
    return result;
});

// 运行流水线
pipeline.tick();
```

---

**参见**: 其他附录章节以获得更多信息

