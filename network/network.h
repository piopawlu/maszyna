#pragma once
#include <memory>
#include <functional>
#include <optional>
#include <queue>
#include <chrono>
#include "network/message.h"
#include "command.h"

namespace network
{
    //m7todo: separate client/server connection class?
    class connection
	{
		friend class server;
		friend class client;

	private:
		const int CATCHUP_PACKETS = 500;

		/*
		std::queue<
		    std::pair<std::chrono::high_resolution_clock::time_point,
		    std::shared_ptr<frame_info>>> delta_queue;

		command_queue::commands_map client_commands_queue;
		bool is_client;

		//std::chrono::high_resolution_clock::time_point last_time;
		//double accum = -1.0;
		*/
		bool is_client;

	protected:
		std::shared_ptr<std::istream> backbuffer;
		size_t backbuffer_pos;
		size_t packet_counter;

		void send_complete(std::shared_ptr<std::string> buf);
		void catch_up();

	public:
		std::function<void(const message &msg)> message_handler;

		virtual void connected() = 0;
		virtual void send_message(const message &msg) = 0;
		virtual void send_messages(const std::vector<std::shared_ptr<message>> &messages) = 0;

		connection(bool client = false, size_t counter = 0);
		void set_handler(std::function<void(const message &msg)> handler);

		virtual void disconnect() = 0;

		enum peer_state {
			AWAITING_HELLO,
			CATCHING_UP,
			ACTIVE,
			DEAD
		};
		peer_state state;
	};

	class server
	{
	private:
		std::shared_ptr<std::istream> backbuffer;

	protected:
		void handle_message(std::shared_ptr<connection> conn, const message &msg);

		std::vector<std::shared_ptr<connection>> clients;

		command_queue::commands_map client_commands_queue;

	public:
		server(std::shared_ptr<std::istream> buf);
		void push_delta(const frame_info &msg);
		command_queue::commands_map pop_commands();
	};

	class client
	{
	protected:
		virtual void connect() = 0;
		void handle_message(std::shared_ptr<connection> conn, const message &msg);
		std::shared_ptr<connection> conn;
		size_t resume_frame_counter = 0;
		size_t reconnect_delay = 0;

		const size_t RECONNECT_DELAY_FRAMES = 60;

		std::queue<
		    std::pair<std::chrono::high_resolution_clock::time_point,
		        frame_info>> delta_queue;

	public:
		void update();
		std::tuple<double, double, command_queue::commands_map> get_next_delta();
		void send_commands(command_queue::commands_map commands);
		int get_frame_counter() {
			return resume_frame_counter;
		}
		int get_awaiting_frames() {
			return delta_queue.size();
		}
	};

	class backend_manager
	{
	public:
		virtual std::shared_ptr<server> create_server(std::shared_ptr<std::fstream>, const std::string &conf) = 0;
		virtual std::shared_ptr<client> create_client(const std::string &conf) = 0;
		virtual void update() = 0;
	};

	extern std::unordered_map<std::string, backend_manager*> backend_list;
}