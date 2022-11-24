#include <zmq.h>

#include <assert.h>  
#include <time.h>
#include <cstdlib>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>

// basic file operations
#include <iostream>
#include <fstream>
#include <limits>
#include <sstream>

// for thread
#include <chrono>
#include <thread>
#include <condition_variable>
#include <mutex>
using namespace std::chrono_literals;

// ZMQ 4.3.4
// The aim of this code is to test 'bidirectional' PUB-SUB patter.
// It is a very C-style code. It has been developed only for testing purposes.
// There is a simple filtering applied on the SUB socket.

// I am not using CZMQ, just plain ZeroMQ.
//ZeroMQ supports a few different authentication methods : NULL, PLAINand CURVE.The only one worth talking about is CURVE, which is based on elliptic public key encryption.
/*This is standard public/private key encryption – both the client and the server each have a public and a private key. 
The client only needs to know the server’s public key to connect to it. 
In ZeroMQ this works by creating key pairs with zmq_curve_keypair and then setting these keys to the sockets with various calls to zmq_setsockopt. 
Once the connection is made, ZeroMQ encrypts the entire conversation.*/

// It is possible to add an extra authentication - ZAP (ZeroMQ Authentication Protocol).

std::string globalWPath = "D:\\ZMQ testing app\\ZMQ-PUB_SUB\\ZMQ-server\\x64\\Debug\\";
std::string globalWPath2 = "Z:\\keys\\";
std::condition_variable READY_TO_BIND;
std::mutex m;

std::fstream& GotoLine(std::fstream& file, const char* num)
{
    std::stringstream strNumber;
    strNumber << num;
    unsigned int intNumber;
    strNumber >> intNumber;

    file.seekg(std::ios::beg);
    for (int i = 0; i < intNumber - 1; ++i) {
        file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
    return file;
}

class Publisher 
{
public:
    Publisher(const char* port, const char* addressForPub, const char* nodeId, const char* friendNodeId, const char server_secret_key[41])
        :  mPort(port), mAddressForPub(addressForPub)
    {
        std::cout << " --------- PUBLISHER ---------------------------------------- \n";
        mPublisher = zmq_socket(mContext, ZMQ_PUB);
        std::string addressPortTCP = "tcp://" + std::string(mAddressForPub) + ":" + std::string(mPort);

        // Show keys
        //std::cout << "Publisher's secret key: " << std::string(server_secret_key) << "\n\n";
        //const int curve_server_enable = 1;
        //zmq_setsockopt(mPublisher, ZMQ_CURVE_SERVER, &curve_server_enable, sizeof(curve_server_enable));
        //zmq_setsockopt(mPublisher, ZMQ_CURVE_SECRETKEY, server_secret_key, 40);
       
        //auto wait = std::unique_lock<std::mutex>(m);
        //READY_TO_BIND.wait(wait);
        auto rc = zmq_bind(mPublisher, addressPortTCP.c_str());
        assert(rc == 0);
 
        //  Initialize random number generator
        srand((unsigned)time(NULL));

        std::cout << "I'm publisher binded to port "<< mPort <<" \n\n";
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
            char update[100];
            sprintf_s(update, "%05d-%d", prefix, message);
            zmq_send(mPublisher, update, 100, 0);

            std::this_thread::sleep_for(2000ms);
        }
    }

    const char* mAddressForPub;
    const char* mPort;
    const char* mNodeId;
    void* mPublisher;
    void* mContext = zmq_ctx_new();
    // A ØMQ context is thread safe and may be shared among as many application threads as necessary,
    // without any additional locking required on the part of the caller.
};

class Subscriber
{
public:
    Subscriber(const char* port, const char* address, const char* nodeId, const char* friendNodeId)
        : mPort(port), mAddress (address), mNodeId(nodeId)
    {
        std::cout << " --------- SUBSCRIBER ---------------------------------------- \n";
        // Generate client keys
        //char client_public_key[41], client_secret_key[41];
        //auto isGenerated = zmq_curve_keypair(client_public_key, client_secret_key);

        //if (isGenerated == 0)
        //{
        //    std::cout << "Subscriber - zmq_curve_keypair - success! \n";
        //    std::cout << "Client (sub) generated public key: " << std::string(client_public_key) << "\n";
        //    std::cout << "Client (sub) generated secret key: " << std::string(client_secret_key) << "\n";
        //}
        //else
        //{
        //    std::cout << "zmq_curve_keypair - failed! \n";
        //}

        ////std::this_thread::sleep_for(5000ms);
        //char server_public_key[41];
        //server_public_key[0] = 0;
        //std::string fileName = globalWPath + "publicKey";
        //while (server_public_key[0] == 0)
        //{
        //    std::fstream server_public_key_file(fileName);

        //    GotoLine(server_public_key_file, friendNodeId);
        //    server_public_key_file >> server_public_key;
        //}

        //std::cout << "Subscriber received public key: " << std::string(server_public_key) << "\n\n";

        // SUBSCRIBER response 
        mSubscriber = zmq_socket(mContext, ZMQ_SUB);
        std::string addressPortTCP = "tcp://" + std::string(mAddress) + ":" + std::string(mPort);

        // zmq_setsockopt set ØMQ socket options
        // ZMQ_SUBSCRIBE socket option shall establish a new message filter on a ZMQ_SUB socket. 
        // Newly created ZMQ_SUB sockets shall filter out all incoming messages, therefore you should call this option to establish an initial message filter. 
        // An empty option_value of length zero shall subscribe to all incoming messages.
        // A non - empty option_value shall subscribe to all messages beginning with the specified prefix. 
        // Multiple filters may be attached to a single ZMQ_SUB socket, in which case a message shall be accepted if it matches at least one filter.
        const char filter[1] = { *nodeId };
        auto rc_res = zmq_setsockopt(mSubscriber, ZMQ_SUBSCRIBE, filter, 1);
        assert(rc_res == 0);

        // Set the server's public key, and our public and secret keys. You have to do this before 
        // binding or else it won't work
        //auto result = zmq_setsockopt(mSubscriber, ZMQ_CURVE_SERVERKEY, server_public_key, 40);
        //result = zmq_setsockopt(mSubscriber, ZMQ_CURVE_PUBLICKEY, client_public_key, 40);
        //result = zmq_setsockopt(mSubscriber, ZMQ_CURVE_SECRETKEY, client_secret_key, 40);
       // assert(result == 0);
        //auto notify = std::unique_lock<std::mutex>(m);
        //READY_TO_BIND.notify_all();
        
        rc_res = zmq_connect(mSubscriber, addressPortTCP.c_str());
        assert(rc_res == 0);

        std::cout << "I'm subscriber connected to port " << mPort << " and I only want to receive message with prefix " << nodeId <<".\n\n";
    }

