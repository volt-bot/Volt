#pragma once
#include <asio/asio.hpp>
#include <span>

namespace net {
	// customize the regular Asio types and functions such that they can be
	//  - conveniently used in await expressions
	//  - composed into higher level, augmented operations

	template <typename... Ts>
	using tResult = std::tuple<std::error_code, Ts...>;
	using use_await = asio::as_tuple_t<asio::use_awaitable_t<>>;
	using tSocket = use_await::as_default_on_t<asio::ip::tcp::socket>;
	using tAcceptor = use_await::as_default_on_t<asio::ip::tcp::acceptor>;
	using tTimer = use_await::as_default_on_t<asio::steady_timer>;

	using tEndpoint = asio::ip::tcp::endpoint;
	using tEndpoints = std::span<const tEndpoint>;
	using tByteSpan = std::span<std::byte>;
	using tConstByteSpan = std::span<const std::byte>;

	template <std::size_t N>
	using tSendBuffers = std::array<asio::const_buffer, N>;
	using tConstBuffers = std::span<asio::const_buffer>;

	enum tPort : uint16_t {};
}