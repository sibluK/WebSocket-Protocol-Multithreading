#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <thread>
#include <iostream>

namespace beast = boost::beast;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;
using namespace std;

void do_session(tcp::socket socket);
void do_listen(asio::io_context& io_context, unsigned short port);
void send_message(beast::websocket::stream<tcp::socket>& ws);

int main()
{
	try
	{
		asio::io_context io_context;
		do_listen(io_context, 8080);
		io_context.run();
	}
	catch (const exception& e)
	{
		cout << "Error: " << e.what() << endl;
		return 1;
	}
	return 0;
}

void do_session(tcp::socket socket)
{
	try
	{
		thread::id thread_id = this_thread::get_id();
		cout << "New thread created for connection: " << thread_id << endl;

		beast::websocket::stream<tcp::socket> ws(move(socket));
		ws.accept();

		beast::flat_buffer buffer;

		while (true)
		{
			ws.read(buffer);
			string message = beast::buffers_to_string(buffer.data());
			buffer.consume(buffer.size());

			cout << endl;
			cout << "Received message: " << message << endl;
			cout << "Thread ID: " << thread_id << endl;
			cout << endl;

			send_message(ws);
		}
	}
	catch (const exception& e)
	{
		cout << "Error: " << e.what() << endl;
	}
}


void do_listen(asio::io_context& io_context, unsigned short port)
{
	try
	{
		tcp::acceptor acceptor(io_context, { tcp::v4(), port });

		while (true)
		{
			tcp::socket socket(io_context);
			acceptor.accept(socket);
			thread t(do_session, move(socket));
			t.detach();
		}
	}
	catch (const exception& e)
	{
		cout << "Error: " << e.what() << endl;
	}
}

void send_message(beast::websocket::stream<tcp::socket>& ws)
{
	try
	{
		string text;
		cout << "Send message: ";
		cin >> text;

		ws.text(ws.got_text()); // nustato kokio tipo zinute gavo ir kokia reikia siusti (text arba binary)
		ws.write(asio::buffer(text)); // siuncia zinute streamu
	}
	catch(const exception &e)
	{
		cout << "Error: " << e.what() << endl;
	}
}



