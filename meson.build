project('diagnostics', 'c', 'cpp',
    default_options: [
        'optimization=s', # Keep Optimize for Size (-Os)
        'b_ndebug=true'   # Disable asserts to save space
        # Removed b_lto=true to prevent aggressive inlining bloat
    ]
)
project_name = 'diagnostics'

# Compiler flags
cflags = [
    '-Wall', 
    '-ffunction-sections', 
    '-fdata-sections',
    '-fvisibility=hidden'
]

# C++ specific flags to remove heavy runtime overhead
cppflags = [
    '-Wall', 
    '-std=c++17', 
    '-ffunction-sections', 
    '-fdata-sections', 
    '-fno-exceptions', 
    '-fno-rtti',
    '-fvisibility=hidden'
]

# Force the linker to strip ALL symbols and debug info immediately at build time
linker_flags = [
    '-Wl,--gc-sections',
    '-Wl,--strip-all' # <-- Forces stripping of the .so in the build dir
]

# Dependencies
cc = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')
proto_c_dep = cc.find_library('protobuf-c', required: false)
#opencv_dep = dependency('opencv4', required: true)
#jxl_dep = dependency('libjxl', required: true)
#jxl_threads_dep = dependency('libjxl_threads', required: true)

# Manually defined OpenJPEG to avoid Yocto sysroot prefixing issues
#openjp2_inc = include_directories('/home/buoe/openjpeg/build_aarch64/install/include/openjpeg-2.5')
#openjp2_lib = cc.find_library('openjp2', dirs : '/home/buoe/openjpeg/build_aarch64/install/lib')
#openjp2_dep = declare_dependency(include_directories : openjp2_inc, dependencies : openjp2_lib)

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
active_module = 'src/diagnostics_module.c' 

# Build sources array
sources = c_sources + [active_module]

# Include directories
dirs = include_directories(
    'src/include',
    'src/include/utils',
)

# Dependencies array
deps = [proto_c_dep]

# Shared library (SO)
shared_library(project_name, sources,
    include_directories: dirs,
    c_args: cflags + ['-DSHARED_MEMORY=1'],
    cpp_args: cppflags + ['-DSHARED_MEMORY=1'],
    link_args: linker_flags,
    dependencies: deps
)

# Check for cross-compiling
if not get_option('cross-compile')
    test_sources = sources + [
        'src/test.c',
        'src/utils/yaml_parser.c'
    ]
    
    libyaml_dep = dependency('yaml-0.1')
    m_dep = cc.find_library('m', required: false)
    
    executable_deps = deps + [libyaml_dep, m_dep]
    
    executable(project_name + '-exec', test_sources,
        include_directories: dirs,
        c_args: cflags + ['-DSHARED_MEMORY=0'],
        cpp_args: cppflags + ['-DSHARED_MEMORY=0'],
        link_args: linker_flags,
        dependencies: executable_deps
    )
endif
