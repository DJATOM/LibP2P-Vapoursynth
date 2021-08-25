project('LibP2P', ['cpp'],
    license: 'LGPL2.1+',
    default_options: ['buildtype=release', 'b_ndebug=if-release', 'cpp_std=c++14', 'strip=true'],
    meson_version: '>=0.48.0',
    version: '1.0'
)

sources = [
    'src/libp2p_plugin.cpp',
    'src/p2p_api.cpp'
]

vapoursynth_dep = dependency('vapoursynth', version: '>=R55').partial_dependency(compile_args: true, includes: true)

deps = [
  vapoursynth_dep
]

shared_module('p2p', sources,
    include_directories: 'src',
    dependencies: deps,
    install: true,
    install_dir: join_paths(vapoursynth_dep.get_pkgconfig_variable('libdir'), 'vapoursynth'),
    gnu_symbol_visibility: 'hidden'
)