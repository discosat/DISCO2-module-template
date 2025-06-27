project('test_pipeline', 'c', 'cpp')
project_name = 'test_pipeline'

# Compiler and linker flags
cflags = ['-Wall', '-g', '-O0']
cppflags = ['-Wall', '-std=c++17', '-g', '-O0']

# Dependencies
cc = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')
proto_c_dep = cc.find_library('protobuf-c', required: false)
opencv_dep = dependency('opencv4', required: true)  # You can change to opencv if opencv4 fails

# Source files
c_sources = [
    'src/utils/memory_util.c',
    'src/utils/config_util.c',
    'src/utils/batch_util.c',
    'src/utils/error_util.c',
    #'src/utils/logger.c', 
    'src/utils/metadata_util.c',
    'src/utils/metadata.pb-c.c',
]

# Change this to switch the active module!
active_module = 'src/detection_dead_pixel.c'  # <-- Change this to switch modules, e.g. 'src/distortion_correction.cpp'

# Decide whether to compile it as C or C++
use_cpp = active_module.endswith('.cpp')

cpp_sources = []
if use_cpp
    cpp_sources += active_module
else
    c_sources += active_module
endif

dirs = include_directories(
    'src/include',
    'src/include/utils',
)

# Shared library (SO)
shared_library(project_name, cpp_sources + c_sources,
    include_directories: dirs,
    c_args: cflags + ['-DSHARED_MEMORY=1'],
    cpp_args: cppflags + ['-DSHARED_MEMORY=1'],
    dependencies: [proto_c_dep, opencv_dep]
)

# Check for cross-compiling
if not get_option('cross-compile') # = ./configure test
    # If not cross-compiling, include the executable target
    test_sources = cpp_sources + c_sources + [
        'src/test.c',
        'src/utils/yaml_parser.c'
    ]

    libyaml_dep = dependency('yaml-0.1')
    m_dep = cc.find_library('m', required: false)

    executable(project_name + '-exec', test_sources,
        include_directories: dirs,
        c_args: cflags + ['-g', '-DSHARED_MEMORY=0'],
        cpp_args: cppflags + ['-g', '-DSHARED_MEMORY=0'],
        dependencies: [libyaml_dep, m_dep, proto_c_dep, opencv_dep]
    )
endif

# Standalone demosaic test executable
executable(
    'test_demosaic',
    ['src/test_demosaic.cpp', 'src/utils/metadata.pb-c.c'],
    include_directories: dirs,
    c_args: cflags,
    dependencies: [opencv_dep, proto_c_dep],
)

