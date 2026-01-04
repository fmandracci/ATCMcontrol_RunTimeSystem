cmake_minimum_required(VERSION 3.16)

project(FarosPLC VERSION 3.000)
project(FarosPLC LANGUAGES C)

set(BUILD_SOURCES
	main.c
	CANopen/CANopen.c
	ioData/crosstable.c
	ioData/dataClients.c
#	ioData/dataClock.c
	ioData/dataDiagnostics.c
	ioData/dataEngine.c
#	ioData/dataFastIO.c
	ioData/dataFbMect.c
	ioData/dataHmiPlc.c
	ioData/dataImpl.c
	ioData/dataMain.c
	ioData/dataRetentives.c
	ioData/dataServers.c
	ioData/dataUtils.c
	ioData/hmi_plc.c
	ioData/system_ini.c
#	ioData/xx_aes.c
#	ioData/xx_gpio.c
#	ioData/xx_lpc.c
#	ioData/xx_spi.c
	ioTest/tstImpl.c
	ioTest/tstMain.c
	ioTest/tstSimu.c
	osKernel/fcAdapt.c
	osKernel/mectAdapt.c
	osKernel/osFieldB.c
	osKernel/osFlash.c
	osKernel/osLib.c
	osKernel/osMain.c
	osKernel/osTask.c
	osShared/osLoad.c
	osShared/osMsg.c
	osShared/osUtil.c
	vmKernel/actDFull.c
	vmKernel/actDIncr.c
	vmKernel/actDebug.c
	vmKernel/actDown.c
	vmKernel/actEtc.c
	vmKernel/actFile.c
	vmKernel/dbiMain.c
	vmKernel/fileMain.c
#	vmKernel/intArith.c
#	vmKernel/intConv.c
	vmKernel/intInter.c
	vmKernel/intLevel.c
	vmKernel/intMain.c
#	vmKernel/intWid16.c
#	vmKernel/intWid32.c
#	vmKernel/intWid64.c
#	vmKernel/intWid8.c
#	vmKernel/intWide.c
	vmKernel/ioMain.c
	vmKernel/md5.c
	vmKernel/vmMain.c
	vmKernel/vmPrc.c
	vmKernel/vmTimer.c
	vmKernel/vmmAct.c
	vmKernel/vmmBreak.c
	vmKernel/vmmCmd.c
	vmKernel/vmmCom.c
	vmKernel/vmmLoad.c
	vmKernel/vmmMain.c
	vmKernel/vmmMsg.c
	vmKernel/vmmOnlCh.c
	vmKernel/vmmRet.c
	vmKernel/vmmSys.c
	vmKernel/vmmUtil.c
	vmLib/libDef.c
	vmLib/libFile.c
	vmLib/libIec.c
	vmLib/libModbus.c
	vmLib/libSfc.c
	vmLib/libSys.c
	vmLib/libSys2.c
	vmLib/libUtil.c
)

set(PROJECT_SOURCES
	${BUILD_SOURCES}
	vmKernel/intArith.c
	vmKernel/intConv.c
	vmKernel/intWide.c
	vmKernel/intWid8.c
	vmKernel/intWid16.c
	vmKernel/intWid32.c
	vmKernel/intWid64.c
)

set(BUILD_HEADERS
	inc.data/CANopen.h
	inc.data/dataMain.h
	inc.fc/fcDef.h
	inc.fc/fcMain.h
	inc.mect/mectMain.h
	inc/BuildNr.h
	inc/intDef.h
	inc/intOpcds.h
	inc/iolDef.h
	inc/libDef.h
	inc/md5.h
	inc/osAlign.h
	inc/osDef.h
	inc/osFile.h
	inc/osFirst.h
	inc/osLast.h
	inc/osSocket.h
	inc/osTarget.h
	inc/stdInc.h
	inc/vmShared.h
	inc/vmmDef.h
	inc/vmmMain.h
	ioData/crosstable.h
	ioData/dataImpl.h
	ioData/hmi_plc.h
	ioData/system_ini.h
#	ioData/xx_aes.h
#	ioData/xx_gpio.h
#	ioData/xx_lpc.h
#	ioData/xx_spi.h
	vmKernel/vmmSys.h
	vmLib/fbIec.h
	vmLib/fbSys.h
	vmLib/fbUtil.h
	vmLib/funFile.h
	vmLib/funIec.h
	vmLib/funSys.h
	vmLib/funSys2.h
	vmLib/funSys22.h
	vmLib/funUtil.h
	vmLib/libFile.h
	vmLib/libIec.h
	vmLib/libModbus.h
	vmLib/libSfc.h
	vmLib/libSys.h
	vmLib/libSys2.h
	vmLib/libUtil.h
)

add_executable(fcrts ${BUILD_SOURCES})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# no add_compile_definitions( )
target_compile_definitions(fcrts PUBLIC _SOF_4CPC_SRC_)
target_compile_definitions(fcrts PUBLIC TP3000)
target_compile_definitions(fcrts PUBLIC USE_PTHREAD_PRIO_INHERIT)
target_compile_definitions(fcrts PUBLIC USE_LIBMODBUS)

add_compile_options(
	-D_GNU_SOURCE -Werror-implicit-function-declaration
	-Wall -Wno-attributes -fno-builtin -fno-strict-aliasing
	-Wall -Wextra
	-O3
)

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

target_link_libraries(fcrts m)

#	pkg_check_modules(LIBMODBUS REQUIRED libmodbus)
#	include_directories(${LIBMODBUS_INCLUDE_DIRS})
#	target_link_libraries(fcrts ${LIBMODBUS_LIBRARIES})

#	pkg_check_modules(GPIOD REQUIRED libgpiod)
#	include_directories(${GPIOD_INCLUDE_DIRS})
#	target_link_libraries(fcrts ${GPIOD_LIBRARIES})

#	target_include_directories(FarosPLC INTERFACE ${CMAKE_SOURCE_DIR})
#	target_link_libraries(fcrts ${CMAKE_THREAD_LIBS_INIT} rt m)

#	set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static")

include(GNUInstallDirs)
install(TARGETS fcrts
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
