add_executable(logfile_test LogFile_test.cc)
target_link_libraries(logfile_test muduo_base)

add_executable(logging_test Logging_test.cc)
target_link_libraries(logging_test muduo_base)

add_executable(asyncLogging_test AsyncLogging_test.cc)
target_link_libraries(asyncLogging_test muduo_base)

add_executable(asyncLogging_bench AsyncLogging_bench.cc)
target_link_libraries(asyncLogging_bench muduo_base)
