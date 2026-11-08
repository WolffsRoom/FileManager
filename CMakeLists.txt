cmake_minimum_required(VERSION 3.14)

# File Manager: FMVita's capabilities on the system's native UI framework (ScePaf).
#
# Standard hard-float VitaSDK: ScePaf takes float arguments in VFP registers (a
# softfp build crashed in transition::DoReverse with -100.0f read as a pointer).
if(NOT DEFINED ENV{VITASDK})
  message(FATAL_ERROR "VITASDK not set. Use scripts/build.sh (runs the softfp Docker image).")
endif()
set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "Vita toolchain")

project(FileManager C CXX ASM)
include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)

set(VITA_APP_NAME "File Manager")
set(VITA_TITLEID  "FILEMGR01")
set(VITA_VERSION  "00.50")

find_package(Python3 REQUIRED COMPONENTS Interpreter)

# Flags ----------------------------------------------------------------------
set(COMMON_FLAGS "-O2 -mword-relocations -Wall -Wno-unknown-pragmas -Wno-unused-variable -Wno-unused-function -Wno-sign-compare -Wno-format-truncation")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu11 ${COMMON_FLAGS}")
# -fshort-wchar: paf::wstring is UTF-16 on the system side.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11 ${COMMON_FLAGS} -fshort-wchar -include limits -fno-rtti -fno-exceptions -fno-threadsafe-statics -Wno-reorder -Wno-class-memaccess")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-q -Wl,--no-wchar-size-warning -Wl,--wrap=sceIoOpen -Wl,--wrap=sceIoRead -Wl,--wrap=sceIoWrite -Wl,--wrap=sceIoClose")
set(VITA_MAKE_FSELF_FLAGS "${VITA_MAKE_FSELF_FLAGS} -a 0x2808000000000000")
set(VITA_MKSFOEX_FLAGS "${VITA_MKSFOEX_FLAGS} -d PARENTAL_LEVEL=1 -d ATTRIBUTE=33408 -d ATTRIBUTE_MINOR=16")
# Localized title shown by the LiveArea (content_manager style) and the system.
# The spaces are U+00A0: vita.cmake splits these flags on ASCII spaces.
# TITLE_07 = Portuguese, TITLE_17 = Portuguese (Brazil).
set(VITA_MKSFOEX_FLAGS "${VITA_MKSFOEX_FLAGS} -s TITLE_07=Gerenciador de arquivos -s TITLE_17=Gerenciador de arquivos")

# ScePaf stubs ---------------------------------------------------------------
# Generated from paf/nids/ScePaf.yml. That file omits the C/C++ runtime symbols
# (memset, new, __cxa_*) so they resolve to newlib/libstdc++: crt0 calls them
# before ScePaf is loaded.
set(PAF_STUB_DIR "${CMAKE_BINARY_DIR}/paf_stubs")
if(NOT EXISTS "${PAF_STUB_DIR}/CMakeLists.txt")
  execute_process(
    COMMAND vita-libs-gen -c "${CMAKE_SOURCE_DIR}/paf/nids/ScePaf.yml" "${CMAKE_SOURCE_DIR}/paf/nids/SceCommonGuiDialog.yml" "${PAF_STUB_DIR}"
    RESULT_VARIABLE PAF_GEN_RESULT)
  if(NOT PAF_GEN_RESULT EQUAL 0)
    message(FATAL_ERROR "vita-libs-gen failed: ${PAF_GEN_RESULT}")
  endif()
endif()
add_subdirectory("${PAF_STUB_DIR}" "${CMAKE_BINARY_DIR}/paf_stubs_build" EXCLUDE_FROM_ALL)

# VitaShell's PFS bridge, used to expose protected application data.
add_subdirectory(modules/kernel)
add_subdirectory(modules/user)
add_subdirectory(modules/usbdevice)

