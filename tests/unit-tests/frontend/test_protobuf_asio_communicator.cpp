/*
 * Copyright © 2012 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Thomas Voss <thomas.voss@canonical.com>
 *              Alan Griffiths <alan@octopull.co.uk>
 */

#include "mir/frontend/protobuf_asio_communicator.h"

#include "mir_protobuf.pb.h"
#include "mir_client/mir_rpc_channel.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <memory>

namespace mf = mir::frontend;

namespace mir
{
namespace
{
struct SessionCounter : mir::protobuf::DisplayServer
{
    int session_count;
    int connected_sessions;
    std::mutex guard;
    std::condition_variable wait_condition;

    SessionCounter() : session_count(0), connected_sessions(0)
    {
    }

    SessionCounter(SessionCounter const &) = delete;
    void create_surface(google::protobuf::RpcController* /*controller*/,
                 const mir::protobuf::SurfaceParameters* request,
                 mir::protobuf::Surface* response,
                 google::protobuf::Closure* done)
    {
        response->mutable_id()->set_value(13);
        response->set_width(request->width());
        response->set_height(request->height());
        response->set_pixel_format(request->pixel_format());

        std::unique_lock<std::mutex> lock(guard);
        ++session_count;
        ++connected_sessions;
        wait_condition.notify_one();

        done->Run();
    }

    void disconnect(google::protobuf::RpcController* /*controller*/,
                 const mir::protobuf::Void* /*request*/,
                 mir::protobuf::Void* /*response*/,
                 google::protobuf::Closure* done)
    {
        std::unique_lock<std::mutex> lock(guard);
        --connected_sessions;
        wait_condition.notify_one();
        done->Run();
    }
};

struct NullDeleter
{
    void operator()(void* )
    {
    }
};

class MockLogger : public mir::client::Logger
{
    std::ostringstream out;

    std::ostream& dummy_error() { return std::cerr << "ERROR: "; }
//    std::ostream& dummy_error() { return out << "ERROR: "; }

//    std::ostream& dummy_debug() { return std::cerr << "DEBUG: "; }
    std::ostream& dummy_debug() { return out << "DEBUG: "; }

public:
    MockLogger()
    {
        ON_CALL(*this, error())
            .WillByDefault(::testing::Invoke(this, &MockLogger::dummy_error));
        EXPECT_CALL(*this, error()).Times(0);

        ON_CALL(*this, debug())
            .WillByDefault(::testing::Invoke(this, &MockLogger::dummy_debug));
        EXPECT_CALL(*this, debug()).Times(testing::AtLeast(0));
    }

    MOCK_METHOD0(error,std::ostream& ());
    MOCK_METHOD0(debug,std::ostream& ());
};

class MockIpcFactory : public mf::ProtobufIpcFactory
{
public:
    MockIpcFactory(mir::protobuf::DisplayServer& server) :
        server(&server, NullDeleter())
    {
        using namespace testing;

        ON_CALL(*this, make_ipc_server()).WillByDefault(Return(this->server));

        // called during initialisation:
        // there's always a server awaiting the next connection
        EXPECT_CALL(*this, make_ipc_server()).Times(1);
    }

    MOCK_METHOD0(make_ipc_server, std::shared_ptr<mir::protobuf::DisplayServer>());

private:
    std::shared_ptr<mir::protobuf::DisplayServer> server;
};

struct TestServer
{
    static std::string const & socket_name()
    {
        static std::string socket_name("./mir_test_pb_asio_socket");
        return socket_name;
    }

    TestServer() :
        factory(std::make_shared<MockIpcFactory>(collector)),
        comm(socket_name(), factory)
    {
    }

    void expect_session_count(int expected_count)
    {
        std::unique_lock<std::mutex> ul(collector.guard);
        while (collector.session_count != expected_count)
            collector.wait_condition.wait(ul);

        EXPECT_EQ(expected_count, collector.session_count);
    }

    void expect_connected_session_count(int expected_count)
    {
        std::unique_lock<std::mutex> ul(collector.guard);
        while (collector.connected_sessions != expected_count)
            collector.wait_condition.wait(ul);

        EXPECT_EQ(expected_count, collector.connected_sessions);
    }

    // "Server" side
    SessionCounter collector;
    std::shared_ptr<MockIpcFactory> factory;
    mf::ProtobufAsioCommunicator comm;
};

struct TestClient
{
    TestClient() :
        logger(std::make_shared<MockLogger>()),
        channel(TestServer::socket_name(), logger),
        display_server(&channel),
        connect_done_called(false),
        disconnect_done_called(false),
        connect_done_count(0),
        disconnect_done_count(0)
    {
        surface_parameters.set_width(640);
        surface_parameters.set_height(480);
        surface_parameters.set_pixel_format(0);

        ON_CALL(*this, connect_done()).WillByDefault(testing::Invoke(this, &TestClient::on_connect_done));
        ON_CALL(*this, disconnect_done()).WillByDefault(testing::Invoke(this, &TestClient::on_disconnect_done));
    }

