#include "Client.h"

#ifndef LOG_SECTION
#define LOG_SECTION "Http"
#endif

#include "../../Logger.h"

#include <boost/asio.hpp>
#include <fcntl.h>
#include <io.h>

class AsioTimer : public curlion::Timer {
public:
    explicit AsioTimer(boost::asio::io_service& io_service) : timer_(io_service) {

    }

    void Start(long timeout_ms, const std::function<void()>& callback) override {

        timer_.expires_from_now(boost::posix_time::milliseconds(timeout_ms));
        timer_.async_wait(std::bind(&AsioTimer::TimerTriggered, this, callback, std::placeholders::_1));
    }

    void Stop() override {

        timer_.cancel();
    }

private:
    void TimerTriggered(const std::function<void()>& callback, const boost::system::error_code& error) {

        if (error == boost::asio::error::operation_aborted) {
            return;
        }

        callback();
    }

private:
    boost::asio::deadline_timer timer_;
};

class AsioSingleSocketWatcher : public std::enable_shared_from_this<AsioSingleSocketWatcher> {
public:
    AsioSingleSocketWatcher(const std::shared_ptr<boost::asio::ip::tcp::socket>& socket,
        curlion::SocketWatcher::Event event,
        const curlion::SocketWatcher::EventCallback& callback) :
        socket_(socket),
        event_(event),
        callback_(callback),
        is_stopped_(false) {


    }

    void Start() {

        Watch();
    }

    void Stop() {

        //Stop may be called inside an event callback, so a flag variable
        //is needed for stopping the watching.
        is_stopped_ = true;

        //The socket may be closed before calling Stop, check for
        //preventing exception.
        if (socket_->is_open()) {
            socket_->cancel();
        }
    }

private:
    void Watch() {

        bool watch_read_event = (event_ == curlion::SocketWatcher::Event::Read) || (event_ == curlion::SocketWatcher::Event::ReadWrite);
        bool watch_write_event = (event_ == curlion::SocketWatcher::Event::Write) || (event_ == curlion::SocketWatcher::Event::ReadWrite);

        if (watch_read_event) {

            auto bridge_callback = std::bind(&AsioSingleSocketWatcher::EventTriggered,
                this->shared_from_this(),
                false,
                std::placeholders::_1,
                std::placeholders::_2);

            socket_->async_read_some(boost::asio::null_buffers(), bridge_callback);
        }

        if (watch_write_event) {

            auto bridge_callback = std::bind(&AsioSingleSocketWatcher::EventTriggered,
                this->shared_from_this(),
                true,
                std::placeholders::_1,
                std::placeholders::_2);

            socket_->async_write_some(boost::asio::null_buffers(), bridge_callback);
        }
    }

    void EventTriggered(bool can_write,
        const boost::system::error_code& error,
        std::size_t size) {

        if (error == boost::asio::error::operation_aborted) {
            return;
        }

        callback_(socket_->native_handle(), can_write);

        //End watching once stopped.
        if (!is_stopped_) {
            Watch();
        }
    }

private:
    std::shared_ptr<boost::asio::ip::tcp::socket> socket_;
    curlion::SocketWatcher::Event event_;
    curlion::SocketWatcher::EventCallback callback_;
    bool is_stopped_;
};

class AsioSocketManager : public curlion::SocketFactory, public curlion::SocketWatcher {
public:
    explicit AsioSocketManager(boost::asio::io_service& io_service) : io_service_(io_service) {

    }


    curl_socket_t Open(curlsocktype socket_type, const curl_sockaddr* address) override {

        if ((socket_type != CURLSOCKTYPE_IPCXN) || (address->family != AF_INET)) {
            return CURL_SOCKET_BAD;
        }

        auto asio_socket = std::make_shared<boost::asio::ip::tcp::socket>(io_service_);

        boost::system::error_code error;
        asio_socket->open(boost::asio::ip::tcp::v4(), error);

        if (error) {
            return CURL_SOCKET_BAD;
        }

        sockets_.insert(std::make_pair(asio_socket->native_handle(), asio_socket));
        return asio_socket->native_handle();
    }


