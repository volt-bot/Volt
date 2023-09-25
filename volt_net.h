#pragma once
#include "volt_net_common.h"
#include "volt_executor.h"
#include <iostream>
#include <chrono>

namespace net{
	using namespace std::string_view_literals;
	static constexpr auto Local = "localhost"sv;

	// precondition: TimeBudget > 0
	auto resolveHostEndpoints(std::string_view HostName, tPort Port,
		std::chrono::milliseconds TimeBudget)
		-> std::vector<tEndpoint> {
		using tResolver = asio::ip::tcp::resolver;
		auto Flags = tResolver::numeric_service;
		if (HostName.empty() || HostName == Local) {
			Flags |= tResolver::passive;
			HostName = Local;
		}

		std::vector<tEndpoint> Endpoints;
		const auto toEndpoints = [&](const auto&, auto Resolved) {
			Endpoints.reserve(Resolved.size());
			for (const auto& Element : Resolved)
				Endpoints.push_back(Element.endpoint());
		};

		asio::io_context Executor;
		tResolver Resolver(Executor);
		Resolver.async_resolve(HostName, std::to_string(Port), Flags, toEndpoints);
		Executor.run_for(TimeBudget);
		return Endpoints;
	}

	// the connection is implemented as an independent coroutine.
	// it will be brought down by internal events or from the outside using a
	// stop signal.

	[[nodiscard]] auto registerClient(net::tSocket Socket)
		-> asio::awaitable<void> {
		net::tTimer Timer(Socket.get_executor());
		const auto WatchDog = executor::abort(Socket, Timer);
		using namespace std::chrono_literals;
		//while (Socket.is_open()) {
			std::cout << "sv: connected" << std::endl;
			//std::this_thread::sleep_for(1s);
		//}
			co_return;
	}

		
	//the tcp acceptor is a coroutine.
	// it spawns new, independent coroutines on connect.
	[[nodiscard]] auto acceptConnections(net::tAcceptor Acceptor)
		-> asio::awaitable<void> {
		const auto WatchDog = executor::abort(Acceptor);
		while (Acceptor.is_open()) {
			auto [Error, Socket] = co_await Acceptor.async_accept();
			if (not Error and Socket.is_open())
				//std::cout << "svr: connected" << std::endl;
				executor::commission(Acceptor.get_executor(), registerClient, std::move(Socket));
		}
	}


	// start serving a list of given endpoints.
	// each endpoint is served by an independent coroutine.

	auto serve(asio::io_context& Context, net::tEndpoints Endpoints) {
		std::size_t NumberOfAcceptors = 0;
		auto Error = std::make_error_code(std::errc::function_not_supported);

		for (const auto& Endpoint : Endpoints) {
			try {
				executor::commission(Context, acceptConnections, net::tAcceptor{ Context, Endpoint });
				std::cout<<"accept connections at {} "<<Endpoint.address().to_string()<<"\n";
				++NumberOfAcceptors;
			}
			catch (const std::system_error& Ex) {
				Error = Ex.code();
			}
		}
		if (NumberOfAcceptors == 0)
			return size_t(0);
		return NumberOfAcceptors;
	}


}