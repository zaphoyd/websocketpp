#AddOption('--prefix',
#          dest='prefix',
#          nargs=1, type='string',
#          action='store',
#          metavar='DIR',
#          help='installation prefix')
#env = Environment(PREFIX = GetOption('prefix'))


env = Environment() # Initialize the environment

env.VariantDir("release/", "src/");

lib_sources = ["base64/base64.cpp","md5/md5.c","messages/data.cpp","network_utilities.cpp","processors/hybi_header.cpp","sha1/sha1.cpp","uri.cpp"]
lib_path = ['/usr/lib','/usr/local/lib','#/release']

static_lib=env.StaticLibrary(target = 'release/websocketpp', source = lib_sources, srcdir="release")
shared_lib=env.SharedLibrary(target = 'release/websocketpp', source = lib_sources, srcdir="release",LIBS=['boost_regex'],LIBPATH=lib_path)

# Echo Server
env.VariantDir("#/release/echo_server","examples/echo_server")
env.Program(target="#/release/echo_server/echo_server",srcdir="#/release/echo_server/",source=["echo_server.cpp"],LIBS=[shared_lib,'boost_system','boost_date_time','boost_regex','boost_thread','pthread'],LIBPATH=lib_path)

lib_rt = ''
if env['PLATFORM'] == 'posix':
	lib_rt = 'rt'

# Echo Server
env.VariantDir("#/release/wsperf","examples/wsperf")
env.Program(target="#/release/wsperf/wsperf",srcdir="#/release/wsperf/",source=["wsperf.cpp","request.cpp","case.cpp","generic.cpp"],LIBS=[shared_lib,'boost_system','boost_date_time','boost_regex','boost_thread','pthread',lib_rt,'boost_random','boost_chrono','boost_program_options'],LIBPATH=lib_path)