#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();

	if(_ip2ethernet_maps.count(next_hop_ip)) {
		_frames_out.push(create_ethernet_frame(dgram, _ip2ethernet_maps[next_hop_ip].ethernet_address));
		return;
	}

	struct unsent_datagram datagram{};
	datagram.dgram = dgram;
	datagram.next_hop_ip = next_hop_ip;
	_unsent_datagrams.push_back(datagram);
	arp_request(next_hop_ip);
	return;
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
	if(frame.header().type == EthernetHeader::TYPE_IPv4 && frame.header().dst == _ethernet_address) {
		IPv4Datagram ipv4_datagram;
		if(ipv4_datagram.parse(frame.payload()) == ParseResult::NoError)
			return ipv4_datagram;
		else
			return std::nullopt;
	} else if(frame.header().type == EthernetHeader::TYPE_ARP) {
		ARPMessage arp{};
		if(arp.parse(frame.payload()) != ParseResult::NoError)
			return std::nullopt;
		if(_ip2ethernet_maps.count(arp.sender_ip_address)) {
			struct ip2ethernet i2e{arp.sender_ethernet_address, 0};
			_ip2ethernet_maps[arp.sender_ip_address] = i2e;
		} else {
			_ip2ethernet_maps[arp.sender_ip_address].ethernet_address = arp.sender_ethernet_address;
			_ip2ethernet_maps[arp.sender_ip_address].live_time = _timer + 30000;
		}
		if(arp.target_ip_address == _ip_address.ipv4_numeric()) {
			if(arp.opcode == ARPMessage::OPCODE_REPLY)
				retransmit_datagram(arp.sender_ip_address, arp.sender_ethernet_address);
			if(arp.opcode == ARPMessage::OPCODE_REQUEST)
				arp_reply(arp);
		}
	}
	return std::nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
	_timer += ms_since_last_tick;

	//! Refresh all the out-dated ip-ethernet map
	for(auto it = _ip2ethernet_maps.begin(); it != _ip2ethernet_maps.end();) {
		if(it->second.live_time < _timer)
			it = _ip2ethernet_maps.erase(it);
		else
			++it;
	}
}

EthernetFrame NetworkInterface::create_ethernet_frame(const InternetDatagram &dgram, const EthernetAddress next_hop_ethernet) {
	EthernetFrame eframe{};
	struct EthernetHeader header{};
	header.dst = next_hop_ethernet;
	header.src = _ethernet_address;
	header.type = EthernetHeader::TYPE_IPv4;
	eframe.header() = header;
	eframe.payload() = dgram.serialize();

	return eframe;
}

void NetworkInterface::arp_request(const uint32_t broadcast_ip) {
	for(std::list<unresponse_arp>::iterator arp = _unresponse_arps.begin(); arp != _unresponse_arps.end(); arp++) {
		if(arp->broadcast_ip == broadcast_ip) {
			if(_timer <= arp->live_time) {
				return;
			} else {
				_unresponse_arps.erase(arp);
				break;
			}
		}
	}
	EthernetFrame eframe{};
	EthernetHeader header{};
	header.dst = ETHERNET_BROADCAST;
	header.src = _ethernet_address;
	header.type = EthernetHeader::TYPE_ARP;
	ARPMessage arp{};
	arp.sender_ethernet_address = _ethernet_address;
	arp.sender_ip_address = _ip_address.ipv4_numeric();
	arp.target_ip_address = broadcast_ip;
	arp.opcode = ARPMessage::OPCODE_REQUEST;
	eframe.header() = header;
	eframe.payload() = arp.serialize();

	_frames_out.push(eframe);

	struct unresponse_arp uarp{};
	uarp.eframe = eframe;
	uarp.broadcast_ip = broadcast_ip;
	uarp.live_time = _timer + 5000;
	_unresponse_arps.push_back(uarp);
}

void NetworkInterface::arp_reply(const ARPMessage old_arp) {
	EthernetFrame eframe{};
	EthernetHeader header{};
	header.dst = old_arp.sender_ethernet_address;
	header.src = _ethernet_address;
	header.type = EthernetHeader::TYPE_ARP;
	ARPMessage arp{};
	arp.sender_ethernet_address = _ethernet_address;
	arp.target_ethernet_address = old_arp.sender_ethernet_address;
	arp.sender_ip_address = _ip_address.ipv4_numeric();
	arp.target_ip_address = old_arp.sender_ip_address;
	arp.opcode = ARPMessage::OPCODE_REPLY;
	eframe.header() = header;
	eframe.payload() = arp.serialize();

	_frames_out.push(eframe);
}

void NetworkInterface::retransmit_datagram(uint32_t ip_address, EthernetAddress ethernet_address) {
	for(std::list<unsent_datagram>::iterator dgram = _unsent_datagrams.begin(); dgram != _unsent_datagrams.end(); ++dgram) {
		if(dgram->next_hop_ip == ip_address) {
			_frames_out.push(create_ethernet_frame(dgram->dgram, ethernet_address));
			dgram = _unsent_datagrams.erase(dgram);
			continue;
		}
	}
}