    ~Subscriber()
    {
        zmq_close(mSubscriber);
        zmq_ctx_destroy(mContext);
    }

    void receiveMsg()
    {
        std::cout << "\n Start receiving messages... \n";
        char response[100];
        while (true)
        {           
            auto const received = zmq_recv(mSubscriber, response, 100, 0);
            std::stringstream ss;
            ss << response;
            std::cout << "Received message. Bytes: " << received << ". Message: " << ss.str() << "\n";

            std::this_thread::sleep_for(2000ms);
        }
    }

    const char* mPort;
    const char* mAddress;
    const char* mNodeId;
    void* mSubscriber;
    void* mContext = zmq_ctx_new();
    // A ØMQ context is thread safe and may be shared among as many application threads as necessary,
    // without any additional locking required on the part of the caller.

    const char* mServer_public_key;
};


class Connection
{
public:
    Connection(const char* publishingPort, const char* subscribingPort, const char* addressToReceive, const char* addressForPub, const char* nodeId, const char* friendNodeId, const char server_secret_key[41])
        : mPub(publishingPort, addressForPub, nodeId, friendNodeId, server_secret_key),
        mSub(subscribingPort, addressToReceive, nodeId, friendNodeId)
    {
        std::cout << "-------- Create PUB_SUB connection ------- \n";

        //// There’s no start and no end to this stream of updates, it’s like a never ending broadcast.
        std::thread first(&Publisher::sendMsg, mPub);
        std::thread second(&Subscriber::receiveMsg, mSub);

        // synchronize threads:
        first.join();                // pauses until first finishes
        second.join();               // pauses until second finishes

        //std::vector<std::thread> threads;
        //threads.emplace_back(std::thread(&Publisher::sendMsg, mPub));
        //threads.emplace_back(std::thread(&Subscriber::receiveMsg, mSub));
    }

    ~Connection() = default;

    Publisher mPub;  
    Subscriber mSub;
};


int main(int argc, char* argv[])
{
    // Read port numbers for sending and responding
    const char* nodeId = (argc > 0) ? argv[1] : "0";
    const char* friendNodeId = (argc > 1) ? argv[2] : "0";
    const char* portToSend = (argc > 2) ? argv[3] : "0"; //5000
    const char* portToReceive = (argc > 3) ? argv[4] : "0"; //5001
    const char* addressToReceive = (argc > 4) ? argv[5] : "localhost";
    const char* addressForPub = (argc > 5) ? argv[6] : "*";

    std::cout << " --------- MAIN ---------------------------------------- \n";
    if (!zmq_has("curve"))
    {
        std::cout << "ZeroMQ library has not been built with Curve support \n";
        return 0;
    }
    else
    {
        std::cout << "ZeroMQ library has been built with Curve support. \n";
        std::cout << "Now, generate curve keypair - public and secret key. Pass public key to the subscriber and private key to the publisher\n";
    }

    // Generate server keys. The server thread needs to know the private key and the client thread 
    // needs to know the public key.
    char server_public_key[41], server_secret_key[41];
    //auto isKeyPairGenerated = zmq_curve_keypair(server_public_key, server_secret_key);
    //if (isKeyPairGenerated == 0)
    //{
    //    std::cout << "zmq_curve_keypair - success! \n";
    //    std::cout << "Public key - saved into the file: " << std::string(server_public_key) << "\n";;
    //    std::cout << "Secret key - passed to the publisher: "<< std::string(server_secret_key) << "\n\n";;

    //    std::ofstream publicKey;
    //    std::string fileNamePublicKey = globalWPath+"publicKey"; 
    //    publicKey.open(fileNamePublicKey, std::ios::out | std::ios::app);
    //    publicKey << server_public_key << std::endl;
    //    publicKey.close();
    //}
    //else
    //{
    //    std::cout << "zmq_curve_keypair - failed! \n";
    //}

    // Create connection 
    Connection connection(portToSend, portToReceive, addressToReceive, addressForPub, nodeId, friendNodeId, server_secret_key);
   
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