    std::shared_ptr<MockLogger> logger;
    mir::client::MirRpcChannel channel;
    mir::protobuf::DisplayServer::Stub display_server;
    mir::protobuf::SurfaceParameters surface_parameters;
    mir::protobuf::Surface surface;
    mir::protobuf::Void ignored;

    MOCK_METHOD0(connect_done, void ());
    MOCK_METHOD0(disconnect_done, void ());

    void on_connect_done()
    {
        connect_done_called.store(true);

        auto old = connect_done_count.load();

        while (!connect_done_count.compare_exchange_weak(old, old+1));
    }

    void on_disconnect_done()
    {
        disconnect_done_called.store(true);

        auto old = disconnect_done_count.load();

        while (!disconnect_done_count.compare_exchange_weak(old, old+1));
    }

    void wait_for_connect_done()
    {
        for (int i = 0; !connect_done_called.load() && i < 100; ++i)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            std::this_thread::yield();
        }
        connect_done_called.store(false);
    }
    void wait_for_disconnect_done()
    {
        for (int i = 0; !disconnect_done_called.load() && i < 100; ++i)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            std::this_thread::yield();
        }
        disconnect_done_called.store(false);
    }

    void wait_for_connect_count(int count)
    {
        for (int i = 0; count != connect_done_count.load() && i < 10000; ++i)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            std::this_thread::yield();
        }
    }
    void wait_for_disconnect_count(int count)
    {
        for (int i = 0; count != disconnect_done_count.load() && i < 10000; ++i)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            std::this_thread::yield();
        }
    }

    std::atomic<bool> connect_done_called;
    std::atomic<bool> disconnect_done_called;

    std::atomic<int> connect_done_count;
    std::atomic<int> disconnect_done_count;
};

struct BasicTestFixture : public ::testing::Test
{
    void SetUp()
    {
        ::testing::Mock::VerifyAndClearExpectations(server.factory.get());
    }

    void TearDown()
    {
        server.comm.stop();
    }

    TestServer server;
};


struct ProtobufAsioCommunicatorTestFixture : public BasicTestFixture
{
    TestClient client;
};


struct ProtobufAsioMultiClientCommunicatorTestFixture : public BasicTestFixture
{
    static int const number_of_clients = 10;

    TestClient client[number_of_clients];
};
}

TEST_F(ProtobufAsioCommunicatorTestFixture, connection_results_in_a_callback)
{
    EXPECT_CALL(*server.factory, make_ipc_server()).Times(1);
    server.comm.start();

    EXPECT_CALL(client, connect_done()).Times(1);

    client.display_server.create_surface(
        0,
        &client.surface_parameters,
        &client.surface,
        google::protobuf::NewCallback(&client, &TestClient::connect_done));

    client.wait_for_connect_done();

    server.expect_session_count(1);
}

TEST_F(ProtobufAsioCommunicatorTestFixture,
        a_connection_attempt_results_in_a_session_being_connected)
{
    EXPECT_CALL(*server.factory, make_ipc_server()).Times(1);
    server.comm.start();

    EXPECT_CALL(client, connect_done()).Times(1);

    client.display_server.create_surface(
        0,
        &client.surface_parameters,
        &client.surface,
        google::protobuf::NewCallback(&client, &TestClient::connect_done));

    client.wait_for_connect_done();

    server.expect_connected_session_count(1);
}

TEST_F(ProtobufAsioCommunicatorTestFixture,
       each_connection_attempt_results_in_a_new_session_being_created)
{
    EXPECT_CALL(*server.factory, make_ipc_server()).Times(1);
    server.comm.start();

    int const connection_count{5};

    EXPECT_CALL(client, connect_done()).Times(connection_count);

    for (int i = 0; i != connection_count; ++i)
    {
        client.display_server.create_surface(
            0,
            &client.surface_parameters,
            &client.surface,
            google::protobuf::NewCallback(&client, &TestClient::connect_done));

        client.wait_for_connect_done();
    }

    server.expect_session_count(connection_count);
    server.expect_connected_session_count(connection_count);
}

TEST_F(ProtobufAsioCommunicatorTestFixture,
       connect_then_disconnect_a_session)
{
    EXPECT_CALL(*server.factory, make_ipc_server()).Times(1);
    server.comm.start();

    EXPECT_CALL(client, connect_done()).Times(1);
    client.display_server.create_surface(
        0,
        &client.surface_parameters,
        &client.surface,
        google::protobuf::NewCallback(&client, &TestClient::connect_done));

    client.wait_for_connect_done();

    server.expect_connected_session_count(1);

    EXPECT_CALL(client, disconnect_done()).Times(1);
    client.display_server.disconnect(
        0,
        &client.ignored,
        &client.ignored,
        google::protobuf::NewCallback(&client, &TestClient::disconnect_done));

    client.wait_for_disconnect_done();
    server.expect_connected_session_count(0);
}

