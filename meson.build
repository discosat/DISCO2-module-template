project('disco-modules', 'c', 'cpp',
    default_options: ['c_std=c11', 'cpp_std=c++17'])

# Compiler and linker flags
cflags = ['-Wall', '-O2', '-DSHARED_MEMORY=1', '-D_GNU_SOURCE']
cppflags = ['-Wall', '-O2', '-DSHARED_MEMORY=1', '-D_GNU_SOURCE']

# Compilers
cc = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')

# Include directories
dirs = include_directories('src/include', 'src/include/utils')

# Shared utility sources linked into every module
util_sources = files(
    'src/utils/memory_util.c',
    'src/utils/config_util.c',
    'src/utils/batch_util.c',
    'src/utils/error_util.c',
    'src/utils/metadata_util.c',
    'src/utils/metadata.pb-c.c',
)

# Common dependency: protobuf-c (used by utils)
proto_c_dep = cc.find_library('protobuf-c', required: true)

# Optional dependencies — only pulled in by the modules that need them
opencv_dep = dependency('opencv4', required: false)
jxl_dep = dependency('libjxl', required: false)
jxl_threads_dep = dependency('libjxl_threads', required: false)
openjp2_dep = dependency('libopenjp2', required: false)
tflite_dep = cpp.find_library('tensorflow-lite', required: false)

# ── Module definitions ──────────────────────────────────────────────
# Each entry: [library_name, source_file, [dependencies]]

modules = {
    'id':            {'src': 'src/id.c',                          'deps': [proto_c_dep]},
    'demosaic':      {'src': 'src/demosaic_module.cpp',           'deps': [proto_c_dep, opencv_dep]},
    'color':         {'src': 'src/color_module.cpp',              'deps': [proto_c_dep, opencv_dep]},
    'distortion':    {'src': 'src/distortion_module.cpp',         'deps': [proto_c_dep, opencv_dep]},
    'resize':        {'src': 'src/resize_module.cpp',             'deps': [proto_c_dep, opencv_dep]},
    'tile-v1':       {'src': 'src/tile_moduel-v1.cpp',            'deps': [proto_c_dep]},
    'tile-v2':       {'src': 'src/tile_module-v2.cpp',            'deps': [proto_c_dep]},
    'jpegxl':        {'src': 'src/jpegxl_module.c',               'deps': [proto_c_dep, jxl_dep, jxl_threads_dep]},
    'jpeg2000':      {'src': 'src/jpeg2000_module.c',             'deps': [proto_c_dep, openjp2_dep]},
    'classification':{'src': 'src/tflite_classification.cpp',     'deps': [proto_c_dep, tflite_dep]},
    'segmentation':  {'src': 'src/tflite_binary_segmentation.cpp','deps': [proto_c_dep, tflite_dep]},
}

foreach name, mod : modules
    # Skip modules whose dependencies are not available
    all_found = true
    foreach d : mod['deps']
        if not d.found()
            all_found = false
        endif
    endforeach

    if all_found
        shared_library('lib' + name, util_sources + files(mod['src']),
            include_directories: dirs,
            c_args: cflags,
            cpp_args: cppflags,
            dependencies: mod['deps'],
            install: true,
            install_dir: get_option('prefix') / 'share' / 'pipeline',
        )
    else
        warning('Skipping module ' + name + ': missing dependencies')
    endif
endforeach

# ── Test executable (native builds only) ────────────────────────────
if not get_option('cross-compile')
    libyaml_dep = dependency('yaml-0.1', required: false)
    m_dep = cc.find_library('m', required: false)

    if libyaml_dep.found()
        executable('disco-modules-test',
            util_sources + files('src/id.c', 'src/test.c', 'src/utils/yaml_parser.c'),
            include_directories: dirs,
            c_args: ['-Wall', '-g', '-DSHARED_MEMORY=0'],
            cpp_args: ['-Wall', '-g', '-DSHARED_MEMORY=0'],
            dependencies: [proto_c_dep, libyaml_dep, m_dep],
        )
    endif
endif
