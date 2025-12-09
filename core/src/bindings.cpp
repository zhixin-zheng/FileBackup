#include <pybind11/pybind11.h>
#include <pybind11/stl.h> 
#include "backup_system.h"
#include "filter.h"

namespace py = pybind11;

PYBIND11_MODULE(backup_core_py, m) {
    m.doc() = "Python bindings for the C++ Backup System";

    py::class_<Backup::Filter>(m, "FilterOptions")
        .def(py::init<>())
        .def_readwrite("nameRegex", &Backup::Filter::nameRegex)
        .def_readwrite("nameKeywords", &Backup::Filter::nameKeywords)
        .def_readwrite("suffixes", &Backup::Filter::suffixes)
        .def_readwrite("minSize", &Backup::Filter::minSize)
        .def_readwrite("maxSize", &Backup::Filter::maxSize)
        .def_readwrite("startTime", &Backup::Filter::startTime)
        .def_readwrite("endTime", &Backup::Filter::endTime)
        .def_readwrite("userName", &Backup::Filter::userName)
        .def_readwrite("enabled", &Backup::Filter::enabled);

    py::class_<Backup::BackupSystem>(m, "BackupSystem")
        .def(py::init<>())
        .def("setCompressionAlgorithm", &Backup::BackupSystem::setCompressionAlgorithm)
        .def("setPassword", &Backup::BackupSystem::setPassword)
        .def("setFilter", &Backup::BackupSystem::setFilter)
        .def("backup", &Backup::BackupSystem::backup, py::call_guard<py::gil_scoped_release>())
        .def("restore", &Backup::BackupSystem::restore, py::call_guard<py::gil_scoped_release>())
        .def("verify", &Backup::BackupSystem::verify, py::call_guard<py::gil_scoped_release>());
}