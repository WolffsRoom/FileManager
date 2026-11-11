add_executable(bginstaller
  main.c
)

target_compile_options(bginstaller PRIVATE -std=gnu11 -Wall -O2)
target_link_options(bginstaller PRIVATE -Wl,-q -nostdlib)

target_link_libraries(bginstaller
  SceLibKernel_stub
  SceKernelThreadMgr_stub
  SceIofilemgr_stub
  SceSysmodule_stub
  ScePromoterUtil_stub
  gcc
)

vita_create_self(bginstaller.suprx bginstaller CONFIG exports.yml UNSAFE)