    bool Close(curl_socket_t socket) override {

        auto iterator = sockets_.find(socket);
        if (iterator == sockets_.end()) {
            return false;
        }

        auto asio_socket = iterator->second;
        sockets_.erase(iterator);

        boost::system::error_code error;
        asio_socket->close(error);
        return !error;
    }


    void Watch(curl_socket_t socket, Event event, const EventCallback& callback) override {

        auto iterator = sockets_.find(socket);
        if (iterator == sockets_.end()) {
            return;
        }

        auto watcher = std::make_shared<AsioSingleSocketWatcher>(iterator->second, event, callback);
        watcher->Start();
        watchers_.insert(std::make_pair(socket, watcher));
    }


    void StopWatching(curl_socket_t socket) override {

        auto iterator = watchers_.find(socket);
        if (iterator == watchers_.end()) {
            return;
        }

        auto watcher = iterator->second;
        watcher->Stop();
        watchers_.erase(iterator);
    }

private:
    std::map<curl_socket_t, std::shared_ptr<boost::asio::ip::tcp::socket>> sockets_;
    std::map<curl_socket_t, std::shared_ptr<AsioSingleSocketWatcher>> watchers_;
    boost::asio::io_service& io_service_;
};

Client::Client()
{
    io_service = new boost::asio::io_service;
    // auto work = std::make_shared<boost::asio::io_service::work>(io_service);

    auto timer = std::make_shared<AsioTimer>(*(boost::asio::io_service*)io_service);
    auto socket_manager = std::make_shared<AsioSocketManager>(*(boost::asio::io_service*)io_service);

    connection_manager = std::make_unique<curlion::ConnectionManager>(socket_manager, socket_manager, timer);
}

Client::~Client()
{
    delete io_service;
}

std::error_condition Client::StartConnection(const std::shared_ptr<curlion::Connection>& connection)
{
    return connection_manager->StartConnection(connection);
}

std::error_condition Client::AbortConnection(const std::shared_ptr<curlion::Connection>& connection)
{
    return connection_manager->AbortConnection(connection);
}

bool Client::Execute(const std::shared_ptr<curlion::HttpConnection>& connection, bool allowNon200)
{
    char errbuf[CURL_ERROR_SIZE];
    curl_easy_setopt(connection->GetHandle(), CURLOPT_ERRORBUFFER, errbuf);

    int retryCount = 5;
    FILE* vFp;
    do {
        if (retryCount == 1) {
            connection->SetVerbose(true);
            vFp = CreateTempFile();
            curl_easy_setopt(connection->GetHandle(), CURLOPT_STDERR, vFp);
        }
        connection->Start();
        if (connection->GetResult() != CURLE_OK) {
            LOG_ERROR("Curl error %d: %s", connection->GetResult(),
                strlen(errbuf) ?
                errbuf :
                curl_easy_strerror(connection->GetResult()));
        }
        else if (!allowNon200 && connection->GetResponseCode() != 200) {
            LOG_ERROR("Response code was %d. Response data: %s", connection->GetResponseCode(), connection->GetResponseBody().c_str());
        }
        else {
            return true;
        }
    } while (--retryCount);

    auto vSize = ftell(vFp);
    auto vData = std::unique_ptr<char[]>(new char[vSize + 1]);

    rewind(vFp);
    fread(vData.get(), vSize, 1, vFp);
    vData.get()[vSize] = '\0';
    fclose(vFp);

    LOG_ERROR("Ran out of retries. Debug info:\n%s", vData.get());
    return false;
}

// from https://github.com/martinh/libconfuse/blob/c0e7e74bc492a1791c044208d26502a559c03cb9/src/fmemopen.c#L124
FILE* Client::CreateTempFile()
{
    int fd;
    FILE* fp;
    char tp[MAX_PATH - 13];
    char fn[MAX_PATH + 1];

    if (!GetTempPathA(sizeof(tp), tp))
        return NULL;

    if (!GetTempFileNameA(tp, "confuse", 0, fn))
        return NULL;

    fd = _open(fn,
        _O_CREAT | _O_RDWR | _O_SHORT_LIVED | _O_TEMPORARY | _O_BINARY,
        _S_IREAD | _S_IWRITE);
    if (fd == -1)
        return NULL;

    fp = _fdopen(fd, "w+");
    if (!fp) {
        _close(fd);
        return NULL;
    }

    return fp;
}
