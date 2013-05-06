import os, sys, commands
env = Environment(ENV = os.environ)

# figure out a better way to configure this
#env["CXX"] = "clang++"
if os.environ.has_key('CXX'):
    env['CXX'] = os.environ['CXX']

if os.environ.has_key('CXXFLAGS'):
    #env['CXXFLAGS'] = os.environ['CXXFLAGS']
    env.Append(CXXFLAGS = os.environ['CXXFLAGS'])

if os.environ.has_key('LINKFLAGS'):
    #env['LDFLAGS'] = os.environ['LDFLAGS']
    env.Append(LINKFLAGS = os.environ['LINKFLAGS'])

## Boost
##
## Note: You need to either set BOOSTROOT to the root of a stock Boost distribution
## or set BOOST_INCLUDES and BOOST_LIBS if Boost comes with your OS distro e.g. and
## needs BOOST_INCLUDES=/usr/include/boost and BOOST_LIBS=/usr/lib like Ubuntu.
##
if os.environ.has_key('BOOSTROOT'):
    os.environ['BOOST_ROOT'] = os.environ['BOOSTROOT']

if os.environ.has_key('BOOST_ROOT'):
   env['BOOST_INCLUDES'] = os.environ['BOOST_ROOT']
   env['BOOST_LIBS'] = os.path.join(os.environ['BOOST_ROOT'], 'stage', 'lib')
elif os.environ.has_key('BOOST_INCLUDES') and os.environ.has_key('BOOST_LIBS'):
   env['BOOST_INCLUDES'] = os.environ['BOOST_INCLUDES']
   env['BOOST_LIBS'] = os.environ['BOOST_LIBS']
else:
   raise SCons.Errors.UserError, "Neither BOOST_ROOT, nor BOOST_INCLUDES + BOOST_LIBS was set!"

if os.environ.has_key('WSPP_ENABLE_CPP11'):
   env['WSPP_ENABLE_CPP11'] = True
else:
   env['WSPP_ENABLE_CPP11'] = False

boost_linkshared = False

def boostlibs(libnames,localenv):
   if localenv['PLATFORM'].startswith('win'):
      # Win/VC++ supports autolinking. nothing to do.
      # http://www.boost.org/doc/libs/1_49_0/more/getting_started/windows.html#auto-linking
      return []
   else:
      libs = []
      prefix = localenv['SHLIBPREFIX'] if boost_linkshared else localenv['LIBPREFIX']
      suffix = localenv['SHLIBSUFFIX'] if boost_linkshared else localenv['LIBSUFFIX']
      for name in libnames:
         lib = File(os.path.join(localenv['BOOST_LIBS'], '%sboost_%s%s' % (prefix, name, suffix)))
         libs.append(lib)
      return libs

if env['PLATFORM'].startswith('win'):
   env.Append(CPPDEFINES = ['WIN32',
                            'NDEBUG',
                            'WIN32_LEAN_AND_MEAN',
                            '_WIN32_WINNT=0x0600',
                            '_CONSOLE',
                            'BOOST_TEST_DYN_LINK',
                            'NOMINMAX',
                            '_WEBSOCKETPP_CPP11_MEMORY_',
                            '_WEBSOCKETPP_CPP11_FUNCTIONAL_'])
   arch_flags  = '/arch:SSE2'
   opt_flags   = '/Ox /Oi /fp:fast'
   warn_flags  = '/W3 /wd4996 /wd4995 /wd4355'
   env['CCFLAGS'] = '%s /EHsc /GR /GS- /MD /nologo %s %s' % (warn_flags, arch_flags, opt_flags)
   env['LINKFLAGS'] = '/INCREMENTAL:NO /MANIFEST /NOLOGO /OPT:REF /OPT:ICF /MACHINE:X86'
elif env['PLATFORM'] == 'posix':
   env.Append(CPPDEFINES = ['NDEBUG'])
   env.Append(CCFLAGS = ['-Wall'])
   #env.Append(CCFLAGS = ['-O3', '-fomit-frame-pointer'])
   #env['LINKFLAGS'] = ''
