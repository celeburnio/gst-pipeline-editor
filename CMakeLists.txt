cmake_minimum_required (VERSION 3.0)

include(GNUInstallDirs)

project(gst-pipeline-editor VERSION "0.0.0" LANGUAGES C)

find_package(PkgConfig)
pkg_check_modules(GST_PE_DEPS REQUIRED  gio-2.0
                                        gstreamer-1.0>=1.16.0
                                        gstreamer-base-1.0>=1.1.6.0)

configure_file(src/gst-pe-config.h.in src/gst-pe-config.h @ONLY)

add_executable(${PROJECT_NAME}
  src/main.c
  src/gst-pipeline-editor.c)

set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 99)
target_compile_options(${PROJECT_NAME} PUBLIC "-Wall" ${GST_PE_DEPS_CFLAGS_OTHER})
target_include_directories(${PROJECT_NAME} PUBLIC ${GST_PE_DEPS_INCLUDE_DIRS} ${CMAKE_BINARY_DIR}/src)
target_link_libraries(${PROJECT_NAME} ${GST_PE_DEPS_LDFLAGS})
install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
