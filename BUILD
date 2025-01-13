package(default_visibility = ["//visibility:public"])

#################
## WebSocket++ ##
#################
cc_library(
    name = "websocketpp",
    hdrs = glob(["websocketpp/**/*.hpp"]),
    includes = ["websocketpp"],
    include_prefix = "websocketpp/",
    deps = [
	"@asio//:asio",
    ],
)
