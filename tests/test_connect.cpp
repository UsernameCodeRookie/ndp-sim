#include <gtest/gtest.h>

#include <iostream>
#include <memory>

#include "debug.h"
#include "graph.h"
#include "pe.h"

// -------------------- Two-PE Connection Test --------------------
TEST(GraphTest, TwoPEConnection) {
  auto dbg = std::make_shared<PrintDebugger>();

  // Create a graph
  Graph g;

  // Create two PEs with simple signed multiply-accumulate (smac)
  auto pe0 = g.addNode<PE>("PE0", 4, 4, Op::smac, false);
  auto pe1 = g.addNode<PE>("PE1", 4, 4, Op::smac, false);

  // Connect PE0 output to PE1 inPort2 (as accumulation input)
  g.connect(pe0->outPort, pe1->inPort2);

  // Feed input operands to PE0
  pe0->inPort0.write({3, false});
  pe0->inPort1.write({5, false});
  pe0->inPort2.write({0, false});  // initial accumulation

  // Tick the graph for a few cycles
  for (int t = 0; t < 5; ++t) {
    g.tick(dbg);
  }

  // Read output of PE0 and feed to PE1 inPort2 automatically via connection
  Data out0;
  ASSERT_TRUE(pe1->inPort2.peek(out0));

  // Feed remaining operands to PE1
  pe1->inPort0.write({2, false});
  pe1->inPort1.write({4, false});

  // Tick the graph to propagate
  for (int t = 0; t < 5; ++t) {
    g.tick(dbg);
  }

  // Check final output of PE1
  Data out1;
  ASSERT_TRUE(pe1->outPort.peek(out1));

  std::cout << "PE0 output: " << out0.data << "\n";
  std::cout << "PE1 output: " << out1.data << "\n";

  // Verify correctness:
  // PE0: smac(3,5,0) = 3*5+0 = 15
  // PE1: smac(2,4,15) = 2*4+15 = 23
  EXPECT_EQ(out0.data, 15);
  EXPECT_EQ(out1.data, 23);
}
