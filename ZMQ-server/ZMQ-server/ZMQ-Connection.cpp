#include <zmq.h>
#include <assert.h>  
#include <time.h>
#include <cstdlib>
#include <iostream>
#include <string>
#include <sstream>

// for sleep
#include <chrono>
#include <thread>
using namespace std::chrono_literals;


class Publisher 
{
public:
    Publisher(const char* port)
        :  mPort(port)
    {
        mPublisher = zmq_socket(mContext, ZMQ_PUB);
        std::string address = "tcp://10.88.100.135:" + std::string(mPort);
        auto rc = zmq_bind(mPublisher, address.c_str());
        assert(rc == 0);

        //  Initialize random number generator
        srand((unsigned)time(NULL));

        std::cout << "I'm publisher binded to port "<< mPort <<" \n";
    }

    ~Publisher()
    {
        zmq_close(mPublisher);
        zmq_ctx_destroy(mContext);
    }

    void sendMsg()
    {
        std::cout << "Start sending messages... \n";

        while (true)
        {
            //  Get values that will fool the boss
            int prefix, message;
            prefix = rand();
            message = rand();
            std::cout << "Send the following data: " << prefix << " " << message << "\n";

            //  Send message to all subscribers
            char update[20];
            sprintf_s(update, "%05d-%d", prefix, message);
            zmq_send(mPublisher, update, 20, 0);

            std::this_thread::sleep_for(5000ms);
        }
    }


    const char* mPort;
    void* mPublisher;
    void* mContext = zmq_ctx_new();
    // A ØMQ context is thread safe and may be shared among as many application threads as necessary,
    // without any additional locking required on the part of the caller.
};

class Subscriber
{
public:
    Subscriber(const char* port)
        : mPort(port)
    {
        // SUBSCRIBER response 
        mSubscriber = zmq_socket(mContext, ZMQ_SUB);
        std::string address = "tcp://10.88.100.135:" + std::string(mPort);
        auto  rc_res = zmq_connect(mSubscriber, address.c_str());
        assert(rc_res == 0);

        // zmq_setsockopt set ØMQ socket options
        // ZMQ_SUBSCRIBE socket option shall establish a new message filter on a ZMQ_SUB socket. 
        // Newly created ZMQ_SUB sockets shall filter out all incoming messages, therefore you should call this option to establish an initial message filter. 
        //An empty option_value of length zero shall subscribe to all incoming messages.
        //A non - empty option_value shall subscribe to all messages beginning with the specified prefix. 
        //Multiple filters may be attached to a single ZMQ_SUB socket, in which case a message shall be accepted if it matches at least one filter.
        char filter[1] = { '1' };
        rc_res = zmq_setsockopt(mSubscriber, ZMQ_SUBSCRIBE,filter , 1);
        assert(rc_res == 0);

        std::cout << "I'm subscriber connected to port " << mPort << "and I only want to receive message with prefix '1' !!!" << " \n";
    }

    ~Subscriber()
    {
        zmq_close(mSubscriber);
        zmq_ctx_destroy(mContext);
    }

    void receiveMsg()
    {
        std::cout << "\n Start receiving messages... \n";
        char response[20];
        while (true)
        {           
            auto const received = zmq_recv(mSubscriber, response, 20, 0);
            std::stringstream ss;
            ss << response;
            std::cout << "Received message. Bytes: " << received << ". Message: " << ss.str() << "\n";

            std::this_thread::sleep_for(5000ms);
        }
    }

    const char* mPort;
    void* mSubscriber;
    void* mContext = zmq_ctx_new();
    // A ØMQ context is thread safe and may be shared among as many application threads as necessary,
    // without any additional locking required on the part of the caller.
};


class Connection
{
public:
    Connection(const char* publishingPort, const char* subscribingPort)
        : mPub(publishingPort),
        mSub(subscribingPort)
    {
        std::cout << "-- Create connection -- \n";

        // There’s no start and no end to this stream of updates, it’s like a never ending broadcast.
        std::thread first(&Publisher::sendMsg, mPub);
        std::thread second(&Subscriber::receiveMsg, mSub);

        // synchronize threads:
        first.join();                // pauses until first finishes
        second.join();               // pauses until second finishes
    }

    ~Connection() = default;

    Publisher mPub;
    Subscriber mSub;
};


int main(int argc, char* argv[])
{
    // Read port numbers for sending and responding
    const char* portToSend = (argc > 0) ? argv[1] : "0";
    const char* portToReceived = (argc > 1) ? argv[2] : "0";

    // Create connection 
    Connection connection(portToSend, portToReceived);
   
    return 0;
}


// NOTES:
// 1. Contexts help manage any sockets that are created as well as the number of threads ZeroMQ uses behind the scenes. 
// Create one when you initialize a process and destroy it as the process is terminated. 
// Contexts can be shared between threads and, in fact, are the only ZeroMQ objects that can safely do this.

// So is it a 'container' for ZMQ stuff?


// 2. When you use a SUB socket you must set a subscription using zmq_setsockopt() and SUBSCRIBE, as in this code. 
// 3. If you don’t set any subscription, you won’t get any messages.
// 4. The subscriber can set many subscriptions, which are added together.

// 5. The PUB-SUB socket pair is asynchronous.
// 6. The client does zmq_recv(), in a loop (or once if that’s all it needs). Trying to send a message to a SUB socket will cause an error. 
// Similarly, the service does zmq_send() as often as it needs to, but must not do zmq_recv() on a PUB socket.
// 7. you do not know precisely when a subscriber starts to get messages. Even if you start a subscriber, wait a while, and then start the publisher, the subscriber will always miss the first messages that the publisher sends.
// This is because as the subscriber connects to the publisher (something that takes a small but non-zero time), the publisher may already be sending messages out.
// 

// Source: https://zguide.zeromq.org/docs/chapter1/