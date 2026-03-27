project('my_kernel', 'c',
  version : '0.1',
  default_options : [
    'c_std=c11',
    'buildtype=debug',
    'optimization=0',
    'b_staticpic=false'
  ]
)

add_project_arguments(
  [
    '-ffreestanding', '-fno-builtin',
    '-fno-stack-protector',
    '-fno-stack-check',
    '-fno-lto',
    '-m64',
    '-mcmodel=kernel',
    '-mno-red-zone',
    '-mno-mmx', '-mno-sse', '-mno-sse2', '-mno-80387', '-mno-bmi', '-mno-bmi2'
  ],
  language : 'c'
)

linker_script = meson.current_source_dir() / 'kernel.ld'
add_project_link_arguments(
  [
    '-T' + linker_script,
    '-nostdlib',
    '-static',
    '-z', 'max-page-size=0x1000'
  ],
  language : 'c'
)

find_cmd = run_command('find', 'src', '-name', '*.[cS]', check : true)
sources = find_cmd.stdout().strip().split('\n')

precompile_script = custom_target('precompile_script',
  build_always_stale : true,
  output : 'flanterm_ready.stamp',
  command : [
    find_program('python3'),
    meson.current_source_dir() / 'tools/pre-compile-setup.py',
    meson.current_build_dir(),
  ],
  build_by_default : true,
  console : true
)

kernel_exe = executable('kernel.elf',
  [sources, precompile_script],
  include_directories : [include_directories('src/')],
  install : true
)

custom_target('bootimg',
  build_always_stale : true,
  input : [
    kernel_exe,
    'limine.conf'
  ],
  output : 'Cuore.x86-64.iso',
  command : [
    find_program('python3'),
    meson.current_source_dir() / 'tools/post-compile-setup.py',
    '@OUTPUT@',
    '@INPUT0@',
    '@INPUT1@',
    meson.current_build_dir()
  ],
  build_by_default : true,
  console : true
)