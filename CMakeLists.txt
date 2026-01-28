########################################################################
# Project setup -- only needed if device support is a stand-alone build
# We recommend that the support module be built in-tree with the driver.
########################################################################
cmake_minimum_required(VERSION 2.9)
project(SoapyMalahitRR CXX)
enable_testing()

#select the release build type by default to get optimization flags
if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE "Release")
   message(STATUS "Build type not specified: defaulting to release.")
endif(NOT CMAKE_BUILD_TYPE)
set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "")

########################################################################
# Header and library resources needed to communicate with the device.
# These may be found within the build tree or in an external project.
########################################################################
set(MY_DEVICE_INCLUDE_DIRS "")
set(MY_DEVICE_LIBRARIES "")

########################################################################
# build the module
########################################################################
find_package(SoapySDR CONFIG)
if (NOT SoapySDR_FOUND)
    message(WARNING "SoapySDR development files not found - skipping support")
    return()
endif ()

find_package(ALSA REQUIRED)
if (NOT ALSA_FOUND)
    message(WARNING "ALSA development files not found - skipping support")
    return()
endif ()

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBGPIOD QUIET libgpiod)

if (NOT LIBGPIOD_FOUND)
    message(WARNING "libgpiod development files not found - GPIO support may be missing")
else()
    message(STATUS "Using libgpiod: ${LIBGPIOD_CFLAGS} ${LIBGPIOD_LDFLAGS}")
endif()

add_compile_definitions(CURRENT_FIRMWARE="malahit-r1-fw-102.bin")
add_compile_options(-Wno-unused-parameter)

include_directories(. ${ALSA_INCLUDE_DIRS})

SOAPY_SDR_MODULE_UTIL(
    TARGET MalahitRR
    SOURCES
        MalahitSDR.cpp
        GPIO.cpp
        ALSA.cpp
        I2C.cpp
        SPI.cpp
        STM.cpp
    LIBRARIES
        ${ALSA_LIBRARIES}
        gpiod
)

add_executable(malahit
    malahit/malahit.cpp
    GPIO.cpp
    ALSA.cpp
    I2C.cpp
    SPI.cpp
    STM.cpp
)

target_link_libraries(malahit
    ${ALSA_LIBRARIES}
    gpiod
)

install(TARGETS malahit DESTINATION bin)
