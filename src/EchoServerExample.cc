#include <phosg/Network.hh>
#include <phosg-event/EventBase.hh>
#include <phosg-event/CoroutineStreamServer.hh>
#include <phosg-event/AwaitableBufferEvent.hh>

using namespace std;



class EchoServer : public CoroutineStreamServer {
public:
  EchoServer(EventBase& base, SSL_CTX* ssl_ctx = nullptr);
  virtual ~EchoServer() = default;

  virtual task<> handle_connection(AwaitableBufferEvent& abev) {
    for (;;) {
      co_await abev.await_bytes_available(1);
      abev.get_output().add_buffer(abev.get_input());
    }
  }
};



int main(int argc, char** argv) {
  int port = (argc > 1) ? atoi(argv[1]) : 5050;

  EventBase base;
  EchoServer server(base, nullptr);

  scoped_fd fd = listen(port);
  server.add_socket(fd);

  base.dispatch();

  return 0;
}
