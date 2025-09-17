QT += core widgets

CONFIG += c++17

TARGET = seizure_analyzer
TEMPLATE = app

SOURCES += \
    ../main.cpp \
    seizure_analyzer.cpp \
    ../core/hdf5_reader.cpp \
    ../core/fpga_logger.cpp \
    ../core/halo_response_decoder.cpp \
    ../core/hdf5_writer.cpp

HEADERS += \
    seizure_analyzer.h \
    ../core/hdf5_reader.h \
    ../core/fpga_logger.h \
    ../core/halo_response_decoder.h \
    ../core/hdf5_writer.h

# FORMS += \
#     seizure_analyzer.ui

# HDF5 support
macx {
    INCLUDEPATH += /opt/homebrew/include
    LIBS += -L/opt/homebrew/lib -lhdf5
}

win32 {
    LIBS += -lhdf5
}

unix:!macx {
    LIBS += -lhdf5
}
