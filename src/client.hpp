#pragma once

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/make_shared.hpp>
#include "transmit.hpp"



typedef boost::asio::ip::tcp::resolver resolver_type;
typedef boost::asio::ip::tcp::socket socket_type;
typedef boost::shared_ptr<socket_type> socket_ptr;


template<class UserSession>
class client
{
public:
    typedef client this_type;

    client(boost::asio::io_context& io_context, std::string const& host, std::string const& port)
            : io_context_(io_context)
            , resolver_(io_context)
            , host_(host)
            , port_(port)
    {
    }

    void set_user_session(UserSession user_session)
    {
        user_session_ = user_session;
    }

    void start()
    {
        async_resolve();
    }

private:
    void async_resolve()
    {
        resolver_.async_resolve(host_, port_,
                                boost::bind(&this_type::handle_resolve, this,
                                            boost::asio::placeholders::error,
                                            boost::asio::placeholders::results));
    }

    void handle_resolve(const boost::system::error_code& ec, resolver_type::results_type const& results)
    {
        if (!ec && !results.empty()) {
            async_connect(results);
        }
    }

    void async_connect(resolver_type::results_type const& endpoints)
    {
        socket_ptr socket = boost::make_shared<socket_type>(boost::ref(io_context_));
        boost::asio::async_connect(*socket, endpoints,
                              boost::bind(&this_type::handle_connect, this,
                                          boost::asio::placeholders::error,
                                          socket));
    }

    void handle_connect(const boost::system::error_code& ec, socket_ptr socket)
    {
        if (!ec) {
            boost::shared_ptr<transmit> t = transmit::create(socket, user_session_);
            t->start();
        }
    }

private:
    boost::asio::io_context& io_context_;
    resolver_type resolver_;
    std::string host_;
    std::string port_;
    UserSession user_session_;
};