TEST_F(ProtobufAsioCommunicatorTestFixture,
       double_disconnection_attempt_has_no_effect)
{
    EXPECT_CALL(*server.factory, make_ipc_server()).Times(1);
    server.comm.start();

    EXPECT_CALL(client, connect_done()).Times(1);
    client.display_server.create_surface(
        0,
        &client.surface_parameters,
        &client.surface,
        google::protobuf::NewCallback(&client, &TestClient::connect_done));

    client.wait_for_connect_done();
    server.expect_connected_session_count(1);

    EXPECT_CALL(client, disconnect_done()).Times(1);
    client.display_server.disconnect(
        0,
        &client.ignored,
        &client.ignored,
        google::protobuf::NewCallback(&client, &TestClient::disconnect_done));

    client.wait_for_disconnect_done();
    server.expect_connected_session_count(0);

    EXPECT_CALL(*client.logger, error()).Times(testing::AtLeast(1));

    // We don't expect this to be called, so it can't auto destruct
    std::unique_ptr<google::protobuf::Closure> new_callback(google::protobuf::NewPermanentCallback(&client, &TestClient::disconnect_done));
    client.display_server.disconnect(0, &client.ignored, &client.ignored, new_callback.get());
    client.wait_for_disconnect_done();

    server.expect_connected_session_count(0);
}

TEST_F(ProtobufAsioMultiClientCommunicatorTestFixture,
       multiple_clients_can_connect_and_disconnect)
{
    EXPECT_CALL(*server.factory, make_ipc_server()).Times(number_of_clients);
    server.comm.start();

    for (int i = 0; i != number_of_clients; ++i)
    {
        EXPECT_CALL(client[i], connect_done()).Times(1);
        client[i].display_server.create_surface(
            0,
            &client[i].surface_parameters,
            &client[i].surface,
            google::protobuf::NewCallback(&client[i], &TestClient::connect_done));
        client[i].wait_for_connect_done();
    }

    server.expect_session_count(number_of_clients);
    server.expect_connected_session_count(number_of_clients);

    for (int i = 0; i != number_of_clients; ++i)
    {
        EXPECT_CALL(client[i], disconnect_done()).Times(1);
        client[i].display_server.disconnect(
            0,
            &client[i].ignored,
            &client[i].ignored,
            google::protobuf::NewCallback(&client[i], &TestClient::disconnect_done));
        client[i].wait_for_disconnect_done();
    }

    server.expect_session_count(number_of_clients);
    server.expect_connected_session_count(0);
}

TEST_F(ProtobufAsioMultiClientCommunicatorTestFixture,
       multiple_clients_can_connect_and_disconnect_asynchronously)
{
    EXPECT_CALL(*server.factory, make_ipc_server()).Times(number_of_clients);
    server.comm.start();

    for (int i = 0; i != number_of_clients; ++i)
    {
        EXPECT_CALL(client[i], connect_done()).Times(1);
        client[i].display_server.create_surface(
            0,
            &client[i].surface_parameters,
            &client[i].surface,
            google::protobuf::NewCallback(&client[i], &TestClient::connect_done));
    }

    for (int i = 0; i != number_of_clients; ++i)
    {
        client[i].wait_for_connect_done();
    }

    server.expect_session_count(number_of_clients);
    server.expect_connected_session_count(number_of_clients);

    for (int i = 0; i != number_of_clients; ++i)
    {
        EXPECT_CALL(client[i], disconnect_done()).Times(1);
        client[i].display_server.disconnect(
            0,
            &client[i].ignored,
            &client[i].ignored,
            google::protobuf::NewCallback(&client[i], &TestClient::disconnect_done));
    }

    for (int i = 0; i != number_of_clients; ++i)
    {
        client[i].wait_for_disconnect_done();
    }

    server.expect_session_count(number_of_clients);
    server.expect_connected_session_count(0);
}

TEST_F(ProtobufAsioCommunicatorTestFixture,
       each_connection_attempt_results_in_a_new_session_being_created_asynchronously)
{
    EXPECT_CALL(*server.factory, make_ipc_server()).Times(1);
    server.comm.start();

    int const connection_count{5};

    EXPECT_CALL(client, connect_done()).Times(connection_count);

    for (int i = 0; i != connection_count; ++i)
    {
        client.display_server.create_surface(
            0,
            &client.surface_parameters,
            &client.surface,
            google::protobuf::NewCallback(&client, &TestClient::connect_done));
    }

    server.expect_session_count(connection_count);
    server.expect_connected_session_count(connection_count);

    client.wait_for_connect_count(connection_count);
}
}
