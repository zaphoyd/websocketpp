package(default_visibility = ["//visibility:public"])

#################
## WebSocket++ ##
#################
cc_library(
    name = "websocketpp",
    hdrs = glob(["websocketpp/**/*.hpp"]),
    strip_include_prefix = ".",
    #includes = ["."],
    deps = [
	"@asio//:asio",
    ],
)
