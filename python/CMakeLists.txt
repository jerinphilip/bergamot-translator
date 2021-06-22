
set_target_properties(bergamot-translator PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
add_library(pybergamot MODULE bergamot.cpp)
target_link_libraries(pybergamot PUBLIC pybind11::module bergamot-translator)
target_include_directories(pybergamot PUBLIC ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/src)