elif env['PLATFORM'] == 'darwin':
   #env.Append(CPPDEFINES = ['NDEBUG'])
   env.Append(CCFLAGS = ['-Wall','-O0'])
   #env.Append(CCFLAGS = ['-O3', '-fomit-frame-pointer'])
   #env['LINKFLAGS'] = ''

if env['PLATFORM'].startswith('win'):
   #env['LIBPATH'] = env['BOOST_LIBS']
   pass
else:
   env['LIBPATH'] = ['/usr/lib',
                     '/usr/local/lib'] #, env['BOOST_LIBS']

# Compiler specific warning flags
if env['CXX'].startswith('g++'):
   #env.Append(CCFLAGS = ['-Wconversion'])
   env.Append(CCFLAGS = ['-Wcast-align'])
elif env['CXX'].startswith('clang++'):
   #env.Append(CCFLAGS = ['-Wcast-align'])
   #env.Append(CCFLAGS = ['-Wglobal-constructors'])
   #env.Append(CCFLAGS = ['-Wconversion'])
   env.Append(CCFLAGS = ['-Wno-padded'])

   # Wpadded
   # Wsign-conversion

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

## Append WebSocket++ path
env.Append(CPPPATH = ['#'])

##### Set up C++11 environment
polyfill_libs = [] # boost libraries used as drop in replacements for incomplete
				   # C++11 STL implimentations
env_cpp11 = env.Clone ()

if env_cpp11['CXX'].startswith('g++'):
   # TODO: check g++ version

   # g++ STL lacks support for <regex>

   GCC_VERSION = commands.getoutput(env_cpp11['CXX'] + ' -dumpversion')

   if GCC_VERSION > "4.4.0":
      print "C++11 build environment partially enabled"
      env_cpp11.Append(WSPP_CPP11_ENABLED = "true",CXXFLAGS = ['-std=c++0x'],TOOLSET = ['g++'],CPPDEFINES = ['_WEBSOCKETPP_CPP11_STL_','_WEBSOCKETPP_NO_CPP11_REGEX_'])
      # libstdc++ does not yet support <regex>
      # boost regex is a drop in replacement
      polyfill_libs += boostlibs(['regex'],env_cpp11)
   else:
      print "C++11 build environment is not supported on this version of G++"
elif env_cpp11['CXX'].startswith('clang++'):
   print "C++11 build environment enabled"
   env_cpp11.Append(WSPP_CPP11_ENABLED = "true",CXXFLAGS = ['-std=c++0x','-stdlib=libc++'],LINKFLAGS = ['-stdlib=libc++'],TOOLSET = ['clang++'],CPPDEFINES = ['_WEBSOCKETPP_CPP11_STL_'])

   # look for optional second boostroot compiled with clang's libc++ STL library
   # this prevents warnings/errors when linking code built with two different
   # incompatible STL libraries.
   if os.environ.has_key('BOOST_ROOT_CPP11'):
      env_cpp11['BOOST_INCLUDES'] = os.environ['BOOST_ROOT_CPP11']
      env_cpp11['BOOST_LIBS'] = os.path.join(os.environ['BOOST_ROOT_CPP11'], 'stage', 'lib')
   elif os.environ.has_key('BOOST_INCLUDES_CPP11') and os.environ.has_key('BOOST_LIBS_CPP11'):
      env_cpp11['BOOST_INCLUDES'] = os.environ['BOOST_INCLUDES_CPP11']
      env_cpp11['BOOST_LIBS'] = os.environ['BOOST_LIBS_CPP11']
else:
   print "C++11 build environment disabled"

# if the build system is known to allow the isystem modifier for library include
# values then use it for the boost libraries. Otherwise just add them to the 
# regular CPPPATH values.
if env['CXX'].startswith('g++') or env['CXX'].startswith('clang'):
	env.Append(CPPFLAGS = '-isystem ' + env['BOOST_INCLUDES'])
else:
	env.Append(CPPPATH = [env['BOOST_INCLUDES']])
env.Append(LIBPATH = [env['BOOST_LIBS']])

# if the build system is known to allow the isystem modifier for library include
# values then use it for the boost libraries. Otherwise just add them to the 
# regular CPPPATH values.
if env_cpp11['CXX'].startswith('g++') or env_cpp11['CXX'].startswith('clang'):
	env_cpp11.Append(CPPFLAGS = '-isystem ' + env_cpp11['BOOST_INCLUDES'])
