project('lunasvg', 'cpp',
    version: '3.5.0',
    license: 'MIT',
    meson_version: '>=1.3.0',
    default_options: ['cpp_std=c++17']
)

plutovg_dep = dependency('plutovg',
    required: true,
    version: '>=1.3.0',
    fallback: ['plutovg', 'plutovg_dep']
)

lunasvg_sources = [
    'source/lunasvg/lunasvg.cpp',
    'source/lunasvg/graphics.cpp',
    'source/lunasvg/element.cpp',
    'source/lunasvg/filterelement.cpp',
    'source/lunasvg/geometryelement.cpp',
    'source/lunasvg/layoutstate.cpp',
    'source/lunasvg/paintelement.cpp',
    'source/lunasvg/parser.cpp',
    'source/lunasvg/property.cpp',
    'source/lunasvg/renderstate.cpp',
    'source/lunasvg/textelement.cpp'
]

lunasvg_compile_args = []
lunasvg_cpp_args = ['-DLUNASVG_BUILD']

if get_option('default_library') == 'static'
    lunasvg_compile_args += ['-DLUNASVG_BUILD_STATIC']
    lunasvg_cpp_args += ['-DLUNASVG_BUILD_STATIC']
endif

if get_option('load-system-fonts').disabled()
    lunasvg_cpp_args += ['-DLUNASVG_DISABLE_LOAD_SYSTEM_FONTS']
endif

lunasvg_lib = library('lunasvg', lunasvg_sources,
    include_directories: include_directories('include', 'source/lunasvg'),
    dependencies: plutovg_dep,
    version: meson.project_version(),
    cpp_args: lunasvg_cpp_args,
    gnu_symbol_visibility: 'hidden',
    install: true
)

lunasvg_dep = declare_dependency(
    link_with: lunasvg_lib,
    include_directories: include_directories('include'),
    compile_args: lunasvg_compile_args
)

meson.override_dependency('lunasvg', lunasvg_dep)
install_headers('include/lunasvg.h', subdir: 'lunasvg')

pkgmod = import('pkgconfig')
pkgmod.generate(lunasvg_lib,
    name: 'LunaSVG',
    description: 'SVG rendering and manipulation library',
    filebase: 'lunasvg',
    subdirs: 'lunasvg'
)
