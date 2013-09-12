#include <bitcoin/bitcoin.hpp>
#include <obelisk/obelisk.hpp>
#include "beginwindow.h"

using namespace bc;
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

bool stopped = false;

void handle_start(const std::error_code& ec)
{
    if (ec)
    {
        std::cerr << "ERROR: " << ec.message() << std::endl;
        exit(-1);
    }
}

void output_to_file(std::ofstream& file, bc::log_level level,
    const std::string& domain, const std::string& body)
{
    if (body.empty())
        return;
    file << bc::level_repr(level);
    if (!domain.empty())
        file << " [" << domain << "]";
    file << ": " << body << std::endl;
}

// warning: this is not good code!
void broadcast_subsystem()
{
    std::ofstream outfile("wallet.log");
    log_debug().set_output_function(
        std::bind(output_to_file, std::ref(outfile), _1, _2, _3));
    log_info().set_output_function(
        std::bind(output_to_file, std::ref(outfile), _1, _2, _3));
    log_warning().set_output_function(
        std::bind(output_to_file, std::ref(outfile), _1, _2, _3));
    log_error().set_output_function(
        std::bind(output_to_file, std::ref(outfile), _1, _2, _3));
    log_fatal().set_output_function(
        std::bind(output_to_file, std::ref(outfile), _1, _2, _3));
    threadpool pool(4);
    // Create dependencies for our protocol object.
    hosts hst(pool);
    handshake hs(pool);
    network net(pool);
    // protocol service.
    protocol prot(pool, hst, hs, net);
    prot.set_max_outbound(4);
    // Perform node discovery if needed and then creating connections.
    prot.start(handle_start);
    // wait
    while (!stopped)
    {
        sleep(0.2);
        // if any new shit then broadcast it.
        broadcast_mutex.lock();
        if (tx_broadcast_queue.empty())
        {
            broadcast_mutex.unlock();
            continue;
        }
        transaction_type tx = tx_broadcast_queue.back();
        tx_broadcast_queue.pop_back();
        broadcast_mutex.unlock();
        auto ignore_send = [](const std::error_code&, size_t) {};
        prot.broadcast(tx, ignore_send);
    }
    auto ignore_stop = [](const std::error_code&) {};
    prot.stop(ignore_stop);
    // Safely close down.
    pool.stop();
    pool.join();
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    bc::threadpool pool(1);
    obelisk::fullnode_interface fullnode(pool, "tcp://46.4.92.107:9091");
    BeginWindow window(fullnode);
    std::thread thr([&fullnode]()
        {
            while (!stopped)
            {
                fullnode.update();
                sleep(0.1);
            }
        });
    std::thread broadcaster(broadcast_subsystem);
    app.exec();
    stopped = true;
    broadcaster.join();
    thr.join();
    pool.stop();
    pool.join();
    return 0;
}

