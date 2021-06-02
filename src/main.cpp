#include <activemq/library/ActiveMQCPP.h>
#include <activemq/core/ActiveMQConnectionFactory.h>

#include <decaf/lang/Runnable.h>
#include <decaf/lang/Thread.h>
#include <decaf/util/concurrent/CountDownLatch.h>

#include <cms/CMSException.h>
#include <cms/ExceptionListener.h>
#include <cms/MessageListener.h>
#include <cms/BytesMessage.h>
#include <cms/Connection.h>
#include <cms/ConnectionFactory.h>
#include <cms/Session.h>

#include "zlib.h"

#include <atomic>
#include <cstdlib>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <iostream>
#include <memory>


namespace {
  constexpr std::string_view c_hostname{"darwin-dist-44ae45.nationalrail.co.uk"};
  constexpr std::string_view c_hostport{"61613"};
  constexpr std::string_view c_topic{"darwin.pushport-v16"};
  constexpr long c_waitMillisec{15000};
  constexpr int c_decompressedBufferSizeMultiplier{8};
  constexpr int c_numMessagesToProcess{100};
  std::string_view getEnv(std::string_view var) {
    const char* p_val = std::getenv(var.data());
    return (p_val != nullptr) ? p_val : "";
  }
}


class HelloWorldConsumer : public cms::ExceptionListener,
                           public cms::MessageListener,
                           public decaf::lang::Runnable {
private:
  std::string m_brokerURI;
  std::string m_usr;
  std::string m_pwd;
  std::atomic_size_t m_messageCount{0};
  decaf::util::concurrent::CountDownLatch m_latch;
  decaf::util::concurrent::CountDownLatch m_doneLatch;
  cms::Connection* m_connection{nullptr};
  cms::Session* m_session{nullptr};
  cms::Destination* m_destination{nullptr};
  cms::MessageConsumer* m_consumer{nullptr};

public:
  HelloWorldConsumer(std::string brokerURI, std::string usr, std::string pwd, int numMessages)
  : m_brokerURI(std::move(brokerURI)),
    m_usr(std::move(usr)),
    m_pwd(std::move(pwd)),
    m_latch(1),
    m_doneLatch(numMessages) {}

  ~HelloWorldConsumer() override {cleanup();}
  HelloWorldConsumer(const HelloWorldConsumer&) = delete;
  HelloWorldConsumer(HelloWorldConsumer&&) = delete;
  HelloWorldConsumer& operator=(const HelloWorldConsumer&) = delete;
  HelloWorldConsumer& operator=(HelloWorldConsumer&&) = delete;

  void close() {cleanup();}

  void waitUntilReady() {m_latch.await();}

  void run() override {
    try {
      // Create a ConnectionFactory
      std::unique_ptr<cms::ConnectionFactory> connectionFactory{
        cms::ConnectionFactory::createCMSConnectionFactory(m_brokerURI)};

      auto* amq_cf = dynamic_cast<activemq::core::ActiveMQConnectionFactory*>(connectionFactory.get());
      if (amq_cf == nullptr) {
        std::cerr << "amq_cf == nullptr" << std::endl;
      } else {
        amq_cf->setWatchTopicAdvisories(false);
      }

      // Create a Connection
      std::string clientID{getEnv("HOSTNAME")};
      clientID += ".HelloWorldConsumer";
      m_connection = connectionFactory->createConnection(m_usr, m_pwd, clientID);
      m_connection->start();
      m_connection->setExceptionListener(this);

      // Create a Session
      m_session = m_connection->createSession(cms::Session::AUTO_ACKNOWLEDGE);

      // Create the destination (Topic or Queue)
      m_destination = m_session->createTopic(c_topic.data());

      // Create a MessageConsumer from the Session to the Topic or Queue
      m_consumer = m_session->createConsumer(m_destination);
      m_consumer->setMessageListener(this);

      std::cout.flush();
      std::cerr.flush();

      // Indicate we are ready for messages.
      m_latch.countDown();

      // Wait while asynchronous messages come in.
      m_doneLatch.await(c_waitMillisec);

    } catch (cms::CMSException& ex) {
      // Indicate we are ready for messages.
      m_latch.countDown();
      ex.printStackTrace();
    }

  }

  void onMessage(const cms::Message* message) override {
    try {
        ++m_messageCount;
        const auto* bytesMessage = dynamic_cast<const cms::BytesMessage*> (message);
        if (bytesMessage == nullptr) {
          throw cms::CMSException("Message is not a BytesMessage");
        }

        std::cout << std::endl << "Message " << m_messageCount << " received" << std::endl;
        if (message != nullptr) {
          std::cout << "-- type: " << message->getCMSType() << std::endl;
          std::cout << "-- ID: " << message->getCMSMessageID() << std::endl;
          for (const std::string& propName : message->getPropertyNames()) {
            std::cout << "-- " << propName << ": " << message->getStringProperty(propName) << std::endl;
          }
        }

        int destSize = c_decompressedBufferSizeMultiplier * std::stoi(message->getStringProperty("content-length"));
        std::vector<unsigned char> dest(static_cast<size_t>(destSize), '\0');
        size_t srcSize = bytesMessage->getBodyLength();
        auto src = std::span{bytesMessage->getBodyBytes(), srcSize};
        std::vector<unsigned char> srcVec{src.begin(), src.end()};

        std::cout << "-- bodyLength: " << srcSize << std::endl;
        std::cout << "-- bodyBytes: ";
        for (const unsigned char c : srcVec) {
          std::byte b{c};
          std::cout << std::hex << std::to_integer<uint16_t>(b) << std::dec << " ";
        }
        std::cout << std::endl;

        z_stream zstr{
          .next_in = srcVec.data(),
          .avail_in = static_cast<unsigned>(srcVec.size()),
          .next_out = dest.data(),
          .avail_out = static_cast<unsigned>(destSize),
        };
        int zStatus = inflateInit2(&zstr, 16); // 16 means gzip format
        if (zStatus != Z_OK) {
          throw cms::CMSException("zlib inflateInit2 returned status " + std::to_string(zStatus));
        }

        zStatus = inflate(&zstr, Z_NO_FLUSH);
        if (zStatus != Z_OK && zStatus != Z_STREAM_END) {
          throw cms::CMSException("zlib inflate returned status " + std::to_string(zStatus));
        }

        auto destEnd = std::find(dest.begin(), dest.end(),'\0');
        std::string decompressedText{dest.begin(), destEnd};
        std::cout << "-- content: " << decompressedText << std::endl;

    } catch (cms::CMSException& ex) {
      ex.printStackTrace();
    } catch (const std::exception& ex) {
      std::cerr << "onMessage std::exception: " << ex.what() << std::endl;
    } catch (...) {
      std::cerr << "onMessage unknown exception" << std::endl;
    }

    // No matter what, tag the count down latch until done.
    m_doneLatch.countDown();
  }

  void onException(const cms::CMSException& ex) override {
    std::cerr << "CMS exception occurred. Shutting down client" << std::endl;
    ex.printStackTrace();
    exit(EXIT_FAILURE);
  }

private:
  void cleanup() {
    std::cout << "CLEANUP" << std::endl;
    if (m_connection != nullptr) {
      try {
        m_connection->close();
      } catch (cms::CMSException& ex) {
        ex.printStackTrace();
      }
    }

    // Destroy resources.
    try {
        delete m_destination;
        m_destination = nullptr;
        delete m_consumer;
        m_consumer = nullptr;
        delete m_session;
        m_session = nullptr;
        delete m_connection;
        m_connection = nullptr;
    } catch (cms::CMSException& ex) {
        ex.printStackTrace();
    }
  }
};


int main() {
  std::cout << "STARTING" << std::endl;
  activemq::library::ActiveMQCPP::initializeLibrary();

  std::string username{getEnv("DARWIN_USERNAME").data()};
  if (username.empty()) {
    std::cerr << "DARWIN_USERNAME not set" << std::endl;
    return EXIT_FAILURE;
  }
  std::string pwd{getEnv("DARWIN_PASS").data()};
  if (pwd.empty()) {
    std::cerr << "DARWIN_PASS not set" << std::endl;
    return EXIT_FAILURE;
  }

  std::string uri{"tcp://"};
  uri += c_hostname;
  uri += ":";
  uri += c_hostport;
  uri += "?wireFormat=stomp";

  {
    HelloWorldConsumer consumer{uri, username, pwd, c_numMessagesToProcess};
    decaf::lang::Thread consumerThread{&consumer};
    consumerThread.start();
    consumerThread.join();
    consumer.close();
  } // close and delete threads at end of scope, before shutdownLibrary

  activemq::library::ActiveMQCPP::shutdownLibrary();

  std::cout << "EXITING" << std::endl;
  return EXIT_SUCCESS;
}
