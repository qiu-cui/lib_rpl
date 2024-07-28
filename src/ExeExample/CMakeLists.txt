set(EXE_TARGET_NAME ExeExample)

set(CMAKE_CXX_STANDARD 17)

set(${EXE_TARGET_NAME}_SRC
        main.cpp
)

add_executable(${EXE_TARGET_NAME} ${${EXE_TARGET_NAME}_SRC})

target_link_libraries(${EXE_TARGET_NAME} rpl::rpl)

install(TARGETS ${EXE_TARGET_NAME} DESTINATION bin)
