#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <thread>
#include <iostream>
#include <vector>
#include <mutex>
#include <queue>

namespace beast = boost::beast;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;
using namespace std;

class ThreadPool {
public:
    ThreadPool(size_t size) : stop(false)
    {
        for (size_t i = 0; i < size; i++)
        {
            workers.emplace_back([this] {
                while (true)
                {
                    function<void()> task;
                    {
                        unique_lock<mutex> lock(queue_mutex);
                        condition.wait(lock, [this] {
                            return stop || !tasks.empty();
                            });
                        if (stop && tasks.empty()) return;
                        task = move(tasks.front());
                        tasks.pop();
                    }
                    task();
                }
                });
        }
    }

    template<class F>
    void enqueue(F&& f)
    {
        {
            unique_lock<mutex> lock(queue_mutex);
            tasks.emplace(forward<F>(f));
        }
        condition.notify_one();
    }

    ~ThreadPool()
    {
        {
            unique_lock<mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for (auto& worker : workers) worker.detach();
    }

    bool isFull() const
    {
        return tasks.size() >= workers.size();
    }

    bool isEmpty()
    {
        return tasks.empty();
    }

private:
    vector<thread> workers;
    queue<function<void()>> tasks;
    mutex queue_mutex;
    condition_variable condition;
    bool stop;
};

void do_session(tcp::socket socket, thread::id thread_id);
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
        cerr << "Error: " << e.what() << endl;
        return 1;
    }
    return 0;
}

void do_session(tcp::socket socket, thread::id thread_id)
{
    try
    {
        cout << "Connected using [" << thread_id << "] thread." << endl;

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
            cout << "Thread: [" << thread_id << "]" << endl;
            cout << endl;

            send_message(ws);

            if (message == "quit") {
                cout << "Received 'quit' message. Terminating connection." << endl;
                break;
            }
        }

        cout << "Thread [" << thread_id << "] released" << endl;
        cout << endl;
    }
    catch (beast::system_error const& se)
    {
        if (se.code() != beast::websocket::error::closed)
        {
            cout << se.code().message() << endl;
        }
    }
}

void do_listen(asio::io_context& io_context, unsigned short port)
{
    try
    {
        ThreadPool pool(2);

        tcp::acceptor acceptor(io_context, { tcp::v4(), port});
        bool allSocketDisconnected = true;

        while (true)
        {
            tcp::socket socket(io_context);
            acceptor.accept(socket);

            if (pool.isFull())
            {
                beast::websocket::stream<tcp::socket> ws(std::move(socket));
                ws.accept();
                ws.text(ws.got_text());
                ws.write(asio::buffer("Server is busy. Please try again later."));
                ws.close(beast::websocket::close_code::normal);
            }
            else
            {
                auto shared_socket = make_shared<tcp::socket>(move(socket));
                pool.enqueue([shared_socket]() {
                    auto thread_id = this_thread::get_id();
                    do_session(move(*shared_socket), thread_id);
                    });

                if (!pool.isEmpty())
                {
                    allSocketDisconnected = false;
                }
            }
        }

        if (allSocketDisconnected)
        {
            cout << "All sockets disconnected. Waiting for connection..." << endl;
        }

    }
    catch (beast::system_error const& se)
    {
        if (se.code() != beast::websocket::error::closed)
        {
            cout << se.code().message() << endl;
        }
    }
}

void send_message(beast::websocket::stream<tcp::socket>& ws)
{
    try
    {
        string text;
        cout << "Send message: ";
        cin >> text;

        ws.text(ws.got_text());
        ws.write(asio::buffer(text));
    }
    catch (beast::system_error const& se)
    {
        if (se.code() != beast::websocket::error::closed)
        {
            cout << se.code().message() << endl;

        }
    }
}
