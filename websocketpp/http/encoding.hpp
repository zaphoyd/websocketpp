//
// Created by Dan Toškan on 11.4.2024.
//

#ifndef HTTP_ENCODING_HPP
#define HTTP_ENCODING_HPP

#include <websocketpp/http/constants.hpp>

#ifndef WEBSOCKETPP_WITH_GZIP
#define WEBSOCKETPP_WITH_GZIP 0
#elif WEBSOCKETPP_WITH_GZIP
#include <zlib.h>
#endif

#ifndef WEBSOCKETPP_WITH_DEFLATE
#define WEBSOCKETPP_WITH_DEFLATE 0
#elif WEBSOCKETPP_WITH_DEFLATE
#include <zlib.h>
#endif

#ifndef WEBSOCKETPP_WITH_BROTLI
#define WEBSOCKETPP_WITH_BROTLI 0
#elif WEBSOCKETPP_WITH_BROTLI
#include <brotli/decode.h>
#include <brotli/encode.h>
#endif

#ifndef WEBSOCKETPP_WITH_ZSTD
#define WEBSOCKETPP_WITH_ZSTD 0
#elif WEBSOCKETPP_WITH_ZSTD
#include "zstd.h"
#endif

namespace websocketpp
{
namespace http
{

inline constexpr bool is_encoding_supported(content_encoding::value encoding)
{
	switch (encoding)
	{
		case content_encoding::gzip: return WEBSOCKETPP_WITH_GZIP; break;
		case content_encoding::deflate: return WEBSOCKETPP_WITH_DEFLATE; break;
		case content_encoding::brotli: return WEBSOCKETPP_WITH_BROTLI; break;
		case content_encoding::compress: return false; break; // unsupported because browsers don't support it
		case content_encoding::zstd: return WEBSOCKETPP_WITH_ZSTD; break;
		default: break;
	}
	return false;
}

}    // namespace http

namespace encoding
{
#if WEBSOCKETPP_WITH_GZIP
namespace gzip
{
constexpr int MOD_GZIP_ZLIB_WINDOWSIZE = 15;
constexpr int MOD_GZIP_ZLIB_CFACTOR = 9;
inline std::string compress(const std::string& str, lib::error_code& ec,
                          int compressionlevel = Z_BEST_COMPRESSION)
{
	z_stream zs;                        // z_stream is zlib's control structure
	memset(&zs, 0, sizeof(zs));

	if (deflateInit2(&zs,
	                 compressionlevel,
	                 Z_DEFLATED,
	                 MOD_GZIP_ZLIB_WINDOWSIZE + 16,
	                 MOD_GZIP_ZLIB_CFACTOR,
	                 Z_DEFAULT_STRATEGY) != Z_OK
	) {
		throw(std::runtime_error("deflateInit2 failed while compressing."));
	}

	zs.next_in = (Bytef*)str.data();
	zs.avail_in = str.size();           // set the z_stream's input

	int ret;
	char outbuffer[32768];
	std::string outstring;

	// retrieve the compressed bytes blockwise
	do {
		zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
		zs.avail_out = sizeof(outbuffer);

		ret = deflate(&zs, Z_FINISH);

		if (outstring.size() < zs.total_out) {
			// append the block to the output string
			outstring.append(outbuffer,
			                 zs.total_out - outstring.size());
		}
	} while (ret == Z_OK);

	deflateEnd(&zs);

	if (ret != Z_STREAM_END) {          // an error occurred that was not EOF
		std::ostringstream oss;
		oss << "Exception during zlib compression: (" << ret << ") " << zs.msg;
		throw(std::runtime_error(oss.str()));
	}

	return outstring;
}

inline std::string decompress(const std::string& str, lib::error_code& ec)
{
	z_stream zs;                        // z_stream is zlib's control structure
	memset(&zs, 0, sizeof(zs));

	if (inflateInit2(&zs, MOD_GZIP_ZLIB_WINDOWSIZE + 16) != Z_OK)
		throw(std::runtime_error("inflateInit failed while decompressing."));

	zs.next_in = (Bytef*)str.data();
	zs.avail_in = str.size();

	int ret;
	char outbuffer[32768];
	std::string outstring;

	// get the decompressed bytes blockwise using repeated calls to inflate
	do {
		zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
		zs.avail_out = sizeof(outbuffer);

		ret = inflate(&zs, 0);

		if (outstring.size() < zs.total_out) {
			outstring.append(outbuffer,
			                 zs.total_out - outstring.size());
		}

	} while (ret == Z_OK);

	inflateEnd(&zs);

	if (ret != Z_STREAM_END) {          // an error occurred that was not EOF
		std::ostringstream oss;
		oss << "Exception during zlib decompression: (" << ret << ") "
		    << zs.msg;
		throw(std::runtime_error(oss.str()));
	}

	return outstring;
}

}    // namespace gzip
#endif
#if WEBSOCKETPP_WITH_DEFLATE
namespace zlib
{
constexpr int ZLIB_BUFFER_SIZE = 10240;
/** Compress a STL string using zlib with given compression level and return* the binary data. */
inline std::string compress(const std::string& str, lib::error_code& ec, int compressionlevel = Z_BEST_COMPRESSION)
{
	z_stream zs;    // z_stream is zlib's control structure
	memset(&zs, 0, sizeof(zs));

	if (deflateInit(&zs, compressionlevel) != Z_OK)
		throw(std::runtime_error("deflateInit failed while compressing."));

	zs.next_in = (Bytef*)str.data();
	zs.avail_in = str.size();    // set the z_stream's input

	int ret;
	char outbuffer[ZLIB_BUFFER_SIZE];
	std::string outstring;

	// retrieve the compressed bytes blockwise
	do {
		zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
		zs.avail_out = sizeof(outbuffer);

		ret = deflate(&zs, Z_FINISH);

		if (outstring.size() < zs.total_out)
		{
			// append the block to the output string
			outstring.append(outbuffer, zs.total_out - outstring.size());
		}
	} while (ret == Z_OK);

	deflateEnd(&zs);

	if (ret != Z_STREAM_END)
	{    // an error occurred that was not EOF
		std::ostringstream oss;
		oss << "Exception during zlib compression: (" << ret << ") " << zs.msg;
		throw(std::runtime_error(oss.str()));
	}

	return outstring;
}

/** Decompress an STL string using zlib and return the original data. */
inline std::string decompress(const std::string& str, lib::error_code& ec)
{
	z_stream zs;    // z_stream is zlib's control structure
	memset(&zs, 0, sizeof(zs));

	if (inflateInit(&zs) != Z_OK)
		throw(std::runtime_error("inflateInit failed while decompressing."));

	zs.next_in = (Bytef*)str.data();
	zs.avail_in = str.size();

	int ret;
	char outbuffer[ZLIB_BUFFER_SIZE];
	std::string outstring;

	// get the decompressed bytes blockwise using repeated calls to inflate
	do {
		zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
		zs.avail_out = sizeof(outbuffer);

		ret = inflate(&zs, 0);

		if (outstring.size() < zs.total_out)
		{
			outstring.append(outbuffer, zs.total_out - outstring.size());
		}

	} while (ret == Z_OK);

	inflateEnd(&zs);

	if (ret != Z_STREAM_END)
	{    // an error occurred that was not EOF
		std::ostringstream oss;
		oss << "Exception during zlib decompression: (" << ret << ") " << zs.msg;
		throw(std::runtime_error(oss.str()));
	}

	return outstring;
}
}    // namespace zlib
#endif

#if WEBSOCKETPP_WITH_BROTLI
namespace brotli
{
constexpr size_t BROTLI_BUFFER_SIZE = 2048;
inline std::string compress(const std::string& data, lib::error_code& ec)
{
	auto instance = BrotliEncoderCreateInstance(nullptr, nullptr, nullptr);
	std::array<uint8_t, BROTLI_BUFFER_SIZE> buffer;
	std::stringstream result;

	size_t available_in = data.length(), available_out = buffer.size();
	const uint8_t* next_in = reinterpret_cast<const uint8_t*>(data.c_str());
	uint8_t* next_out = buffer.data();

	do {
		BrotliEncoderCompressStream(instance, BROTLI_OPERATION_FINISH, &available_in, &next_in, &available_out, &next_out, nullptr);
		result.write(reinterpret_cast<const char*>(buffer.data()), buffer.size() - available_out);
		available_out = buffer.size();
		next_out = buffer.data();
	} while (!(available_in == 0 && BrotliEncoderIsFinished(instance)));

	BrotliEncoderDestroyInstance(instance);
	return result.str();
}

inline std::string decompress(const std::string& data, lib::error_code& ec)
{
	auto instance = BrotliDecoderCreateInstance(nullptr, nullptr, nullptr);
	std::array<uint8_t, BROTLI_BUFFER_SIZE> buffer;
	std::stringstream result;

	size_t available_in = data.length(), available_out = buffer.size();
	const uint8_t* next_in = reinterpret_cast<const uint8_t*>(data.c_str());
	uint8_t* next_out = buffer.data();
	BrotliDecoderResult oneshot_result;

	do {
		oneshot_result = BrotliDecoderDecompressStream(instance, &available_in, &next_in, &available_out, &next_out, nullptr);
		result.write(reinterpret_cast<const char*>(buffer.data()), buffer.size() - available_out);
		available_out = buffer.size();
		next_out = buffer.data();
	} while (!(available_in == 0 && oneshot_result == BROTLI_DECODER_RESULT_SUCCESS));

	BrotliDecoderDestroyInstance(instance);
	return result.str();
}
}    // namespace brotli
#endif

#if WEBSOCKETPP_WITH_ZSTD
namespace zstd {
inline std::string compress(const std::string& data, lib::error_code& ec, int compress_level = ZSTD_CLEVEL_DEFAULT) {

	size_t est_compress_size = ZSTD_compressBound(data.size());

	std::string comp_buffer;
	comp_buffer.resize(est_compress_size);

	auto compress_size =
	  ZSTD_compress((void*)comp_buffer.data(), est_compress_size, data.data(),
	                data.size(), compress_level);

	comp_buffer.resize(compress_size);
	comp_buffer.shrink_to_fit();

	return comp_buffer;
}

inline std::string decompress(const std::string& data, lib::error_code& ec) {

	size_t const est_decomp_size =
	  ZSTD_getDecompressedSize(data.data(), data.size());

	std::string decomp_buffer;
	decomp_buffer.resize(est_decomp_size);

	size_t const decomp_size = ZSTD_decompress(
	  (void*)decomp_buffer.data(), est_decomp_size, data.data(), data.size());

	decomp_buffer.resize(decomp_size);
	decomp_buffer.shrink_to_fit();
	return decomp_buffer;
}

} // namespace zstd
#endif

inline std::string decompress(http::content_encoding::value encoding, bool is_transfer_encoding, const std::string& str, lib::error_code& ec)
{
	switch (encoding)
	{
#if WEBSOCKETPP_WITH_DEFLATE
		case http::content_encoding::deflate:
			return zlib::decompress(str, ec);
			break;
#endif
#if WEBSOCKETPP_WITH_BROTLI
		case http::content_encoding::brotli:
			return brotli::decompress(str, ec);
			break;
#endif
#if WEBSOCKETPP_WITH_GZIP
		case http::content_encoding::gzip:
			return gzip::decompress(str, ec);
			break;
#endif
#if WEBSOCKETPP_WITH_ZSTD
		case http::content_encoding::zstd:
			return zstd::decompress(str, ec);
			break;
#endif
		default: break;
	}

	ec = http::error::make_error_code(is_transfer_encoding ? http::error::unsupported_transfer_encoding : http::error::unsupported_content_encoding);
	return {};
}

inline std::string compress(http::content_encoding::value encoding, bool is_transfer_encoding, const std::string& str, lib::error_code& ec)
{
	switch (encoding)
	{
#if WEBSOCKETPP_WITH_DEFLATE
		case http::content_encoding::deflate:
			return zlib::compress(str, ec);
			break;
#endif
#if WEBSOCKETPP_WITH_BROTLI
		case http::content_encoding::brotli:
			return brotli::compress(str, ec);
			break;
#endif
#if WEBSOCKETPP_WITH_GZIP
		case http::content_encoding::gzip:
			return gzip::compress(str, ec);
			break;
#endif
#if WEBSOCKETPP_WITH_ZSTD
		case http::content_encoding::zstd:
			return zstd::compress(str, ec);
			break;
#endif
		default: break;
	}

	ec = http::error::make_error_code(is_transfer_encoding ? http::error::unsupported_transfer_encoding : http::error::unsupported_content_encoding);
	return {};
}

}    // namespace encoding

}    // namespace websocketpp

#endif    // HTTP_CONSTANTS_HPP
