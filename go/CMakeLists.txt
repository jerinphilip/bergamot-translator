# SWIG integration
find_package(SWIG 4.0.2)
if(SWIG_FOUND)
    include (UseSWIG)
    set(IPATH ".")
    set (SWIG_FILE "go_bergamot.i")
    set_source_files_properties(${IPATH}/${SWIG_FILE} PROPERTIES CPLUSPLUS ON)
    
    find_program(GO_EXECUTABLE go)
    if (GO_EXECUTABLE)
        message("Bindings for Go will be generated using ${GO_EXECUTABLE}")
        set(SWIG_MODULE_NAME "go_bergamot")
        set(CMAKE_SWIG_FLAGS -c++ -go -cgo -intgosize 64)
        message("CMAKE_SWIG_FLAGS: ${CMAKE_SWIG_FLAGS}")

        swig_add_library(${SWIG_MODULE_NAME} 
            TYPE SHARED 
            LANGUAGE go 
            OUTPUT_DIR .
            SOURCES ${IPATH}/${SWIG_FILE}
        )

        get_property(INCDIRS DIRECTORY ../3rd_party/marian-dev/src PROPERTY INCLUDE_DIRECTORIES)
        target_include_directories(${SWIG_MODULE_NAME} PUBLIC ${INCDIRS})
        
        get_property(INCLUDE_DIRECTORIES DIRECTORY ../3rd_party/ssplit-cpp/src PROPERTY INCLUDE_DIRECTORIES)
        target_include_directories(${SWIG_MODULE_NAME} PUBLIC ${INCLUDE_DIRECTORIES})

        swig_link_libraries(${SWIG_MODULE_NAME} PUBLIC bergamot-translator)

        set_target_properties(${SWIG_MODULE_NAME} PROPERTIES SWIG_USE_TARGET_INCLUDE_DIRECTORIES TRUE)
    else()
        message("Go executable not found, golang integration not done")
    endif()
else()
    message("Swig not found, integration not done")
endif()