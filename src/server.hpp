#pragma once

#include <boost/make_shared.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include "transmit.hpp"



typedef boost::asio::ip::tcp::socket socket_type;
typedef boost::shared_ptr<socket_type> socket_ptr;


template <class UserSessionCreator>
class server : public boost::enable_shared_from_this<server>
{
public:
    typedef server this_type;

public:
    static boost::shared_ptr<this_type> create(boost::asio::io_context& io_context, unsigned short port)
    {
        return boost::make_shared<this_type>(boost::ref(io_context), port);
    }

    server(boost::asio::io_context& io_context, unsigned short port)
        : io_context_(io_context)
        , acceptor_(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port))
    {
    }

    void set_user_session_creator(user_session_creator creator)
    {
        user_session_creator_ = creator;
    }

    void start()
    {
        io_context_.post(boost::bind(&this_type::async_accept, this));
    }

private:
    void async_accept()
    {
        socket_ptr socket = boost::make_shared<socket_type>(boost::ref(io_context_));
        acceptor_.async_accept(*socket,
                               boost::bind(&this_type::handle_accept, this,
                                           boost::asio::placeholders::error,
                                           socket));
    }

    void handle_accept(boost::system::error_code ec, socket_ptr socket)
    {
        if (!ec) {
            auto user_session = user_session_creator_();
            boost::shared_ptr<transmit> t = transmit::create(socket, user_session);
            t->start();
            async_accept();
        }
    }

private:
    boost::asio::io_context& io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    UserSessionCreator user_session_creator_;
};

