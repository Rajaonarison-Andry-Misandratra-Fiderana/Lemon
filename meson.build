project('lemon', ['c', 'cpp'],
  version : '0.13.0',
)

subdir('protocols')

prefix = get_option('prefix')
sysconfdir = get_option('sysconfdir')

# Strip prefix from sysconfdir if present, ensure absolute path
if sysconfdir.startswith(prefix)
  sysconfdir = sysconfdir.substring(prefix.length())
  if not sysconfdir.startswith('/')
    sysconfdir = '/' + sysconfdir
  endif
endif

cc = meson.get_compiler('c')
libm = cc.find_library('m')
xcb = dependency('xcb', required : get_option('xwayland'))
xlibs = dependency('xcb-icccm', required : get_option('xwayland'))
wayland_server_dep = dependency('wayland-server',version: '>=1.23.1')
wlroots_dep = dependency('wlroots-0.19',version: '>=0.19.0')
xkbcommon_dep = dependency('xkbcommon')
libinput_dep = dependency('libinput',version: '>=1.27.1')
libwayland_client_dep = dependency('wayland-client')
pcre2_dep = dependency('libpcre2-8')
libscenefx_dep = dependency('scenefx-0.4',version: '>=0.4.1')

# Optional faster allocator (jemalloc/mimalloc). Off by default.
fast_alloc_dep = dependency('', required: false)
if get_option('jemalloc')
  fast_alloc_dep = dependency('jemalloc', required: false)
  if not fast_alloc_dep.found()
    fast_alloc_dep = cc.find_library('jemalloc', required: true)
  endif
endif


# 获取版本信息
git = find_program('git', required : false)
is_git_repo = false

# 检查当前目录是否是 Git 仓库
if git.found()
  git_status = run_command(git, 'rev-parse', '--is-inside-work-tree', check : false)
  if git_status.returncode() == 0 and git_status.stdout().strip() == 'true'
    is_git_repo = true
  endif
endif

if is_git_repo
  # 如果是 Git 目录，获取 Commit Hash 和最新的 tag
  commit_hash = run_command(git, 'rev-parse', '--short', 'HEAD', check : false).stdout().strip()
  latest_tag = meson.project_version()
  version_with_hash = '@0@(@1@)'.format(latest_tag, commit_hash)
else
  # 如果不是 Git 目录，使用项目版本号和 "release" 字符串
  commit_hash = 'release'
  latest_tag = meson.project_version()
  version_with_hash = '@0@(@1@)'.format(latest_tag, commit_hash)
endif

# 定义编译参数
c_args = [
  '-Wno-unused-function',
  '-DWLR_USE_UNSTABLE',
  '-D_POSIX_C_SOURCE=200809L',
  '-DVERSION="@0@"'.format(version_with_hash),
  '-DSYSCONFDIR="@0@"'.format('/etc'),
]

link_args = []

buildtype = get_option('buildtype')

if buildtype == 'debug'
  c_args += ['-g', '-O0']
else
  c_args += [
    '-O3',
    '-fno-plt',
    '-fno-semantic-interposition',
    '-fvisibility=hidden',
    '-fstack-protector-strong',
    '-fomit-frame-pointer',
    '-ffunction-sections',
    '-fdata-sections',
    '-pipe',
    '-DNDEBUG',
  ]
  link_args += [
    '-Wl,-O1',
    '-Wl,--as-needed',
    '-Wl,--gc-sections',
    '-Wl,-z,relro',
    '-Wl,-z,now',
  ]
endif

# Link-time optimization
if get_option('lto') and buildtype != 'debug'
  if cc.has_argument('-flto=thin')
    lto_flag = '-flto=thin'
  else
    lto_flag = '-flto'
  endif
  c_args += lto_flag
  link_args += lto_flag
endif

# Local CPU tuning (opt-in)
if get_option('native')
  c_args += ['-march=native', '-mtune=native']
endif

# 仅在 debug 选项启用时添加调试参数
if get_option('asan')
  c_args += [
    '-fsanitize=address',
    '-fno-omit-frame-pointer',
    '-fno-optimize-sibling-calls'
  ]
  link_args += '-fsanitize=address'
endif

if xcb.found() and xlibs.found()
  c_args += '-DXWAYLAND'
endif

lemon_deps = [
  libm,
  xcb,
  xlibs,
  libscenefx_dep,
  wayland_server_dep,
  wlroots_dep,
  xkbcommon_dep,
  libinput_dep,
  libwayland_client_dep,
  pcre2_dep,
]
if get_option('jemalloc') and fast_alloc_dep.found()
  lemon_deps += fast_alloc_dep
endif

executable('lemon',
  'src/lemon.c',
  'src/common/util.c',
  'src/ext-protocol/wlr_ext_workspace_v1.c',
  wayland_sources,
  dependencies : lemon_deps,
  install : true,
  c_args : c_args,
  link_args : link_args,
)

# build mmsg
dwl_ipc_protocol = 'protocols/dwl-ipc-unstable-v2.xml'

wayland_scanner_client_header = generator(
  wayland_scanner,
  output: '@BASENAME@-protocol.h', 
  arguments: ['client-header', '@INPUT@', '@OUTPUT@']
)

wayland_scanner_private_code = generator(
  wayland_scanner,
  output: '@BASENAME@-protocol.c',
  arguments: ['private-code', '@INPUT@', '@OUTPUT@']
)

# mmsg needs GNU extensions (usleep, reallocarray); strip the POSIX-only
# define from c_args.
mmsg_c_args = []
foreach arg : c_args
  if arg != '-D_POSIX_C_SOURCE=200809L'
    mmsg_c_args += arg
  endif
endforeach
mmsg_c_args += '-D_GNU_SOURCE'

# 在 mmsg 目标中使用生成器
executable('mmsg',
  'mmsg/mmsg.c',
  wayland_scanner_private_code.process(dwl_ipc_protocol),
  wayland_scanner_client_header.process(dwl_ipc_protocol),
  dependencies: [
    libwayland_client_dep
  ],
  install: true,
  c_args: mmsg_c_args,
  link_args: link_args,
)

desktop_install_dir = join_paths(prefix, 'share/wayland-sessions')
portal_install_dir = join_paths(prefix, 'share/xdg-desktop-portal')
install_data('assets/lemon.desktop', install_dir : desktop_install_dir)
install_data('assets/lemon-portals.conf', install_dir : portal_install_dir)
# Config lives only at ~/.config/lemon/lemon.conf — copy assets/lemon.conf manually.
