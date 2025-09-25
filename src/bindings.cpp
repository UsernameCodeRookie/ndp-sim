#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "graph.h"
#include "pe.h"

namespace py = pybind11;

PYBIND11_MODULE(ndp_sim_py, m) {
  py::class_<Graph, std::shared_ptr<Graph>>(m, "Graph")
      .def(py::init<>())
      .def(
          "addNode",
          [](Graph &g, const std::string &id, int inCap, int outCap, Op opcode,
             bool transOut = false) {
            return g.addNode<PE>(id, inCap, outCap, opcode, transOut);
          },
          py::arg("id"), py::arg("inCap"), py::arg("outCap"), py::arg("opcode"),
          py::arg("transOut") = false)
      .def("connect", &Graph::connect<int>, py::arg("src"), py::arg("dst"),
           py::arg("strategyStr") = "broadcast")
      .def("tick", &Graph::tick, py::arg("dbg") = nullptr);

  py::class_<Port<int>>(m, "Port")
      .def_readwrite("data", &Port<int>::data)
      .def_readwrite("valid", &Port<int>::valid);
}