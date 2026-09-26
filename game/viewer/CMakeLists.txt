add_library(viewer STATIC viewer.cpp viewer.h camera_controller.cpp camera_controller.h)
target_include_directories(viewer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(viewer PUBLIC engine)
