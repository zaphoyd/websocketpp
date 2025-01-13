package(default_visibility = ["//visibility:public"])

#################
## WebSocket++ ##
#################
cc_library(
    name = "websocketpp",
    hdrs = glob(["**/*.hpp"]),
    includes = ["websocketpp"],
    include_prefix = "websocketpp/",
    deps = [
	"@asio//:asio",
    ],
)
