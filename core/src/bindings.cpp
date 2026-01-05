#include <pybind11/pybind11.h>
#include <pybind11/stl.h> 
#include "backup_system.h"
#include "filter.h"
#include "scheduler.h"

namespace py = pybind11;

PYBIND11_MODULE(backup_core_py, m) {
    m.doc() = "Python bindings for the C++ Backup System";

    // Filter
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

    // BackupSystem
    py::class_<Backup::BackupSystem>(m, "BackupSystem")
        .def(py::init<>())
        .def("setCompressionAlgorithm", &Backup::BackupSystem::setCompressionAlgorithm)
        .def("setPassword", &Backup::BackupSystem::setPassword)
        .def("setFilter", &Backup::BackupSystem::setFilter)
        .def("backup", &Backup::BackupSystem::backup, py::call_guard<py::gil_scoped_release>())
        .def("restore", &Backup::BackupSystem::restore, py::call_guard<py::gil_scoped_release>())
        .def("verify", &Backup::BackupSystem::verify, py::call_guard<py::gil_scoped_release>());

    // BackupScheduler
    py::class_<Backup::BackupScheduler>(m, "BackupScheduler")
        .def(py::init<>())
        .def("start", &Backup::BackupScheduler::start, py::call_guard<py::gil_scoped_release>())
        .def("stop", &Backup::BackupScheduler::stop, py::call_guard<py::gil_scoped_release>())
        .def("addScheduledTask", &Backup::BackupScheduler::addScheduledTask, 
             "Add timed task", py::arg("src"), py::arg("dstDir"), py::arg("prefix"), py::arg("interval"), py::arg("maxKeep"))
        .def("addRealtimeTask", &Backup::BackupScheduler::addRealtimeTask, 
             "Add realtime task", py::arg("src"), py::arg("dstDir"), py::arg("prefix"), py::arg("maxKeep"))
        .def("setTaskFilter", &Backup::BackupScheduler::setTaskFilter)
        .def("setTaskPassword", &Backup::BackupScheduler::setTaskPassword)
        .def("setTaskCompressionAlgorithm", &Backup::BackupScheduler::setTaskCompressionAlgorithm);
}