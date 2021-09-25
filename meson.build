project('LibP2P', ['cpp'],
    license: 'LGPL2.1+',
    default_options: ['buildtype=release', 'b_ndebug=if-release', 'cpp_std=c++14', 'strip=true'],
    meson_version: '>=0.48.0',
    version: '1.0'
)

libs = []

sources = [
    'src/vs_plugin.cpp'
]

with_simd = false
libp2p_cppflags = ['-std=c++14']
if host_machine.cpu_family().startswith('x86')
    libp2p_cppflags += '-DP2P_SIMD'
    with_simd = true
endif

libs += static_library('p2p_main',
    [
        'src/libp2p/p2p_api.cpp',
        'src/libp2p/v210.cpp',
        'src/libp2p/simd/cpuinfo_x86.cpp',
        'src/libp2p/simd/p2p_simd.cpp',
    ],
    cpp_args: libp2p_cppflags
)

if with_simd
    libp2p_cppflags += '-msse4.1'
    libs += static_library('p2p_sse41', 'src/libp2p/simd/p2p_sse41.cpp',
        cpp_args: libp2p_cppflags
    )
endif

vapoursynth_dep = dependency('vapoursynth', version: '>=R55').partial_dependency(compile_args: true, includes: true)

deps = [
  vapoursynth_dep
]

shared_module('p2p', sources,
    include_directories: 'src',
    dependencies: deps,
    link_with: libs,
    install: true,
    install_dir: join_paths(vapoursynth_dep.get_pkgconfig_variable('libdir'), 'vapoursynth'),
    gnu_symbol_visibility: 'hidden'
)