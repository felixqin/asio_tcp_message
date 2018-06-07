#pragma once

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include "message.hpp"



template <class UserSession>
class transmit : public boost::enable_shared_from_this<transmit>
{
public:
    typedef transmit this_type;
    typedef boost::asio::ip::tcp::socket socket_type;
    typedef boost::shared_ptr<socket_type> socket_ptr;

public:
    static boost::shared_ptr<this_type> create(socket_ptr socket, ISessionPtr user_session)
    {
        return boost::make_shared<this_type>(socket, user_session);
    }

    transmit(socket_ptr socket, ISessionPtr user_session)
            : controller_(*this)
            , socket_(socket)
            , strand_(socket->get_io_context())
            , user_session_(user_session)
            , stop_flag_(false)
    {
    }

    ~transmit()
    {
        user_session_->on_exit();
    }

    void start()
    {
        user_session_->on_enter(controller_);
        async_read_header();
    }

private:
    void async_read_header()
    {
        boost::asio::async_read(*socket_, boost::asio::buffer(&header_, sizeof(header_)),
                                boost::bind(&this_type::handle_read_header, shared_from_this(),
                                            boost::asio::placeholders::error,
                                            boost::asio::placeholders::bytes_transferred));
    }

    void handle_read_header(boost::system::error_code ec, size_t bytes_transferred)
    {
        if (!ec && bytes_transferred == sizeof(header_)) {
            network_to_host(header_);
            async_read_payload();
        }
    }

    void async_read_payload()
    {
        boost::asio::async_read(*socket_, boost::asio::buffer(payload.data(), payload.size()),
                                boost::bind(&this_type::handle_read_payload, shared_from_this(),
                                            boost::asio::placeholders::error,
                                            boost::asio::placeholders::bytes_transferred,
                                            payload));
    }

    void handle_read_payload(boost::system::error_code ec, size_t bytes_transferred, CPayload const& payload)
    {
        if (!ec && bytes_transferred == payload.size()) {
            user_session_->onMessage((MessageCode )header_.message, header_.peer, header_.request, payload);
            if (!stop_flag_) {
                async_read_header();
            }
        }
    }

private:
    void async_write_header(MessageCode code, uint32_t peer, uint32_t request, uint32_t payload_len)
    {
        host_to_network(*header);
        boost::asio::async_write(*socket_, boost::asio::buffer(header, sizeof(*header)),
                                 boost::bind(&this_type::handle_write_header, shared_from_this(),
                                             boost::asio::placeholders::error,
                                             boost::asio::placeholders::bytes_transferred,
                                             header));
    }

    void handle_write_header(boost::system::error_code ec, size_t bytes_transferred, MessageHeader* header)
    {
        free(header);
    }

    void async_write_payload(CPayload const& payload)
    {
        boost::asio::async_write(*socket_, boost::asio::buffer(payload.data(), payload.size()),
                                 boost::bind(&this_type::handle_write_payload, shared_from_this(),
                                             boost::asio::placeholders::error,
                                             boost::asio::placeholders::bytes_transferred,
                                             payload));
    }

    void handle_write_payload(boost::system::error_code ec, size_t bytes_transferred, CPayload const& payload)
    {
    }

    void async_write_message(MessageCode code, uint32_t peer, uint32_t request, CPayload const& payload)
    {
        // does here need lock for sync?
        async_write_header(code, peer, request, payload.size());
        async_write_payload(payload);
    }

private:
    class controller : public IController
    {
    public:
        typedef transmit owner_type;

        explicit controller(owner_type& owner)
                : owner_(owner)
        {
        }

        void post(MessageCode code, uint32_t peer, uint32_t request, Payload const& payload)
        {
            owner_.strand_.post(
                    boost::bind(&owner_type::async_write_message, owner_.shared_from_this(),
                                code, peer, request, payload));
        }

        void stop()
        {
            owner_.stop_flag_ = false;
        }

    private:
        owner_type& owner_;
    };

private:
    controller controller_;
    socket_ptr socket_;
    boost::asio::io_service::strand strand_;
    UserSession user_session_;
    bool stop_flag_;
};

