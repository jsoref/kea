// Copyright (C) 2014 Internet Systems Consortium, Inc. ("ISC")
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
// LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
// OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
// PERFORMANCE OF THIS SOFTWARE.

#ifndef DHCP6_CLIENT_H
#define DHCP6_CLIENT_H

#include <asiolink/io_address.h>
#include <dhcp/duid.h>
#include <dhcp/option.h>
#include <dhcp6/tests/dhcp6_test_utils.h>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>

namespace isc {
namespace dhcp {
namespace test {


/// @brief DHCPv6 client used for unit testing.
///
/// This class implements a DHCPv6 "client" which interoperates with the
/// @c NakedDhcpv6Srv class. It calls @c NakedDhcpv6Srv::fakeRecive to
/// deliver client messages to the server for processing. The server places
/// the response in the @c NakedDhcpv6Srv::fake_sent_ container. The client
/// pops messages from this container which simulates reception of the
/// response from the server.
///
/// The client maintains the leases it acquired from the server. If it has
/// acquired the lease as a result of SARR exchange, it will use this lease
/// when the Rebind process is triggered by the unit test.
///
/// The client exposes a set of functions which simulate different exchange
/// types between the client and the server. It also provides the access to
/// the objects encapsulating responses from the server so as it is possible
/// to verify from the unit test that the server's response is correct.
///
/// @todo This class has been implemented to simplify the structure of the
/// unit test and to make unit tests code self-explanatory. Currently,
/// this class is mostly used by the unit tests which test Rebind processing
/// logic. At some point we may want to use this class to test some other
/// message types, e.g. Renew, in which case it may need to be extended.
/// Also, once we implement the support for multiple IAAddr and IAPrefix
/// options within single IA, the logic which maintains leases will have
/// to be extended to support it.
class Dhcp6Client : public boost::noncopyable {
public:

    /// @brief Holds an information about single lease.
    struct LeaseInfo {
        /// @brief A structure describing the lease.
        Lease6 lease_;

        /// @brief Holds the last status code that server has sent for
        /// the particular lease.
        uint16_t status_code_;
    };

    /// @brief Holds the current client configuration obtained from the
    /// server over DHCP.
    ///
    /// Currently it simply contains the collection of leases acquired.
    struct Configuration {
        std::vector<LeaseInfo> leases_;
    };

    /// @brief Holds the DHCPv6 messages taking part in transaction between
    /// the client and the server.
    struct Context {
        /// @brief Holds the last sent message from the client to the server.
        Pkt6Ptr query_;
        /// @brief Holds the last sent message by the server to the client.
        Pkt6Ptr response_;
    };

    /// @brief Creates a new client.
    ///
    /// This constructor initializes the class members to default values.
    Dhcp6Client();

    /// @brief Performs a 4-way echange between the client and the server.
    ///
    /// If the 4-way exchange is successful, the client should acquire leases
    /// according to the server's current configuration and the type of leases
    /// that have been requested (IA_NA, IA_PD).
    ///
    /// The leases acquired are accessible through the @c config_ member.
    void doSARR();

    /// @brief Send Solicit and receive Advertise.
    ///
    /// This function simulates the first transaction of the 4-way exchange,
    /// i.e. sends a Solicit to the server and receives Advertise. It doesn't
    /// set the lease configuration in the @c config_.
    void doSolicitAdvertise();

    /// @brief Sends a Rebind to the server and receives the Reply.
    ///
    /// This function simulates sending the Rebind message to the server and
    /// receiving server's response (if any). The client uses existing leases
    /// (either address or prefixes) and places them in the Rebind message.
    /// If the server responds to the Rebind (and extends the lease lifetimes)
    /// the current lease configuration is updated.
    void doRebind();

    /// @brief Sends Request to the server and receives Reply.
    ///
    /// This function simulates sending the Request message to the server and
    /// receiving server's response (if any). The client copies IA options
    /// from the current context (server's Advertise) to request acquisition
    /// of offered IAs. If the server responds to the Request (leases are
    /// acquired) the client's lease configuration is updated.
    void doRequestReply();

    /// @brief Simulates aging of leases by the specified number of seconds.
    ///
    /// This function moves back the time of acquired leases by the specified
    /// number of seconds. It is useful for checking whether the particular
    /// lease has been later updated (e.g. as a result of Rebind) as it is
    /// expected that the fresh lease has cltt set to "now" (not to the time
    /// in the past).
    void fastFwdTime(const uint32_t secs);

    /// @brief Returns DUID option used by the client.
    OptionPtr getClientId() const;

    /// @brief Returns current context.
    const Context& getContext() const {
        return (context_);
    }

    /// @brief Returns lease at specified index.
    ///
    /// @param at Index of the lease held by the client.
    /// @return A lease at the specified index.
    Lease6 getLease(const size_t at) const {
        return (config_.leases_[at].lease_);
    }

    /// @brief Returns status code set by the server for the lease.
    ///
    /// @param at Index of the lease held by the client.
    /// @return A status code for the lease at the specified index.
    uint16_t getStatusCode(const size_t at) const {
        return (config_.leases_[at].status_code_);
    }

    /// @brief Returns number of acquired leases.
    size_t getLeaseNum() const {
        return (config_.leases_.size());
    }

