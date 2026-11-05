//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////
// Remere's Map Editor is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Remere's Map Editor is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "net/net_connection.h"
#include <spdlog/spdlog.h>

NetworkMessage::NetworkMessage() {
	clear();
}

void NetworkMessage::clear() {
	buffer.resize(4);
	position = 4;
	size = 0;
}

void NetworkMessage::expand(const size_t length) {
	if (position + length >= buffer.size()) {
		buffer.resize(position + length + 1);
	}
	size += length;
}

template <>
std::string NetworkMessage::read<std::string>() {
	const uint16_t length = read<uint16_t>();
	char* strBuffer = reinterpret_cast<char*>(&buffer[position]);
	position += length;
	return std::string(strBuffer, length);
}

template <>
Position NetworkMessage::read<Position>() {
	Position pos;
	pos.x = read<uint16_t>();
	pos.y = read<uint16_t>();
	pos.z = read<uint8_t>();
	return pos;
}

template <>
void NetworkMessage::write<std::string>(const std::string& value) {
	const size_t length = value.length();
	write<uint16_t>(length);

	expand(length);
	memcpy(&buffer[position], &value[0], length);
	position += length;
}

template <>
void NetworkMessage::write<Position>(const Position& value) {
	write<uint16_t>(value.x);
	write<uint16_t>(value.y);
	write<uint8_t>(value.z);
}

// NetworkConnection
NetworkConnection::NetworkConnection() :
	service(nullptr), thread(), stopped(false) {
	//
}

NetworkConnection::~NetworkConnection() {
	stop();
}

NetworkConnection& NetworkConnection::getInstance() {
	static NetworkConnection connection;
	return connection;
}

bool NetworkConnection::start() {
	if (thread.joinable()) {
		if (stopped) {
			return false;
		}
		return true;
	}

	stopped = false;
	if (!service) {
		service = std::make_unique<boost::asio::io_context>();
	}

	thread = std::thread([this]() -> void {
		boost::asio::io_context& serviceRef = *service;
		try {
			while (!stopped) {
				serviceRef.run_one();
				serviceRef.restart();
			}
		} catch (std::exception& e) {
			spdlog::error("{}", e.what());
		}
	});
	return true;
}

void NetworkConnection::stop() {
	if (!service) {
		return;
	}

	spdlog::info("NetworkConnection::stop started");
	spdlog::default_logger()->flush();

	service->stop();
	stopped = true;

	if (thread.joinable()) {
		spdlog::info("NetworkConnection::stop - joining thread");
		spdlog::default_logger()->flush();
		thread.join();
		spdlog::info("NetworkConnection::stop - thread joined");
		spdlog::default_logger()->flush();
	}

	service.reset();

	spdlog::info("NetworkConnection::stop finished");
	spdlog::default_logger()->flush();
}

boost::asio::io_context& NetworkConnection::get_service() {
	return *service;
}
