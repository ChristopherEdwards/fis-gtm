#################################################################
#								#
#	Copyright 2013, 2014 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

if("${CMAKE_SIZEOF_VOID_P}" EQUAL 4)
  set(arch "x86")
  set(bits 32)
  set(FIND_LIBRARY_USE_LIB64_PATHS FALSE)
  # Set arch to i586 in order to compile for Galileo
  set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -march=i586")
  set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -Wa,-march=i586")
else()
  set(arch "x86_64")
  set(bits 64)
endif()

# Platform directories
list(APPEND gt_src_list sr_linux)
if(${bits} EQUAL 32)
  list(APPEND gt_src_list sr_i386 sr_x86_regs sr_unix_nsb)
else()
  list(APPEND gt_src_list sr_x86_64 sr_x86_regs)
  set(gen_xfer_desc 1)
endif()

# Assembler
set(CMAKE_INCLUDE_FLAG_ASM "-Wa,-I") # gcc -I does not make it to "as"

# Compiler
if(${CYGWIN})
  set(CMAKE_C_FLAGS
    "${CMAKE_C_FLAGS} -U__STRICT_ANSI__ -fsigned-char -Wmissing-prototypes -Wreturn-type -Wpointer-sign -fno-omit-frame-pointer")
else()
set(CMAKE_C_FLAGS
  "${CMAKE_C_FLAGS} -ansi -fsigned-char -fPIC -Wmissing-prototypes -Wreturn-type -Wpointer-sign -fno-omit-frame-pointer")
endif()

set(CMAKE_C_FLAGS_RELEASE
  "${CMAKE_C_FLAGS_RELEASE} -fno-defer-pop -fno-strict-aliasing -ffloat-store")

add_definitions(
  #-DNOLIBGTMSHR #gt_cc_option_DBTABLD=-DNOLIBGTMSHR
  -D_GNU_SOURCE
  -D_FILE_OFFSET_BITS=64
  -D_XOPEN_SOURCE=600
  -D_LARGEFILE64_SOURCE
  )

# Linker
set(gtm_link  "-Wl,-u,gtm_filename_to_id -Wl,-u,gtm_zstatus -Wl,--version-script,\"${GTM_BINARY_DIR}/gtmexe_symbols.export\"")
set(gtm_dep   "${GTM_BINARY_DIR}/gtmexe_symbols.export")

set(libgtmshr_link "-Wl,-u,gtm_ci -Wl,-u,gtm_filename_to_id -Wl,--version-script,\"${GTM_BINARY_DIR}/gtmshr_symbols.export\"")
set(libgtmshr_dep  "${GTM_BINARY_DIR}/gtmexe_symbols.export")

if(${bits} EQUAL 32)
  set(libmumpslibs "-lncurses -lm -ldl -lc -lpthread -lrt")
else()
  set(libmumpslibs "-lelf -lncurses -lm -ldl -lc -lpthread -lrt")
endif()
