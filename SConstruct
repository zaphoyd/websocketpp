#AddOption('--prefix',
#          dest='prefix',
#          nargs=1, type='string',
#          action='store',
#          metavar='DIR',
#          help='installation prefix')
#env = Environment(PREFIX = GetOption('prefix'))


import os, sys
env = Environment(ENV = os.environ)

#env["CXX"] = "clang++"

## Boost
##
## Note: You need to either set BOOSTROOT to the root of a stock Boost distribution
## or set BOOST_INCLUDES and BOOST_LIBS if Boost comes with your OS distro e.g. and
## needs BOOST_INCLUDES=/usr/include/boost and BOOST_LIBS=/usr/lib like Ubuntu.
##
if os.environ.has_key('BOOSTROOT'):
   env['BOOST_INCLUDES'] = os.environ['BOOSTROOT']
   env['BOOST_LIBS'] = os.path.join(os.environ['BOOSTROOT'], 'stage', 'lib')
elif os.environ.has_key('BOOST_INCLUDES') and os.environ.has_key('BOOST_LIBS'):
   env['BOOST_INCLUDES'] = os.environ['BOOST_INCLUDES']
   env['BOOST_LIBS'] = os.environ['BOOST_LIBS']
else:
   raise SCons.Errors.UserError, "Neither BOOSTROOT, nor BOOST_INCLUDES + BOOST_LIBS was set!"

env.Append(CPPPATH = [env['BOOST_INCLUDES']])
env.Append(LIBPATH = [env['BOOST_LIBS']])

boost_linkshared = False

def boostlibs(libnames):
   if env['PLATFORM'].startswith('win'):
      # Win/VC++ supports autolinking. nothing to do.
      # http://www.boost.org/doc/libs/1_49_0/more/getting_started/windows.html#auto-linking
      return []
   else:
      libs = []
      prefix = env['SHLIBPREFIX'] if boost_linkshared else env['LIBPREFIX']
      suffix = env['SHLIBSUFFIX'] if boost_linkshared else env['LIBSUFFIX']
      for name in libnames:
         lib = File(os.path.join(env['BOOST_LIBS'], '%sboost_%s%s' % (prefix, name, suffix)))
         libs.append(lib)
      return libs

if env['PLATFORM'].startswith('win'):
   env.Append(CPPDEFINES = ['WIN32',
                            'NDEBUG',
                            'WIN32_LEAN_AND_MEAN',
                            '_WIN32_WINNT=0x0600',
                            '_CONSOLE',
                            '_WEBSOCKETPP_CPP11_FRIEND_'])
   arch_flags  = '/arch:SSE2'
   opt_flags   = '/Ox /Oi /fp:fast'
   warn_flags  = '/W3 /wd4996 /wd4995 /wd4355'
   env['CCFLAGS'] = '%s /EHsc /GR /GS- /MD /nologo %s %s' % (warn_flags, arch_flags, opt_flags)
   env['LINKFLAGS'] = '/INCREMENTAL:NO /MANIFEST /NOLOGO /OPT:REF /OPT:ICF /MACHINE:X86'
elif env['PLATFORM'] == 'posix':
   env.Append(CPPDEFINES = ['NDEBUG'])
   env.Append(CCFLAGS = ['-Wall', '-fno-strict-aliasing'])
   env.Append(CCFLAGS = ['-O3', '-fomit-frame-pointer', '-march=core2'])
   #env['LINKFLAGS'] = ''


if env['PLATFORM'].startswith('win'):
   env['LIBPATH'] = env['BOOST_LIBS']
else:
   env['LIBPATH'] = ['/usr/lib',
                     '/usr/local/lib',
                     env['BOOST_LIBS']]

platform_libs = []
tls_libs = []
tls_build = False

if env['PLATFORM'] == 'posix':
   platform_libs = ['pthread', 'rt']
   tls_libs = ['ssl', 'crypto']
   tls_build = True
elif env['PLATFORM'] == 'darwin':
   tls_libs = ['ssl', 'crypto']
   tls_build = True
elif env['PLATFORM'].startswith('win'):
   # Win/VC++ supports autolinking. nothing to do.
   pass


releasedir = 'build/release/'
debugdir = 'build/debug/'
builddir = releasedir

Export('env')
Export('platform_libs')
Export('boostlibs')
Export('tls_libs')

## END OF CONFIG !!

## TARGETS:

static_lib, shared_lib = SConscript('src/SConscript',
                                    variant_dir = builddir + 'websocketpp',
                                    duplicate = 0)

wslib = static_lib
Export('wslib')

wsperf = SConscript('#/examples/wsperf/SConscript',
                    variant_dir = builddir + 'wsperf',
                    duplicate = 0)

echo_server = SConscript('#/examples/echo_server/SConscript',
                         variant_dir = builddir + 'echo_server',
                         duplicate = 0)

if tls_build:
   echo_server_tls = SConscript('#/examples/echo_server_tls/SConscript',
                            variant_dir = builddir + 'echo_server_tls',
                            duplicate = 0)

echo_client = SConscript('#/examples/echo_client/SConscript',
                         variant_dir = builddir + 'echo_client',
                         duplicate = 0)

chat_client = SConscript('#/examples/chat_client/SConscript',
                         variant_dir = builddir + 'chat_client',
                         duplicate = 0)

chat_server = SConscript('#/examples/chat_server/SConscript',
                         variant_dir = builddir + 'chat_server',
                         duplicate = 0)

concurrent_server = SConscript('#/examples/concurrent_server/SConscript',
                         variant_dir = builddir + 'concurrent_server',
                         duplicate = 0)

telemetry_server = SConscript('#/examples/telemetry_server/SConscript',
                         variant_dir = builddir + 'telemetry_server',
                         duplicate = 0)