# Resource -------------------------------------------------------------------
file(GLOB RCO_INPUTS CONFIGURE_DEPENDS resource/*.xml resource/locale/*.xml resource/texture/* resource/ui/*.png)
set(RCO_FILE "${CMAKE_BINARY_DIR}/file_manager_plugin.rco")
set(RCO_HEADER "${CMAKE_BINARY_DIR}/generated/file_manager_plugin.h")
add_custom_command(
  OUTPUT ${RCO_FILE} ${RCO_HEADER}
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/generated"
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tools/rcoc.py" compile
          "${CMAKE_SOURCE_DIR}/resource/file_manager_plugin.xml" -o "${RCO_FILE}" -x "${RCO_HEADER}"
  DEPENDS ${RCO_INPUTS} tools/rcoc.py tools/rco_schema.json
  COMMENT "Compiling file_manager_plugin.rco")
add_custom_target(file_manager_rco DEPENDS ${RCO_FILE} ${RCO_HEADER})

# Application ----------------------------------------------------------------
add_executable(FileManager
  src/paf_runtime.cpp
  src/main.cpp
  src/event.cpp
  src/dialog.cpp
  src/option_menu.cpp
  src/browser_menu.cpp
  src/settings.cpp
  src/settings_menu.cpp
  src/preview_menu.cpp
  src/ftp.cpp
  src/ftp_menu.cpp
  src/text_view.cpp
  src/info_menu.cpp
  src/notice_menu.cpp
  src/music_menu.cpp
  src/qr_menu.cpp
  src/usb.cpp
  src/usb_menu.cpp
  third_party/quirc_src/lib/quirc.c
  third_party/quirc_src/lib/decode.c
  third_party/quirc_src/lib/identify.c
  third_party/quirc_src/lib/version_db.c
  src/core/music.c
  legacy/audio/player.c
  legacy/audio/mp3player.c
  legacy/audio/mp3xing.c
  legacy/audio/id3.c
  legacy/audio/oggplayer.c
  legacy/audio/vita_audio.c
  legacy/libmad/bit.c
  legacy/libmad/decoder.c
  legacy/libmad/fixed.c
  legacy/libmad/frame.c
  legacy/libmad/huffman.c
  legacy/libmad/layer12.c
  legacy/libmad/layer3.c
  legacy/libmad/stream.c
  legacy/libmad/synth.c
  legacy/libmad/timer.c
  src/core/bubble.c
  src/core/install.c
  legacy/sha1.c
  src/text_input.cpp
  src/format.cpp
  src/core/fs.cpp
  src/core/pfs.c
  src/core/psarc.c
  src/core/fileops.c
	  src/core/download.c
  legacy/strnatcmp.c
)
add_dependencies(FileManager file_manager_rco)
add_dependencies(FileManager kernel.skprx-self user.suprx-self vitashell_user_stubs usbdevice.skprx-self)

# FMVita's audio engine and libmad are older C: keep GCC 15's stricter defaults as warnings.
file(GLOB LEGACY_AUDIO_SOURCES legacy/audio/*.c legacy/libmad/*.c)
set_source_files_properties(${LEGACY_AUDIO_SOURCES} PROPERTIES COMPILE_OPTIONS
  "-Wno-error=implicit-function-declaration;-Wno-error=incompatible-pointer-types;-Wno-error=int-conversion;-Wno-implicit-function-declaration;-Wno-incompatible-pointer-types;-Wno-int-conversion;-Wno-pointer-sign;-std=gnu99")

target_include_directories(FileManager PRIVATE
  src
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/third_party/quirc_src/lib
  "${CMAKE_BINARY_DIR}/generated"
  paf
  paf/sdk_wrapper
  paf/include
	modules/kernel
	modules/user
)

target_link_libraries(FileManager
  ScePaf_stub
  SceCommonGuiDialog_stub
  SceSysmodule_stub
  SceLibKernel_stub
  SceKernelThreadMgr_stub
  SceAppUtil_stub
  SceAppMgr_stub
  ScePower_stub
  SceCommonDialog_stub
  SceCtrl_stub
  SceTouch_stub
  SceIofilemgr_stub
  SceGxm_stub
  SceDisplay_stub
  SceMotion_stub
  SceAudio_stub
  SceRtc_stub
  SceAvPlayer_stub
  SceFios2_stub
  SceMusicExport_stub
  SceCamera_stub
  SceMtpIfDriver_stub
  SceUsbstorVStorDriver_stub
  taihen_stub
  SceVshBridge_stub
  ftpvita
  archive
  vorbisfile
  vorbis
  ogg
  png
  zstd
  lzma
  bz2
  crypto
  expat
  z
  ScePromoterUtil_stub
  SceNet_stub
  SceNetCtl_stub
	  SceHttp_stub
	  SceSsl_stub
  SceKernelModulemgr_stub
  taihen_stub
  ${CMAKE_BINARY_DIR}/modules/user/libVitaShellUser_stub_weak.a
  c
  m
)

vita_create_self(eboot.bin FileManager UNSAFE)
vita_create_vpk(FileManager.vpk ${VITA_TITLEID} eboot.bin
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
  FILE
    ${RCO_FILE} resource/file_manager_plugin.rco
	${CMAKE_BINARY_DIR}/modules/kernel/kernel.skprx module/kernel.skprx
	${CMAKE_BINARY_DIR}/modules/user/user.suprx module/user.suprx
	${CMAKE_BINARY_DIR}/modules/usbdevice/usbdevice.skprx module/usbdevice.skprx
	resources/umass.skprx module/umass.skprx
    legacy/resources/head.bin resource/head.bin
    resource/sce_sys/icon0.png sce_sys/icon0.png
    resource/sce_sys/pic0.png sce_sys/pic0.png
    resource/sce_sys/livearea/contents/gate.png sce_sys/livearea/contents/gate.png
    resource/sce_sys/livearea/contents/labg.png sce_sys/livearea/contents/labg.png
    resource/sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
)
