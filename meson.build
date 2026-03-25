project('rbayer_classification', 'c', 'cpp')
project_name = 'rbayer_classification'

# Compiler and linker flags
cflags = ['-Wall', '-g', '-O0']
cppflags = ['-Wall', '-std=c++17', '-g', '-O0']

# Dependencies
cc = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')
proto_c_dep = cc.find_library('protobuf-c', required: false)
opencv_dep = dependency('opencv4', required: true)
jxl_dep = dependency('libjxl', required: true)
jxl_threads_dep = dependency('libjxl_threads', required: true)
openjp2_dep = dependency('libopenjp2', required: true)
tflite_dep = cpp.find_library('tensorflow-lite', required: true)

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
active_module = 'src/tflite_classification.cpp' # <-- Change this to switch modules

# Build sources array similar to reference
sources = c_sources + [active_module]

# Include directories
dirs = include_directories(
    'src/include',
    'src/include/utils',
)

# Dependencies array
deps = [proto_c_dep, opencv_dep, jxl_dep, jxl_threads_dep, openjp2_dep, tflite_dep]

# Shared library (SO)
lib = shared_library(project_name, sources,
    include_directories: dirs,
    c_args: cflags + ['-DSHARED_MEMORY=1'],
    cpp_args: cppflags + ['-DSHARED_MEMORY=1'],
    dependencies: deps
)

# Fix libopenjp2 NEEDED entry: the sysroot library has SONAME libopenjp2.so.7
# but the target (yocto) only ships libopenjp2.so.2.4.0
patchelf = find_program('patchelf')
custom_target('patch_openjp2_soname',
    input: lib,
    output: 'lib' + project_name + '.patched',
    command: [patchelf, '--replace-needed', 'libopenjp2.so.7', 'libopenjp2.so.2.4.0', '@INPUT@'],
    build_by_default: true
)

# Check for cross-compiling
if not get_option('cross-compile')
    # If not cross-compiling, include the executable target
    test_sources = sources + [
        'src/test.c',
        'src/utils/yaml_parser.c'
    ]
    
    libyaml_dep = dependency('yaml-0.1')
    m_dep = cc.find_library('m', required: false)
    
    deps += [libyaml_dep, m_dep]
    
    executable(project_name + '-exec', test_sources,
        include_directories: dirs,
        c_args: cflags + ['-g', '-DSHARED_MEMORY=0'],
        cpp_args: cppflags + ['-g', '-DSHARED_MEMORY=0'],
        dependencies: deps
    )
endif
