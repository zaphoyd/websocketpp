#AddOption('--prefix',
#          dest='prefix',
#          nargs=1, type='string',
#          action='store',
#          metavar='DIR',
#          help='installation prefix')
#env = Environment(PREFIX = GetOption('prefix'))


import os, sys
env = Environment(ENV = os.environ)

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


env.VariantDir("release/", "src/");

if env['PLATFORM'].startswith('win'):
   lib_path = env['BOOST_LIBS']
else:
   lib_path = ['/usr/lib',
               '/usr/local/lib',
               env['BOOST_LIBS'],
               '#/release']

platform_libs = []
if env['PLATFORM'] == 'posix':
   platform_libs = ['pthread', 'rt']
elif env['PLATFORM'].startswith('win'):
   # Win/VC++ supports autolinking. nothing to do.
   pass

#### END OF GLOBAL CONF ########################################################

## WebSocket++ library
##
lib_sources = ["base64/base64.cpp",
               "md5/md5.c",
               "messages/data.cpp",
               "network_utilities.cpp",
               "processors/hybi_header.cpp",
               "sha1/sha1.cpp",
               "uri.cpp"]

static_lib = env.StaticLibrary(target = 'release/websocketpp',
                               source = lib_sources,
                               srcdir = "release")
#shared_lib=env.SharedLibrary(target = 'release/websocketpp', source = lib_sources, srcdir="release",LIBS=['boost_regex'],LIBPATH=lib_path)

wslib = static_lib

## Echo Server
##
env.VariantDir("#/release/echo_server", "examples/echo_server")
env.Program(target = "#/release/echo_server/echo_server",
            srcdir = "#/release/echo_server/",
            source = ["echo_server.cpp"],
            LIBS = [wslib, platform_libs] + boostlibs(['system',
                                                       'date_time',
                                                       'regex',
                                                       'thread']),
            LIBPATH = lib_path)

## wsperf
##
env.VariantDir("#/release/wsperf", "examples/wsperf")
env.Program(target = "#/release/wsperf/wsperf",
            srcdir = "#/release/wsperf/",
            source = ["wsperf.cpp",
                      "request.cpp",
                      "case.cpp",
                      "generic.cpp"],
            LIBS = [wslib, platform_libs] + boostlibs(['system',
                                                       'date_time',
                                                       'regex',
                                                       'thread',
                                                       'random',
                                                       'chrono',
                                                       'program_options']),
            LIBPATH = lib_path)
