#include <pybind11/pybind11.h>
#include <pybind11/stl_bind.h>
#include <translator/annotation.h>
#include <translator/response.h>
#include <translator/response_options.h>
#include <translator/service.h>

#include <string>
#include <vector>

namespace py = pybind11;

using marian::bergamot::Alignment;
using marian::bergamot::AnnotatedText;
using marian::bergamot::ByteRange;
using marian::bergamot::ConcatStrategy;
using marian::bergamot::Point;
using marian::bergamot::QualityScoreType;
using marian::bergamot::Response;
using marian::bergamot::ResponseOptions;
using marian::bergamot::Service;

PYBIND11_MAKE_OPAQUE(std::vector<Response>);
PYBIND11_MAKE_OPAQUE(std::vector<std::string>);
PYBIND11_MAKE_OPAQUE(std::vector<Point>);
PYBIND11_MAKE_OPAQUE(std::vector<Alignment>);

// Nothing fancy; Super wasteful.  It is simply easier to do analysis in a
// Jupyter notebook, @jerinphilip is not doing efficiency here.

class ServicePyAdapter {
 public:
  ServicePyAdapter(const std::string &config) {
    py::call_guard<py::gil_scoped_release> gil_guard();
    service_.reset(std::move(new Service(config)));
  }
  Response translate(std::string input, const ResponseOptions options) {
    py::call_guard<py::gil_scoped_release> gil_guard();
    std::future<Response> future = service_->translate(std::move(input), options);
    future.wait();
    return future.get();
  }

 private:
  std::unique_ptr<Service> service_{nullptr};
};

PYBIND11_MODULE(pybergamot, m) {
  py::class_<ByteRange>(m, "ByteRange")
      .def(py::init<>())
      .def_readonly("begin", &ByteRange::begin)
      .def_readonly("end", &ByteRange::end)
      .def("__repr__", [](const ByteRange &range) {
        return "{" + std::to_string(range.begin) + ", " + std::to_string(range.end) + "}";
      });

  py::class_<AnnotatedText>(m, "AnnotatedText")
      .def(py::init<>())
      .def("numWords", &AnnotatedText::numWords)
      .def("numSentences", &AnnotatedText::numSentences)
      .def("isUnknown", &AnnotatedText::isUnknown)
      .def("word", &AnnotatedText::wordAsByteRange)
      .def("sentence", &AnnotatedText::sentenceAsByteRange)
      .def_readonly("text", &AnnotatedText::text);

  py::bind_vector<std::vector<Point>>(m, "Alignment");
  py::bind_vector<std::vector<Alignment>>(m, "VectorAlignment");

  py::class_<Response>(m, "Response")
      .def(py::init<>())
      .def_readonly("source", &Response::source)
      .def_readonly("target", &Response::target)
      .def_readonly("alignments", &Response::alignments);

  py::bind_vector<std::vector<Response>>(m, "VectorResponse");

  py::class_<ResponseOptions>(m, "ResponseOptions")
      .def(py::init<>())
      .def_readwrite("qualityScores", &ResponseOptions::qualityScores)
      .def_readwrite("alignment", &ResponseOptions::alignment)
      .def_readwrite("sentenceMappings", &ResponseOptions::sentenceMappings)
      .def_readwrite("alignmentThreshold", &ResponseOptions::alignmentThreshold)
      .def_readwrite("qualityScoreType", &ResponseOptions::alignmentThreshold)
      .def_readwrite("concatStrategy", &ResponseOptions::alignmentThreshold);

  py::enum_<ConcatStrategy>(m, "ConcatStrategy")
      .value("FAITHFUL", ConcatStrategy::FAITHFUL)
      .value("SPACE", ConcatStrategy::SPACE)
      .export_values();

  py::enum_<QualityScoreType>(m, "QualityScoreType")
      .value("FREE", QualityScoreType::FREE)
      .value("EXPENSIVE", QualityScoreType::EXPENSIVE)
      .export_values();

  py::bind_vector<std::vector<std::string>>(m, "VectorString");
  py::class_<ServicePyAdapter>(m, "Service")
      .def(py::init<const std::string &>())
      .def("translate", &ServicePyAdapter::translate);

  py::class_<Point>(m, "Point")
      .def_readonly("src", &Point::src)
      .def_readonly("tgt", &Point::tgt)
      .def_readonly("prob", &Point::prob)
      .def("__repr__", [](const Point &p) {
        return "{(" + std::to_string(p.src) + ", " + std::to_string(p.tgt) + ") = " + std::to_string(p.prob) + "}";
      });
}