else:
	env_cpp11.Append(CPPPATH = [env_cpp11['BOOST_INCLUDES']])
env_cpp11.Append(LIBPATH = [env_cpp11['BOOST_LIBS']])

releasedir = 'build/release/'
debugdir = 'build/debug/'
testdir = 'build/test/'
builddir = releasedir

Export('env')
Export('env_cpp11')
Export('platform_libs')
Export('boostlibs')
Export('tls_libs')
Export('polyfill_libs')

## END OF CONFIG !!

## TARGETS:

if not env['PLATFORM'].startswith('win'):
    # Unit tests, add test folders with SConscript files to to_test list.
    to_test = ['utility','http','logger','random','processors','message_buffer','extension','transport/iostream','transport/asio','roles','endpoint','connection'] #,'http','processors','connection'

    for t in to_test:
       new_tests = SConscript('#/test/'+t+'/SConscript',variant_dir = testdir + t, duplicate = 0)
       for a in new_tests:
          new_alias = Alias('test', [a], a.abspath)
          AlwaysBuild(new_alias)

# Main test application
#main = SConscript('#/examples/dev/SConscript',variant_dir = builddir + 'dev',duplicate = 0)

# testee_server
testee_server = SConscript('#/examples/testee_server/SConscript',variant_dir = builddir + 'testee_server',duplicate = 0)

# echo_server
echo_server = SConscript('#/examples/echo_server/SConscript',variant_dir = builddir + 'echo_server',duplicate = 0)

# echo_server_tls
if not env['PLATFORM'].startswith('win'):
    echo_server_tls = SConscript('#/examples/echo_server_tls/SConscript',variant_dir = builddir + 'echo_server_tls',duplicate = 0)

# broadcast_server
broadcast_server = SConscript('#/examples/broadcast_server/SConscript',variant_dir = builddir + 'broadcast_server',duplicate = 0)

# echo_client
echo_client = SConscript('#/examples/echo_client/SConscript',variant_dir = builddir + 'echo_client',duplicate = 0)

# utility_client
utility_client = SConscript('#/examples/utility_client/SConscript',variant_dir = builddir + 'utility_client',duplicate = 0)

# subprotocol_server
subprotocol_server = SConscript('#/examples/subprotocol_server/SConscript',variant_dir = builddir + 'subprotocol_server',duplicate = 0)

if not env['PLATFORM'].startswith('win'):
    # iostream_server
    iostream_server = SConscript('#/examples/iostream_server/SConscript',variant_dir = builddir + 'iostream_server',duplicate = 0)

    # telemetry_client
    telemetry_client = SConscript('#/examples/telemetry_client/SConscript',variant_dir = builddir + 'telemetry_client',duplicate = 0)

    # print_server
    print_server = SConscript('#/examples/print_server/SConscript',variant_dir = builddir + 'print_server',duplicate = 0)

#
#wsperf = SConscript('#/examples/wsperf/SConscript',
#                    variant_dir = builddir + 'wsperf',
#                    duplicate = 0)

#echo_server = SConscript('#/examples/echo_server/SConscript',
#                         variant_dir = builddir + 'echo_server',
#                         duplicate = 0)

#if tls_build:
#   echo_server_tls = SConscript('#/examples/echo_server_tls/SConscript',
#                            variant_dir = builddir + 'echo_server_tls',
#                            duplicate = 0)

#echo_client = SConscript('#/examples/echo_client/SConscript',
#                         variant_dir = builddir + 'echo_client',
#                         duplicate = 0)

#chat_client = SConscript('#/examples/chat_client/SConscript',
#                         variant_dir = builddir + 'chat_client',
#                         duplicate = 0)

#chat_server = SConscript('#/examples/chat_server/SConscript',
#                         variant_dir = builddir + 'chat_server',
#                         duplicate = 0)

#concurrent_server = SConscript('#/examples/concurrent_server/SConscript',
#                         variant_dir = builddir + 'concurrent_server',
#                         duplicate = 0)
