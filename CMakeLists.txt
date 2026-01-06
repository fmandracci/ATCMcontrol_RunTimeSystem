cmake_minimum_required(VERSION 3.16)

project(FarosPLC VERSION 3.000)
project(FarosPLC LANGUAGES C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(BUILD_SOURCES
	main.c
	plc/plc.c
	src/crosstable.c
	src/dataClients.c
	src/dataClock.c
	src/dataDiagnostics.c
	src/dataEngine.c
	src/dataFastIO.c
	src/dataHmiPlc.c
	src/dataImpl.c
	src/dataServers.c
	src/dataUtils.c
	src/hmi_plc.c
	src/system_ini.c
	src/xx_canopen.c
	src/xx_gpio.c
	src/xx_mect.c
	src/xx_pthread.c
	src/xx_retentives.c
)

set(BUILD_HEADERS
        main.h
	plc/plc.h
	src/crosstable.h
	src/dataImpl.h
	src/hmi_plc.h
	src/system_ini.h
	src/xx_canopen.h
	src/xx_gpio.h
	src/xx_mect.h
	src/xx_pthread.h
	src/xx_retentives.h
)

add_executable(fcrts ${BUILD_SOURCES} ${BUILD_HEADERS})

# no add_compile_definitions( )
target_compile_definitions(fcrts PUBLIC KIT_RPI4)

target_compile_definitions(fcrts PUBLIC _GNU_SOURCE)
target_compile_options(fcrts PUBLIC
        -Werror-implicit-function-declaration
	-Wall -Wno-attributes -fno-builtin -fno-strict-aliasing
	-Wall -Wextra
	-O3
)

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

target_link_libraries(fcrts m)

pkg_check_modules(LIBMODBUS REQUIRED libmodbus)
include_directories(${LIBMODBUS_INCLUDE_DIRS})
target_link_libraries(fcrts ${LIBMODBUS_LIBRARIES})

pkg_check_modules(GPIOD REQUIRED libgpiod)
include_directories(${GPIOD_INCLUDE_DIRS})
target_link_libraries(fcrts ${GPIOD_LIBRARIES})

#	target_include_directories(FarosPLC INTERFACE ${CMAKE_SOURCE_DIR})
#	target_link_libraries(fcrts ${CMAKE_THREAD_LIBS_INIT} rt m)

#	set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static")

include(GNUInstallDirs)
install(TARGETS fcrts
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