    /// @brief Returns the server that the client is communicating with.
    boost::shared_ptr<isc::test::NakedDhcpv6Srv> getServer() const {
        return (srv_);
    }

    /// @brief Modifies the client's DUID (adds one to it).
    ///
    /// The DUID should be modified to test negative scenarios when the client
    /// acquires a lease and tries to renew it with a different DUID. The server
    /// should detect the DUID mismatch and react accordingly.
    ///
    /// The DUID modification affects the value returned by the
    /// @c Dhcp6Client::getClientId
    void modifyDUID();

    /// @brief Sets destination address for the messages being sent by the
    /// client.
    ///
    /// By default, the client uses All_DHCP_Relay_Agents_and_Servers
    /// multicast address to communicate with the server. In certain cases
    /// it ay be desired that different address is used (e.g. unicast in Renew).
    /// This function sets the new address for all future exchanges with the
    /// server.
    ///
    /// @param dest_addr New destination address.
    void setDestAddress(const asiolink::IOAddress& dest_addr) {
        dest_addr_ = dest_addr;
    }

    /// @brief Place IA_NA options to request address assignment.
    ///
    /// This function configures the client to place IA_NA options in its
    /// Solicit messages to request the IPv6 address assignment.
    ///
    /// @param use Parameter which 'true' value indicates that client should
    /// request address assignment.
    void useNA(const bool use = true) {
        use_na_ = use;
    }

    /// @brief Place IA_PD options to request prefix assignment.
    ///
    /// This function configures the client to place IA_PD options in its
    /// Solicit messages to request the IPv6 address assignment.
    ///
    /// @param use Parameter which 'true' value indicates that client should
    /// request prefix assignment.
    void usePD(const bool use = true) {
        use_pd_ = use;
    }

    /// @brief Simulate sending messages through a relay.
    ///
    /// @param use Parameter which 'true' value indicates that client should
    /// simulate sending messages via relay.
    void useRelay(const bool use = true) {
        use_relay_ = use;
    }

    /// @brief Lease configuration obtained by the client.
    Configuration config_;

    /// @brief Link address of the relay to be used for relayed messages.
    asiolink::IOAddress relay_link_addr_;

private:

    /// @brief Applies the new leases for the client.
    ///
    /// This method is called when the client obtains a new configuration
    /// from the server in the Reply message. This function adds new leases
    /// or replaces existing ones.
    ///
    /// @param reply Server response.
    void applyConfiguration(const Pkt6Ptr& reply);

    /// @brief Applies configuration for the single lease.
    ///
    /// This method is called by the @c Dhcp6Client::applyConfiguration for
    /// each individual lease.
    ///
    /// @param lease_info Structure holding new lease information.
    void applyLease(const LeaseInfo& lease_info);

    /// @brief Copy IA options from one message to another.
    ///
    /// This method copies IA_NA and IA_PD options from one message to another.
    /// It is useful when the client needs to construct the Request message
    /// using addresses and prefixes returned by the client in Advertise.
    ///
    /// @param source Message from which IA options will be copied.
    /// @param dest Message to which IA options will be copied.
    void copyIAs(const Pkt6Ptr& source, const Pkt6Ptr& dest);

    /// @brief Creates IA options from existing configuration.
    ///
    /// This method iterates over existing leases that client acquired and
    /// places corresponding IA_NA or IA_PD options into a specified message.
    /// This is useful to construct Renew or Rebind message from the existing
    /// configuration that client has obtained using 4-way exchange.
    ///
    /// @param dest Message to which the IA options will be added.
    void copyIAsFromLeases(const Pkt6Ptr& dest) const;

    /// @brief Creates client's side DHCP message.
    ///
    /// @param msg_type Type of the message to be created.
    /// @return An instance of the message created.
    Pkt6Ptr createMsg(const uint8_t msg_type);

    /// @brief Generates DUID for the client.
    ///
    /// @param duid_type Type of the DUID. Currently, only LLT is accepted.
    /// @return Object encapsulating a DUID.
    DuidPtr generateDUID(DUID::DUIDType duid_type) const;

    /// @brief Simulates reception of the message from the server.
    ///
    /// @return Received message.
    Pkt6Ptr receiveOneMsg();

    /// @brief Simulates sending a message to the server.
    ///
    /// @param msg Message to be sent.
    void sendMsg(const Pkt6Ptr& msg);

    /// @brief Current context (sent and received message).
    Context context_;

    /// @biref Current transaction id (altered on each send).
    uint32_t curr_transid_;

    /// @brief Currently use destination address.
    asiolink::IOAddress dest_addr_;

    /// @brief Currently used DUID.
    DuidPtr duid_;

    /// @brief Currently used link local address.
    asiolink::IOAddress link_local_;

    /// @brief Pointer to the server that the client is communicating with.
    boost::shared_ptr<isc::test::NakedDhcpv6Srv> srv_;

    bool use_na_;    ///< Enable address assignment.
    bool use_pd_;    ///< Enable prefix delegation.
    bool use_relay_; ///< Enable relaying messages to the server.
};

} // end of namespace isc::dhcp::test
} // end of namespace isc::dhcp
} // end of namespace isc

#endif // DHCP6_CLIENT
