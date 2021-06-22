#include <pybind11/pybind11.h>
#include <translator/response.h>

namespace py = pybind11;

using marian::bergamot::Response;

PYBIND11_MODULE(bergamot, m) {
  py::class_<Response>(m, "Response")
      .def(py::init<>())
      .def("source", &Response::getOriginalText)
      .def("target", &Response::getTranslatedText);
}
