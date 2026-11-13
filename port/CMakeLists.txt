cmake_minimum_required(VERSION 3.20)
project(gow3_nx
    VERSION 0.1.0
    LANGUAGES C CXX ASM
    DESCRIPTION "Recompiled PS3 game project (ps3recomp)"
)

# ---------------------------------------------------------------------------
# Standards
# ---------------------------------------------------------------------------
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Prefer Ninja if available
if(NOT CMAKE_GENERATOR MATCHES "Ninja" AND NOT DEFINED ENV{CMAKE_GENERATOR})
    message(STATUS "Tip: use -G Ninja for faster builds")
endif()

# ---------------------------------------------------------------------------
# The two directories this project is built from
# ---------------------------------------------------------------------------
# PS3RECOMP_DIR is the toolkit checkout: include/, runtime/, libs/, tools/.
# RECOMP_DIR is where the lifter's output for THIS game lives. They are
# separate on purpose -- a port keeps its generated code in its own tree and
# points at whatever toolkit checkout it is building against:
#
#   cmake -B build -G Ninja \
#         -DPS3RECOMP_DIR=/path/to/ps3recomp \
#         -DRECOMP_DIR=/path/to/my_game/recompiled
#

# ---- GoW3 -> Switch: capa de compatibilidad Horizon (nx/compat) ----
get_filename_component(NX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../nx" ABSOLUTE)
get_filename_component(PS3RECOMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../ref/ps3recomp" ABSOLUTE)
set(RECOMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ppu" CACHE PATH "")
if(NINTENDO_SWITCH OR CMAKE_SYSTEM_NAME STREQUAL "NintendoSwitch")
    include_directories(SYSTEM "${NX_DIR}/compat/include")
    include_directories("${NX_DIR}/compat")
    include_directories(SYSTEM "$ENV{DEVKITPRO}/portlibs/switch/include")
    add_compile_options("$<$<COMPILE_LANGUAGE:C,CXX>:SHELL:-include nx_compat.h>")
    add_compile_options($<$<COMPILE_LANGUAGE:C>:-Wno-error=incompatible-pointer-types>)
    add_library(gow3_nx_compat STATIC "${NX_DIR}/compat/nx_compat.cpp" "${NX_DIR}/compat/nx_ucontext.S"
                "${NX_DIR}/gm/guest_memory_switch.cpp")
    set(GOW3_NX ON)
    # Backend RSX Vulkan sobre el NVK estatico (el mismo SDK que usa KOF). -DGOW3_NX_VULKAN=OFF vuelve al null.
    option(GOW3_NX_VULKAN "RSX por Vulkan/NVK" ON)
    get_filename_component(GOW3_NVK_SDK_DEFAULT "${CMAKE_CURRENT_SOURCE_DIR}/../../kof2002um-nx/tools/nvk-switch-sdk" ABSOLUTE)
    set(GOW3_NVK_SDK "${GOW3_NVK_SDK_DEFAULT}" CACHE PATH "SDK de NVK (include/vulkan y lib/libvulkan.a)")
    if(GOW3_NX_VULKAN)
        set(PS3RECOMP_RSX_VULKAN ON CACHE BOOL "" FORCE)
        set(PS3RECOMP_VULKAN_INCLUDE_DIR "${GOW3_NVK_SDK}/include" CACHE PATH "" FORCE)
    endif()
endif()
if(NOT DEFINED PS3RECOMP_DIR)
    # Default: this project is still sitting inside ps3recomp/templates/project/
    get_filename_component(PS3RECOMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../" ABSOLUTE)
endif()
get_filename_component(PS3RECOMP_DIR "${PS3RECOMP_DIR}" ABSOLUTE)

set(RECOMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/recompiled" CACHE PATH
    "Directory holding the lifter's output: ppu_recomp.h and the lifted sources")
get_filename_component(RECOMP_DIR "${RECOMP_DIR}" ABSOLUTE)

message(STATUS "ps3recomp SDK:   ${PS3RECOMP_DIR}")
message(STATUS "Lifted output:   ${RECOMP_DIR}")

if(NOT EXISTS "${RECOMP_DIR}/ppu_recomp.h")
    message(FATAL_ERROR
        "No ppu_recomp.h in ${RECOMP_DIR}.\n"
        "That header and the lifted sources beside it are what "
        "tools/ppu_lifter.py writes. Run the lifter, drop its output there, "
        "or point -DRECOMP_DIR= at wherever it already is.")
endif()

# The lifted translation units. A game's lift is hundreds of generated files,
# so they are globbed rather than listed; re-run CMake after a fresh lift.
file(GLOB RECOMP_SOURCES CONFIGURE_DEPENDS
    "${RECOMP_DIR}/*.c"
    "${RECOMP_DIR}/*.cpp"
)
if(NOT RECOMP_SOURCES)
    message(FATAL_ERROR
        "${RECOMP_DIR} has a ppu_recomp.h but no .c or .cpp beside it. The "
        "lifted functions and the function table live in those; without them "
        "there is nothing to run.")
endif()
list(LENGTH RECOMP_SOURCES RECOMP_SOURCE_COUNT)
message(STATUS "Lifted sources:  ${RECOMP_SOURCE_COUNT} file(s)")

# ---------------------------------------------------------------------------
# ps3recomp runtime library
# ---------------------------------------------------------------------------
# Built from source as a subdirectory rather than imported as a prebuilt
# archive. A static library carries none of its own link requirements, so an
# imported one arrives without SDL2, pthreads, the Metal frameworks or the
# Winsock/XInput imports -- the link then fails on every platform for a
# different reason. add_subdirectory hands all of that over with the target.
add_subdirectory("${PS3RECOMP_DIR}" "${CMAKE_BINARY_DIR}/ps3recomp_sdk"
                 EXCLUDE_FROM_ALL)
# 57a: frame records in the runtime so RSX_WALKER_PROF can walk the RSX thread's stack.
if(NOT MSVC)
    target_compile_options(ps3recomp_runtime PRIVATE -fno-omit-frame-pointer -mno-omit-leaf-frame-pointer)
endif()

# ---- GoW3 -> Switch: glslang (HLSL -> SPIR-V) para los shaders del juego ----
# Fuente: glslang 11.0.0 de kof2002um-nx, compilado aca con HLSL (KOF lo compila sin).
# Sin spirv-opt (ENABLE_OPT OFF): no corre la legalizacion de HLSL.
option(GOW3_NX_GLSLANG "Shaders del juego por glslang" ON)
if(GOW3_NX AND GOW3_NX_VULKAN AND GOW3_NX_GLSLANG)
    get_filename_component(GOW3_GLSLANG_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../../kof2002um-nx/tools/rexglue-sdk/thirdparty/glslang" ABSOLUTE)
    set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
    set(ENABLE_SPVREMAPPER OFF CACHE BOOL "" FORCE)
    set(ENABLE_HLSL ON CACHE BOOL "" FORCE)
    set(ENABLE_OPT OFF CACHE BOOL "" FORCE)
    set(ENABLE_CTEST OFF CACHE BOOL "" FORCE)
    set(ENABLE_PCH OFF CACHE BOOL "" FORCE)
    set(BUILD_EXTERNAL OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(SKIP_GLSLANG_INSTALL ON CACHE BOOL "" FORCE)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
    set(UNIX 1)   # glslang elige la capa de SO por WIN32/UNIX; la de Unix anda sobre los pthreads de libnx
    add_subdirectory("${GOW3_GLSLANG_SRC}" "${CMAKE_BINARY_DIR}/glslang" EXCLUDE_FROM_ALL)
    unset(UNIX)
    foreach(t glslang SPIRV MachineIndependent GenericCodeGen OGLCompiler OSDependent HLSL)
        if(TARGET ${t})
            target_compile_definitions(${t} PRIVATE _GNU_SOURCE)   # por el -include nx_compat.h (siginfo_t)
        endif()
    endforeach()
    add_library(gow3_glslang_limits STATIC "${GOW3_GLSLANG_SRC}/StandAlone/ResourceLimits.cpp")
    target_include_directories(gow3_glslang_limits PRIVATE "${GOW3_GLSLANG_SRC}")
    target_compile_definitions(gow3_glslang_limits PRIVATE _GNU_SOURCE)
    target_compile_definitions(ps3recomp_runtime PRIVATE PS3RECOMP_HAVE_SPIRV_TRANSLATION=1)
    target_include_directories(ps3recomp_runtime PRIVATE "${NX_DIR}/compat/glslang_shim" "${GOW3_GLSLANG_SRC}")
    target_link_libraries(ps3recomp_runtime PRIVATE gow3_glslang_limits glslang SPIRV)
endif()

# ---------------------------------------------------------------------------
# The HLE NID table
# ---------------------------------------------------------------------------
# Maps each firmware import NID onto the handler in the runtime that answers
# it. A port may keep a hand-curated one beside its lift; otherwise generate
# the canonical every-module table, which is what the toolkit's own boot
# harness uses.
if(EXISTS "${RECOMP_DIR}/ppu_hle_nids.cpp")
    set(HLE_NID_TABLE "${RECOMP_DIR}/ppu_hle_nids.cpp")
    message(STATUS "HLE NID table:   ${HLE_NID_TABLE}")
else()
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    set(HLE_NID_TABLE "${CMAKE_CURRENT_BINARY_DIR}/ppu_hle_nids.cpp")
    # The generator discovers handlers from library definitions. Rebuild when
    # an existing module changes, and reconfigure when a new module is added.
    file(GLOB_RECURSE HLE_NID_SOURCES CONFIGURE_DEPENDS
        "${PS3RECOMP_DIR}/libs/*.c")
    # A removed input cannot make the output older than a remaining dependency.
    # Track the list itself so deletions also remove stale handler references.
    set(HLE_NID_SOURCE_LIST "${CMAKE_CURRENT_BINARY_DIR}/hle_nid_sources.txt")
    file(GENERATE OUTPUT "${HLE_NID_SOURCE_LIST}" CONTENT "${HLE_NID_SOURCES}\n")
    add_custom_command(
        OUTPUT "${HLE_NID_TABLE}"
        COMMAND "${Python3_EXECUTABLE}"
                "${PS3RECOMP_DIR}/tools/gen_hle_nids.py"
                --out "${HLE_NID_TABLE}" --all
        DEPENDS "${PS3RECOMP_DIR}/tools/gen_hle_nids.py"
                "${PS3RECOMP_DIR}/tools/nid_database.py"
                ${HLE_NID_SOURCES} "${HLE_NID_SOURCE_LIST}"
        COMMENT "Generating the HLE NID -> handler table")
    message(STATUS "HLE NID table:   generated (every module)")
endif()

# ---------------------------------------------------------------------------
# The game executable
# ---------------------------------------------------------------------------
# The PPU boot scaffold is compiled here rather than taken from the runtime
# library: every one of these files #includes the lifter's per-game
# ppu_recomp.h, so they can only be built against a particular lift.
set(PS3RECOMP_SCAFFOLD
    "${PS3RECOMP_DIR}/runtime/ppu/ppu_loader.cpp"
    "${PS3RECOMP_DIR}/runtime/ppu/ppu_hle.cpp"
    "${PS3RECOMP_DIR}/runtime/ppu/ppu_fs.cpp"
    "${PS3RECOMP_DIR}/runtime/ppu/ppu_sysprx.cpp"
)

# ---------------------------------------------------------------------------
# Lifted SPU images (optional)
# ---------------------------------------------------------------------------
# A title that runs SPU work gets its images lifted into <SPU_DIR>/<image>/ by
# tools/spu_lifter.py, with a registration TU generated by
# tools/build_spu_workloads.py. cellSpurs dispatches them by content
# fingerprint, so the generated table is keyed to the images THIS build ships:
# a table generated from another release registers signatures that can never
# match, which is worse than shipping none. Both the table and the job-module
# registry self-register from constructors and are therefore optional -- a tree
# that has not lifted its SPU images yet configures and links without them.
set(SPU_DIR "${CMAKE_CURRENT_SOURCE_DIR}/spu" CACHE PATH
    "Directory holding the lifted SPU images (<SPU_DIR>/<image>/spu_recomp.c)")
option(PORT_ENABLE_SPU "Compile and register the title's lifted SPU images" ON)

set(PORT_SPU_INPUTS "" CACHE STRING
    "Input files to scan for all embedded SPU ELF programs (semicolon-separated decrypted ELF/PRX/data paths)")
set(SPU_SOURCES "")
if(PORT_ENABLE_SPU AND PORT_SPU_INPUTS)
    include("${PS3RECOMP_DIR}/cmake/PS3RecompSPU.cmake")
    ps3recomp_generate_spu("${CMAKE_CURRENT_BINARY_DIR}/generated_spu" ${PORT_SPU_INPUTS})
    list(APPEND SPU_SOURCES ${SPU_GENERATED_SOURCES}
         "${PS3RECOMP_DIR}/runtime/spu/spu_dispatch_mt.c")
elseif(PORT_ENABLE_SPU)
    file(GLOB SPU_LIFTED CONFIGURE_DEPENDS "${SPU_DIR}/*/spu_recomp.c")
    if(SPU_LIFTED)
        list(LENGTH SPU_LIFTED SPU_IMAGE_COUNT)
        message(STATUS "Lifted SPU:      ${SPU_IMAGE_COUNT} image(s)")
        # The musttail indirect-branch fast path. Excluded from the runtime
        # library on purpose (see the SDK root CMakeLists): it must be built
        # here, by clang, or the tail call degrades to a plain call and a guest
        # loop that branches indirectly walks the thread stack off its guard
        # page a few thousand iterations in.
        list(APPEND SPU_SOURCES ${SPU_LIFTED}
             "${PS3RECOMP_DIR}/runtime/spu/spu_dispatch_mt.c")
        # Generated beside the lift; each is present only once its generator
        # has been run, so each is included only if it is actually here.
        # WWS jobmods relocatables (tools/lift_reloc_jobmods.py, --reloc a base 0x4000),
        # registrados por reloc_register.c (tools/register_reloc_jobmods.py).
        file(GLOB SPU_RELOC_LIFTED CONFIGURE_DEPENDS "${SPU_DIR}/reloc/*/spu_recomp.c")
        if(SPU_RELOC_LIFTED AND EXISTS "${SPU_DIR}/reloc_register.c")
            list(LENGTH SPU_RELOC_LIFTED SPU_RELOC_COUNT)
            message(STATUS "Reloc jobmods:   ${SPU_RELOC_COUNT}")
            list(APPEND SPU_SOURCES ${SPU_RELOC_LIFTED} "${SPU_DIR}/reloc_register.c")
        endif()
        foreach(gen IN ITEMS spu_workloads.c jobmods_register.c taskset_policy_bytes.c wwspm_register.c)
            if(EXISTS "${SPU_DIR}/${gen}")
                list(APPEND SPU_SOURCES "${SPU_DIR}/${gen}")
            else()
                message(STATUS "  (no ${gen} -- not registered)")
            endif()
        endforeach()
    else()
        message(STATUS "Lifted SPU:      none found in ${SPU_DIR}")
    endif()
endif()

add_executable(${PROJECT_NAME}
    main.cpp
    stubs.cpp
    lfqueue.cpp
    ${PS3RECOMP_SCAFFOLD}
    ${HLE_NID_TABLE}
    ${RECOMP_SOURCES}
    ${SPU_SOURCES}
)

# The lifted SPU sources use bare #include "spu_context.h". Scope that include
# directory (and their flags) to those TUs alone: adding it to the whole target
# changes the compile command of every lifted PPU TU, and those are large enough
# that the needless rebuild is measured in tens of minutes.
#   SPU_USE_DISPATCH_MT -- route SPU_IB_DISPATCH through the musttail path.
#   -O2                 -- the target-wide -O1 exists for the huge PPU TUs; the
#                          SPU sources are small and decode-hot, so they get
#                          full optimization. The last -O on the line wins.
# 38b: SPU_LS_NO_DIAG (rigaz29/ps3recomp c0925006): the lifted SPU code's lqd/stqd lose the
# LS diagnostics gate (Ascension-R: RenderJobs -7.7%, EDGE -10.5%). The interpreter and the
# runtime keep theirs. -DGOW3_SPU_LS_DIAG=ON brings it back for SPU_LS_WATCH/SPU_SMC_WATCH.
option(GOW3_SPU_LS_DIAG "LS diagnostics gate in the lifted SPU code" OFF)
set(GOW3_SPU_DEFS SPU_USE_DISPATCH_MT)
if(NOT GOW3_SPU_LS_DIAG)
    list(APPEND GOW3_SPU_DEFS SPU_LS_NO_DIAG)
endif()
if(SPU_SOURCES)
    set_source_files_properties(${SPU_SOURCES} PROPERTIES
        INCLUDE_DIRECTORIES "${PS3RECOMP_DIR}/runtime/spu"
        COMPILE_DEFINITIONS "${GOW3_SPU_DEFS}")
    if(NOT MSVC OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set_source_files_properties(${SPU_SOURCES} PROPERTIES COMPILE_OPTIONS "-O2")
    endif()
endif()

# app.rc carries the icon, the version block and the manifest, and needs a
# resource compiler. Listing it as a source on a host that has none is not a
# link error, it is a "no rule to make" for a file nothing can consume.
if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/app.rc")
    target_sources(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/app.rc")
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    "${RECOMP_DIR}"                            # the generated ppu_recomp.h
    "${PS3RECOMP_DIR}/include"
    "${PS3RECOMP_DIR}/runtime/ppu"
    "${PS3RECOMP_DIR}/runtime/spu"
    "${PS3RECOMP_DIR}/runtime/memory"
    "${PS3RECOMP_DIR}/runtime/syscalls"
    "${PS3RECOMP_DIR}/runtime/platform"        # win32_compat.h and friends
    "${PS3RECOMP_DIR}/libs/video"
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ps3recomp_runtime
)

# The runtime is C++, and on Apple also Objective-C. Letting CMake infer the
# link language from the generated C would leave the C++ standard library out.
set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX)

# Windows system import libraries. dbghelp symbolizes crash-handler addresses
# against the PDB, user32 and gdi32 are what the backend's window and its GDI
# present are made of, and winmm is timeBeginPeriod. None of the four names
# anything a POSIX linker can find; the D3D12 backend's own imports come in
# through the #pragma comment(lib) in libs/video/rsx_d3d12_backend.c.
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        dbghelp
        user32
        gdi32
        winmm
    )
endif()

# ---------------------------------------------------------------------------
# Compiler-specific flags
# ---------------------------------------------------------------------------
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall -Wextra -Wno-unused-parameter
        -fno-strict-aliasing
        # The lift spells PPC arithmetic as signed C arithmetic, which must wrap;
        # without this, clang exploits overflow UB (e.g. folds "x*x < 0" to false).
        -fwrapv
        # The target-wide -O1 the SPU note above relies on, for a build whose
        # configuration sets no optimisation level (no CMAKE_BUILD_TYPE, or one
        # CMake has no defaults for). Without it every lifted PPU TU compiled at
        # -O0: each static inline helper (ppc_rlw64, vmx_w, vm_read32_inl) is a
        # real call that spills to the stack, and those calls were most of the
        # PPU thread's profile. -O1 keeps the huge TUs' build time sane. Debug
        # and the optimised configurations keep their own levels.
        $<$<NOT:$<CONFIG:Debug,Release,RelWithDebInfo,MinSizeRel>>:-O1>
    )
elseif(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W3 /wd4100 /wd4244 /wd4267
        /Zc:__cplusplus         # report correct __cplusplus value
        # ppu_recomp.h defines PPU_FENCE, the lowering of the guest's
        # sync/lwsync/eieio/isync, and in C that is <stdatomic.h>. The lifter
        # writes C++ by default, but --single-file writes ppu_recomp.c, and a
        # port that took it would not compile without this. The runtime's own
        # build already carries the same flag for the same reason.
        /experimental:c11atomics
    )
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        _CRT_SECURE_NO_WARNINGS
    )
endif()

# The lifted translation units are machine-generated and can run to hundreds of
# thousands of lines each, so they get their own flags:
#
#   - warnings off. They are about generated code and nobody will act on them,
#     and at this volume they bury the diagnostics that matter.
#   - no floating-point contraction. arm64 fuses a multiply and an add by
#     default; a PPC fmadd is spelt as one where the lifter means one.
#   - /bigobj on MSVC. A single lifted file routinely passes the 65,536-section
#     limit of the default object format.
if(MSVC)
    set_source_files_properties(${RECOMP_SOURCES} PROPERTIES
        COMPILE_OPTIONS "/w;/bigobj")
else()
    set_source_files_properties(${RECOMP_SOURCES} PROPERTIES
        COMPILE_OPTIONS "-w;-ffp-contract=off")
endif()

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)

# ---------------------------------------------------------------------------
# The window launcher (macOS)
# ---------------------------------------------------------------------------
# A title that replaces its executable (sys_game_process_exitspawn, done by a
# port as an exec) loses its window with the old process image. The launcher
# holds the window for the whole session instead: it runs the port with
# PS3RECOMP_HOST_SURFACES set, the Metal backend renders into IOSurfaces it
# shares (libs/video/rsx_host_surface.h), and the image an exec brings up
# attaches to the same window again. One Objective-C file and no runtime
# library, so it builds in seconds:
#
#   ./MyGameRecomp_window [--title T] [--size WxH] -- ./MyGameRecomp <args>
if(APPLE)
    enable_language(OBJC)
    add_executable(${PROJECT_NAME}_window "${PS3RECOMP_DIR}/tools/host_window/host_window.m")
    target_include_directories(${PROJECT_NAME}_window PRIVATE "${PS3RECOMP_DIR}/libs/video")
    target_compile_definitions(${PROJECT_NAME}_window PRIVATE HOST_WINDOW_TITLE="${PROJECT_NAME}")
    target_compile_options(${PROJECT_NAME}_window PRIVATE -fobjc-arc -Wall -Wextra -Wno-unused-parameter)
    target_link_libraries(${PROJECT_NAME}_window PRIVATE
        "-framework AppKit" "-framework Metal" "-framework QuartzCore" "-framework IOSurface")
    install(TARGETS ${PROJECT_NAME}_window RUNTIME DESTINATION bin)
endif()

# ---- GoW3 -> Switch: enlace y NRO ----
if(GOW3_NX)
    # Directo en el ejecutable: pisa simbolos weak de libnx (no puede ir en la .a).
    target_sources(${PROJECT_NAME} PRIVATE "${NX_DIR}/compat/nx_exception.cpp")
    # Reparte los hilos nuevos entre los nucleos 0-2 (libnx los crea todos en el 0).
    target_sources(${PROJECT_NAME} PRIVATE "${NX_DIR}/compat/nx_thread_cores.c")
    target_link_options(${PROJECT_NAME} PRIVATE "-Wl,--wrap=pthread_create")
    if(GOW3_NX_VULKAN)
        # libvulkan.a por ruta completa y dos veces (rescaneo del archivo), como rexglue_switch.cmake de KOF.
        target_sources(${PROJECT_NAME} PRIVATE "${NX_DIR}/compat/nx_elf_stubs.c")
        target_link_libraries(${PROJECT_NAME} PRIVATE gow3_nx_compat SDL2 EGL glapi drm_nouveau
            "${GOW3_NVK_SDK}/lib/libvulkan.a" expat zstd z nx
            "${GOW3_NVK_SDK}/lib/libvulkan.a" expat zstd z nx stdc++ m)
    else()
    target_link_libraries(${PROJECT_NAME} PRIVATE gow3_nx_compat SDL2 EGL glapi drm_nouveau nx m)
    endif()
    nx_create_nro(${PROJECT_NAME})
endif()
